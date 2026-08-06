/**
 * @file    tlv_worker.c
 * @brief   TLV 增量属性处理线程实现 (v2: 查表分发 + 多段消息)
 *
 * 从 queue_tlv_property 消费 TLV 消息，解析结构体并打印诊断信息。
 * 后续可扩展: 通过 MQTT device.property_report 增量上报。
 *
 * v2 优化:
 *   - TlvProcessor 查表替代 19 路 switch-case
 *   - tlv_property_send_multi() 多段消息合并发送
 *   - 消息格式: AA 55 | lenH lenL | TLV_CMD_PROPERTY (0x35) 或 TLV_CMD_FACTORY_DEVICE (0x36) | 结构体段序列
 */
#include "tlv_worker.h"
#include "tlv_codec.h"
#include "queue_worker.h"
#include "config.h"
#include "mqttssl_worker.h"  /* g_property_report_pending, property_report_collect() 等 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>

/* ============================================================================
 * 结构体类型标签 (在 mqttssl_worker.h 中定义)
 * 每个 struct_type 对应一个结构体的 TLV 序列化数据
 * ============================================================================
 * 设备/系统节点 (0x01~0x0A):
 *   TLV_STRUCT_DEVICE      = 0x01  → DeviceState
 *   TLV_STRUCT_NETWORK     = 0x02  → NetworkState
 *   TLV_STRUCT_POWER       = 0x03  → PowerState        例: 03 00 0C 01 00 01 01 02 00 01 55 03 00 01 00
 *   TLV_STRUCT_DOOR        = 0x04  → DoorState         例: 04 00 13 01 00 01 01 02 00 01 00 03 00 08 00 00 00 00 00 00 00 00
 *   TLV_STRUCT_ENVIRONMENT = 0x05  → EnvironmentState
 *   TLV_STRUCT_ACTUATOR    = 0x06  → ActuatorState      (聚合 9 个子结构体)
 *   TLV_STRUCT_CAMERA      = 0x07  → CameraState
 *   TLV_STRUCT_SETTINGS    = 0x08  → SettingsState
 *   TLV_STRUCT_FIRMWARE    = 0x09  → FirmwareState
 *   TLV_STRUCT_STORAGE     = 0x0A  → StorageState
 *
 * 执行器子节点 (0x61~0x69):
 *   TLV_STRUCT_EXHAUST     = 0x61  → ActuatorExhaust    例: 61 00 0B 01 00 01 01 02 00 01 03 03 00 01 01
 *   TLV_STRUCT_LIGHT       = 0x62  → ActuatorLight      例: 62 00 0A 01 00 01 01 02 00 01 50 03 00 01 01
 *   TLV_STRUCT_UVB         = 0x63  → ActuatorUvb        例: 63 00 0D 01 00 01 01 02 00 01 02 03 00 01 04 04 00 01 01
 *   TLV_STRUCT_ANION       = 0x64  → ActuatorAnion      例: 64 00 08 01 00 01 01 02 00 01 01
 *   TLV_STRUCT_PLASMA      = 0x65  → ActuatorPlasma     例: 65 00 08 01 00 01 01 02 00 01 01
 *   TLV_STRUCT_CLIMATE     = 0x66  → ActuatorClimate    例: 66 00 16 01 00 01 01 02 00 08 40 3E 00 00 00 00 00 00 03 00 01 01 04 00 01 01
 *   TLV_STRUCT_HUMIDIFIER  = 0x67  → ActuatorHumidifier
 *   TLV_STRUCT_INLET_FAN   = 0x68  → ActuatorInletFan   例: 68 00 0A 01 00 01 01 02 00 01 02 03 00 01 01
 *   TLV_STRUCT_FILTER      = 0x69  → ActuatorFilter     例: 69 00 08 01 00 01 50 02 00 01 00
 *   TLV_STRUCT_FACTORY_DEVICE = 0x6A → FactoryDeviceTlv   (出厂烧录上报, 6 个 STR 字段)
 */

/* ============================================================================
 * 各结构体打印函数
 * ============================================================================ */

static void print_device_state(const DeviceState *s)
{
    printf_info("[TLV] DeviceState: bind=%s work=%s fault=%s lifecycle=%s faults=%d\n",
                bind_status_to_str(s->bind_status),
                work_status_to_str(s->work_status),
                fault_level_to_str(s->fault_summary),
                lifecycle_status_to_str(s->lifecycle_status),
                s->faults_count);
}

static void print_network_state(const NetworkState *s)
{
    printf_info("[TLV] NetworkState: type=%s rssi=%ddBm ip=%s mqtt=%d\n",
                network_type_to_str(s->network_type),
                s->rssi, s->ip, s->mqtt_connected);
}

static void print_power_state(const PowerState *s)
{
    printf_info("[TLV] PowerState: power_mode=%s battery=%d%% charging=%d\n",
                power_mode_to_str(s->power_mode),
                s->battery_percent, s->charging);
}

static void print_door_state(const DoorState *s)
{
    printf_info("[TLV] DoorState: door=%s lock=%s last_open=%.0f\n",
                door_status_to_str(s->door_status),
                lock_status_to_str(s->lock_status),
                s->last_open_time);
}

static void print_environment_state(const EnvironmentState *s)
{
    printf_info("[TLV] Environment: temp=%.1fC humidity=%.1f%% pm25=%.1f co2=%.0f\n",
                s->temperature, s->humidity, s->pm25, s->co2);
}

static void print_actuator_exhaust(const ActuatorExhaust *s)
{
    printf_info("[TLV] Exhaust: enabled=%d speed=%d interval=%.1fh running=%d\n",
                s->enabled, s->speed_level, s->interval_hours, s->running);
}

static void print_actuator_light(const ActuatorLight *s)
{
    printf_info("[TLV] Light: enabled=%d brightness=%d running=%d\n",
                s->enabled, s->brightness_level, s->running);
}

static void print_actuator_uvb(const ActuatorUvb *s)
{
    printf_info("[TLV] UVB: enabled=%d level=%d daily_hours=%d running=%d\n",
                s->enabled, s->level, s->daily_hours, s->running);
}

static void print_actuator_anion(const ActuatorAnion *s)
{
    printf_info("[TLV] Anion: enabled=%d running=%d\n", s->enabled, s->running);
}

static void print_actuator_plasma(const ActuatorPlasma *s)
{
    printf_info("[TLV] Plasma: enabled=%d running=%d\n", s->enabled, s->running);
}

static void print_actuator_climate(const ActuatorClimate *s)
{
    printf_info("[TLV] Climate: enabled=%d target=%.1fC status=%s running=%d\n",
                s->enabled, s->target_celsius,
                climate_control_status_to_str(s->control_status),
                s->running);
}

static void print_actuator_humidifier(const ActuatorHumidifier *s)
{
    printf_info("[TLV] Humidifier: enabled=%d interval=%.1fh run_hours=%.1f running=%d liquid=%s\n",
                s->enabled, s->interval_hours, s->running_hours,
                s->running, liquid_status_to_str(s->liquid_status));
}

static void print_actuator_inlet_fan(const ActuatorInletFan *s)
{
    printf_info("[TLV] InletFan: enabled=%d speed=%d running=%d\n",
                s->enabled, s->speed_level, s->running);
}

static void print_actuator_filter(const ActuatorFilter *s)
{
    printf_info("[TLV] Filter: life=%d%% need_replace=%d\n",
                s->life_percent, s->need_replace);
}

static void print_actuator_state(const ActuatorState *s)
{
    printf_info("[TLV] ActuatorState: exhaust(en=%d spd=%d int=%.1f run=%d) "
                "light(en=%d bri=%d run=%d) uvb(en=%d lv=%d dh=%d run=%d)\n",
                s->exhaust.enabled, s->exhaust.speed_level,
                s->exhaust.interval_hours, s->exhaust.running,
                s->light.enabled, s->light.brightness_level, s->light.running,
                s->uvb.enabled, s->uvb.level, s->uvb.daily_hours, s->uvb.running);
    printf_info("[TLV] ActuatorState: anion(en=%d run=%d) plasma(en=%d run=%d) "
                "climate(en=%d tgt=%.1f st=%s run=%d)\n",
                s->anion.enabled, s->anion.running,
                s->plasma.enabled, s->plasma.running,
                s->climate.enabled, s->climate.target_celsius,
                climate_control_status_to_str(s->climate.control_status),
                s->climate.running);
    printf_info("[TLV] ActuatorState: humidifier(en=%d int=%.1f rh=%.1f run=%d liq=%s) "
                "inlet(en=%d spd=%d run=%d) filter(life=%d%% repl=%d)\n",
                s->humidifier.enabled, s->humidifier.interval_hours,
                s->humidifier.running_hours, s->humidifier.running,
                liquid_status_to_str(s->humidifier.liquid_status),
                s->inlet_fan.enabled, s->inlet_fan.speed_level,
                s->inlet_fan.running,
                s->filter.life_percent, s->filter.need_replace);
}

static void print_camera_state(const CameraState *s)
{
    printf_info("[TLV] Camera: status=%s stream=%s provider=%s codec=%s "
                "res=%s fps=%.1f audio=%d privacy=%d\n",
                camera_status_to_str(s->camera_status),
                stream_status_to_str(s->stream_status),
                s->provider, video_codec_to_str(s->video_codec),
                resolution_to_str(s->resolution),
                s->fps, s->audio_enabled, s->privacy_mode);
    if (s->last_session_id[0])
        printf_info("[TLV] Camera: session_id=%s error_code=%s\n",
                    s->last_session_id, s->last_error_code);
}

static void print_settings_state(const SettingsState *s)
{
    printf_info("[TLV] Settings: temp_unit=%s display(bri=%d timeout=%d mode=%s) "
                "volume=%d lang=%s password=%d\n",
                s->temperature_unit,
                s->display.brightness_percent, s->display.screen_timeout_minutes,
                screen_mode_to_str(s->display.screen_mode),
                s->volume_percent, s->language, s->local_password_enabled);
}

static void print_firmware_state(const FirmwareState *s)
{
    printf_info("[TLV] Firmware: fw=%s hw=%s config_ver=%d\n",
                s->firmware_version, s->hardware_version, s->config_version);
}

static void print_storage_state(const StorageState *s)
{
    printf_info("[TLV] Storage: total=%dMB free=%dMB\n",
                s->storage_total_mb, s->storage_free_mb);
}

static void print_factory_device_tlv(const FactoryDeviceTlv *s)
{
    printf_info("[TLV] FactoryDevice: country=%s product_key=%s "
                "model=%s hw=%s fw=%s\n",
                s->sales_country_code, s->product_key,
                s->model, s->hardware_version, s->firmware_version);
}

/* ============================================================================
 * v2: TlvProcessor 查表 — 替代 19 路 switch-case
 * 每个 entry: { struct_type, unpack函数, print函数 }
 * 新增 struct_type 只需在此表中加一行
 * ============================================================================ */

/** 所有结构体的联合体，用于栈上分配 (避免重复 malloc) */
typedef union {
    DeviceState        device;
    NetworkState       network;
    PowerState         power;
    DoorState          door;
    EnvironmentState   environment;
    ActuatorState      actuator;
    CameraState        camera;
    SettingsState      settings;
    FirmwareState      firmware;
    StorageState       storage;
    ActuatorExhaust    exhaust;
    ActuatorLight      light;
    ActuatorUvb        uvb;
    ActuatorAnion      anion;
    ActuatorPlasma     plasma;
    ActuatorClimate    climate;
    ActuatorHumidifier humidifier;
    ActuatorInletFan   inlet_fan;
    ActuatorFilter     filter;
    FactoryDeviceTlv   factory_device;
} TlvStructUnion;

/** TLV 处理器入口: 一个 struct_type 对应一个处理器 */
typedef struct {
    uint8_t  struct_type;
    int    (*unpack)(void *dst, const uint8_t *buf, int buf_size);
    void   (*print)(const void *src);
    int    (*handler)(const void *src);  /* 业务处理回调, 可为 NULL */
} TlvProcessor;

static const TlvProcessor g_tlv_processors[] = {
    { TLV_STRUCT_DEVICE,      (int(*)(void*,const uint8_t*,int))device_state_tlv_unpack,      (void(*)(const void*))print_device_state,      NULL },
    { TLV_STRUCT_NETWORK,     (int(*)(void*,const uint8_t*,int))network_state_tlv_unpack,     (void(*)(const void*))print_network_state,     NULL },
    { TLV_STRUCT_POWER,       (int(*)(void*,const uint8_t*,int))power_state_tlv_unpack,       (void(*)(const void*))print_power_state,       NULL },
    { TLV_STRUCT_DOOR,        (int(*)(void*,const uint8_t*,int))door_state_tlv_unpack,        (void(*)(const void*))print_door_state,        NULL },
    { TLV_STRUCT_ENVIRONMENT, (int(*)(void*,const uint8_t*,int))environment_state_tlv_unpack, (void(*)(const void*))print_environment_state, NULL },
    { TLV_STRUCT_ACTUATOR,    (int(*)(void*,const uint8_t*,int))actuator_state_tlv_unpack,    (void(*)(const void*))print_actuator_state,    NULL },
    { TLV_STRUCT_CAMERA,      (int(*)(void*,const uint8_t*,int))camera_state_tlv_unpack,      (void(*)(const void*))print_camera_state,      NULL },
    { TLV_STRUCT_SETTINGS,    (int(*)(void*,const uint8_t*,int))settings_state_tlv_unpack,    (void(*)(const void*))print_settings_state,    NULL },
    { TLV_STRUCT_FIRMWARE,    (int(*)(void*,const uint8_t*,int))firmware_state_tlv_unpack,    (void(*)(const void*))print_firmware_state,    NULL },
    { TLV_STRUCT_STORAGE,     (int(*)(void*,const uint8_t*,int))storage_state_tlv_unpack,     (void(*)(const void*))print_storage_state,     NULL },
    { TLV_STRUCT_EXHAUST,     (int(*)(void*,const uint8_t*,int))actuator_exhaust_tlv_unpack,  (void(*)(const void*))print_actuator_exhaust,  NULL },
    { TLV_STRUCT_LIGHT,       (int(*)(void*,const uint8_t*,int))actuator_light_tlv_unpack,    (void(*)(const void*))print_actuator_light,    NULL },
    { TLV_STRUCT_UVB,         (int(*)(void*,const uint8_t*,int))actuator_uvb_tlv_unpack,      (void(*)(const void*))print_actuator_uvb,      NULL },
    { TLV_STRUCT_ANION,       (int(*)(void*,const uint8_t*,int))actuator_anion_tlv_unpack,    (void(*)(const void*))print_actuator_anion,    NULL },
    { TLV_STRUCT_PLASMA,      (int(*)(void*,const uint8_t*,int))actuator_plasma_tlv_unpack,   (void(*)(const void*))print_actuator_plasma,   NULL },
    { TLV_STRUCT_CLIMATE,     (int(*)(void*,const uint8_t*,int))actuator_climate_tlv_unpack,  (void(*)(const void*))print_actuator_climate,  NULL },
    { TLV_STRUCT_HUMIDIFIER,  (int(*)(void*,const uint8_t*,int))actuator_humidifier_tlv_unpack,(void(*)(const void*))print_actuator_humidifier,NULL },
    { TLV_STRUCT_INLET_FAN,   (int(*)(void*,const uint8_t*,int))actuator_inlet_fan_tlv_unpack,(void(*)(const void*))print_actuator_inlet_fan,NULL },
    { TLV_STRUCT_FILTER,      (int(*)(void*,const uint8_t*,int))actuator_filter_tlv_unpack,   (void(*)(const void*))print_actuator_filter,   NULL },
    { TLV_STRUCT_FACTORY_DEVICE, (int(*)(void*,const uint8_t*,int))factory_device_tlv_unpack, (void(*)(const void*))print_factory_device_tlv, (int(*)(const void*))handle_factory_device_tlv },
};
#define TLV_PROCESSOR_COUNT (sizeof(g_tlv_processors) / sizeof(g_tlv_processors[0]))

static const TlvProcessor *tlv_find_processor(uint8_t struct_type)
{
    for (int i = 0; i < TLV_PROCESSOR_COUNT; i++) {
        if (g_tlv_processors[i].struct_type == struct_type)
            return &g_tlv_processors[i];
    }
    return NULL;
}

static int tlv_dispatch(uint8_t struct_type, const uint8_t *data, int data_len)
{
    const TlvProcessor *proc = tlv_find_processor(struct_type);
    if (!proc) {
        printf_info("[TLV] unknown struct_type=0x%02X data_len=%d\n",
                    struct_type, data_len);
        return -1;
    }

    TlvStructUnion u;
    if (proc->unpack(&u, data, data_len) < 0) return -1;
    proc->print(&u);
    if (proc->handler) proc->handler(&u);
    return 0;
}

/* ============================================================================
 * 发送 TLV 消息到队列 — tlv_property_send (单段)
 *
 * 消息格式: AA 55 | lenH lenL | TLV_CMD_PROPERTY (0x35) | 结构体段序列
 * 每个结构体段: [struct_type:1B][seg_len:2B BE][tlv_data:seg_len]
 *
 * 示例: 发送 PowerState (power_mode=1, battery=85%, charging=0)
 *   输入: struct_type=0x03, data_len=12, tlv_data=01 00 01 01 02 00 01 55 03 00 01 00
 *   输出: AA 55  00 0F  35  03  00 0C  01 00 01 01 02 00 01 55 03 00 01 00
 *         │      │      │  │    │    └──────────────────────────────────┘
 *         │      │      │  │    │              TLV 数据 (12B)
 *         │      │      │  │    └─ seg_len = 0x000C = 12
 *         │      │      │  └─ struct_type = 0x03 = PowerState
 *         │      │      └─ 命令字 TLV_CMD_PROPERTY (0x35)
 *         │      └─ lenH|lenL = 0x000F = 15 (3+12)
 *         └─ 帧头 AA 55
 * ============================================================================ */

int tlv_property_send(uint8_t struct_type, const uint8_t *tlv_data, int data_len)
{
    if (!tlv_data || data_len <= 0 || data_len > 65532) return -1;

    /* 结构体段 = 1B type + 2B seg_len + data */
    int seg_total = 3 + data_len;
    /* 总长度 = 5B header + 结构体段 */
    int total = 5 + seg_total;
    char *msg = (char *)malloc(total);
    if (!msg) return -1;

    /* 帧头: AA 55 | lenH lenL | TLV_CMD_PROPERTY (0x35) */
    msg[0] = 0xAA;
    msg[1] = 0x55;
    msg[2] = (char)((seg_total >> 8) & 0xFF);  /* lenH */
    msg[3] = (char)(seg_total & 0xFF);          /* lenL */
    msg[4] = TLV_CMD_PROPERTY;                   /* 命令字 */
    msg[5] = (char)struct_type;
    msg[6] = (char)((data_len >> 8) & 0xFF);
    msg[7] = (char)(data_len & 0xFF);
    memcpy(msg + 8, tlv_data, data_len);

    int ret = packet_data_to_queue(msg, "queue_tlv_property",
                                    queue_tlv_property, false);
    if (ret != 0) {
        free(msg);
        return -1;
    }
    return 0;
}

/* ============================================================================
 * 发送 TLV 消息到队列 — tlv_property_send_multi (多段)
 *
 * 示例: 同时发送 PowerState(12B) + DoorState(19B)
 *   输入: struct_types = {0x03, 0x04}
 *         tlv_data[0]  = 01 00 01 01 02 00 01 55 03 00 01 00   (PowerState, 12B)
 *         tlv_data[1]  = 01 00 01 01 02 00 01 00 03 00 08 00 00 00 00 00 00 00 00 (DoorState, 19B)
 *         data_lens    = {12, 19}, count = 2
 *   输出: AA 55  00 25  35  03  00 0C  01 00 01 01 02 00 01 55 03 00 01 00  04  00 13  01 00 01 01 02 00 01 00 03 00 08 00 00 00 00 00 00 00 00
 *         │      │      │  │    │    └──────────────────────────────────┘ │  │    └────────────────────────────────────────────────────────┘
 *         │      │      │  │    │              PowerState (12B)           │  │                   DoorState (19B)
 *         │      │      │  │    └─ seg_len = 0x000C = 12                  │  └─ seg_len = 0x0013 = 19
 *         │      │      │  └─ struct_type = 0x03 = PowerState             └─ struct_type = 0x04 = DoorState
 *         │      │      └─ 命令字 TLV_CMD_PROPERTY (0x35)
 *         │      └─ lenH|lenL = 0x0025 = 37 (3+12+3+19)
 *         └─ 帧头 AA 55
 * ============================================================================ */

int tlv_property_send_multi(const uint8_t *struct_types,
                            const uint8_t *const *tlv_data,
                            const int *data_lens, int count)
{
    if (!struct_types || !tlv_data || !data_lens || count <= 0) return -1;

    /* 计算所有结构体段的总字节数 */
    int seg_total = 0;
    for (int i = 0; i < count; i++) {
        if (!tlv_data[i] || data_lens[i] <= 0 || data_lens[i] > 65532) return -1;
        seg_total += 3 + data_lens[i];  /* 1B type + 2B seg_len + data */
    }

    int total = 5 + seg_total;
    char *msg = (char *)malloc(total);
    if (!msg) return -1;

    /* 帧头: AA 55 | lenH lenL | TLV_CMD_PROPERTY (0x35) */
    msg[0] = 0xAA;
    msg[1] = 0x55;
    msg[2] = (char)((seg_total >> 8) & 0xFF);
    msg[3] = (char)(seg_total & 0xFF);
    msg[4] = TLV_CMD_PROPERTY;

    /* 依次写入各结构体段 */
    int offset = 5;
    for (int i = 0; i < count; i++) {
        msg[offset]     = (char)struct_types[i];
        msg[offset + 1] = (char)((data_lens[i] >> 8) & 0xFF);
        msg[offset + 2] = (char)(data_lens[i] & 0xFF);
        memcpy(msg + offset + 3, tlv_data[i], data_lens[i]);
        offset += 3 + data_lens[i];
    }

    int ret = packet_data_to_queue(msg, "queue_tlv_property",
                                    queue_tlv_property, false);
    if (ret != 0) {
        free(msg);
        return -1;
    }
    return 0;
}

/* ============================================================================
 * TLV 处理线程主循环 — process_tlv_worker_task
 *
 * 接收消息格式: AA 55 | lenH lenL | 命令字 | 结构体段序列
 * 每个结构体段: [struct_type:1B][seg_len:2B BE][tlv_data:seg_len]
 *
 * 分支处理:
 *   0x35 (TLV_CMD_PROPERTY):     多段循环解析, 查表分发各属性结构体
 *   0x36 (TLV_CMD_FACTORY_DEVICE): 单段直接解析, 仅处理 FactoryDeviceTlv
 *
 * 解析示例 (0x35): 收到 AA 55 00 0F 35 03 00 0C 01 00 01 01 02 00 01 55 03 00 01 00
 *   recvdata[0..1] = AA 55  → 帧头 ✓
 *   recvdata[2..3] = 00 0F  → payload_len = 15
 *   recvdata[4]    = 35     → cmd = TLV_CMD_PROPERTY → 多段循环解析
 *   recvdata[5]    = 03     → struct_type = 0x03 = PowerState (seg#0)
 *   recvdata[6..7] = 00 0C  → seg_len = 12
 *   recvdata[8..19]= TLV 数据 → tlv_dispatch(0x03, data, 12) → print_power_state
 *
 * 解析示例 (0x36): 收到 AA 55 00 43 36 6A 00 40 01 00 02 ...
 *   recvdata[0..1] = AA 55  → 帧头 ✓
 *   recvdata[4]    = 36     → cmd = TLV_CMD_FACTORY_DEVICE → 单段直接解析
 *   recvdata[5]    = 6A     → struct_type = 0x6A (校验通过)
 *   recvdata[6..7] = 00 40  → seg_len = 64
 *   recvdata[8..71]= TLV 数据 → tlv_dispatch(0x6A, data, 64) → handle_factory_device_tlv
 * ============================================================================ */

int process_tlv_worker_task(void)
{
    {
        /* 阻塞等待 TLV 消息 (1 秒超时) */
        char *recvdata = (char *)tiny_queue_pop_waittime(queue_tlv_property, 1);
        if (!recvdata) return -1;   /* 超时返回 NULL */

        /* 校验帧头: recvdata[0]=AA, recvdata[1]=55 */
        if ((uint8_t)recvdata[0] != 0xAA ||
            (uint8_t)recvdata[1] != 0x55) {
            printf_info("[TLV] invalid frame header: %02X %02X, skip\n",
                        (uint8_t)recvdata[0], (uint8_t)recvdata[1]);
            free(recvdata);
            return -1;
        }

        int payload_len = ((uint8_t)recvdata[2] << 8) | (uint8_t)recvdata[3];
        uint8_t cmd = (uint8_t)recvdata[4];

        /* ================================================================
         * 0x35: 设备属性上报 (TLV_CMD_PROPERTY) — 多段结构体循环解析
         *   解析每段后 collect 到全局缓冲区, 全部解析完设置 g_property_report_pending=1,
         *   由 MQTT 线程消费标志并发布 device.property_report 增量属性。
         * ================================================================ */
        if (cmd == TLV_CMD_PROPERTY) {
            if (payload_len < 3) {
                printf_info("[TLV-PROPERTY] invalid payload_len=%d\n", payload_len);
                free(recvdata);
                return -1;
            }

            /* 清空上一轮收集缓冲区 */
            property_report_collect_reset();

            const uint8_t *payload = (const uint8_t *)(recvdata + 5);
            int offset = 0;
            int seg_count = 0;

            while (offset + 3 <= payload_len) {
                uint8_t struct_type = payload[offset];
                int seg_len = (payload[offset + 1] << 8) | payload[offset + 2];
                offset += 3;

                if (offset + seg_len > payload_len) {
                    printf_info("[TLV-PROPERTY] seg_len=%d exceeds remaining=%d, skip\n",
                                seg_len, payload_len - offset);
                    break;
                }

                const uint8_t *tlv_data = payload + offset;
                printf_info("[TLV-PROPERTY] seg#%d struct_type=0x%02X data_len=%d\n",
                            seg_count, struct_type, seg_len);

                tlv_dispatch(struct_type, tlv_data, seg_len);

                /* 收集 TLV 段, 供 MQTT 线程发布 device.property_report */
                property_report_collect(struct_type, tlv_data, seg_len);

                offset += seg_len;
                seg_count++;
            }

            if (seg_count == 0) {
                printf_info("[TLV-PROPERTY] no valid segment parsed\n");
            } else {
                printf_info("[TLV-PROPERTY] parsed %d segments, triggering device.property_report\n",
                            seg_count);
                /* 设置标志, 通知 MQTT 线程发布 device.property_report */
                g_property_report_pending = 1;
            }
        }
        /* ================================================================
         * 0x36: 工厂设备信息上报 (TLV_CMD_FACTORY_DEVICE) — 单段直接解析
         * ================================================================ */
        else if (cmd == TLV_CMD_FACTORY_DEVICE) {
            if (payload_len < 3) {
                printf_info("[TLV-FACTORY] invalid payload_len=%d\n", payload_len);
                free(recvdata);
                return -1;
            }

            const uint8_t *payload = (const uint8_t *)(recvdata + 5);
            uint8_t struct_type = payload[0];
            int seg_len = (payload[1] << 8) | payload[2];

            if (struct_type != TLV_STRUCT_FACTORY_DEVICE) {
                printf_info("[TLV-FACTORY] unexpected struct_type=0x%02X, skip\n",
                            struct_type);
                free(recvdata);
                return -1;
            }

            if (3 + seg_len > payload_len) {
                printf_info("[TLV-FACTORY] seg_len=%d exceeds payload_len=%d, skip\n",
                            seg_len, payload_len);
                free(recvdata);
                return -1;
            }

            const uint8_t *tlv_data = payload + 3;
            printf_info("[TLV-FACTORY] struct_type=0x%02X data_len=%d\n",
                        struct_type, seg_len);

            tlv_dispatch(struct_type, tlv_data, seg_len);
        }
        /* ================================================================
         * 0x37: 设备全量状态快照 (TLV_CMD_SNAPSHOT) — 多段循环解析 + 触发 MQTT device.snapshot
         * 与 0x35 相同的多段解析, 解析完所有段后设置 g_snapshot_pending=1,
         * 由 MQTT 线程消费标志并发布 device.snapshot 全量状态。
         * ================================================================ */
        else if (cmd == TLV_CMD_SNAPSHOT) {
            if (payload_len < 3) {
                printf_info("[TLV-SNAPSHOT] invalid payload_len=%d\n", payload_len);
                free(recvdata);
                return -1;
            }

            const uint8_t *payload = (const uint8_t *)(recvdata + 5);
            int offset = 0;
            int seg_count = 0;

            while (offset + 3 <= payload_len) {
                uint8_t struct_type = payload[offset];
                int seg_len = (payload[offset + 1] << 8) | payload[offset + 2];
                offset += 3;

                if (offset + seg_len > payload_len) {
                    printf_info("[TLV-SNAPSHOT] seg_len=%d exceeds remaining=%d, skip\n",
                                seg_len, payload_len - offset);
                    break;
                }

                const uint8_t *tlv_data = payload + offset;
                printf_info("[TLV-SNAPSHOT] seg#%d struct_type=0x%02X data_len=%d\n",
                            seg_count, struct_type, seg_len);

                tlv_dispatch(struct_type, tlv_data, seg_len);

                offset += seg_len;
                seg_count++;
            }

            if (seg_count == 0) {
                printf_info("[TLV-SNAPSHOT] no valid segment parsed\n");
            } else {
                printf_info("[TLV-SNAPSHOT] parsed %d segments, triggering device.snapshot\n", seg_count);
                /* 设置标志, 通知 MQTT 线程发布 device.snapshot */
                g_snapshot_pending = 1;
            }
        }
        else {
            printf_info("[TLV] unknown cmd=0x%02X, skip\n", cmd);
        }

        free(recvdata);
    }

    return 0;
}
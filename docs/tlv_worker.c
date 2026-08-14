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
#include <time.h>

/* §53.4: 滤芯剩余寿命低提醒阈值(百分比), 降至该值触发 filter.life_low */
#ifndef FILTER_LIFE_LOW_PERCENT
#define FILTER_LIFE_LOW_PERCENT 20
#endif

/* §53 注意事项: device.faults 与 device.alarm 成对维护。
 * 异常发生时更新 faults 并上报告警, 恢复时从 faults 移除并补发 recovered=true。 */

/* 在 device.faults 中新增一条异常(按 code 去重, 已存在则仅更新发生时间) */
static void device_fault_add(const char *code, FaultLevel level,
                             const char *module, const char *source)
{
    DeviceFullState *st = &g_device_state;
    int64_t now = (int64_t)time(NULL);

    if (!code || !code[0]) return;
    for (int i = 0; i < st->device.faults_count; i++) {
        if (strcmp(st->device.faults[i].code, code) == 0) {
            st->device.faults[i].occurred_at = now;
            st->device.faults[i].cleared_at = 0;
            return;
        }
    }
    if (st->device.faults_count >= 50) return;   /* 上限 50 条 */

    st->device.faults = (FaultEntry *)realloc(st->device.faults,
                        (st->device.faults_count + 1) * sizeof(FaultEntry));
    if (!st->device.faults) return;
    FaultEntry *f = &st->device.faults[st->device.faults_count++];
    memset(f, 0, sizeof(*f));
    snprintf(f->code, sizeof(f->code), "%s", code);
    f->level = level;
    if (module) snprintf(f->module, sizeof(f->module), "%s", module);
    if (source) snprintf(f->source, sizeof(f->source), "%s", source);
    f->occurred_at = now;
    f->cleared_at = 0;
}

/* 从 device.faults 移除一条异常, 返回 1=找到并移除, 0=不存在 */
static int device_fault_remove(const char *code)
{
    DeviceFullState *st = &g_device_state;
    if (!code || !code[0]) return 0;
    for (int i = 0; i < st->device.faults_count; i++) {
        if (strcmp(st->device.faults[i].code, code) == 0) {
            /* 移除并前移后续元素 */
            memmove(&st->device.faults[i], &st->device.faults[i + 1],
                    (st->device.faults_count - i - 1) * sizeof(FaultEntry));
            st->device.faults_count--;
            return 1;
        }
    }
    return 0;
}

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

static void print_event_alarm_tlv(const EventAlarmTlv *s)
{
    printf_info("[TLV] EventAlarm: is_alarm=%d code=%s level=%s recovered=%d "
                "payload=%s\n",
                s->is_alarm, s->code, s->level, s->recovered,
                s->payload[0] ? s->payload : "(null)");
}

/* ============================================================================
 * v2: TlvProcessor 查表 — 替代 19 路 switch-case
 * 每个 entry: { struct_type, unpack函数, print函数, handler }
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
    EventAlarmTlv      event_alarm;
} TlvStructUnion;

/** TLV 处理器入口: 一个 struct_type 对应一个处理器 */
typedef struct {
    uint8_t  struct_type;
    int    (*unpack)(void *dst, const uint8_t *buf, int buf_size);
    void   (*print)(const void *src);
    int    (*handler)(const void *src);  /* 业务处理回调, 可为 NULL */
} TlvProcessor;

/* ============================================================================
 * Handler 函数: 将 TLV 解析结果同步到 g_device_state
 * D5 屏通过 0x35 上报的执行器状态需要同步到 g_device_state，
 * 确保 device.snapshot 上报时使用最新实际状态而非旧值。
 * ============================================================================ */

static int handle_actuator_exhaust_tlv(const void *src)
{
    const TlvStructUnion *u = (const TlvStructUnion *)src;
    g_device_state.actuators.exhaust = u->exhaust;
    return 0;
}

static int handle_actuator_light_tlv(const void *src)
{
    const TlvStructUnion *u = (const TlvStructUnion *)src;
    g_device_state.actuators.light = u->light;
    return 0;
}

static int handle_actuator_uvb_tlv(const void *src)
{
    const TlvStructUnion *u = (const TlvStructUnion *)src;
    g_device_state.actuators.uvb = u->uvb;
    return 0;
}

static int handle_actuator_anion_tlv(const void *src)
{
    const TlvStructUnion *u = (const TlvStructUnion *)src;
    g_device_state.actuators.anion = u->anion;
    return 0;
}

static int handle_actuator_plasma_tlv(const void *src)
{
    const TlvStructUnion *u = (const TlvStructUnion *)src;
    g_device_state.actuators.plasma = u->plasma;
    return 0;
}

static int handle_actuator_climate_tlv(const void *src)
{
    const TlvStructUnion *u = (const TlvStructUnion *)src;
    g_device_state.actuators.climate = u->climate;
    return 0;
}

static int handle_actuator_humidifier_tlv(const void *src)
{
    const TlvStructUnion *u = (const TlvStructUnion *)src;
    g_device_state.actuators.humidifier = u->humidifier;
    return 0;
}

static int handle_actuator_inlet_fan_tlv(const void *src)
{
    const TlvStructUnion *u = (const TlvStructUnion *)src;
    g_device_state.actuators.inlet_fan = u->inlet_fan;
    return 0;
}

static int handle_actuator_filter_tlv(const void *src)
{
    const TlvStructUnion *u = (const TlvStructUnion *)src;
    /* 滤芯寿命耗尽告警 (§53): 记录上一次寿命, 用于边沿触发 */
    static int s_prev_life = -1;
    /* filter.life_low 事件(§53.4): 剩余寿命低提醒, 边沿触发只报一次 */
    static int s_life_low_sent = 0;

    g_device_state.actuators.filter = u->filter;

    /* 剩余寿命低提醒: life_percent 降至阈值(如 20%) 以下触发一次, 更换(>阈值)后复位 */
    if (u->filter.life_percent <= FILTER_LIFE_LOW_PERCENT) {
        if (!s_life_low_sent) {
            char payload[128];
            snprintf(payload, sizeof(payload),
                     "{\"life_percent\":%d,\"need_replace\":%d}",
                     u->filter.life_percent, u->filter.need_replace);
            event_report_queue("filter.life_low", "WARN", payload);
            device_fault_add("FILTER_LIFE_LOW", FAULT_LEVEL_WARN,
                             "FILTER", "DEVICE");
            printf_info("[EVENT] event queued: filter.life_low (life %d%%)\n",
                        u->filter.life_percent);
            s_life_low_sent = 1;
        }
    } else {
        if (s_life_low_sent) {
            device_fault_remove("FILTER_LIFE_LOW");
        }
        s_life_low_sent = 0;   /* 更换/恢复后允许下次再提醒 */
    }

    /* 首次上报(s_prev_life==-1)不触发, 只记录基准 */
    if (s_prev_life >= 0) {
        /* 从 >0 降到 0 → 触发 FILTER_LIFE_EXHAUSTED 告警 */
        if (s_prev_life > 0 && u->filter.life_percent <= 0) {
            char payload[128];
            snprintf(payload, sizeof(payload),
                "{\"life_percent\":%d,\"need_replace\":%d}",
                u->filter.life_percent, u->filter.need_replace);
            alarm_report_queue("FILTER_LIFE_EXHAUSTED", "MEDIUM", 0, payload);
            device_fault_add("FILTER_LIFE_EXHAUSTED", FAULT_LEVEL_WARN,
                             "FILTER", "DEVICE");
            printf_info("[EVENT] alarm queued: FILTER_LIFE_EXHAUSTED (life %d%% -> %d%%)\n",
                        s_prev_life, u->filter.life_percent);
        }
        /* 从 0 恢复 >0 → 补发 recovered=true 恢复消息 */
        else if (s_prev_life <= 0 && u->filter.life_percent > 0) {
            alarm_report_queue("FILTER_LIFE_EXHAUSTED", "MEDIUM", 1, NULL);
            device_fault_remove("FILTER_LIFE_EXHAUSTED");
            printf_info("[EVENT] alarm recovered: FILTER_LIFE_EXHAUSTED (life %d%% -> %d%%)\n",
                        s_prev_life, u->filter.life_percent);
        }
    }
    s_prev_life = u->filter.life_percent;
    return 0;
}

static int handle_door_state_tlv(const void *src)
{
    const TlvStructUnion *u = (const TlvStructUnion *)src;
    /* 门状态事件 (§53): 记录上一次门状态, 用于边沿触发 */
    static DoorStatus s_prev_door = DOOR_STATUS_UNKNOWN;

    g_device_state.door = u->door;

    /* 首次上报(s_prev_door==UNKNOWN)只记录基准, 不触发 */
    if (s_prev_door != DOOR_STATUS_UNKNOWN && s_prev_door != u->door.door_status) {
        if (u->door.door_status == DOOR_STATUS_OPEN) {
            char payload[128];
            snprintf(payload, sizeof(payload),
                "{\"door_status\":\"OPEN\",\"lock_status\":\"%s\"}",
                lock_status_to_str(u->door.lock_status));
            event_report_queue("door_opened", "INFO", payload);
            printf_info("[EVENT] event queued: door_opened\n");
        } else if (u->door.door_status == DOOR_STATUS_CLOSED) {
            char payload[128];
            snprintf(payload, sizeof(payload),
                "{\"door_status\":\"CLOSED\",\"lock_status\":\"%s\"}",
                lock_status_to_str(u->door.lock_status));
            event_report_queue("door_closed", "INFO", payload);
            printf_info("[EVENT] event queued: door_closed\n");
        }
    }
    s_prev_door = u->door.door_status;
    return 0;
}

static int handle_environment_state_tlv(const void *src)
{
    const TlvStructUnion *u = (const TlvStructUnion *)src;

    g_device_state.environment = u->environment;

    /* 温度告警 (§53.4 产品定稿阈值, 30s 持续防抖 + 滞回):
     *   TEMP_HIGH: >35°C 持续30s 触发, 回落 ≤30°C 持续30s 恢复
     *   TEMP_LOW : <13°C 持续30s 触发, 回升 ≥18°C 持续30s 恢复 */
    {
        static int s_temp_high_active = 0;
        static int s_temp_low_active  = 0;
        static time_t s_high_since = 0;   /* 温度进入触发/恢复区间的起始时刻 */
        static time_t s_low_since  = 0;
        static double s_last_temp = -1000.0;
        const double temp = u->environment.temperature;
        const time_t now = time(NULL);

        /* 温度变化才刷新防抖计时 (每次 handler 都刷新会因上报频率导致 30s 永不满足) */
        if (s_last_temp != temp) {
            s_last_temp = temp;
            s_high_since = now;
            s_low_since  = now;
        }

        /* ---- TEMP_HIGH ---- */
        if (s_temp_high_active) {
            /* 已触发: 回落 ≤30°C 持续30s → 恢复 */
            if (temp <= 30.0) {
                if (s_high_since == 0) s_high_since = now;
                if (now - s_high_since >= 30) {
                    char payload[128];
                    snprintf(payload, sizeof(payload),
                             "{\"temperature\":%.1f,\"threshold\":30}",
                             temp);
                    alarm_report_queue("TEMP_HIGH", "HIGH", 1, payload);
                    device_fault_remove("TEMP_HIGH");
                    printf_info("[EVENT] alarm recovered: TEMP_HIGH (temp %.1f)\n", temp);
                    s_temp_high_active = 0;
                    s_high_since = 0;
                }
            } else {
                s_high_since = 0;
            }
        } else {
            /* 未触发: >35°C 持续30s → 触发 */
            if (temp > 35.0) {
                if (s_high_since == 0) s_high_since = now;
                if (now - s_high_since >= 30) {
                    char payload[128];
                    snprintf(payload, sizeof(payload),
                             "{\"temperature\":%.1f,\"threshold\":35}",
                             temp);
                    alarm_report_queue("TEMP_HIGH", "HIGH", 0, payload);
                    device_fault_add("TEMP_HIGH", FAULT_LEVEL_ERROR,
                                     "TEMPERATURE", "SENSOR");
                    printf_info("[EVENT] alarm queued: TEMP_HIGH (temp %.1f)\n", temp);
                    s_temp_high_active = 1;
                    s_high_since = 0;
                }
            } else {
                s_high_since = 0;
            }
        }

        /* ---- TEMP_LOW ---- */
        if (s_temp_low_active) {
            /* 已触发: 回升 ≥18°C 持续30s → 恢复 */
            if (temp >= 18.0) {
                if (s_low_since == 0) s_low_since = now;
                if (now - s_low_since >= 30) {
                    char payload[128];
                    snprintf(payload, sizeof(payload),
                             "{\"temperature\":%.1f,\"threshold\":18}",
                             temp);
                    alarm_report_queue("TEMP_LOW", "HIGH", 1, payload);
                    device_fault_remove("TEMP_LOW");
                    printf_info("[EVENT] alarm recovered: TEMP_LOW (temp %.1f)\n", temp);
                    s_temp_low_active = 0;
                    s_low_since = 0;
                }
            } else {
                s_low_since = 0;
            }
        } else {
            /* 未触发: <13°C 持续30s → 触发 */
            if (temp < 13.0) {
                if (s_low_since == 0) s_low_since = now;
                if (now - s_low_since >= 30) {
                    char payload[128];
                    snprintf(payload, sizeof(payload),
                             "{\"temperature\":%.1f,\"threshold\":13}",
                             temp);
                    alarm_report_queue("TEMP_LOW", "HIGH", 0, payload);
                    device_fault_add("TEMP_LOW", FAULT_LEVEL_ERROR,
                                     "TEMPERATURE", "SENSOR");
                    printf_info("[EVENT] alarm queued: TEMP_LOW (temp %.1f)\n", temp);
                    s_temp_low_active = 1;
                    s_low_since = 0;
                }
            } else {
                s_low_since = 0;
            }
        }
    }
    return 0;
}

static int handle_event_alarm_tlv(const void *src)
{
    const TlvStructUnion *u = (const TlvStructUnion *)src;
    const EventAlarmTlv *e = &u->event_alarm;

    if (!e->code[0]) {
        printf_info("[EVENT] 0x38 empty code, skip\n");
        return -1;
    }

    if (e->is_alarm) {
        alarm_report_queue(e->code, e->level, e->recovered,
                           e->payload[0] ? e->payload : NULL);
        printf_info("[EVENT] 0x38 alarm queued: code=%s level=%s recovered=%d\n",
                    e->code, e->level, e->recovered);
    } else {
        event_report_queue(e->code, e->level,
                           e->payload[0] ? e->payload : NULL);
        printf_info("[EVENT] 0x38 event queued: code=%s level=%s\n",
                    e->code, e->level);
    }
    return 0;
}

static const TlvProcessor g_tlv_processors[] = {
    { TLV_STRUCT_DEVICE,      (int(*)(void*,const uint8_t*,int))device_state_tlv_unpack,      (void(*)(const void*))print_device_state,      NULL },
    { TLV_STRUCT_NETWORK,     (int(*)(void*,const uint8_t*,int))network_state_tlv_unpack,     (void(*)(const void*))print_network_state,     NULL },
    { TLV_STRUCT_POWER,       (int(*)(void*,const uint8_t*,int))power_state_tlv_unpack,       (void(*)(const void*))print_power_state,       NULL },
    { TLV_STRUCT_DOOR,        (int(*)(void*,const uint8_t*,int))door_state_tlv_unpack,        (void(*)(const void*))print_door_state,        (int(*)(const void*))handle_door_state_tlv },
    { TLV_STRUCT_ENVIRONMENT, (int(*)(void*,const uint8_t*,int))environment_state_tlv_unpack, (void(*)(const void*))print_environment_state, (int(*)(const void*))handle_environment_state_tlv },
    { TLV_STRUCT_ACTUATOR,    (int(*)(void*,const uint8_t*,int))actuator_state_tlv_unpack,    (void(*)(const void*))print_actuator_state,    NULL },
    { TLV_STRUCT_CAMERA,      (int(*)(void*,const uint8_t*,int))camera_state_tlv_unpack,      (void(*)(const void*))print_camera_state,      NULL },
    { TLV_STRUCT_SETTINGS,    (int(*)(void*,const uint8_t*,int))settings_state_tlv_unpack,    (void(*)(const void*))print_settings_state,    NULL },
    { TLV_STRUCT_FIRMWARE,    (int(*)(void*,const uint8_t*,int))firmware_state_tlv_unpack,    (void(*)(const void*))print_firmware_state,    NULL },
    { TLV_STRUCT_STORAGE,     (int(*)(void*,const uint8_t*,int))storage_state_tlv_unpack,     (void(*)(const void*))print_storage_state,     NULL },
    { TLV_STRUCT_EXHAUST,     (int(*)(void*,const uint8_t*,int))actuator_exhaust_tlv_unpack,  (void(*)(const void*))print_actuator_exhaust,  (int(*)(const void*))handle_actuator_exhaust_tlv },
    { TLV_STRUCT_LIGHT,       (int(*)(void*,const uint8_t*,int))actuator_light_tlv_unpack,    (void(*)(const void*))print_actuator_light,    (int(*)(const void*))handle_actuator_light_tlv },
    { TLV_STRUCT_UVB,         (int(*)(void*,const uint8_t*,int))actuator_uvb_tlv_unpack,      (void(*)(const void*))print_actuator_uvb,      (int(*)(const void*))handle_actuator_uvb_tlv },
    { TLV_STRUCT_ANION,       (int(*)(void*,const uint8_t*,int))actuator_anion_tlv_unpack,    (void(*)(const void*))print_actuator_anion,    (int(*)(const void*))handle_actuator_anion_tlv },
    { TLV_STRUCT_PLASMA,      (int(*)(void*,const uint8_t*,int))actuator_plasma_tlv_unpack,   (void(*)(const void*))print_actuator_plasma,   (int(*)(const void*))handle_actuator_plasma_tlv },
    { TLV_STRUCT_CLIMATE,     (int(*)(void*,const uint8_t*,int))actuator_climate_tlv_unpack,  (void(*)(const void*))print_actuator_climate,  (int(*)(const void*))handle_actuator_climate_tlv },
    { TLV_STRUCT_HUMIDIFIER,  (int(*)(void*,const uint8_t*,int))actuator_humidifier_tlv_unpack,(void(*)(const void*))print_actuator_humidifier,(int(*)(const void*))handle_actuator_humidifier_tlv },
    { TLV_STRUCT_INLET_FAN,   (int(*)(void*,const uint8_t*,int))actuator_inlet_fan_tlv_unpack,(void(*)(const void*))print_actuator_inlet_fan,(int(*)(const void*))handle_actuator_inlet_fan_tlv },
    { TLV_STRUCT_FILTER,      (int(*)(void*,const uint8_t*,int))actuator_filter_tlv_unpack,   (void(*)(const void*))print_actuator_filter,   (int(*)(const void*))handle_actuator_filter_tlv },
    { TLV_STRUCT_FACTORY_DEVICE, (int(*)(void*,const uint8_t*,int))factory_device_tlv_unpack, (void(*)(const void*))print_factory_device_tlv, (int(*)(const void*))handle_factory_device_tlv },
    { TLV_STRUCT_EVENT_ALARM,   (int(*)(void*,const uint8_t*,int))event_alarm_tlv_unpack,   (void(*)(const void*))print_event_alarm_tlv,   (int(*)(const void*))handle_event_alarm_tlv },
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
 *   输出: AA 55  00 10  35  03  00 0C  01 00 01 01 02 00 01 55 03 00 01 00
 *         │      │      │  │    │    └──────────────────────────────────┘
 *         │      │      │  │    │              TLV 数据 (12B)
 *         │      │      │  │    └─ seg_len = 0x000C = 12
 *         │      │      │  └─ struct_type = 0x03 = PowerState
 *         │      │      └─ 命令字 TLV_CMD_PROPERTY (0x35)
 *         │      └─ lenH|lenL = 0x0010 = 16 (1+3+12, 含cmd)
 *         └─ 帧头 AA 55
 * ============================================================================ */

int tlv_property_send(uint8_t struct_type, const uint8_t *tlv_data, int data_len)
{
    if (!tlv_data || data_len <= 0 || data_len > 65532) return -1;

    /* len字段包含cmd: cmd(1) + struct_type(1) + seg_len(2) + data */
    int seg_total = 4 + data_len;
    /* 总长度 = AA 55 + len(2) + seg_total */
    int total = 4 + seg_total;
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
 *   输出: AA 55  00 26  35  03  00 0C  01 00 01 01 02 00 01 55 03 00 01 00  04  00 13  01 00 01 01 02 00 01 00 03 00 08 00 00 00 00 00 00 00 00
 *         │      │      │  │    │    └──────────────────────────────────┘ │  │    └────────────────────────────────────────────────────────┘
 *         │      │      │  │    │              PowerState (12B)           │  │                   DoorState (19B)
 *         │      │      │  │    └─ seg_len = 0x000C = 12                  │  └─ seg_len = 0x0013 = 19
 *         │      │      │  └─ struct_type = 0x03 = PowerState             └─ struct_type = 0x04 = DoorState
 *         │      │      └─ 命令字 TLV_CMD_PROPERTY (0x35)
 *         │      └─ lenH|lenL = 0x0026 = 38 (1+3+12+3+19, 含cmd)
 *         └─ 帧头 AA 55
 * ============================================================================ */

int tlv_property_send_multi(const uint8_t *struct_types,
                            const uint8_t *const *tlv_data,
                            const int *data_lens, int count)
{
    if (!struct_types || !tlv_data || !data_lens || count <= 0) return -1;

    /* 计算所有结构体段的总字节数, len字段包含cmd */
    int seg_total = 1;  /* cmd(1) */
    for (int i = 0; i < count; i++) {
        if (!tlv_data[i] || data_lens[i] <= 0 || data_lens[i] > 65532) return -1;
        seg_total += 3 + data_lens[i];  /* struct_type(1) + seg_len(2) + data */
    }

    int total = 4 + seg_total;  /* AA(1)+55(1)+len(2) + seg_total */
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
 *   0x37 (TLV_CMD_SNAPSHOT):     多段循环解析 + 触发 MQTT device.snapshot
 *   0x38 (TLV_CMD_EVENT_ALARM):  单段直接解析 EventAlarmTlv → device.event / device.alarm
 *
 * 解析示例 (0x35): 收到 AA 55 00 10 35 03 00 0C 01 00 01 01 02 00 01 55 03 00 01 00
 *   recvdata[0..1] = AA 55  → 帧头 ✓
 *   recvdata[2..3] = 00 10  → payload_len = 16 (含cmd)
 *   recvdata[4]    = 35     → cmd = TLV_CMD_PROPERTY → 多段循环解析
 *   recvdata[5]    = 03     → struct_type = 0x03 = PowerState (seg#0)
 *   recvdata[6..7] = 00 0C  → seg_len = 12
 *   recvdata[8..19]= TLV 数据 → tlv_dispatch(0x03, data, 12) → print_power_state
 *
 * 解析示例 (0x36): 收到 AA 55 00 44 36 6A 00 40 01 00 02 ...
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
         * 0x38: 事件/告警上报指令 (TLV_CMD_EVENT_ALARM) — 单段直接解析
         * D5/LCD 收到告警/事件后发此指令, V851 解析后入队并触发
         * MQTT device.event (is_alarm=0) 或 device.alarm (is_alarm=1) 上报。
         * ================================================================ */
        else if (cmd == TLV_CMD_EVENT_ALARM) {
            if (payload_len < 3) {
                printf_info("[TLV-EVENTALARM] invalid payload_len=%d\n", payload_len);
                free(recvdata);
                return -1;
            }

            const uint8_t *payload = (const uint8_t *)(recvdata + 5);
            uint8_t struct_type = payload[0];
            int seg_len = (payload[1] << 8) | payload[2];

            if (struct_type != TLV_STRUCT_EVENT_ALARM) {
                printf_info("[TLV-EVENTALARM] unexpected struct_type=0x%02X, skip\n",
                            struct_type);
                free(recvdata);
                return -1;
            }

            if (3 + seg_len > payload_len) {
                printf_info("[TLV-EVENTALARM] seg_len=%d exceeds payload_len=%d, skip\n",
                            seg_len, payload_len);
                free(recvdata);
                return -1;
            }

            const uint8_t *tlv_data = payload + 3;
            printf_info("[TLV-EVENTALARM] struct_type=0x%02X data_len=%d\n",
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
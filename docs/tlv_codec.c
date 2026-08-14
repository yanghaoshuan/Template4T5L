/**
 * @file    tlv_codec.c
 * @brief   设备状态结构体 TLV 编解码实现 (v2: 描述符表驱动)
 *          TLV: [Type:1B][Length:2B big-endian][Value:Length bytes]
 *
 * v2 优化:
 *   - 用 TlvFieldDesc 描述符表 + tlv_generic_pack/unpack 替代 ~1500 行重复代码
 *   - 每个简单结构体的 pack/unpack 退化为 1 行 wrapper
 *   - 新增字段只需在描述符表中加一行
 *
 * TLV 字节布局示意:
 *   ┌──────┬──────┬──────┬───────────────────┐
 *   │ Type │ LenH │ LenL │ Value (Len bytes)  │
 *   │ 1B   │ 1B   │ 1B   │ variable          │
 *   └──────┴──────┴──────┴───────────────────┘
 *   例: 01 00 01 01 → Type=0x01, Len=1, Value=0x01 (u8字段)
 *   例: 03 00 04 00 00 20 00 → Type=0x03, Len=4, Value=0x00002000=8192 (u32 BE)
 *   例: 02 00 08 40 3E 00 00 00 00 00 00 → Type=0x02, Len=8, Value=30.0 (double IEEE754)
 *   例: 01 00 05 32 2E 31 2E 30 → Type=0x01, Len=5, Value="2.1.0" (string)
 */
#include "tlv_codec.h"
#include <string.h>
#include <stdint.h>
#include <stddef.h>   /* offsetof */

/* ============================================================================
 * 内部辅助函数 — TLV 写入
 *   tlv_put:  通用写入 → buf[0]=Type, buf[1..2]=Len(BE), buf[3..]=Value
 *   tlv_put_u8:     u8 字段 → 例: 01 00 01 55
 *   tlv_put_u32:    u32 字段 → 例: 03 00 04 00 00 20 00
 *   tlv_put_double: double 字段 → 例: 02 00 08 40 3E 00 00 00 00 00 00
 *   tlv_put_str:    string 字段 → 例: 01 00 05 32 2E 31 2E 30
 * ============================================================================ */

static inline int tlv_put(uint8_t *buf, int buf_size, uint8_t type,
                          const uint8_t *val, uint16_t len)
{
    if (buf_size < 3 + (int)len) return -1;
    buf[0] = type;
    buf[1] = (uint8_t)((len >> 8) & 0xFF);
    buf[2] = (uint8_t)(len & 0xFF);
    memcpy(buf + 3, val, len);
    return 3 + len;
}

static inline int tlv_put_u8(uint8_t *buf, int buf_size, uint8_t type, uint8_t val)
{
    return tlv_put(buf, buf_size, type, &val, 1);
}

static inline int tlv_put_u32(uint8_t *buf, int buf_size, uint8_t type, uint32_t val)
{
    uint8_t tmp[4];
    tmp[0] = (uint8_t)((val >> 24) & 0xFF);
    tmp[1] = (uint8_t)((val >> 16) & 0xFF);
    tmp[2] = (uint8_t)((val >> 8) & 0xFF);
    tmp[3] = (uint8_t)(val & 0xFF);
    return tlv_put(buf, buf_size, type, tmp, 4);
}

static inline int tlv_put_double(uint8_t *buf, int buf_size, uint8_t type, double val)
{
    union { double d; uint64_t u; } u;
    u.d = val;
    uint8_t tmp[8];
    tmp[0] = (uint8_t)((u.u >> 56) & 0xFF);
    tmp[1] = (uint8_t)((u.u >> 48) & 0xFF);
    tmp[2] = (uint8_t)((u.u >> 40) & 0xFF);
    tmp[3] = (uint8_t)((u.u >> 32) & 0xFF);
    tmp[4] = (uint8_t)((u.u >> 24) & 0xFF);
    tmp[5] = (uint8_t)((u.u >> 16) & 0xFF);
    tmp[6] = (uint8_t)((u.u >> 8) & 0xFF);
    tmp[7] = (uint8_t)(u.u & 0xFF);
    return tlv_put(buf, buf_size, type, tmp, 8);
}

static inline int tlv_put_str(uint8_t *buf, int buf_size, uint8_t type,
                              const char *str)
{
    uint16_t len = (uint16_t)strlen(str);
    if (len == 0) return 0;
    return tlv_put(buf, buf_size, type, (const uint8_t *)str, len);
}

static inline int tlv_put_bytes(uint8_t *buf, int buf_size, uint8_t type,
                                const uint8_t *data, uint16_t len)
{
    if (len == 0) return 0;
    return tlv_put(buf, buf_size, type, data, len);
}

/* ============================================================================
 * 内部辅助函数 — TLV 读取
 * ============================================================================ */

static inline int tlv_next(const uint8_t *buf, int buf_size,
                           uint8_t *type, uint8_t **val, uint16_t *len)
{
    if (buf_size < 3) return -1;
    *type = buf[0];
    *len  = (uint16_t)(((uint16_t)buf[1] << 8) | buf[2]);
    if (buf_size < 3 + (int)(*len)) return -1;
    *val = (uint8_t *)(buf + 3);
    return 3 + (int)(*len);
}

static inline int tlv_read_u8(const uint8_t *buf, int buf_size,
                              uint8_t type, uint8_t *out)
{
    uint8_t t;
    uint8_t *v;
    uint16_t l;
    int consumed = tlv_next(buf, buf_size, &t, &v, &l);
    if (consumed < 0 || t != type || l != 1) return -1;
    *out = v[0];
    return consumed;
}

static inline int tlv_read_u32(const uint8_t *buf, int buf_size,
                               uint8_t type, uint32_t *out)
{
    uint8_t t;
    uint8_t *v;
    uint16_t l;
    int consumed = tlv_next(buf, buf_size, &t, &v, &l);
    if (consumed < 0 || t != type || l != 4) return -1;
    *out = ((uint32_t)v[0] << 24) | ((uint32_t)v[1] << 16) |
           ((uint32_t)v[2] << 8)  |  (uint32_t)v[3];
    return consumed;
}

static inline int tlv_read_double(const uint8_t *buf, int buf_size,
                                  uint8_t type, double *out)
{
    uint8_t t;
    uint8_t *v;
    uint16_t l;
    int consumed = tlv_next(buf, buf_size, &t, &v, &l);
    if (consumed < 0 || t != type || l != 8) return -1;
    union { double d; uint64_t u; } u;
    u.u = ((uint64_t)v[0] << 56) | ((uint64_t)v[1] << 48) |
          ((uint64_t)v[2] << 40) | ((uint64_t)v[3] << 32) |
          ((uint64_t)v[4] << 24) | ((uint64_t)v[5] << 16) |
          ((uint64_t)v[6] << 8)  |  (uint64_t)v[7];
    *out = u.d;
    return consumed;
}

static inline int tlv_read_str(const uint8_t *buf, int buf_size,
                               uint8_t type, char *out, int out_max)
{
    uint8_t t;
    uint8_t *v;
    uint16_t l;
    int consumed = tlv_next(buf, buf_size, &t, &v, &l);
    if (consumed < 0 || t != type) return -1;
    int copy_len = (int)l < (out_max - 1) ? (int)l : (out_max - 1);
    memcpy(out, v, copy_len);
    out[copy_len] = '\0';
    return consumed;
}

static inline int tlv_skip(const uint8_t *buf, int buf_size)
{
    if (buf_size < 3) return -1;
    uint16_t l = (uint16_t)(((uint16_t)buf[1] << 8) | buf[2]);
    if (buf_size < 3 + (int)l) return -1;
    return 3 + (int)l;
}

static inline int tlv_read_nested(const uint8_t *buf, int buf_size,
                                  uint8_t type,
                                  const uint8_t **nested, uint16_t *nested_len)
{
    uint8_t t;
    uint8_t *v;
    uint16_t l;
    int consumed = tlv_next(buf, buf_size, &t, &v, &l);
    if (consumed < 0 || t != type) return -1;
    *nested = v;
    *nested_len = l;
    return consumed;
}

/* ============================================================================
 * 通用 TLV pack/unpack — 基于描述符表
 * ============================================================================ */

static inline int tlv_find_field(const TlvFieldDesc *fields, int field_count, uint8_t tag)
{
    for (int i = 0; i < field_count; i++) {
        if (fields[i].tag == tag) return i;
    }
    return -1;
}

int tlv_generic_pack(const void *src, const TlvFieldDesc *fields,
                     int field_count, uint8_t *buf, int buf_size)
{
    if (!src || !fields || !buf || buf_size < 0) return -1;
    int total = 0, ret;

    for (int i = 0; i < field_count; i++) {
        const TlvFieldDesc *f = &fields[i];
        const char *base = (const char *)src;

        switch (f->type) {
        case TLV_FIELD_U8: {
            int v = *(const int *)(base + f->offset);
            ret = tlv_put_u8(buf + total, buf_size - total, f->tag, (uint8_t)v);
            break;
        }
        case TLV_FIELD_U32: {
            int v = *(const int *)(base + f->offset);
            ret = tlv_put_u32(buf + total, buf_size - total, f->tag, (uint32_t)v);
            break;
        }
        case TLV_FIELD_DOUBLE: {
            double v = *(const double *)(base + f->offset);
            ret = tlv_put_double(buf + total, buf_size - total, f->tag, v);
            break;
        }
        case TLV_FIELD_STR: {
            const char *v = (const char *)(base + f->offset);
            ret = tlv_put_str(buf + total, buf_size - total, f->tag, v);
            break;
        }
        default:
            return -1;
        }
        if (ret < 0) return -1;
        total += ret;
    }
    return total;
}

int tlv_generic_unpack(void *dst, const TlvFieldDesc *fields,
                       int field_count, const uint8_t *buf, int buf_size)
{
    if (!dst || !fields || !buf || buf_size < 0) return -1;
    int offset = 0, consumed;

    while (offset < buf_size) {
        uint8_t tag = buf[offset];
        int idx = tlv_find_field(fields, field_count, tag);

        if (idx < 0) {
            consumed = tlv_skip(buf + offset, buf_size - offset);
            if (consumed < 0) return -1;
            offset += consumed;
            continue;
        }

        const TlvFieldDesc *f = &fields[idx];
        char *base = (char *)dst;

        switch (f->type) {
        case TLV_FIELD_U8: {
            uint8_t v;
            consumed = tlv_read_u8(buf + offset, buf_size - offset, tag, &v);
            if (consumed < 0) return -1;
            *(int *)(base + f->offset) = (int)v;
            break;
        }
        case TLV_FIELD_U32: {
            uint32_t v;
            consumed = tlv_read_u32(buf + offset, buf_size - offset, tag, &v);
            if (consumed < 0) return -1;
            *(int *)(base + f->offset) = (int)v;
            break;
        }
        case TLV_FIELD_DOUBLE: {
            double v;
            consumed = tlv_read_double(buf + offset, buf_size - offset, tag, &v);
            if (consumed < 0) return -1;
            *(double *)(base + f->offset) = v;
            break;
        }
        case TLV_FIELD_STR: {
            consumed = tlv_read_str(buf + offset, buf_size - offset, tag,
                                    base + f->offset, f->extra);
            if (consumed < 0) return -1;
            break;
        }
        default:
            consumed = tlv_skip(buf + offset, buf_size - offset);
            if (consumed < 0) return -1;
            break;
        }
        offset += consumed;
    }
    return offset;
}

/* ============================================================================
 * 描述符表 — 各结构体的字段定义
 * ============================================================================ */

/* ---- 54.1 DeviceState ---- */
#define TLV_TAG_DEV_BIND_STATUS         0x01
#define TLV_TAG_DEV_WORK_STATUS         0x02
#define TLV_TAG_DEV_ONLINE_STATUS       0x03
#define TLV_TAG_DEV_FAULT_SUMMARY       0x04
#define TLV_TAG_DEV_FAULTS_COUNT        0x05
#define TLV_TAG_DEV_LIFECYCLE_STATUS    0x06

static const TlvFieldDesc device_state_fields[] = {
    { TLV_TAG_DEV_BIND_STATUS,      TLV_FIELD_U8, offsetof(DeviceState, bind_status),      0 },
    { TLV_TAG_DEV_WORK_STATUS,      TLV_FIELD_U8, offsetof(DeviceState, work_status),      0 },
    { TLV_TAG_DEV_ONLINE_STATUS,    TLV_FIELD_U8, offsetof(DeviceState, online_status),    0 },
    { TLV_TAG_DEV_FAULT_SUMMARY,    TLV_FIELD_U8, offsetof(DeviceState, fault_summary),    0 },
    { TLV_TAG_DEV_FAULTS_COUNT,     TLV_FIELD_U32,offsetof(DeviceState, faults_count),     0 },
    { TLV_TAG_DEV_LIFECYCLE_STATUS, TLV_FIELD_U8, offsetof(DeviceState, lifecycle_status), 0 },
};
#define DEVICE_STATE_FIELD_COUNT  (sizeof(device_state_fields) / sizeof(device_state_fields[0]))

/* ---- 54.2 NetworkState ---- */
#define TLV_TAG_NET_TYPE               0x01
#define TLV_TAG_NET_IFNAME             0x02
#define TLV_TAG_NET_RSSI               0x03
#define TLV_TAG_NET_IP                 0x04
#define TLV_TAG_NET_MQTT_CONNECTED     0x05

static const TlvFieldDesc network_state_fields[] = {
    { TLV_TAG_NET_TYPE,           TLV_FIELD_U8,  offsetof(NetworkState, network_type),   0 },
    { TLV_TAG_NET_IFNAME,         TLV_FIELD_STR, offsetof(NetworkState, ifname),         sizeof(((NetworkState*)0)->ifname) },
    { TLV_TAG_NET_RSSI,           TLV_FIELD_U32, offsetof(NetworkState, rssi),           0 },
    { TLV_TAG_NET_IP,             TLV_FIELD_STR, offsetof(NetworkState, ip),             sizeof(((NetworkState*)0)->ip) },
    { TLV_TAG_NET_MQTT_CONNECTED, TLV_FIELD_U8,  offsetof(NetworkState, mqtt_connected), 0 },
};
#define NETWORK_STATE_FIELD_COUNT (sizeof(network_state_fields) / sizeof(network_state_fields[0]))

/* ---- 54.3 PowerState ----
 * 例: {power_mode=1, battery=85%, charging=0}
 *     → 01 00 01 01 02 00 01 55 03 00 01 00 (12B) */
static const TlvFieldDesc power_state_fields[] = {
    { TLV_TAG_POWER_MODE,       TLV_FIELD_U8, offsetof(PowerState, power_mode),      0 },
    { TLV_TAG_BATTERY_PERCENT,  TLV_FIELD_U8, offsetof(PowerState, battery_percent), 0 },
    { TLV_TAG_CHARGING,         TLV_FIELD_U8, offsetof(PowerState, charging),        0 },
};
#define POWER_STATE_FIELD_COUNT (sizeof(power_state_fields) / sizeof(power_state_fields[0]))

/* ---- 54.4 DoorState ----
 * 例: {door=1, lock=0, last_open=0}
 *     → 01 00 01 01 02 00 01 00 03 00 08 00 00 00 00 00 00 00 00 (19B) */
static const TlvFieldDesc door_state_fields[] = {
    { TLV_TAG_DOOR_STATUS,     TLV_FIELD_U8,     offsetof(DoorState, door_status),    0 },
    { TLV_TAG_LOCK_STATUS,     TLV_FIELD_U8,     offsetof(DoorState, lock_status),    0 },
    { TLV_TAG_LAST_OPEN_TIME,  TLV_FIELD_DOUBLE, offsetof(DoorState, last_open_time), 0 },
};
#define DOOR_STATE_FIELD_COUNT (sizeof(door_state_fields) / sizeof(door_state_fields[0]))

/* ---- 54.5 EnvironmentState ---- */
static const TlvFieldDesc environment_state_fields[] = {
    { TLV_TAG_TEMPERATURE, TLV_FIELD_DOUBLE, offsetof(EnvironmentState, temperature), 0 },
    { TLV_TAG_HUMIDITY,    TLV_FIELD_DOUBLE, offsetof(EnvironmentState, humidity),    0 },
    { TLV_TAG_PM25,        TLV_FIELD_DOUBLE, offsetof(EnvironmentState, pm25),        0 },
    { TLV_TAG_CO2,         TLV_FIELD_DOUBLE, offsetof(EnvironmentState, co2),         0 },
};
#define ENVIRONMENT_STATE_FIELD_COUNT (sizeof(environment_state_fields) / sizeof(environment_state_fields[0]))

/* ---- 54.6.1 ActuatorExhaust ----
 * 例: {enabled=1, speed=3, interval=0, running=1}
 *     → 01 00 01 01 02 00 01 03 03 00 08 00 00 00 00 00 00 00 00 04 00 01 01 (23B) */
static const TlvFieldDesc actuator_exhaust_fields[] = {
    { TLV_TAG_EXH_ENABLED,        TLV_FIELD_U8,     offsetof(ActuatorExhaust, enabled),        0 },
    { TLV_TAG_EXH_SPEED_LEVEL,    TLV_FIELD_U8,     offsetof(ActuatorExhaust, speed_level),    0 },
    { TLV_TAG_EXH_INTERVAL_HOURS, TLV_FIELD_DOUBLE, offsetof(ActuatorExhaust, interval_hours), 0 },
    { TLV_TAG_EXH_RUNNING,        TLV_FIELD_U8,     offsetof(ActuatorExhaust, running),        0 },
};
#define EXHAUST_FIELD_COUNT (sizeof(actuator_exhaust_fields) / sizeof(actuator_exhaust_fields[0]))

/* ---- 54.6.2 ActuatorLight ----
 * 例: {enabled=1, brightness=80, running=1}
 *     → 01 00 01 01 02 00 01 50 03 00 01 01 (12B) */
static const TlvFieldDesc actuator_light_fields[] = {
    { TLV_TAG_LIGHT_ENABLED,    TLV_FIELD_U8, offsetof(ActuatorLight, enabled),          0 },
    { TLV_TAG_LIGHT_BRIGHTNESS, TLV_FIELD_U8, offsetof(ActuatorLight, brightness_level), 0 },
    { TLV_TAG_LIGHT_RUNNING,    TLV_FIELD_U8, offsetof(ActuatorLight, running),          0 },
};
#define LIGHT_FIELD_COUNT (sizeof(actuator_light_fields) / sizeof(actuator_light_fields[0]))

/* ---- 54.6.3 ActuatorUvb ---- */
static const TlvFieldDesc actuator_uvb_fields[] = {
    { TLV_TAG_UVB_ENABLED,     TLV_FIELD_U8, offsetof(ActuatorUvb, enabled),     0 },
    { TLV_TAG_UVB_LEVEL,       TLV_FIELD_U8, offsetof(ActuatorUvb, level),       0 },
    { TLV_TAG_UVB_DAILY_HOURS, TLV_FIELD_U8, offsetof(ActuatorUvb, daily_hours), 0 },
    { TLV_TAG_UVB_RUNNING,     TLV_FIELD_U8, offsetof(ActuatorUvb, running),     0 },
};
#define UVB_FIELD_COUNT (sizeof(actuator_uvb_fields) / sizeof(actuator_uvb_fields[0]))

/* ---- 54.6.4 ActuatorAnion ---- */
static const TlvFieldDesc actuator_anion_fields[] = {
    { TLV_TAG_ANION_ENABLED, TLV_FIELD_U8, offsetof(ActuatorAnion, enabled), 0 },
    { TLV_TAG_ANION_RUNNING, TLV_FIELD_U8, offsetof(ActuatorAnion, running), 0 },
};
#define ANION_FIELD_COUNT (sizeof(actuator_anion_fields) / sizeof(actuator_anion_fields[0]))

/* ---- 54.6.5 ActuatorPlasma ---- */
static const TlvFieldDesc actuator_plasma_fields[] = {
    { TLV_TAG_PLASMA_ENABLED, TLV_FIELD_U8, offsetof(ActuatorPlasma, enabled), 0 },
    { TLV_TAG_PLASMA_RUNNING, TLV_FIELD_U8, offsetof(ActuatorPlasma, running), 0 },
};
#define PLASMA_FIELD_COUNT (sizeof(actuator_plasma_fields) / sizeof(actuator_plasma_fields[0]))

/* ---- 54.6.6 ActuatorClimate ----
 * 例: {enabled=1, target=30.0C, ctrl_status=1, running=1}
 *     → 01 00 01 01 02 00 08 40 3E 00 00 00 00 00 00 03 00 01 01 04 00 01 01 (24B) */
static const TlvFieldDesc actuator_climate_fields[] = {
    { TLV_TAG_CLIMATE_ENABLED,     TLV_FIELD_U8,     offsetof(ActuatorClimate, enabled),        0 },
    { TLV_TAG_CLIMATE_TARGET,      TLV_FIELD_DOUBLE, offsetof(ActuatorClimate, target_celsius), 0 },
    { TLV_TAG_CLIMATE_CTRL_STATUS, TLV_FIELD_U8,     offsetof(ActuatorClimate, control_status), 0 },
    { TLV_TAG_CLIMATE_RUNNING,     TLV_FIELD_U8,     offsetof(ActuatorClimate, running),        0 },
};
#define CLIMATE_FIELD_COUNT (sizeof(actuator_climate_fields) / sizeof(actuator_climate_fields[0]))

/* ---- 54.6.7 ActuatorHumidifier ---- */
static const TlvFieldDesc actuator_humidifier_fields[] = {
    { TLV_TAG_HUMI_ENABLED,        TLV_FIELD_U8,     offsetof(ActuatorHumidifier, enabled),        0 },
    { TLV_TAG_HUMI_INTERVAL_HOURS, TLV_FIELD_DOUBLE, offsetof(ActuatorHumidifier, interval_hours), 0 },
    { TLV_TAG_HUMI_RUNNING_HOURS,  TLV_FIELD_DOUBLE, offsetof(ActuatorHumidifier, running_hours),  0 },
    { TLV_TAG_HUMI_RUNNING,        TLV_FIELD_U8,     offsetof(ActuatorHumidifier, running),        0 },
    { TLV_TAG_HUMI_LIQUID_STATUS,  TLV_FIELD_U8,     offsetof(ActuatorHumidifier, liquid_status),  0 },
};
#define HUMIDIFIER_FIELD_COUNT (sizeof(actuator_humidifier_fields) / sizeof(actuator_humidifier_fields[0]))

/* ---- 54.6.8 ActuatorInletFan ---- */
static const TlvFieldDesc actuator_inlet_fan_fields[] = {
    { TLV_TAG_INLET_ENABLED,     TLV_FIELD_U8, offsetof(ActuatorInletFan, enabled),     0 },
    { TLV_TAG_INLET_SPEED_LEVEL, TLV_FIELD_U8, offsetof(ActuatorInletFan, speed_level), 0 },
    { TLV_TAG_INLET_RUNNING,     TLV_FIELD_U8, offsetof(ActuatorInletFan, running),     0 },
};
#define INLET_FAN_FIELD_COUNT (sizeof(actuator_inlet_fan_fields) / sizeof(actuator_inlet_fan_fields[0]))

/* ---- 54.6.9 ActuatorFilter ----
 * 例: {life=80%, need_replace=0}
 *     → 01 00 01 50 02 00 01 00 (8B) */
static const TlvFieldDesc actuator_filter_fields[] = {
    { TLV_TAG_FILTER_LIFE_PERCENT, TLV_FIELD_U8, offsetof(ActuatorFilter, life_percent), 0 },
    { TLV_TAG_FILTER_NEED_REPLACE, TLV_FIELD_U8, offsetof(ActuatorFilter, need_replace), 0 },
};
#define FILTER_FIELD_COUNT (sizeof(actuator_filter_fields) / sizeof(actuator_filter_fields[0]))

/* ---- 54.7 CameraState ---- */
static const TlvFieldDesc camera_state_fields[] = {
    { TLV_TAG_CAMERA_STATUS,     TLV_FIELD_U8,     offsetof(CameraState, camera_status),     0 },
    { TLV_TAG_STREAM_STATUS,     TLV_FIELD_U8,     offsetof(CameraState, stream_status),     0 },
    { TLV_TAG_PROVIDER,          TLV_FIELD_STR,    offsetof(CameraState, provider),          sizeof(((CameraState*)0)->provider) },
    { TLV_TAG_VIDEO_CODEC,       TLV_FIELD_U8,     offsetof(CameraState, video_codec),       0 },
    { TLV_TAG_RESOLUTION,        TLV_FIELD_U8,     offsetof(CameraState, resolution),        0 },
    { TLV_TAG_FPS,               TLV_FIELD_DOUBLE, offsetof(CameraState, fps),               0 },
    { TLV_TAG_AUDIO_ENABLED,     TLV_FIELD_U8,     offsetof(CameraState, audio_enabled),     0 },
    { TLV_TAG_PRIVACY_MODE,      TLV_FIELD_U8,     offsetof(CameraState, privacy_mode),      0 },
    { TLV_TAG_LAST_SESSION_ID,   TLV_FIELD_STR,    offsetof(CameraState, last_session_id),   sizeof(((CameraState*)0)->last_session_id) },
    { TLV_TAG_LAST_ERROR_CODE,   TLV_FIELD_STR,    offsetof(CameraState, last_error_code),   sizeof(((CameraState*)0)->last_error_code) },
};
#define CAMERA_STATE_FIELD_COUNT (sizeof(camera_state_fields) / sizeof(camera_state_fields[0]))

/* ---- 54.8.1 DisplaySettings ---- */
static const TlvFieldDesc display_settings_fields[] = {
    { TLV_TAG_DISP_BRIGHTNESS,     TLV_FIELD_U8, offsetof(DisplaySettings, brightness_percent),     0 },
    { TLV_TAG_DISP_SCREEN_TIMEOUT, TLV_FIELD_U8, offsetof(DisplaySettings, screen_timeout_minutes), 0 },
    { TLV_TAG_DISP_SCREEN_MODE,    TLV_FIELD_U8, offsetof(DisplaySettings, screen_mode),            0 },
};
#define DISPLAY_SETTINGS_FIELD_COUNT (sizeof(display_settings_fields) / sizeof(display_settings_fields[0]))

/* ---- 54.9 FirmwareState ----
 * 例: {fw="2.1.0", hw="R1", config_ver=5}
 *     → 01 00 05 32 2E 31 2E 30 02 00 02 52 31 03 00 04 00 00 00 05 (23B) */
static const TlvFieldDesc firmware_state_fields[] = {
    { TLV_TAG_FW_VERSION,     TLV_FIELD_STR, offsetof(FirmwareState, firmware_version), sizeof(((FirmwareState*)0)->firmware_version) },
    { TLV_TAG_HW_VERSION,     TLV_FIELD_STR, offsetof(FirmwareState, hardware_version), sizeof(((FirmwareState*)0)->hardware_version) },
    { TLV_TAG_CONFIG_VERSION, TLV_FIELD_U32, offsetof(FirmwareState, config_version),   0 },
};
#define FIRMWARE_STATE_FIELD_COUNT (sizeof(firmware_state_fields) / sizeof(firmware_state_fields[0]))

/* ---- 54.10 StorageState ----
 * 例: {total=8192MB, free=4096MB}
 *     → 01 00 04 00 00 20 00 02 00 04 00 00 10 00 (14B) */
static const TlvFieldDesc storage_state_fields[] = {
    { TLV_TAG_STORAGE_TOTAL, TLV_FIELD_U32, offsetof(StorageState, storage_total_mb), 0 },
    { TLV_TAG_STORAGE_FREE,  TLV_FIELD_U32, offsetof(StorageState, storage_free_mb),  0 },
};
#define STORAGE_STATE_FIELD_COUNT (sizeof(storage_state_fields) / sizeof(storage_state_fields[0]))

/* ---- FactoryDeviceTlv (出厂烧录上报) ----
 * 例: {sales_country="JP", product_key="MCQX_PET_CABIN", model="MCQX-PET-CABIN-V1", hw="HW-V2.0", fw="FW-V1.0.0"}
 *     所有字段均为 TLV_FIELD_STR, device_secret 由内部 generate_device_secret 生成 */
static const TlvFieldDesc factory_device_tlv_fields[] = {
    { TLV_TAG_FACTORY_SALES_COUNTRY, TLV_FIELD_STR, offsetof(FactoryDeviceTlv, sales_country_code), sizeof(((FactoryDeviceTlv*)0)->sales_country_code) },
    { TLV_TAG_FACTORY_PRODUCT_KEY,   TLV_FIELD_STR, offsetof(FactoryDeviceTlv, product_key),       sizeof(((FactoryDeviceTlv*)0)->product_key)       },
    { TLV_TAG_FACTORY_MODEL,         TLV_FIELD_STR, offsetof(FactoryDeviceTlv, model),             sizeof(((FactoryDeviceTlv*)0)->model)             },
    { TLV_TAG_FACTORY_HW_VERSION,    TLV_FIELD_STR, offsetof(FactoryDeviceTlv, hardware_version),  sizeof(((FactoryDeviceTlv*)0)->hardware_version)  },
    { TLV_TAG_FACTORY_FW_VERSION,    TLV_FIELD_STR, offsetof(FactoryDeviceTlv, firmware_version),  sizeof(((FactoryDeviceTlv*)0)->firmware_version)  },
};
#define FACTORY_DEVICE_TLV_FIELD_COUNT (sizeof(factory_device_tlv_fields) / sizeof(factory_device_tlv_fields[0]))

/* ---- FactoryDeviceRejectionTlv (被拒结果下发, V851 → D5) ----
 * 例: {mac="A0B1C2D3E4F5", reason="DUPLICATE_MAC"}
 *     所有字段均为 TLV_FIELD_STR */
static const TlvFieldDesc factory_device_rejection_tlv_fields[] = {
    { TLV_TAG_FACTORY_REJECT_MAC,    TLV_FIELD_STR, offsetof(FactoryDeviceRejectionTlv, mac),    sizeof(((FactoryDeviceRejectionTlv*)0)->mac)    },
    { TLV_TAG_FACTORY_REJECT_REASON, TLV_FIELD_STR, offsetof(FactoryDeviceRejectionTlv, reason), sizeof(((FactoryDeviceRejectionTlv*)0)->reason) },
};
#define FACTORY_DEVICE_REJECTION_TLV_FIELD_COUNT (sizeof(factory_device_rejection_tlv_fields) / sizeof(factory_device_rejection_tlv_fields[0]))

/* ---- BootstrapResultTlv (自举结果下发, V851 → D5/LCD) ----
 * 例: {device_sn="MQXQ-PET-CN-2026-000002", ble_id="AVUU69",
 *       api_endpoint="https://test-api-cn.mcqx.pet", bind_status="UNBOUND",
 *       qr_url="https://b.mcqx.pet?b=AVUU69"}
 *     所有字段均为 TLV_FIELD_STR (struct_type 字段不参与 TLV 编码) */
static const TlvFieldDesc bootstrap_result_tlv_fields[] = {
    { TLV_TAG_BOOT_DEVICE_SN,     TLV_FIELD_STR, offsetof(BootstrapResultTlv, device_sn),    sizeof(((BootstrapResultTlv*)0)->device_sn)    },
    { TLV_TAG_BOOT_BLE_ID,        TLV_FIELD_STR, offsetof(BootstrapResultTlv, ble_id),       sizeof(((BootstrapResultTlv*)0)->ble_id)       },
    { TLV_TAG_BOOT_API_ENDPOINT,  TLV_FIELD_STR, offsetof(BootstrapResultTlv, api_endpoint), sizeof(((BootstrapResultTlv*)0)->api_endpoint) },
    { TLV_TAG_BOOT_BIND_STATUS,   TLV_FIELD_STR, offsetof(BootstrapResultTlv, bind_status),  sizeof(((BootstrapResultTlv*)0)->bind_status)  },
    { TLV_TAG_BOOT_QR_URL,        TLV_FIELD_STR, offsetof(BootstrapResultTlv, qr_url),       sizeof(((BootstrapResultTlv*)0)->qr_url)       },
};
#define BOOTSTRAP_RESULT_TLV_FIELD_COUNT (sizeof(bootstrap_result_tlv_fields) / sizeof(bootstrap_result_tlv_fields[0]))

/* ---- EventAlarmTlv (事件/告警上报指令, D5/LCD → V851) ----
 * 例: {is_alarm=1, code="FILTER_LIFE_EXHAUSTED", level="MEDIUM", recovered=0,
 *      payload="{\"life_percent\":0,\"need_replace\":1}"}
 * is_alarm/recovered 为 u8, code/level/payload 为 STR */
static const TlvFieldDesc event_alarm_tlv_fields[] = {
    { TLV_TAG_EA_IS_ALARM, TLV_FIELD_U8,  offsetof(EventAlarmTlv, is_alarm), 0 },
    { TLV_TAG_EA_CODE,     TLV_FIELD_STR, offsetof(EventAlarmTlv, code),     sizeof(((EventAlarmTlv*)0)->code)     },
    { TLV_TAG_EA_LEVEL,    TLV_FIELD_STR, offsetof(EventAlarmTlv, level),    sizeof(((EventAlarmTlv*)0)->level)    },
    { TLV_TAG_EA_RECOVERED,TLV_FIELD_U8,  offsetof(EventAlarmTlv, recovered),0 },
    { TLV_TAG_EA_PAYLOAD,  TLV_FIELD_STR, offsetof(EventAlarmTlv, payload),  sizeof(((EventAlarmTlv*)0)->payload)  },
};
#define EVENT_ALARM_TLV_FIELD_COUNT (sizeof(event_alarm_tlv_fields) / sizeof(event_alarm_tlv_fields[0]))

/* ============================================================================
 * 简单结构体 — 1 行 wrapper (表驱动)
 * ============================================================================ */

int device_state_tlv_pack(const DeviceState *s, uint8_t *buf, int buf_size)
    { return tlv_generic_pack(s, device_state_fields, DEVICE_STATE_FIELD_COUNT, buf, buf_size); }
int device_state_tlv_unpack(DeviceState *s, const uint8_t *buf, int buf_size)
    { memset(s, 0, sizeof(*s)); return tlv_generic_unpack(s, device_state_fields, DEVICE_STATE_FIELD_COUNT, buf, buf_size); }

int network_state_tlv_pack(const NetworkState *s, uint8_t *buf, int buf_size)
    { return tlv_generic_pack(s, network_state_fields, NETWORK_STATE_FIELD_COUNT, buf, buf_size); }
int network_state_tlv_unpack(NetworkState *s, const uint8_t *buf, int buf_size)
    { memset(s, 0, sizeof(*s)); return tlv_generic_unpack(s, network_state_fields, NETWORK_STATE_FIELD_COUNT, buf, buf_size); }

int power_state_tlv_pack(const PowerState *s, uint8_t *buf, int buf_size)
    { return tlv_generic_pack(s, power_state_fields, POWER_STATE_FIELD_COUNT, buf, buf_size); }
int power_state_tlv_unpack(PowerState *s, const uint8_t *buf, int buf_size)
    { memset(s, 0, sizeof(*s)); return tlv_generic_unpack(s, power_state_fields, POWER_STATE_FIELD_COUNT, buf, buf_size); }

int door_state_tlv_pack(const DoorState *s, uint8_t *buf, int buf_size)
    { return tlv_generic_pack(s, door_state_fields, DOOR_STATE_FIELD_COUNT, buf, buf_size); }
int door_state_tlv_unpack(DoorState *s, const uint8_t *buf, int buf_size)
    { memset(s, 0, sizeof(*s)); return tlv_generic_unpack(s, door_state_fields, DOOR_STATE_FIELD_COUNT, buf, buf_size); }

int environment_state_tlv_pack(const EnvironmentState *s, uint8_t *buf, int buf_size)
    { return tlv_generic_pack(s, environment_state_fields, ENVIRONMENT_STATE_FIELD_COUNT, buf, buf_size); }
int environment_state_tlv_unpack(EnvironmentState *s, const uint8_t *buf, int buf_size)
    { memset(s, 0, sizeof(*s)); return tlv_generic_unpack(s, environment_state_fields, ENVIRONMENT_STATE_FIELD_COUNT, buf, buf_size); }

int actuator_exhaust_tlv_pack(const ActuatorExhaust *s, uint8_t *buf, int buf_size)
    { return tlv_generic_pack(s, actuator_exhaust_fields, EXHAUST_FIELD_COUNT, buf, buf_size); }
int actuator_exhaust_tlv_unpack(ActuatorExhaust *s, const uint8_t *buf, int buf_size)
    { memset(s, 0, sizeof(*s)); return tlv_generic_unpack(s, actuator_exhaust_fields, EXHAUST_FIELD_COUNT, buf, buf_size); }

int actuator_light_tlv_pack(const ActuatorLight *s, uint8_t *buf, int buf_size)
    { return tlv_generic_pack(s, actuator_light_fields, LIGHT_FIELD_COUNT, buf, buf_size); }
int actuator_light_tlv_unpack(ActuatorLight *s, const uint8_t *buf, int buf_size)
    { memset(s, 0, sizeof(*s)); return tlv_generic_unpack(s, actuator_light_fields, LIGHT_FIELD_COUNT, buf, buf_size); }

int actuator_uvb_tlv_pack(const ActuatorUvb *s, uint8_t *buf, int buf_size)
    { return tlv_generic_pack(s, actuator_uvb_fields, UVB_FIELD_COUNT, buf, buf_size); }
int actuator_uvb_tlv_unpack(ActuatorUvb *s, const uint8_t *buf, int buf_size)
    { memset(s, 0, sizeof(*s)); return tlv_generic_unpack(s, actuator_uvb_fields, UVB_FIELD_COUNT, buf, buf_size); }

int actuator_anion_tlv_pack(const ActuatorAnion *s, uint8_t *buf, int buf_size)
    { return tlv_generic_pack(s, actuator_anion_fields, ANION_FIELD_COUNT, buf, buf_size); }
int actuator_anion_tlv_unpack(ActuatorAnion *s, const uint8_t *buf, int buf_size)
    { memset(s, 0, sizeof(*s)); return tlv_generic_unpack(s, actuator_anion_fields, ANION_FIELD_COUNT, buf, buf_size); }

int actuator_plasma_tlv_pack(const ActuatorPlasma *s, uint8_t *buf, int buf_size)
    { return tlv_generic_pack(s, actuator_plasma_fields, PLASMA_FIELD_COUNT, buf, buf_size); }
int actuator_plasma_tlv_unpack(ActuatorPlasma *s, const uint8_t *buf, int buf_size)
    { memset(s, 0, sizeof(*s)); return tlv_generic_unpack(s, actuator_plasma_fields, PLASMA_FIELD_COUNT, buf, buf_size); }

int actuator_climate_tlv_pack(const ActuatorClimate *s, uint8_t *buf, int buf_size)
    { return tlv_generic_pack(s, actuator_climate_fields, CLIMATE_FIELD_COUNT, buf, buf_size); }
int actuator_climate_tlv_unpack(ActuatorClimate *s, const uint8_t *buf, int buf_size)
    { memset(s, 0, sizeof(*s)); return tlv_generic_unpack(s, actuator_climate_fields, CLIMATE_FIELD_COUNT, buf, buf_size); }

int actuator_humidifier_tlv_pack(const ActuatorHumidifier *s, uint8_t *buf, int buf_size)
    { return tlv_generic_pack(s, actuator_humidifier_fields, HUMIDIFIER_FIELD_COUNT, buf, buf_size); }
int actuator_humidifier_tlv_unpack(ActuatorHumidifier *s, const uint8_t *buf, int buf_size)
    { memset(s, 0, sizeof(*s)); return tlv_generic_unpack(s, actuator_humidifier_fields, HUMIDIFIER_FIELD_COUNT, buf, buf_size); }

int actuator_inlet_fan_tlv_pack(const ActuatorInletFan *s, uint8_t *buf, int buf_size)
    { return tlv_generic_pack(s, actuator_inlet_fan_fields, INLET_FAN_FIELD_COUNT, buf, buf_size); }
int actuator_inlet_fan_tlv_unpack(ActuatorInletFan *s, const uint8_t *buf, int buf_size)
    { memset(s, 0, sizeof(*s)); return tlv_generic_unpack(s, actuator_inlet_fan_fields, INLET_FAN_FIELD_COUNT, buf, buf_size); }

int actuator_filter_tlv_pack(const ActuatorFilter *s, uint8_t *buf, int buf_size)
    { return tlv_generic_pack(s, actuator_filter_fields, FILTER_FIELD_COUNT, buf, buf_size); }
int actuator_filter_tlv_unpack(ActuatorFilter *s, const uint8_t *buf, int buf_size)
    { memset(s, 0, sizeof(*s)); return tlv_generic_unpack(s, actuator_filter_fields, FILTER_FIELD_COUNT, buf, buf_size); }

int camera_state_tlv_pack(const CameraState *s, uint8_t *buf, int buf_size)
    { return tlv_generic_pack(s, camera_state_fields, CAMERA_STATE_FIELD_COUNT, buf, buf_size); }
int camera_state_tlv_unpack(CameraState *s, const uint8_t *buf, int buf_size)
    { memset(s, 0, sizeof(*s)); return tlv_generic_unpack(s, camera_state_fields, CAMERA_STATE_FIELD_COUNT, buf, buf_size); }

int display_settings_tlv_pack(const DisplaySettings *s, uint8_t *buf, int buf_size)
    { return tlv_generic_pack(s, display_settings_fields, DISPLAY_SETTINGS_FIELD_COUNT, buf, buf_size); }
int display_settings_tlv_unpack(DisplaySettings *s, const uint8_t *buf, int buf_size)
    { memset(s, 0, sizeof(*s)); return tlv_generic_unpack(s, display_settings_fields, DISPLAY_SETTINGS_FIELD_COUNT, buf, buf_size); }

int firmware_state_tlv_pack(const FirmwareState *s, uint8_t *buf, int buf_size)
    { return tlv_generic_pack(s, firmware_state_fields, FIRMWARE_STATE_FIELD_COUNT, buf, buf_size); }
int firmware_state_tlv_unpack(FirmwareState *s, const uint8_t *buf, int buf_size)
    { memset(s, 0, sizeof(*s)); return tlv_generic_unpack(s, firmware_state_fields, FIRMWARE_STATE_FIELD_COUNT, buf, buf_size); }

int storage_state_tlv_pack(const StorageState *s, uint8_t *buf, int buf_size)
    { return tlv_generic_pack(s, storage_state_fields, STORAGE_STATE_FIELD_COUNT, buf, buf_size); }
int storage_state_tlv_unpack(StorageState *s, const uint8_t *buf, int buf_size)
    { memset(s, 0, sizeof(*s)); return tlv_generic_unpack(s, storage_state_fields, STORAGE_STATE_FIELD_COUNT, buf, buf_size); }

int factory_device_tlv_pack(const FactoryDeviceTlv *s, uint8_t *buf, int buf_size)
    { return tlv_generic_pack(s, factory_device_tlv_fields, FACTORY_DEVICE_TLV_FIELD_COUNT, buf, buf_size); }
int factory_device_tlv_unpack(FactoryDeviceTlv *s, const uint8_t *buf, int buf_size)
    { memset(s, 0, sizeof(*s)); return tlv_generic_unpack(s, factory_device_tlv_fields, FACTORY_DEVICE_TLV_FIELD_COUNT, buf, buf_size); }

int factory_device_rejection_tlv_pack(const FactoryDeviceRejectionTlv *s, uint8_t *buf, int buf_size)
    { return tlv_generic_pack(s, factory_device_rejection_tlv_fields, FACTORY_DEVICE_REJECTION_TLV_FIELD_COUNT, buf, buf_size); }

int bootstrap_result_tlv_pack(const BootstrapResultTlv *s, uint8_t *buf, int buf_size)
    { return tlv_generic_pack(s, bootstrap_result_tlv_fields, BOOTSTRAP_RESULT_TLV_FIELD_COUNT, buf, buf_size); }
int bootstrap_result_tlv_unpack(BootstrapResultTlv *s, const uint8_t *buf, int buf_size)
    { memset(s, 0, sizeof(*s)); return tlv_generic_unpack(s, bootstrap_result_tlv_fields, BOOTSTRAP_RESULT_TLV_FIELD_COUNT, buf, buf_size); }

int event_alarm_tlv_pack(const EventAlarmTlv *s, uint8_t *buf, int buf_size)
    { return tlv_generic_pack(s, event_alarm_tlv_fields, EVENT_ALARM_TLV_FIELD_COUNT, buf, buf_size); }
int event_alarm_tlv_unpack(EventAlarmTlv *s, const uint8_t *buf, int buf_size)
    { memset(s, 0, sizeof(*s)); return tlv_generic_unpack(s, event_alarm_tlv_fields, EVENT_ALARM_TLV_FIELD_COUNT, buf, buf_size); }

/* ============================================================================
 * 嵌套结构体 — 需要特殊处理
 * ============================================================================ */

/* ---- 54.6 ActuatorState (聚合 9 个子结构体) ---- */
#define ACT_TAG_EXHAUST    0x01
#define ACT_TAG_LIGHT      0x02
#define ACT_TAG_UVB        0x03
#define ACT_TAG_ANION      0x04
#define ACT_TAG_PLASMA     0x05
#define ACT_TAG_CLIMATE    0x06
#define ACT_TAG_HUMIDIFIER 0x07
#define ACT_TAG_INLET_FAN  0x08
#define ACT_TAG_FILTER     0x09

static int tlv_put_nested_struct(uint8_t *buf, int buf_size, uint8_t tag,
                                 int (*sub_pack)(const void*, uint8_t*, int),
                                 const void *sub_src)
{
    uint8_t tmp[512];
    int sub_len = sub_pack(sub_src, tmp, sizeof(tmp));
    if (sub_len < 0) return -1;
    return tlv_put_bytes(buf, buf_size, tag, tmp, (uint16_t)sub_len);
}

int actuator_state_tlv_pack(const ActuatorState *s, uint8_t *buf, int buf_size)
{
    if (!s || !buf || buf_size < 0) return -1;
    int total = 0, ret;

    #define PACK_NESTED(tag, member, fn) \
        ret = tlv_put_nested_struct(buf + total, buf_size - total, tag, \
                                    (int(*)(const void*,uint8_t*,int))fn, &s->member); \
        if (ret < 0) return -1; total += ret;

    PACK_NESTED(ACT_TAG_EXHAUST,    exhaust,    actuator_exhaust_tlv_pack);
    PACK_NESTED(ACT_TAG_LIGHT,      light,      actuator_light_tlv_pack);
    PACK_NESTED(ACT_TAG_UVB,        uvb,        actuator_uvb_tlv_pack);
    PACK_NESTED(ACT_TAG_ANION,      anion,      actuator_anion_tlv_pack);
    PACK_NESTED(ACT_TAG_PLASMA,     plasma,     actuator_plasma_tlv_pack);
    PACK_NESTED(ACT_TAG_CLIMATE,    climate,    actuator_climate_tlv_pack);
    PACK_NESTED(ACT_TAG_HUMIDIFIER, humidifier, actuator_humidifier_tlv_pack);
    PACK_NESTED(ACT_TAG_INLET_FAN,  inlet_fan,  actuator_inlet_fan_tlv_pack);
    PACK_NESTED(ACT_TAG_FILTER,     filter,     actuator_filter_tlv_pack);

    #undef PACK_NESTED
    return total;
}

int actuator_state_tlv_unpack(ActuatorState *s, const uint8_t *buf, int buf_size)
{
    if (!s || !buf || buf_size < 0) return -1;
    memset(s, 0, sizeof(*s));
    int offset = 0, consumed;

    #define UNPACK_NESTED(tag, member, fn) \
        case tag: \
            consumed = tlv_read_nested(buf + offset, buf_size - offset, tag, &nested, &nested_len); \
            if (consumed < 0) return -1; \
            fn(&s->member, nested, nested_len); \
            offset += consumed; break;

    while (offset < buf_size) {
        const uint8_t *nested;
        uint16_t nested_len;
        switch (buf[offset]) {
            UNPACK_NESTED(ACT_TAG_EXHAUST,    exhaust,    actuator_exhaust_tlv_unpack);
            UNPACK_NESTED(ACT_TAG_LIGHT,      light,      actuator_light_tlv_unpack);
            UNPACK_NESTED(ACT_TAG_UVB,        uvb,        actuator_uvb_tlv_unpack);
            UNPACK_NESTED(ACT_TAG_ANION,      anion,      actuator_anion_tlv_unpack);
            UNPACK_NESTED(ACT_TAG_PLASMA,     plasma,     actuator_plasma_tlv_unpack);
            UNPACK_NESTED(ACT_TAG_CLIMATE,    climate,    actuator_climate_tlv_unpack);
            UNPACK_NESTED(ACT_TAG_HUMIDIFIER, humidifier, actuator_humidifier_tlv_unpack);
            UNPACK_NESTED(ACT_TAG_INLET_FAN,  inlet_fan,  actuator_inlet_fan_tlv_unpack);
            UNPACK_NESTED(ACT_TAG_FILTER,     filter,     actuator_filter_tlv_unpack);
        default:
            consumed = tlv_skip(buf + offset, buf_size - offset);
            if (consumed < 0) return -1; offset += consumed; break;
        }
    }
    #undef UNPACK_NESTED
    return offset;
}

/* ---- 54.8 SettingsState (含嵌套 DisplaySettings) ---- */

int settings_state_tlv_pack(const SettingsState *s, uint8_t *buf, int buf_size)
{
    if (!s || !buf || buf_size < 0) return -1;
    int total = 0, ret;
    uint8_t tmp[128];

    ret = tlv_put_str(buf + total, buf_size - total, TLV_TAG_TEMP_UNIT,
                      s->temperature_unit);
    if (ret < 0) return -1; total += ret;

    ret = display_settings_tlv_pack(&s->display, tmp, sizeof(tmp));
    if (ret < 0) return -1;
    ret = tlv_put_bytes(buf + total, buf_size - total, TLV_TAG_DISPLAY,
                        tmp, (uint16_t)ret);
    if (ret < 0) return -1; total += ret;

    ret = tlv_put_u8(buf + total, buf_size - total, TLV_TAG_VOLUME_PERCENT,
                     (uint8_t)s->volume_percent);
    if (ret < 0) return -1; total += ret;

    ret = tlv_put_str(buf + total, buf_size - total, TLV_TAG_LANGUAGE, s->language);
    if (ret < 0) return -1; total += ret;

    ret = tlv_put_u8(buf + total, buf_size - total, TLV_TAG_LOCAL_PASSWORD,
                     (uint8_t)s->local_password_enabled);
    if (ret < 0) return -1; total += ret;

    return total;
}

int settings_state_tlv_unpack(SettingsState *s, const uint8_t *buf, int buf_size)
{
    if (!s || !buf || buf_size < 0) return -1;
    memset(s, 0, sizeof(*s));
    int offset = 0, consumed;

    while (offset < buf_size) {
        uint8_t tag = buf[offset];
        switch (tag) {
        case TLV_TAG_TEMP_UNIT:
            consumed = tlv_read_str(buf + offset, buf_size - offset,
                                    TLV_TAG_TEMP_UNIT,
                                    s->temperature_unit, sizeof(s->temperature_unit));
            if (consumed < 0) return -1; offset += consumed; break;
        case TLV_TAG_DISPLAY: {
            const uint8_t *nested;
            uint16_t nested_len;
            consumed = tlv_read_nested(buf + offset, buf_size - offset,
                                       TLV_TAG_DISPLAY, &nested, &nested_len);
            if (consumed < 0) return -1;
            display_settings_tlv_unpack(&s->display, nested, nested_len);
            offset += consumed; break;
        }
        case TLV_TAG_VOLUME_PERCENT: {
            uint8_t v; consumed = tlv_read_u8(buf + offset, buf_size - offset,
                                              TLV_TAG_VOLUME_PERCENT, &v);
            if (consumed < 0) return -1;
            s->volume_percent = (int)v; offset += consumed; break;
        }
        case TLV_TAG_LANGUAGE:
            consumed = tlv_read_str(buf + offset, buf_size - offset,
                                    TLV_TAG_LANGUAGE,
                                    s->language, sizeof(s->language));
            if (consumed < 0) return -1; offset += consumed; break;
        case TLV_TAG_LOCAL_PASSWORD: {
            uint8_t v; consumed = tlv_read_u8(buf + offset, buf_size - offset,
                                              TLV_TAG_LOCAL_PASSWORD, &v);
            if (consumed < 0) return -1;
            s->local_password_enabled = (int)v; offset += consumed; break;
        }
        default:
            consumed = tlv_skip(buf + offset, buf_size - offset);
            if (consumed < 0) return -1; offset += consumed; break;
        }
    }
    return offset;
}
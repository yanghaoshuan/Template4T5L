#include "v851_protocol.h"

#if bleV851_BRIDGE_ENABLED

#include "bridge_json.h"
#include "core_json.h"
#include "pb03f_ble.h"
#include "timer.h"
#include "uart.h"
#include "v851_control_info.h"
#if otaOTA_ENABLED
#include "ota.h"
#endif

#include <string.h>

#pragma optimize(8,size)

#define V851_OTA_TX_MAX                        64U
#define V851_JSON_TX_DEPTH                     2U
#define V851_ERROR_CODE_MAX                    64U
#define V851_DEDUP_DEPTH                       4U
#define V851_STATE_MIN_REPORT_INTERVAL_MS       5000UL
#define V851_STATE_CONTROL_SCAN_INTERVAL_MS      500UL

typedef struct
{
    char command_id[V851_COMMAND_ID_MAX + 1U];
    char server_msg_id[V851_COMMAND_ID_MAX + 1U];
    char cmd[V851_COMMAND_NAME_MAX + 1U];
    const uint8_t *params_json;
    uint16_t params_len;
} V851CommandContext;

typedef struct
{
    uint8_t valid;
    V851CommandContext context;
    V851ControlStatus status;
    char error_code[V851_ERROR_CODE_MAX + 1U];
} V851DedupRecord;

static uint8_t xdata v851_tx_buffer[V851_JSON_TX_DEPTH][V851_JSON_FRAME_MAX];
static uint16_t v851_tx_len[V851_JSON_TX_DEPTH];
static uint8_t v851_tx_head;
static uint8_t v851_tx_tail;
static uint8_t v851_tx_count;

static uint8_t v851_ota_tx_frame[V851_OTA_TX_MAX];
static uint16_t v851_ota_tx_len;
static uint8_t v851_ota_tx_pending;

static uint8_t xdata v851_ack_json[BRIDGE_JSON_MAX + 1U];
static V851DeviceInfo v851_device_info;
static V851ControlHandler v851_handlers[V851_CONTROL_COUNT];
static V851DedupRecord v851_dedup[V851_DEDUP_DEPTH];
static uint8_t v851_dedup_next;
static uint32_t v851_server_time;
static uint32_t v851_server_tick;
static uint8_t v851_time_valid;
static uint32_t v851_ack_seq;
static V851CommandContext v851_ota_command;
static uint8_t v851_ota_command_active;
static uint8_t v851_ota_terminal_pending;
static V851OtaStage v851_ota_terminal_stage;
static uint8_t v851_ota_terminal_progress;
static char v851_ota_terminal_error[V851_ERROR_CODE_MAX + 1U];
static uint8_t xdata v851_state_report_json[BRIDGE_JSON_MAX + 1U];
static uint32_t v851_state_report_seq;
static uint32_t v851_state_last_report_tick;
static uint16_t v851_state_pending_mask;
static uint8_t v851_state_snapshot_pending;
static uint8_t v851_state_snapshot_sent;
static uint8_t v851_state_report_started;
static uint8_t xdata v851_state_control_shadow
    [V851_CONTROL_DEVICE_SETTINGS][4U];
static uint32_t v851_state_control_scan_tick;

static void V851WriteBe16(uint8_t *_data, uint16_t value)
{
    _data[0] = (uint8_t)(value >> 8);
    _data[1] = (uint8_t)value;
}

static void V851CopyText(char *out, uint16_t out_size,
                         const char *value, uint16_t value_len)
{
    if((out == NULL) || (out_size == 0U))
    {
        return;
    }

    if(value_len >= out_size)
    {
        value_len = out_size - 1U;
    }
    if((value != NULL) && (value_len != 0U))
    {
        memcpy(out, value, value_len);
    }
    out[value_len] = '\0';
}

static void V851CopyJsonStringIfPresent(const uint8_t *_data,
                                        uint16_t len,
                                        const char *path,
                                        char *out,
                                        uint16_t out_size)
{
    const char *value;
    json_size_t value_len;
    JSONTypes_t value_type;

    if((JSON_SearchConst((const char *)_data, len,
                         path, (json_size_t)strlen(path),
                         &value, &value_len,
                         &value_type) == JSONSuccess) &&
       (value_type == JSONString) && (value_len < out_size))
    {
        V851CopyText(out, out_size, value, value_len);
    }
}

static void V851StateRequestSnapshot(void)
{
    if((v851_device_info.device_sn[0] != '\0') &&
       (v851_device_info.product_key[0] != '\0'))
    {
        v851_state_snapshot_pending = 1U;
    }
}

uint32_t V851ProtocolGetTimestamp(void)
{
    if(v851_time_valid == 0U)
    {
        return 0UL;
    }

    return v851_server_time + ((uint32_t)(GetSysTick() - v851_server_tick) / 1000UL);
}

const V851DeviceInfo *V851ProtocolGetDeviceInfo(void)
{
    return &v851_device_info;
}

static void V851CacheDeviceHello(const uint8_t *_data, uint16_t len)
{
    V851CopyJsonStringIfPresent(_data, len, "device_sn",
                                v851_device_info.device_sn,
                                sizeof(v851_device_info.device_sn));
    V851CopyJsonStringIfPresent(_data, len, "ble_id",
                                v851_device_info.ble_id,
                                sizeof(v851_device_info.ble_id));
    V851CopyJsonStringIfPresent(_data, len, "product_key",
                                v851_device_info.product_key,
                                sizeof(v851_device_info.product_key));
    V851CopyJsonStringIfPresent(_data, len, "data.device_identity.device_sn",
                                v851_device_info.device_sn,
                                sizeof(v851_device_info.device_sn));
    V851CopyJsonStringIfPresent(_data, len, "data.device_identity.ble_id",
                                v851_device_info.ble_id,
                                sizeof(v851_device_info.ble_id));
    V851CopyJsonStringIfPresent(_data, len, "data.device_identity.ble_name",
                                v851_device_info.ble_name,
                                sizeof(v851_device_info.ble_name));
    V851CopyJsonStringIfPresent(_data, len, "data.device_identity.product_key",
                                v851_device_info.product_key,
                                sizeof(v851_device_info.product_key));
    V851CopyJsonStringIfPresent(_data, len, "data.device_identity.model",
                                v851_device_info.model,
                                sizeof(v851_device_info.model));
    V851CopyJsonStringIfPresent(_data, len, "data.device_identity.hardware_version",
                                v851_device_info.hardware_version,
                                sizeof(v851_device_info.hardware_version));
    V851CopyJsonStringIfPresent(_data, len, "data.device_identity.firmware_version",
                                v851_device_info.firmware_version,
                                sizeof(v851_device_info.firmware_version));
    V851CopyJsonStringIfPresent(_data, len, "data.device_identity.sales_country_code",
                                v851_device_info.sales_country_code,
                                sizeof(v851_device_info.sales_country_code));

    if(v851_device_info.ble_id[0] != '\0')
    {
        Pb03fBleSetProvisionedIdentity((const uint8_t *)v851_device_info.ble_id,
                                      (uint16_t)strlen(v851_device_info.ble_id));
    }
    V851StateRequestSnapshot();
}

static void V851CacheHelloAck(const uint8_t *_data, uint16_t len)
{
    uint32_t server_time;

    V851CopyJsonStringIfPresent(_data, len, "data.bind_status",
                                v851_device_info.bind_status,
                                sizeof(v851_device_info.bind_status));
    V851CopyJsonStringIfPresent(_data, len, "data.sales_country_code",
                                v851_device_info.sales_country_code,
                                sizeof(v851_device_info.sales_country_code));

    if((JSONSearchToNumber(_data, len,
                           "data.server_time",
                           sizeof("data.server_time") - 1U,
                           &server_time) == JSONSuccess) ||
       (JSONSearchToNumber(_data, len,
                           "timestamp", sizeof("timestamp") - 1U,
                           &server_time) == JSONSuccess))
    {
        v851_server_time = server_time;
        v851_server_tick = GetSysTick();
        v851_time_valid = 1U;
    }
    V851StateRequestSnapshot();
}

static V851ControlType V851MapControlType(const char *cmd)
{
    if(strcmp(cmd, "exhaust.set") == 0) return V851_CONTROL_EXHAUST;
    if(strcmp(cmd, "plasma.set") == 0) return V851_CONTROL_PLASMA;
    if(strcmp(cmd, "anion.set") == 0) return V851_CONTROL_ANION;
    if(strcmp(cmd, "climate.set") == 0) return V851_CONTROL_CLIMATE;
    if(strcmp(cmd, "inlet_fan.set") == 0) return V851_CONTROL_INLET_FAN;
    if(strcmp(cmd, "humidifier.set") == 0) return V851_CONTROL_HUMIDIFIER;
    if(strcmp(cmd, "uvb.set") == 0) return V851_CONTROL_UVB;
    if(strcmp(cmd, "light.set") == 0) return V851_CONTROL_LIGHT;
    if(strcmp(cmd, "device_settings.set") == 0) return V851_CONTROL_DEVICE_SETTINGS;
    if(strcmp(cmd, "device_password.set") == 0) return V851_CONTROL_DEVICE_PASSWORD;
    if(strcmp(cmd, "ota.upgrade") == 0) return V851_CONTROL_OTA;
    return V851_CONTROL_INVALID;
}

static const char *V851StatusText(V851ControlStatus status)
{
    switch(status)
    {
        case V851_CONTROL_STATUS_SUCCESS: return "SUCCESS";
        case V851_CONTROL_STATUS_FAILED: return "FAILED";
        case V851_CONTROL_STATUS_BUSY: return "BUSY";
        default: return "UNSUPPORTED";
    }
}

static void V851WriterIdentity(BridgeJsonWriter *writer)
{
    BridgeJsonWriterText(writer, "\"device_sn\":");
    BridgeJsonWriterQuoted(writer, v851_device_info.device_sn,
                           (uint16_t)strlen(v851_device_info.device_sn));
    if(v851_device_info.ble_id[0] != '\0')
    {
        BridgeJsonWriterText(writer, ",\"ble_id\":");
        BridgeJsonWriterQuoted(writer, v851_device_info.ble_id,
                               (uint16_t)strlen(v851_device_info.ble_id));
    }
    BridgeJsonWriterText(writer, ",\"product_key\":");
    BridgeJsonWriterQuoted(writer, v851_device_info.product_key,
                           (uint16_t)strlen(v851_device_info.product_key));
}

static uint16_t V851StateTypeMask(V851ControlType type)
{
    return (uint16_t)((uint16_t)1U << (uint8_t)type);
}

static uint16_t V851StateReadWord(const uint8_t *record, uint8_t index)
{
    index = (uint8_t)(index * 2U);
    return ((uint16_t)record[index] << 8) | record[index + 1U];
}

static uint8_t V851StateReadControl(V851ControlType type,
                                    uint8_t *record,
                                    const char **group,
                                    const char **value_name,
                                    uint16_t *value_max)
{
    uint32_t address;

    *value_name = NULL;
    *value_max = 0U;
    switch(type)
    {
        case V851_CONTROL_EXHAUST:
            address = V851_CONTROL_DGUS_EXHAUST_ADDR;
            *group = "exhaust";
            *value_name = "speed_level";
            *value_max = 6U;
            break;

        case V851_CONTROL_LIGHT:
            address = V851_CONTROL_DGUS_LIGHT_ADDR;
            *group = "light";
            *value_name = "brightness_level";
            *value_max = 4U;
            break;

        case V851_CONTROL_UVB:
            address = V851_CONTROL_DGUS_UVB_ADDR;
            *group = "uvb";
            break;

        case V851_CONTROL_ANION:
            address = V851_CONTROL_DGUS_ANION_ADDR;
            *group = "anion";
            break;

        case V851_CONTROL_PLASMA:
            address = V851_CONTROL_DGUS_PLASMA_ADDR;
            *group = "plasma";
            break;

        case V851_CONTROL_CLIMATE:
            address = V851_CONTROL_DGUS_CLIMATE_ADDR;
            *group = "climate";
            *value_name = "target_celsius";
            *value_max = 100U;
            break;

        case V851_CONTROL_HUMIDIFIER:
            address = V851_CONTROL_DGUS_HUMIDIFIER_ADDR;
            *group = "humidifier";
            break;

        case V851_CONTROL_INLET_FAN:
            address = V851_CONTROL_DGUS_INLET_FAN_ADDR;
            *group = "inlet_fan";
            *value_name = "speed_level";
            *value_max = 4U;
            break;

        default:
            return 0U;
    }

    memset(record, 0, V851_CONTROL_DGUS_SLOT_BYTES);
    read_dgus_vp(address, record, V851_CONTROL_DGUS_SLOT_WORDS);
    return 1U;
}

static uint32_t V851StateNextSequence(void)
{
    v851_state_report_seq++;
    if((v851_state_report_seq == 0UL) ||
       (v851_state_report_seq > 2147483647UL))
    {
        v851_state_report_seq = 1UL;
    }
    return v851_state_report_seq;
}

static void V851StateWriterBool(BridgeJsonWriter *writer, uint16_t value)
{
    BridgeJsonWriterText(writer, (value != 0U) ? "true" : "false");
}

static void V851StateWriterReportHeader(BridgeJsonWriter *writer,
                                        const char *msg_type,
                                        uint32_t timestamp,
                                        uint32_t sequence)
{
    BridgeJsonWriterText(writer, "{\"msg_id\":\"");
    BridgeJsonWriterUint32(writer, timestamp);
    BridgeJsonWriterText(writer, "-");
    BridgeJsonWriterUint32(writer, sequence);
    BridgeJsonWriterText(writer, "\",\"msg_type\":");
    BridgeJsonWriterQuoted(writer, msg_type, (uint16_t)strlen(msg_type));
    BridgeJsonWriterText(writer, ",\"protocol_version\":\"1.0\",");
    V851WriterIdentity(writer);
    BridgeJsonWriterText(writer, ",\"timestamp\":");
    BridgeJsonWriterUint32(writer, timestamp);
    BridgeJsonWriterText(writer, ",\"seq\":");
    BridgeJsonWriterUint32(writer, sequence);
}

static void V851StateWriterProperty(BridgeJsonWriter *writer,
                                    const char *group,
                                    const char *field,
                                    uint16_t value,
                                    uint8_t boolean_value,
                                    uint32_t timestamp,
                                    uint8_t *first)
{
    if(*first == 0U)
    {
        BridgeJsonWriterText(writer, ",");
    }
    *first = 0U;
    BridgeJsonWriterText(writer, "{\"path\":\"actuators.");
    BridgeJsonWriterText(writer, group);
    BridgeJsonWriterText(writer, ".");
    BridgeJsonWriterText(writer, field);
    BridgeJsonWriterText(writer, "\",\"value\":");
    if(boolean_value != 0U)
    {
        V851StateWriterBool(writer, value);
        BridgeJsonWriterText(writer, ",\"value_type\":\"boolean\",");
    }else
    {
        BridgeJsonWriterUint32(writer, value);
        BridgeJsonWriterText(writer, ",\"value_type\":\"number\",");
    }
    BridgeJsonWriterText(writer, "\"reported_at\":");
    BridgeJsonWriterUint32(writer, timestamp);
    BridgeJsonWriterText(writer, "}");
}

static uint8_t V851StateWriterActuator(BridgeJsonWriter *writer,
                                      V851ControlType type,
                                      uint8_t property_mode,
                                      uint32_t timestamp,
                                      uint8_t *first)
{
    uint8_t record[V851_CONTROL_DGUS_SLOT_BYTES];
    const char *group;
    const char *value_name;
    uint16_t enabled;
    uint16_t value;
    uint16_t value_max;

    if(V851StateReadControl(type, record, &group,
                            &value_name, &value_max) == 0U)
    {
        return 0U;
    }
    memcpy(v851_state_control_shadow[type], record,
           (value_name != NULL) ? 4U : 2U);
    enabled = V851StateReadWord(record, 0U);
    value = V851StateReadWord(record, 1U);
    if(property_mode != 0U)
    {
        V851StateWriterProperty(writer, group, "enabled",
                                enabled, 1U, timestamp, first);
        if((value_name != NULL) &&
           (value >= 1U) && (value <= value_max))
        {
            V851StateWriterProperty(writer, group, value_name,
                                    value, 0U, timestamp, first);
        }
    }else
    {
        if(*first == 0U)
        {
            BridgeJsonWriterText(writer, ",");
        }
        *first = 0U;
        BridgeJsonWriterQuoted(writer, group, (uint16_t)strlen(group));
        BridgeJsonWriterText(writer, ":{\"enabled\":");
        V851StateWriterBool(writer, enabled);
        if((value_name != NULL) &&
           (value >= 1U) && (value <= value_max))
        {
            BridgeJsonWriterText(writer, ",");
            BridgeJsonWriterQuoted(writer, value_name,
                                   (uint16_t)strlen(value_name));
            BridgeJsonWriterText(writer, ":");
            BridgeJsonWriterUint32(writer, value);
        }
        BridgeJsonWriterText(writer, "}");
    }
    return 1U;
}

static uint16_t V851StateBuildSnapshot(void)
{
    BridgeJsonWriter writer;
    V851ControlType type;
    uint32_t timestamp;
    uint8_t first;

    timestamp = V851ProtocolGetTimestamp();
    BridgeJsonWriterInit(&writer, v851_state_report_json, BRIDGE_JSON_MAX);
    V851StateWriterReportHeader(&writer, "device.snapshot", timestamp,
                                V851StateNextSequence());
    BridgeJsonWriterText(&writer, ",\"data\":{\"state\":{\"device\":{"
                         "\"bind_status\":");
    BridgeJsonWriterQuoted(&writer, v851_device_info.bind_status,
                           (uint16_t)strlen(v851_device_info.bind_status));
    BridgeJsonWriterText(&writer, "},\"actuators\":{");
    first = 1U;
    for(type = V851_CONTROL_EXHAUST;
        type < V851_CONTROL_DEVICE_SETTINGS;
        type++)
    {
        (void)V851StateWriterActuator(
            &writer, type, 0U, timestamp, &first);
    }
    BridgeJsonWriterText(&writer, "}},\"reported_at\":");
    BridgeJsonWriterUint32(&writer, timestamp);
    BridgeJsonWriterText(&writer, "}}");
    return BridgeJsonWriterFinish(&writer);
}

static uint16_t V851StateBuildPropertyReport(uint16_t pending_mask)
{
    BridgeJsonWriter writer;
    V851ControlType type;
    uint32_t timestamp;
    uint8_t first;

    timestamp = V851ProtocolGetTimestamp();
    BridgeJsonWriterInit(&writer, v851_state_report_json, BRIDGE_JSON_MAX);
    V851StateWriterReportHeader(&writer, "device.property_report",
                                timestamp, V851StateNextSequence());
    BridgeJsonWriterText(&writer, ",\"data\":{\"properties\":[");
    first = 1U;
    for(type = V851_CONTROL_EXHAUST;
        type < V851_CONTROL_DEVICE_SETTINGS;
        type++)
    {
        if((pending_mask & V851StateTypeMask(type)) != 0U)
        {
            (void)V851StateWriterActuator(
                &writer, type, 1U, timestamp, &first);
        }
    }
    if(first != 0U)
    {
        return 0U;
    }
    BridgeJsonWriterText(&writer, "]}}");
    return BridgeJsonWriterFinish(&writer);
}

static void V851StateMarkControlChanged(V851ControlType type)
{
    if(type < V851_CONTROL_DEVICE_SETTINGS)
    {
        v851_state_pending_mask |= V851StateTypeMask(type);
    }
}

static void V851StateScanControls(void)
{
    uint8_t record[V851_CONTROL_DGUS_SLOT_BYTES];
    const char *group;
    const char *value_name;
    uint16_t value_max;
    uint32_t tick;
    V851ControlType type;
    uint8_t compare_len;

    if(v851_state_snapshot_sent == 0U)
    {
        return;
    }
    tick = GetSysTick();
    if((uint32_t)(tick - v851_state_control_scan_tick) <
       V851_STATE_CONTROL_SCAN_INTERVAL_MS)
    {
        return;
    }
    v851_state_control_scan_tick = tick;
    for(type = V851_CONTROL_EXHAUST;
        type < V851_CONTROL_DEVICE_SETTINGS;
        type++)
    {
        (void)V851StateReadControl(type, record, &group,
                                   &value_name, &value_max);
        compare_len = (value_name != NULL) ? 4U : 2U;
        if(memcmp(v851_state_control_shadow[type],
                  record, compare_len) != 0)
        {
            memcpy(v851_state_control_shadow[type],
                   record, compare_len);
            V851StateMarkControlChanged(type);
        }
    }
}

static void V851StateServiceReports(void)
{
    uint16_t json_len;

    V851StateScanControls();
    if((v851_time_valid == 0U) ||
       (v851_device_info.device_sn[0] == '\0') ||
       (v851_device_info.product_key[0] == '\0') ||
       (v851_tx_count >= V851_JSON_TX_DEPTH))
    {
        return;
    }
    if(v851_state_snapshot_pending != 0U)
    {
        json_len = V851StateBuildSnapshot();
    }else
    {
        if((v851_state_snapshot_sent == 0U) ||
           (v851_state_pending_mask == 0U) ||
           ((v851_state_report_started != 0U) &&
            ((uint32_t)(GetSysTick() - v851_state_last_report_tick) <
             V851_STATE_MIN_REPORT_INTERVAL_MS)))
        {
            return;
        }
        json_len = V851StateBuildPropertyReport(v851_state_pending_mask);
    }
    if((json_len == 0U) ||
       (V851ProtocolSendJson(v851_state_report_json, json_len) == 0U))
    {
        return;
    }
    if(v851_state_snapshot_pending != 0U)
    {
        v851_state_snapshot_pending = 0U;
        v851_state_snapshot_sent = 1U;
        v851_state_pending_mask = 0U;
        v851_state_report_started = 0U;
    }else
    {
        v851_state_pending_mask = 0U;
        v851_state_last_report_tick = GetSysTick();
        v851_state_report_started = 1U;
    }
}

static uint8_t V851SendCommandAck(const V851CommandContext *context,
                                  const char *status,
                                  const char *error_code,
                                  const char *error_message,
                                  const char *stage,
                                  uint8_t progress,
                                  const uint8_t *applied_json,
                                  uint16_t applied_len)
{
    BridgeJsonWriter writer;
    uint32_t timestamp;
    uint32_t seq;
    uint16_t ack_len;

    if((context == NULL) || (status == NULL))
    {
        return 0U;
    }

    timestamp = V851ProtocolGetTimestamp();
    seq = ++v851_ack_seq;

build_ack:
    BridgeJsonWriterInit(&writer, v851_ack_json, BRIDGE_JSON_MAX);
    BridgeJsonWriterText(&writer, "{\"msg_id\":\"ACK");
    BridgeJsonWriterUint32(&writer, timestamp);
    BridgeJsonWriterText(&writer, "-");
    BridgeJsonWriterUint32(&writer, seq);
    BridgeJsonWriterText(&writer,
                         "\",\"msg_type\":\"device.command_ack\","
                         "\"protocol_version\":\"1.0\",");
    V851WriterIdentity(&writer);
    BridgeJsonWriterText(&writer, ",\"timestamp\":");
    BridgeJsonWriterUint32(&writer, timestamp);
    BridgeJsonWriterText(&writer, ",\"seq\":");
    BridgeJsonWriterUint32(&writer, seq);
    BridgeJsonWriterText(&writer, ",\"data\":{\"command_id\":");
    BridgeJsonWriterQuoted(&writer, context->command_id,
                           (uint16_t)strlen(context->command_id));
    BridgeJsonWriterText(&writer, ",\"server_msg_id\":");
    BridgeJsonWriterQuoted(&writer, context->server_msg_id,
                           (uint16_t)strlen(context->server_msg_id));
    BridgeJsonWriterText(&writer, ",\"status\":");
    BridgeJsonWriterQuoted(&writer, status, (uint16_t)strlen(status));

    if(stage != NULL)
    {
        BridgeJsonWriterText(&writer, ",\"stage\":");
        BridgeJsonWriterQuoted(&writer, stage, (uint16_t)strlen(stage));
        BridgeJsonWriterText(&writer, ",\"progress\":");
        BridgeJsonWriterUint32(&writer, progress);
    }

    if(error_code != NULL)
    {
        BridgeJsonWriterText(&writer, ",\"error_code\":");
        BridgeJsonWriterQuoted(&writer, error_code, (uint16_t)strlen(error_code));
        if(error_message != NULL)
        {
            BridgeJsonWriterText(&writer, ",\"error_message\":");
            BridgeJsonWriterQuoted(&writer, error_message,
                                   (uint16_t)strlen(error_message));
        }
    }else if(stage == NULL)
    {
        BridgeJsonWriterText(&writer, ",\"result\":{\"cmd\":");
        BridgeJsonWriterQuoted(&writer, context->cmd,
                               (uint16_t)strlen(context->cmd));
        BridgeJsonWriterText(&writer, ",\"executed\":true,\"applied\":");
        if((applied_json != NULL) && (applied_len != 0U))
        {
            BridgeJsonWriterRaw(&writer, applied_json, applied_len);
        }else
        {
            BridgeJsonWriterText(&writer, "{}");
        }
        BridgeJsonWriterText(&writer, "}");
    }

    BridgeJsonWriterText(&writer, "}}");
    ack_len = BridgeJsonWriterFinish(&writer);
    if(ack_len == 0U)
    {
        if((applied_json != NULL) && (applied_len != 0U))
        {
            applied_json = NULL;
            applied_len = 0U;
            goto build_ack;
        }
        return 0U;
    }
    return V851ProtocolSendJson(v851_ack_json, ack_len);
}

static V851DedupRecord *V851FindDedup(const char *command_id)
{
    uint8_t i;

    for(i = 0U; i < V851_DEDUP_DEPTH; i++)
    {
        if((v851_dedup[i].valid != 0U) &&
           (strcmp(v851_dedup[i].context.command_id, command_id) == 0))
        {
            return &v851_dedup[i];
        }
    }
    return NULL;
}

static void V851SaveDedup(const V851CommandContext *context,
                          const V851ControlResult *result)
{
    V851DedupRecord *record;

    record = &v851_dedup[v851_dedup_next];
    memset(record, 0, sizeof(*record));
    record->valid = 1U;
    memcpy(&record->context, context, sizeof(*context));
    record->context.params_json = NULL;
    record->context.params_len = 0U;
    record->status = result->status;
    if(result->error_code != NULL)
    {
        V851CopyText(record->error_code, sizeof(record->error_code),
                     result->error_code, (uint16_t)strlen(result->error_code));
    }

    v851_dedup_next++;
    if(v851_dedup_next >= V851_DEDUP_DEPTH)
    {
        v851_dedup_next = 0U;
    }
}

static uint8_t V851LoadCommandContext(const uint8_t *_data,
                                      uint16_t len,
                                      V851CommandContext *context,
                                      V851ControlCommand *command)
{
    const char *params;
    json_size_t params_len;
    JSONTypes_t params_type;
    uint32_t expire_at = 0UL;

    memset(context, 0, sizeof(*context));
    memset(command, 0, sizeof(*command));

    if((JSONSearchToArray(_data, len,
                          "msg_id", sizeof("msg_id") - 1U,
                          context->server_msg_id,
                          sizeof(context->server_msg_id)) != JSONSuccess) ||
       (JSONSearchToArray(_data, len,
                          "data.command_id",
                          sizeof("data.command_id") - 1U,
                          context->command_id,
                          sizeof(context->command_id)) != JSONSuccess) ||
       (JSONSearchToArray(_data, len,
                          "data.cmd", sizeof("data.cmd") - 1U,
                          context->cmd,
                          sizeof(context->cmd)) != JSONSuccess) ||
       (JSON_SearchConst((const char *)_data, len,
                         "data.params", sizeof("data.params") - 1U,
                         &params, &params_len,
                         &params_type) != JSONSuccess) ||
       (params_type != JSONObject))
    {
        return 0U;
    }

    (void)JSONSearchToNumber(_data, len,
                             "data.expire_at",
                             sizeof("data.expire_at") - 1U,
                             &expire_at);
    context->params_json = (const uint8_t *)params;
    context->params_len = params_len;

    command->type = V851MapControlType(context->cmd);
    command->command_id = context->command_id;
    command->command_id_len = (uint16_t)strlen(context->command_id);
    command->server_msg_id = context->server_msg_id;
    command->server_msg_id_len = (uint16_t)strlen(context->server_msg_id);
    command->cmd = context->cmd;
    command->cmd_len = (uint16_t)strlen(context->cmd);
    command->params_json = context->params_json;
    command->params_len = context->params_len;
    command->expire_at = expire_at;
    return 1U;
}

static void V851SaveOtaContext(const V851CommandContext *context)
{
    memcpy(&v851_ota_command, context, sizeof(v851_ota_command));
    v851_ota_command.params_json = NULL;
    v851_ota_command.params_len = 0U;
    v851_ota_command_active = 1U;
}

static void V851HandleOtaCommand(const V851CommandContext *context)
{
    V851SaveOtaContext(context);

    #if otaOTA_ENABLED
    if(OtaCompleteFlag != 0U)
    {
        if(V851SendCommandAck(&v851_ota_command, "SUCCESS", NULL, NULL,
                              "SUCCESS", 100U, NULL, 0U) != 0U)
        {
            V851ProtocolTask();
            delay_ms(20);
            V851ProtocolTask();
            delay_ms(20);
            OtaAcknowledgeComplete();
            v851_ota_command_active = 0U;
        }
    }else
    {
        (void)V851SendCommandAck(&v851_ota_command, "RECEIVED", NULL, NULL,
                                 "INSTALLING", 0U, NULL, 0U);
    }
    #else
    (void)V851SendCommandAck(&v851_ota_command, "UNSUPPORTED",
                             "UNSUPPORTED_CMD", "T5L OTA is disabled",
                             NULL, 0U, NULL, 0U);
    v851_ota_command_active = 0U;
    #endif
}

static uint8_t V851OtaCommandValid(const uint8_t *_data, uint16_t len)
{
    const char *value;
    json_size_t value_len;
    uint16_t i;
    uint32_t number;
    JSONTypes_t value_type;

    if((JSONSearchToNumber(_data, len,
                           "data.params.firmware_id",
                           sizeof("data.params.firmware_id") - 1U,
                           &number) != JSONSuccess) ||
       (number == 0UL) ||
       (JSONSearchToNumber(_data, len,
                           "data.params.size_bytes",
                           sizeof("data.params.size_bytes") - 1U,
                           &number) != JSONSuccess) ||
       (number == 0UL))
    {
        return 0U;
    }

    if((JSON_SearchConst((const char *)_data, len,
                         "data.params.firmware_version",
                         sizeof("data.params.firmware_version") - 1U,
                         &value, &value_len,
                         &value_type) != JSONSuccess) ||
       (value_type != JSONString) || (value_len == 0U) ||
       (value_len > V851_SHORT_TEXT_MAX) ||
       (JSON_SearchConst((const char *)_data, len,
                         "data.params.firmware_url",
                         sizeof("data.params.firmware_url") - 1U,
                         &value, &value_len,
                         &value_type) != JSONSuccess) ||
       (value_type != JSONString) || (value_len == 0U))
    {
        return 0U;
    }

    if((JSON_SearchConst((const char *)_data, len,
                         "data.params.sha256",
                         sizeof("data.params.sha256") - 1U,
                         &value, &value_len,
                         &value_type) != JSONSuccess) ||
       (value_type != JSONString) || (value_len != 64U))
    {
        return 0U;
    }
    for(i = 0U; i < value_len; i++)
    {
        if(!(((value[i] >= '0') && (value[i] <= '9')) ||
             ((value[i] >= 'a') && (value[i] <= 'f')) ||
             ((value[i] >= 'A') && (value[i] <= 'F'))))
        {
            return 0U;
        }
    }
    return 1U;
}

static void V851HandleServerCommand(const uint8_t *_data, uint16_t len)
{
    V851CommandContext context;
    V851ControlCommand command;
    V851ControlResult result;
    V851ControlHandlerContext handler_context;
    V851DedupRecord *record;
    uint32_t now;

    if(V851LoadCommandContext(_data, len, &context, &command) == 0U)
    {
        return;
    }

    record = V851FindDedup(context.command_id);
    if(record != NULL)
    {
        (void)V851SendCommandAck(&record->context,
                                 V851StatusText(record->status),
                                 (record->error_code[0] != '\0') ? record->error_code : NULL,
                                 NULL, NULL, 0U, NULL, 0U);
        return;
    }

    result.status = V851_CONTROL_STATUS_UNSUPPORTED;
    result.error_code = "UNSUPPORTED_CMD";
    result.error_message = "control handler is not registered";
    result.applied_json = NULL;
    result.applied_len = 0U;

    now = V851ProtocolGetTimestamp();
    if(command.type == V851_CONTROL_OTA)
    {
        if((command.expire_at != 0UL) && (now != 0UL) &&
           (now > command.expire_at))
        {
            (void)V851SendCommandAck(&context, "FAILED",
                                     "INVALID_PARAMS", "command expired",
                                     NULL, 0U, NULL, 0U);
        }else if(V851OtaCommandValid(_data, len) == 0U)
        {
            (void)V851SendCommandAck(&context, "FAILED",
                                     "INVALID_PARAMS", "invalid OTA parameters",
                                     NULL, 0U, NULL, 0U);
        }else
        {
            (void)V851ControlInfoUpdate(&command);
            V851HandleOtaCommand(&context);
        }
        return;
    }

    if((command.expire_at != 0UL) && (now != 0UL) && (now > command.expire_at))
    {
        result.status = V851_CONTROL_STATUS_FAILED;
        result.error_code = "INVALID_PARAMS";
        result.error_message = "command expired";
    }else if(command.type != V851_CONTROL_INVALID)
    {
        (void)V851ControlInfoUpdate(&command);
        if(v851_handlers[command.type] != NULL)
        {
            result.status = V851_CONTROL_STATUS_FAILED;
            result.error_code = NULL;
            result.error_message = NULL;
            result.applied_json = NULL;
            result.applied_len = 0U;
            handler_context.command = &command;
            handler_context.result = &result;
            if(v851_handlers[command.type] == V851ControlInfoDgusHandler)
            {
                /*
                 * Keil C51无法从函数指针调用推导完整overlay调用树。
                 * 对DGUS处理器保留显式调用，避免write_dgus_vp的局部区
                 * 与本函数命令上下文重叠。
                 */
                V851ControlInfoDgusHandler(&handler_context);
            }else
            {
                v851_handlers[command.type](&handler_context);
            }
        }
    }

    if(result.status == V851_CONTROL_STATUS_SUCCESS)
    {
        result.error_code = NULL;
        result.error_message = NULL;
    }else if(result.error_code == NULL)
    {
        if(result.status == V851_CONTROL_STATUS_BUSY)
        {
            result.error_code = "DEVICE_BUSY";
        }else if(result.status == V851_CONTROL_STATUS_UNSUPPORTED)
        {
            result.error_code = "UNSUPPORTED_CMD";
        }else
        {
            result.error_code = "HARDWARE_FAULT";
        }
    }

    if((result.status == V851_CONTROL_STATUS_SUCCESS) &&
       (command.type == V851_CONTROL_DEVICE_PASSWORD))
    {
        result.applied_json = NULL;
        result.applied_len = 0U;
    }else if((result.status == V851_CONTROL_STATUS_SUCCESS) &&
             (result.applied_json == NULL))
    {
        result.applied_json = command.params_json;
        result.applied_len = command.params_len;
    }

    (void)V851SendCommandAck(&context,
                             V851StatusText(result.status),
                             result.error_code,
                             result.error_message,
                             NULL, 0U,
                             result.applied_json,
                             result.applied_len);
    V851SaveDedup(&context, &result);
}

static uint8_t V851BleReplyAllowed(const uint8_t *_data, uint16_t len)
{
    char cmd[V851_COMMAND_NAME_MAX + 1U];

    if(JSONSearchToArray(_data, len,
                         "cmd", sizeof("cmd") - 1U,
                         cmd, sizeof(cmd)) != JSONSuccess)
    {
        return 0U;
    }

    return ((strcmp(cmd, "configure_network_ack") == 0) ||
            (strcmp(cmd, "get_device_info_ack") == 0) ||
            (strcmp(cmd, "network_status") == 0)) ? 1U : 0U;
}

void V851ProtocolReceiveJson(const uint8_t *_data, uint16_t len)
{
    char msg_type[V851_COMMAND_NAME_MAX + 1U];

    if((_data == NULL) || (len == 0U) || (len > BRIDGE_JSON_MAX) ||
       (JSON_Validate((const char *)_data, len) != JSONSuccess))
    {
        return;
    }

    if(JSONSearchToArray(_data, len,
                         "msg_type", sizeof("msg_type") - 1U,
                         msg_type, sizeof(msg_type)) == JSONSuccess)
    {
        if(strcmp(msg_type, "device.hello") == 0)
        {
            V851CacheDeviceHello(_data, len);
        }else if(strcmp(msg_type, "server.hello_ack") == 0)
        {
            V851CacheHelloAck(_data, len);
        }else if(strcmp(msg_type, "server.command") == 0)
        {
            V851HandleServerCommand(_data, len);
        }else if(strcmp(msg_type, "device.heartbeat") == 0)
        {
            /* Cloud heartbeat belongs to V851 and is deliberately ignored by T5L. */
        }
        return;
    }

    if(V851BleReplyAllowed(_data, len) != 0U)
    {
        (void)Pb03fBleSendJson(_data, len);
    }
}

uint8_t V851ProtocolSendJson(const uint8_t *_data, uint16_t len)
{
    uint8_t slot;
    uint16_t body_len;
    uint16_t frame_len;
    uint16_t crc_value;

    if((_data == NULL) || (len == 0U) || (len > BRIDGE_JSON_MAX) ||
       (v851_tx_count >= V851_JSON_TX_DEPTH))
    {
        return 0U;
    }

    slot = v851_tx_tail;
    memcpy(&v851_tx_buffer[slot][5], _data, len);
    if((v851_tx_count == 0U) && (v851_ota_tx_pending == 0U) &&
       (Uart4.TxBusy == 0U) && (Uart4.TxHead == Uart4.TxTail))
    {
        body_len = len + V851_JSON_BODY_OVERHEAD;
        frame_len = body_len + 4U;
        v851_tx_buffer[slot][0] = V851_FRAME_MAGIC_HIGH;
        v851_tx_buffer[slot][1] = V851_FRAME_MAGIC_LOW;
        V851WriteBe16(&v851_tx_buffer[slot][2], body_len);
        v851_tx_buffer[slot][4] = V851_JSON_COMMAND;
        crc_value = crc_16(&v851_tx_buffer[slot][4], len + 1U);
        V851WriteBe16(&v851_tx_buffer[slot][frame_len - 2U], crc_value);
        UartSendData(&Uart4, v851_tx_buffer[slot], frame_len);
        memset(v851_tx_buffer[slot], 0, frame_len);
        return 1U;
    }

    v851_tx_len[slot] = len;
    v851_tx_tail++;
    if(v851_tx_tail >= V851_JSON_TX_DEPTH)
    {
        v851_tx_tail = 0U;
    }
    v851_tx_count++;
    return 1U;
}

uint8_t V851ProtocolSendOtaFrame(const uint8_t *_data, uint16_t len)
{
    if((_data == NULL) || (len == 0U) || (len > V851_OTA_TX_MAX) ||
       (v851_ota_tx_pending != 0U))
    {
        return 0U;
    }

    if((Uart4.TxBusy == 0U) && (Uart4.TxHead == Uart4.TxTail) &&
       (v851_tx_count == 0U))
    {
        UartSendData(&Uart4, (uint8_t *)_data, len);
    }else
    {
        memcpy(v851_ota_tx_frame, _data, len);
        v851_ota_tx_len = len;
        v851_ota_tx_pending = 1U;
    }
    return 1U;
}

void V851ProtocolNotifyOtaState(V851OtaStage stage,
                                uint8_t progress,
                                const char *error_code)
{
    if(v851_ota_command_active == 0U)
    {
        return;
    }
    if(progress > 100U)
    {
        progress = 100U;
    }

    v851_ota_terminal_pending = 1U;
    v851_ota_terminal_stage = stage;
    v851_ota_terminal_progress = progress;
    if(error_code != NULL)
    {
        V851CopyText(v851_ota_terminal_error,
                     sizeof(v851_ota_terminal_error),
                     error_code, (uint16_t)strlen(error_code));
    }else
    {
        v851_ota_terminal_error[0] = '\0';
    }
}

uint8_t V851ProtocolRegisterControlHandler(V851ControlType type,
                                           V851ControlHandler handler)
{
    if((type >= V851_CONTROL_COUNT) || (type == V851_CONTROL_OTA))
    {
        return 0U;
    }
    v851_handlers[type] = handler;
    return 1U;
}

void V851ProtocolTask(void)
{
    uint8_t slot;
    uint8_t sent;
    uint16_t json_len;
    uint16_t body_len;
    uint16_t frame_len;
    uint16_t crc_value;
    V851OtaStage stage;
    uint8_t progress;
    const char *error_code;
    const char *stage_text;
    const char *status;

    if((v851_ota_terminal_pending != 0U) &&
       (v851_tx_count < V851_JSON_TX_DEPTH))
    {
        stage = v851_ota_terminal_stage;
        progress = v851_ota_terminal_progress;
        error_code = (v851_ota_terminal_error[0] != '\0') ?
                     v851_ota_terminal_error : NULL;
        status = "IN_PROGRESS";
        switch(stage)
        {
            case V851_OTA_STAGE_VERIFYING:
                stage_text = "VERIFYING";
                break;
            case V851_OTA_STAGE_REBOOTING:
                stage_text = "REBOOTING";
                break;
            case V851_OTA_STAGE_SUCCESS:
                stage_text = "SUCCESS";
                status = "SUCCESS";
                break;
            case V851_OTA_STAGE_FAILED:
                stage_text = "FAILED";
                status = "FAILED";
                break;
            default:
                stage_text = "INSTALLING";
                break;
        }

        sent = V851SendCommandAck(
            &v851_ota_command, status, error_code,
            (error_code != NULL) ? "T5L OTA processing failed" : NULL,
            stage_text, progress, NULL, 0U);
        if(sent != 0U)
        {
            v851_ota_terminal_pending = 0U;
            if((stage == V851_OTA_STAGE_SUCCESS) ||
               (stage == V851_OTA_STAGE_FAILED))
            {
                v851_ota_command_active = 0U;
            }
        }
    }

    V851StateServiceReports();

    if((Uart4.TxBusy != 0U) || (Uart4.TxHead != Uart4.TxTail))
    {
        return;
    }

    if(v851_ota_tx_pending != 0U)
    {
        UartSendData(&Uart4, v851_ota_tx_frame, v851_ota_tx_len);
        v851_ota_tx_pending = 0U;
        v851_ota_tx_len = 0U;
    }else if(v851_tx_count != 0U)
    {
        slot = v851_tx_head;
        json_len = v851_tx_len[slot];
        body_len = json_len + V851_JSON_BODY_OVERHEAD;
        frame_len = body_len + 4U;
        v851_tx_buffer[slot][0] = V851_FRAME_MAGIC_HIGH;
        v851_tx_buffer[slot][1] = V851_FRAME_MAGIC_LOW;
        V851WriteBe16(&v851_tx_buffer[slot][2], body_len);
        v851_tx_buffer[slot][4] = V851_JSON_COMMAND;
        crc_value = crc_16(&v851_tx_buffer[slot][4], json_len + 1U);
        V851WriteBe16(&v851_tx_buffer[slot][frame_len - 2U], crc_value);
        UartSendData(&Uart4, v851_tx_buffer[slot], frame_len);
        memset(v851_tx_buffer[slot], 0, frame_len);
        v851_tx_len[slot] = 0U;
        v851_tx_head++;
        if(v851_tx_head >= V851_JSON_TX_DEPTH)
        {
            v851_tx_head = 0U;
        }
        v851_tx_count--;
    }
}

void V851ProtocolInit(void)
{
    memset(v851_tx_buffer, 0, sizeof(v851_tx_buffer));
    memset(v851_ack_json, 0, sizeof(v851_ack_json));
    memset(&v851_device_info, 0, sizeof(v851_device_info));
    memset(v851_handlers, 0, sizeof(v851_handlers));
    V851ControlInfoInit();
    memset(v851_dedup, 0, sizeof(v851_dedup));
    memset(&v851_ota_command, 0, sizeof(v851_ota_command));
    memset(v851_state_report_json, 0, sizeof(v851_state_report_json));
    V851CopyText(v851_device_info.bind_status,
                 sizeof(v851_device_info.bind_status),
                 "UNBOUND", sizeof("UNBOUND") - 1U);

    memset(v851_tx_len, 0, sizeof(v851_tx_len));
    v851_tx_head = 0U;
    v851_tx_tail = 0U;
    v851_tx_count = 0U;
    v851_ota_tx_len = 0U;
    v851_ota_tx_pending = 0U;
    v851_dedup_next = 0U;
    v851_server_time = 0UL;
    v851_server_tick = 0UL;
    v851_time_valid = 0U;
    v851_ack_seq = 0UL;
    v851_ota_command_active = 0U;
    v851_ota_terminal_pending = 0U;
    v851_ota_terminal_stage = V851_OTA_STAGE_FAILED;
    v851_ota_terminal_progress = 0U;
    memset(v851_ota_terminal_error, 0, sizeof(v851_ota_terminal_error));
    v851_state_report_seq = 0UL;
    v851_state_last_report_tick = 0UL;
    v851_state_pending_mask = 0U;
    v851_state_snapshot_pending = 0U;
    v851_state_snapshot_sent = 0U;
    v851_state_report_started = 0U;
    memset(v851_state_control_shadow, 0,
           sizeof(v851_state_control_shadow));
}

#endif /* bleV851_BRIDGE_ENABLED */

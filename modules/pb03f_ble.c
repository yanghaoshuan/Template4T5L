#include "pb03f_ble.h"

#if bleV851_BRIDGE_ENABLED

#include "bridge_json.h"
#include "core_json.h"
#include "timer.h"
#include "v851_protocol.h"

#include <string.h>

#define PB_TX_QUEUE_DEPTH                     2U
#define PB_AT_STEP_COUNT                      12U
#define PB_AT_MAX_ATTEMPTS                    3U
#define PB_POWER_DELAY_MS                     500UL
#define PB_AT_TIMEOUT_MS                      1000UL
#define PB_RESTART_DELAY_MS                   5000UL
#define PB_ESCAPE_RETRY_DELAY_MS              5000UL
#define PB_QR_VP_ADDR                         0x05ADU
#define PB_QR_BUFFER_BYTES                    40U
#define PB_AT_COMMAND_MAX                     96U
#define PB_AT_LINE_MAX                        80U

typedef enum
{
    PB_STATE_WAIT_DELAY = 0,
    PB_STATE_SEND_COMMAND,
    PB_STATE_WAIT_RESPONSE,
    PB_STATE_ESCAPE_SEND,
    PB_STATE_ESCAPE_WAIT,
    PB_STATE_ESCAPE_DELAY,
    PB_STATE_TRANSPARENT
} PbState;

typedef enum
{
    PB_AT_RESULT_NONE = 0,
    PB_AT_RESULT_OK,
    PB_AT_RESULT_ERROR
} PbAtResult;

static PbState pb_state;
static PbAtResult pb_at_result;
static uint8_t pb_at_step;
static uint8_t pb_at_attempts;
static uint8_t pb_escape_attempts;
static uint8_t pb_at_ok_match;
static uint8_t pb_at_error_match;
static uint32_t pb_state_tick;
static uint32_t pb_wait_delay;
static uint8_t pb_reconfigure_requested;

static char pb_mac[13];
static uint8_t pb_mac_valid;
static char pb_ble_id[7];
static char pb_ble_name[16];
static char pb_at_line[PB_AT_LINE_MAX];
static uint8_t pb_at_line_len;
static uint8_t pb_at_command[PB_AT_COMMAND_MAX];

static uint8_t pb_boot_match;

static uint8_t xdata pb_rx_json[BRIDGE_JSON_MAX + 1U];
static uint16_t pb_rx_json_len;
static uint16_t pb_rx_msg_id;
static uint16_t pb_rx_chunk_total;
static uint16_t pb_rx_next_chunk;
static uint32_t pb_rx_tick;
static uint8_t pb_rx_active;
static uint8_t pb_rx_locked;
static uint8_t pb_forward_pending;

static uint8_t xdata pb_tx_json[PB_TX_QUEUE_DEPTH][BRIDGE_JSON_MAX];
static uint16_t pb_tx_json_len[PB_TX_QUEUE_DEPTH];
static uint8_t pb_tx_head;
static uint8_t pb_tx_tail;
static uint8_t pb_tx_count;
static uint8_t pb_tx_active;
static uint16_t pb_tx_msg_id;
static uint16_t pb_tx_next_msg_id;
static uint16_t pb_tx_chunk_index;
static uint16_t pb_tx_chunk_total;
static uint8_t pb_tx_frame[PB03F_BLE_ATT_PAYLOAD_MAX];

static const uint8_t pb_ok_token[] = "OK\r\n";
static const uint8_t pb_error_token[] = "ERROR";
static const uint8_t pb_boot_token[] = "company:Ai-Thinker";

static uint16_t PbReadBe16(const uint8_t *_data)
{
    return ((uint16_t)_data[0] << 8) | _data[1];
}

static void PbWriteBe16(uint8_t *_data, uint16_t value)
{
    _data[0] = (uint8_t)(value >> 8);
    _data[1] = (uint8_t)value;
}

static uint8_t PbHexValue(char value)
{
    if((value >= '0') && (value <= '9')) return (uint8_t)(value - '0');
    if((value >= 'A') && (value <= 'F')) return (uint8_t)(value - 'A' + 10);
    if((value >= 'a') && (value <= 'f')) return (uint8_t)(value - 'a' + 10);
    return 0xFFU;
}

static char PbUpperHex(char value)
{
    if((value >= 'a') && (value <= 'f'))
    {
        return (char)(value - 'a' + 'A');
    }
    return value;
}

static uint8_t PbBleIdValid(const uint8_t *ble_id, uint16_t len)
{
    uint16_t i;
    uint8_t value;

    if((ble_id == NULL) || (len != 6U))
    {
        return 0U;
    }

    for(i = 0U; i < len; i++)
    {
        value = ble_id[i];
        if(!(((value >= 'A') && (value <= 'H')) ||
             ((value >= 'J') && (value <= 'N')) ||
             ((value >= 'P') && (value <= 'Z')) ||
             ((value >= '2') && (value <= '9'))))
        {
            return 0U;
        }
    }
    return 1U;
}

static uint8_t PbMatchToken(uint8_t value,
                            const uint8_t *token,
                            uint8_t token_len,
                            uint8_t *match_len)
{
    if(value == token[*match_len])
    {
        (*match_len)++;
        if(*match_len >= token_len)
        {
            *match_len = 0U;
            return 1U;
        }
    }else
    {
        *match_len = (value == token[0]) ? 1U : 0U;
    }
    return 0U;
}

static void PbResetRxAssembly(void)
{
    if(pb_rx_json_len != 0U)
    {
        memset(pb_rx_json, 0, pb_rx_json_len);
    }
    pb_rx_json_len = 0U;
    pb_rx_msg_id = 0U;
    pb_rx_chunk_total = 0U;
    pb_rx_next_chunk = 0U;
    pb_rx_tick = 0UL;
    pb_rx_active = 0U;
}

static void PbKeepCompletedAssembly(void)
{
    pb_rx_msg_id = 0U;
    pb_rx_chunk_total = 0U;
    pb_rx_next_chunk = 0U;
    pb_rx_tick = 0UL;
    pb_rx_active = 0U;
}

static void PbStartConfiguration(uint32_t delay)
{
    pb_state = PB_STATE_WAIT_DELAY;
    pb_state_tick = GetSysTick();
    pb_wait_delay = delay;
    pb_at_step = 0U;
    pb_at_attempts = 0U;
    pb_escape_attempts = 0U;
    pb_at_result = PB_AT_RESULT_NONE;
    pb_at_ok_match = 0U;
    pb_at_error_match = 0U;
    pb_at_line_len = 0U;
    pb_boot_match = 0U;
    pb_tx_active = 0U;
    pb_reconfigure_requested = 0U;
    PbResetRxAssembly();
    pb_rx_locked = 0U;
    pb_forward_pending = 0U;
}

static void PbWriteQr(void)
{
    uint8_t qr_buffer[PB_QR_BUFFER_BYTES];
    const char *identity;
    uint16_t prefix_len;
    uint16_t identity_len;

    if((pb_ble_id[0] == '\0') && (pb_mac_valid == 0U))
    {
        return;
    }

    memset(qr_buffer, 0, sizeof(qr_buffer));
    memcpy(qr_buffer, "https://b.mcqx.pet?b=", sizeof("https://b.mcqx.pet?b=") - 1U);
    prefix_len = sizeof("https://b.mcqx.pet?b=") - 1U;
    identity = (pb_ble_id[0] != '\0') ? pb_ble_id : pb_mac;
    identity_len = (uint16_t)strlen(identity);
    if((prefix_len + identity_len) > sizeof(qr_buffer))
    {
        return;
    }
    memcpy(&qr_buffer[prefix_len], identity, identity_len);
    write_dgus_vp(PB_QR_VP_ADDR, qr_buffer, sizeof(qr_buffer) / 2U);
}

static void PbBuildName(void)
{
    const char *identity;

    identity = (pb_ble_id[0] != '\0') ? pb_ble_id : pb_mac;
    memcpy(pb_ble_name, "MQXQ-", 5U);
    if(identity[0] != '\0')
    {
        memcpy(&pb_ble_name[5], identity, 4U);
        pb_ble_name[9] = '\0';
    }else
    {
        memcpy(&pb_ble_name[5], "0000", 4U);
        pb_ble_name[9] = '\0';
    }
}

static uint32_t PbDerivePin(void)
{
    uint8_t i;
    uint8_t nibble;
    uint32_t pin = 0UL;

    for(i = 0U; i < 12U; i++)
    {
        nibble = PbHexValue(pb_mac[i]);
        if(nibble == 0xFFU)
        {
            return 0UL;
        }
        pin = ((pin * 16UL) + nibble) % 1000000UL;
    }
    return pin;
}

static uint16_t PbCommandText(uint16_t pos, const char *text)
{
    while((*text != '\0') && (pos < PB_AT_COMMAND_MAX))
    {
        pb_at_command[pos++] = (uint8_t)*text++;
    }
    return pos;
}

static uint16_t PbCommandHexByte(uint16_t pos, uint8_t value)
{
    static const char hex_digits[] = "0123456789ABCDEF";

    if((pos + 2U) <= PB_AT_COMMAND_MAX)
    {
        pb_at_command[pos++] = (uint8_t)hex_digits[value >> 4];
        pb_at_command[pos++] = (uint8_t)hex_digits[value & 0x0FU];
    }
    return pos;
}

static uint16_t PbCommandHexText(uint16_t pos, const char *text, uint16_t len)
{
    uint16_t i;

    for(i = 0U; (i < len) && ((pos + 2U) <= PB_AT_COMMAND_MAX); i++)
    {
        pos = PbCommandHexByte(pos, (uint8_t)text[i]);
    }
    return pos;
}

static uint16_t PbBuildAuthCommand(void)
{
    uint32_t pin;
    uint32_t divisor;
    uint16_t pos;
    uint8_t i;

    pin = PbDerivePin();
    pos = PbCommandText(0U, "AT+BLEAUTH=");
    divisor = 100000UL;
    for(i = 0U; i < 6U; i++)
    {
        if(pos < PB_AT_COMMAND_MAX)
        {
            pb_at_command[pos++] = (uint8_t)('0' + ((pin / divisor) % 10UL));
        }
        divisor /= 10UL;
    }
    pos = PbCommandText(pos, "\r\n");
    return pos;
}

static uint16_t PbBuildNameCommand(void)
{
    uint16_t pos;

    PbBuildName();
    pos = PbCommandText(0U, "AT+BLENAME=");
    pos = PbCommandText(pos, pb_ble_name);
    pos = PbCommandText(pos, "\r\n");
    return pos;
}

static uint16_t PbBuildAdvCommand(void)
{
    const char *identity;
    uint16_t identity_len;
    uint16_t name_len;
    uint16_t pos;

    PbBuildName();
    identity = (pb_ble_id[0] != '\0') ? pb_ble_id : pb_mac;
    identity_len = (uint16_t)strlen(identity);
    name_len = (uint16_t)strlen(pb_ble_name);

    pos = PbCommandText(0U, "AT+BLEADVDATA=");
    pos = PbCommandHexByte(pos, 0x02U);
    pos = PbCommandHexByte(pos, 0x01U);
    pos = PbCommandHexByte(pos, 0x06U);
    pos = PbCommandHexByte(pos, (uint8_t)(identity_len + 1U));
    pos = PbCommandHexByte(pos, 0xFFU);
    pos = PbCommandHexText(pos, identity, identity_len);
    pos = PbCommandHexByte(pos, (uint8_t)(name_len + 1U));
    pos = PbCommandHexByte(pos, 0x09U);
    pos = PbCommandHexText(pos, pb_ble_name, name_len);
    pos = PbCommandText(pos, "\r\n");
    return pos;
}

static void PbParseAtLine(void)
{
    uint8_t i;
    uint8_t nibble;

    if((pb_at_line_len >= 20U) &&
       (memcmp(pb_at_line, "+BLEMAC:", 8U) == 0))
    {
        for(i = 0U; i < 12U; i++)
        {
            nibble = PbHexValue(pb_at_line[8U + i]);
            if(nibble == 0xFFU)
            {
                return;
            }
            pb_mac[i] = PbUpperHex(pb_at_line[8U + i]);
        }
        pb_mac[12] = '\0';
        pb_mac_valid = 1U;
        PbWriteQr();
    }
}

static void PbHandleAtBytes(const uint8_t *_data, uint16_t len)
{
    uint16_t i;

    for(i = 0U; i < len; i++)
    {
        if(PbMatchToken(_data[i], pb_error_token,
                        (uint8_t)(sizeof(pb_error_token) - 1U),
                        &pb_at_error_match) != 0U)
        {
            pb_at_result = PB_AT_RESULT_ERROR;
        }
        if(PbMatchToken(_data[i], pb_ok_token,
                        (uint8_t)(sizeof(pb_ok_token) - 1U),
                        &pb_at_ok_match) != 0U)
        {
            if(pb_at_result == PB_AT_RESULT_NONE)
            {
                pb_at_result = PB_AT_RESULT_OK;
            }
        }

        if(_data[i] == '\n')
        {
            PbParseAtLine();
            pb_at_line_len = 0U;
        }else if((_data[i] != '\r') && (pb_at_line_len < (PB_AT_LINE_MAX - 1U)))
        {
            pb_at_line[pb_at_line_len++] = (char)_data[i];
            pb_at_line[pb_at_line_len] = '\0';
        }
    }
}

static uint8_t PbCommitJson(uint8_t slot, uint16_t len)
{
    uint16_t crc_value;

    if((slot != pb_tx_tail) || (len == 0U) || (len > BRIDGE_JSON_MAX))
    {
        return 0U;
    }

    if((len <= PB03F_BLE_CHUNK_PAYLOAD_MAX) &&
       (pb_state == PB_STATE_TRANSPARENT) &&
       (pb_tx_count == 0U) && (pb_tx_active == 0U) &&
       (PB03F_BLE_UART.TxBusy == 0U) &&
       (PB03F_BLE_UART.TxHead == PB03F_BLE_UART.TxTail))
    {
        pb_tx_frame[0] = PB03F_BLE_FRAME_MAGIC_HIGH;
        pb_tx_frame[1] = PB03F_BLE_FRAME_MAGIC_LOW;
        pb_tx_frame[2] = PB03F_BLE_FRAME_VERSION;
        pb_tx_frame[3] = PB03F_BLE_FRAME_FLAG_LAST;
        PbWriteBe16(&pb_tx_frame[4], pb_tx_next_msg_id++);
        PbWriteBe16(&pb_tx_frame[6], 0U);
        PbWriteBe16(&pb_tx_frame[8], 1U);
        PbWriteBe16(&pb_tx_frame[10], len);
        memcpy(&pb_tx_frame[PB03F_BLE_FRAME_HEADER_SIZE],
               pb_tx_json[slot], len);
        crc_value = crc_16(pb_tx_frame,
                           PB03F_BLE_FRAME_HEADER_SIZE + len);
        PbWriteBe16(&pb_tx_frame[PB03F_BLE_FRAME_HEADER_SIZE + len],
                    crc_value);
        UartSendData(&PB03F_BLE_UART, pb_tx_frame,
                     PB03F_BLE_FRAME_HEADER_SIZE + len +
                     PB03F_BLE_FRAME_CRC_SIZE);
        memset(pb_tx_json[slot], 0, len);
        return 1U;
    }

    pb_tx_json_len[slot] = len;
    pb_tx_tail++;
    if(pb_tx_tail >= PB_TX_QUEUE_DEPTH)
    {
        pb_tx_tail = 0U;
    }
    pb_tx_count++;
    return 1U;
}

static uint8_t PbQueueJson(const uint8_t *_data, uint16_t len)
{
    uint8_t slot;

    if((_data == NULL) || (len == 0U) || (len > BRIDGE_JSON_MAX) ||
       (pb_tx_count >= PB_TX_QUEUE_DEPTH))
    {
        return 0U;
    }

    slot = pb_tx_tail;
    memcpy(pb_tx_json[slot], _data, len);
    return PbCommitJson(slot, len);
}

static void PbQueuePacketError(uint16_t msg_id, const char *message)
{
    BridgeJsonWriter writer;
    uint8_t slot;
    uint16_t len;

    if(pb_tx_count >= PB_TX_QUEUE_DEPTH)
    {
        return;
    }

    slot = pb_tx_tail;
    BridgeJsonWriterInit(&writer, pb_tx_json[slot], BRIDGE_JSON_MAX);
    BridgeJsonWriterText(&writer,
        "{\"cmd\":\"packet_ack\",\"request_id\":null,\"result\":\"FAIL\","
        "\"error_code\":\"BLE_PACKET_INVALID\",\"message\":");
    BridgeJsonWriterQuoted(&writer, message, (uint16_t)strlen(message));
    BridgeJsonWriterText(&writer, ",\"data\":{\"msg_id\":");
    BridgeJsonWriterUint32(&writer, msg_id);
    BridgeJsonWriterText(&writer, "}}");
    len = BridgeJsonWriterFinish(&writer);
    if(len == 0U)
    {
        return;
    }

    (void)PbCommitJson(slot, len);
}

static void PbQueueBusinessError(const char *cmd,
                                 const char *request_id,
                                 const char *error_code,
                                 const char *message)
{
    BridgeJsonWriter writer;
    uint8_t slot;
    uint16_t len;
    char ack_cmd[40];
    uint16_t cmd_len;

    if(pb_tx_count >= PB_TX_QUEUE_DEPTH)
    {
        return;
    }

    slot = pb_tx_tail;
    cmd_len = (uint16_t)strlen(cmd);
    if((cmd_len + 4U) >= sizeof(ack_cmd))
    {
        return;
    }
    memcpy(ack_cmd, cmd, cmd_len);
    memcpy(&ack_cmd[cmd_len], "_ack", 5U);
    BridgeJsonWriterInit(&writer, pb_tx_json[slot], BRIDGE_JSON_MAX);
    BridgeJsonWriterText(&writer, "{\"cmd\":");
    BridgeJsonWriterQuoted(&writer, ack_cmd, (uint16_t)strlen(ack_cmd));
    BridgeJsonWriterText(&writer, ",\"request_id\":");
    if(request_id != NULL)
    {
        BridgeJsonWriterQuoted(&writer, request_id, (uint16_t)strlen(request_id));
    }else
    {
        BridgeJsonWriterText(&writer, "null");
    }
    BridgeJsonWriterText(&writer, ",\"result\":\"FAIL\",\"error_code\":");
    BridgeJsonWriterQuoted(&writer, error_code, (uint16_t)strlen(error_code));
    BridgeJsonWriterText(&writer, ",\"message\":");
    BridgeJsonWriterQuoted(&writer, message, (uint16_t)strlen(message));
    BridgeJsonWriterText(&writer, ",\"data\":{}}");
    len = BridgeJsonWriterFinish(&writer);
    if(len == 0U)
    {
        return;
    }

    (void)PbCommitJson(slot, len);
}

static void PbQueueDeviceInfoAck(const char *request_id)
{
    const V851DeviceInfo *info;
    BridgeJsonWriter writer;
    uint8_t slot;
    uint16_t len;

    if(pb_tx_count >= PB_TX_QUEUE_DEPTH)
    {
        return;
    }

    info = V851ProtocolGetDeviceInfo();
    PbBuildName();
    slot = pb_tx_tail;
    BridgeJsonWriterInit(&writer, pb_tx_json[slot], BRIDGE_JSON_MAX);
    BridgeJsonWriterText(&writer,
        "{\"cmd\":\"get_device_info_ack\",\"request_id\":");
    BridgeJsonWriterQuoted(&writer, request_id, (uint16_t)strlen(request_id));
    BridgeJsonWriterText(&writer,
        ",\"result\":\"OK\",\"error_code\":null,\"message\":null,\"data\":{"
        "\"mac\":");
    BridgeJsonWriterQuoted(&writer, pb_mac, (uint16_t)strlen(pb_mac));
    BridgeJsonWriterText(&writer, ",\"device_sn\":");
    BridgeJsonWriterQuoted(&writer, info->device_sn,
                           (uint16_t)strlen(info->device_sn));
    BridgeJsonWriterText(&writer, ",\"ble_id\":");
    BridgeJsonWriterQuoted(&writer, info->ble_id,
                           (uint16_t)strlen(info->ble_id));
    BridgeJsonWriterText(&writer, ",\"ble_name\":");
    BridgeJsonWriterQuoted(&writer, pb_ble_name,
                           (uint16_t)strlen(pb_ble_name));
    BridgeJsonWriterText(&writer, ",\"product_key\":");
    BridgeJsonWriterQuoted(&writer, info->product_key,
                           (uint16_t)strlen(info->product_key));
    BridgeJsonWriterText(&writer, ",\"model\":");
    BridgeJsonWriterQuoted(&writer, info->model,
                           (uint16_t)strlen(info->model));
    BridgeJsonWriterText(&writer, ",\"hardware_version\":");
    BridgeJsonWriterQuoted(&writer, info->hardware_version,
                           (uint16_t)strlen(info->hardware_version));
    BridgeJsonWriterText(&writer, ",\"firmware_version\":");
    BridgeJsonWriterQuoted(&writer, info->firmware_version,
                           (uint16_t)strlen(info->firmware_version));
    BridgeJsonWriterText(&writer, ",\"sales_country_code\":");
    BridgeJsonWriterQuoted(&writer, info->sales_country_code,
                           (uint16_t)strlen(info->sales_country_code));
    BridgeJsonWriterText(&writer, ",\"bind_status\":");
    BridgeJsonWriterQuoted(&writer, info->bind_status,
                           (uint16_t)strlen(info->bind_status));
    BridgeJsonWriterText(&writer, "}}");
    len = BridgeJsonWriterFinish(&writer);
    if(len == 0U)
    {
        return;
    }

    (void)PbCommitJson(slot, len);
}

static void PbFailAssembly(uint16_t msg_id, const char *message)
{
    PbQueuePacketError(msg_id, message);
    PbResetRxAssembly();
}

static void PbHandleCompletedJson(void)
{
    char cmd[33];
    char request_id[65];
    const char *object;
    json_size_t object_len;
    JSONTypes_t object_type;

    if((JSONSearchToArray(pb_rx_json, pb_rx_json_len,
                          "cmd", sizeof("cmd") - 1U,
                          cmd, sizeof(cmd)) != JSONSuccess) ||
       (JSONSearchToArray(pb_rx_json, pb_rx_json_len,
                          "request_id", sizeof("request_id") - 1U,
                          request_id, sizeof(request_id)) != JSONSuccess))
    {
        PbQueueBusinessError("unknown", NULL, "INVALID_PAYLOAD",
                             "cmd or request_id is missing");
        PbResetRxAssembly();
        return;
    }

    if(strcmp(cmd, "get_device_info") == 0)
    {
        PbQueueDeviceInfoAck(request_id);
        PbResetRxAssembly();
    }else if(strcmp(cmd, "configure_network") == 0)
    {
        if((JSON_SearchConst((const char *)pb_rx_json, pb_rx_json_len,
                             "data", sizeof("data") - 1U,
                             &object, &object_len,
                             &object_type) != JSONSuccess) ||
           (object_type != JSONObject))
        {
            PbQueueBusinessError(cmd, request_id, "INVALID_PAYLOAD",
                                 "configure_network data is invalid");
            PbResetRxAssembly();
            return;
        }
        if(V851ProtocolSendJson(pb_rx_json, pb_rx_json_len) != 0U)
        {
            PbResetRxAssembly();
        }else
        {
            pb_forward_pending = 1U;
            pb_rx_locked = 1U;
            PbKeepCompletedAssembly();
        }
    }else
    {
        PbQueueBusinessError(cmd, request_id, "UNSUPPORTED_CMD",
                             "command is not supported by T5L");
        PbResetRxAssembly();
    }
}

static void PbProcessFrame(const uint8_t *_data, uint16_t len)
{
    uint8_t flags;
    uint16_t msg_id;
    uint16_t chunk_index;
    uint16_t chunk_total;
    uint16_t payload_len;

    if((_data == NULL) || (len < (PB03F_BLE_FRAME_HEADER_SIZE +
                                  PB03F_BLE_FRAME_CRC_SIZE)))
    {
        return;
    }

    msg_id = PbReadBe16(&_data[4]);
    payload_len = PbReadBe16(&_data[10]);
    flags = _data[3];
    chunk_index = PbReadBe16(&_data[6]);
    chunk_total = PbReadBe16(&_data[8]);
    if((chunk_total == 0U) || (chunk_total > PB03F_BLE_MAX_CHUNKS) ||
       (chunk_index >= chunk_total))
    {
        PbFailAssembly(msg_id, "invalid chunk index");
        return;
    }
    if((((flags & PB03F_BLE_FRAME_FLAG_LAST) != 0U) &&
        (chunk_index != (chunk_total - 1U))) ||
       (((flags & PB03F_BLE_FRAME_FLAG_LAST) == 0U) &&
        (chunk_index == (chunk_total - 1U))))
    {
        PbFailAssembly(msg_id, "invalid last flag");
        return;
    }

    if(pb_rx_active == 0U)
    {
        if(pb_rx_locked != 0U)
        {
            PbQueuePacketError(msg_id, "previous request is pending");
            return;
        }
        if(chunk_index != 0U)
        {
            PbFailAssembly(msg_id, "missing first chunk");
            return;
        }
        pb_rx_active = 1U;
        pb_rx_msg_id = msg_id;
        pb_rx_chunk_total = chunk_total;
        pb_rx_next_chunk = 0U;
        pb_rx_json_len = 0U;
    }

    if((msg_id != pb_rx_msg_id) ||
       (chunk_total != pb_rx_chunk_total) ||
       (chunk_index != pb_rx_next_chunk))
    {
        PbFailAssembly(msg_id, "missing or duplicate chunk");
        return;
    }
    if((uint16_t)(pb_rx_json_len + payload_len) > BRIDGE_JSON_MAX)
    {
        PbFailAssembly(msg_id, "payload too large");
        return;
    }

    memcpy(&pb_rx_json[pb_rx_json_len],
           &_data[PB03F_BLE_FRAME_HEADER_SIZE], payload_len);
    pb_rx_json_len += payload_len;
    pb_rx_next_chunk++;
    pb_rx_tick = GetSysTick();

    if((flags & PB03F_BLE_FRAME_FLAG_LAST) != 0U)
    {
        if((pb_rx_next_chunk != pb_rx_chunk_total) ||
           (JSON_Validate((const char *)pb_rx_json,
                          pb_rx_json_len) != JSONSuccess))
        {
            PbFailAssembly(msg_id, "invalid json");
            return;
        }
        PbHandleCompletedJson();
    }
}

static uint8_t PbObserveBootByte(uint8_t value)
{
    if(PbMatchToken(value, pb_boot_token,
                    (uint8_t)(sizeof(pb_boot_token) - 1U),
                    &pb_boot_match) != 0U)
    {
        PbStartConfiguration(PB_POWER_DELAY_MS);
        return 1U;
    }
    return 0U;
}

uint8_t Pb03fBleSendJson(const uint8_t *_data, uint16_t len)
{
    char cmd[33];

    if((_data == NULL) || (len == 0U) || (len > BRIDGE_JSON_MAX) ||
       (JSON_Validate((const char *)_data, len) != JSONSuccess) ||
       (JSONSearchToArray(_data, len,
                          "cmd", sizeof("cmd") - 1U,
                          cmd, sizeof(cmd)) != JSONSuccess))
    {
        return 0U;
    }
    if((strcmp(cmd, "configure_network_ack") != 0) &&
       (strcmp(cmd, "get_device_info_ack") != 0) &&
       (strcmp(cmd, "network_status") != 0))
    {
        return 0U;
    }
    return PbQueueJson(_data, len);
}

uint8_t Pb03fBleSetProvisionedIdentity(const uint8_t *ble_id, uint16_t len)
{
    if(PbBleIdValid(ble_id, len) == 0U)
    {
        return 0U;
    }
    if((memcmp(pb_ble_id, ble_id, len) == 0) && (pb_ble_id[len] == '\0'))
    {
        return 1U;
    }

    memcpy(pb_ble_id, ble_id, len);
    pb_ble_id[len] = '\0';
    PbWriteQr();
    if(pb_state == PB_STATE_TRANSPARENT)
    {
        pb_reconfigure_requested = 1U;
    }
    return 1U;
}

const char *Pb03fBleGetMac(void)
{
    return pb_mac;
}

void Pb03fBleReceive(const uint8_t *_data, uint16_t len)
{
    uint16_t i;

    if((_data == NULL) || (len == 0U))
    {
        return;
    }

    if(pb_state != PB_STATE_TRANSPARENT)
    {
        PbHandleAtBytes(_data, len);
        return;
    }

    if((len >= (PB03F_BLE_FRAME_HEADER_SIZE +
                PB03F_BLE_FRAME_CRC_SIZE)) &&
       (_data[0] == PB03F_BLE_FRAME_MAGIC_HIGH) &&
       (_data[1] == PB03F_BLE_FRAME_MAGIC_LOW))
    {
        PbProcessFrame(_data, len);
        return;
    }

    for(i = 0U; i < len; i++)
    {
        if(PbObserveBootByte(_data[i]) != 0U)
        {
            return;
        }
    }
}

void Pb03fBleFrameError(uint16_t msg_id, const char *message)
{
    if(message != NULL)
    {
        PbQueuePacketError(msg_id, message);
    }
}

uint8_t Pb03fBleIsTransparent(void)
{
    return (pb_state == PB_STATE_TRANSPARENT) ? 1U : 0U;
}

void Pb03fBleTask(void)
{
    uint8_t slot;
    uint16_t command_len = 0U;
    uint16_t json_len;
    uint16_t offset;
    uint16_t remaining;
    uint16_t payload_len;
    uint16_t frame_len;
    uint16_t crc_value;

    if(pb_state == PB_STATE_WAIT_DELAY)
    {
        if((uint32_t)(GetSysTick() - pb_state_tick) >= pb_wait_delay)
        {
            pb_state = PB_STATE_SEND_COMMAND;
        }
    }else if(pb_state == PB_STATE_SEND_COMMAND)
    {
        if((PB03F_BLE_UART.TxBusy == 0U) &&
           (PB03F_BLE_UART.TxHead == PB03F_BLE_UART.TxTail))
        {
            pb_at_result = PB_AT_RESULT_NONE;
            pb_at_ok_match = 0U;
            pb_at_error_match = 0U;
            pb_at_line_len = 0U;

            switch(pb_at_step)
            {
                case 0U:
                    command_len = PbCommandText(0U, "AT\r\n");
                    break;
                case 1U:
                    command_len = PbCommandText(0U, "AT+BLEMODE=9\r\n");
                    break;
                case 2U:
                    command_len = PbCommandText(0U,
                        "AT+BLESERUUID=4D515851504554008A3D7C2E9F1B6D10\r\n");
                    break;
                case 3U:
                    command_len = PbCommandText(0U,
                        "AT+BLETXUUID=4D515851504554038A3D7C2E9F1B6D10\r\n");
                    break;
                case 4U:
                    command_len = PbCommandText(0U,
                        "AT+BLERXUUID=4D515851504554028A3D7C2E9F1B6D10\r\n");
                    break;
                case 5U:
                    command_len = PbCommandText(0U, "AT+BLEMTU=240\r\n");
                    break;
                case 6U:
                    pb_mac_valid = 0U;
                    memset(pb_mac, 0, sizeof(pb_mac));
                    command_len = PbCommandText(0U, "AT+BLEMAC?\r\n");
                    break;
                case 7U:
                    command_len = PbBuildAuthCommand();
                    break;
                case 8U:
                    command_len = PbBuildNameCommand();
                    break;
                case 9U:
                    command_len = PbBuildAdvCommand();
                    break;
                case 10U:
                    command_len = PbCommandText(0U, "AT+BLEMODE=0\r\n");
                    break;
                case 11U:
                    command_len = PbCommandText(0U, "AT+TRANSENTER\r\n");
                    break;
                default:
                    break;
            }

            if(command_len != 0U)
            {
                UartSendData(&PB03F_BLE_UART, pb_at_command, command_len);
                pb_at_attempts++;
                pb_state_tick = GetSysTick();
                pb_state = PB_STATE_WAIT_RESPONSE;
            }
        }
    }else if(pb_state == PB_STATE_WAIT_RESPONSE)
    {
        if((pb_at_result == PB_AT_RESULT_OK) &&
           ((pb_at_step != 6U) || (pb_mac_valid != 0U)))
        {
            pb_at_attempts = 0U;
            pb_at_step++;
            if(pb_at_step >= PB_AT_STEP_COUNT)
            {
                pb_state = PB_STATE_TRANSPARENT;
                PbResetRxAssembly();
            }else
            {
                pb_state = PB_STATE_SEND_COMMAND;
            }
        }else if((pb_at_result == PB_AT_RESULT_ERROR) ||
                 ((uint32_t)(GetSysTick() - pb_state_tick) >=
                  PB_AT_TIMEOUT_MS))
        {
            if(pb_at_attempts < PB_AT_MAX_ATTEMPTS)
            {
                pb_state = PB_STATE_SEND_COMMAND;
            }else
            {
                PbStartConfiguration(PB_RESTART_DELAY_MS);
            }
        }
    }else if(pb_state == PB_STATE_ESCAPE_SEND)
    {
        if((PB03F_BLE_UART.TxBusy == 0U) &&
           (PB03F_BLE_UART.TxHead == PB03F_BLE_UART.TxTail))
        {
            pb_at_result = PB_AT_RESULT_NONE;
            pb_at_ok_match = 0U;
            pb_at_error_match = 0U;
            pb_at_line_len = 0U;
            UartSendData(&PB03F_BLE_UART, (uint8_t *)"+++", 3U);
            pb_escape_attempts++;
            pb_state_tick = GetSysTick();
            pb_state = PB_STATE_ESCAPE_WAIT;
        }
    }else if(pb_state == PB_STATE_ESCAPE_WAIT)
    {
        if(pb_at_result == PB_AT_RESULT_OK)
        {
            PbStartConfiguration(50UL);
        }else if((pb_at_result == PB_AT_RESULT_ERROR) ||
                 ((uint32_t)(GetSysTick() - pb_state_tick) >=
                  PB_AT_TIMEOUT_MS))
        {
            if(pb_escape_attempts < PB_AT_MAX_ATTEMPTS)
            {
                pb_state = PB_STATE_ESCAPE_SEND;
            }else
            {
                pb_escape_attempts = 0U;
                pb_state_tick = GetSysTick();
                pb_state = PB_STATE_ESCAPE_DELAY;
            }
        }
    }else if((pb_state == PB_STATE_ESCAPE_DELAY) &&
             ((uint32_t)(GetSysTick() - pb_state_tick) >=
              PB_ESCAPE_RETRY_DELAY_MS))
    {
        pb_state = PB_STATE_ESCAPE_SEND;
    }

    if((pb_rx_active != 0U) &&
       ((uint32_t)(GetSysTick() - pb_rx_tick) >=
        PB03F_BLE_REASSEMBLY_TIMEOUT_MS))
    {
        PbFailAssembly(pb_rx_msg_id, "missing chunk");
    }

    if((pb_forward_pending != 0U) &&
       (V851ProtocolSendJson(pb_rx_json, pb_rx_json_len) != 0U))
    {
        pb_forward_pending = 0U;
        pb_rx_locked = 0U;
        PbResetRxAssembly();
    }

    if((pb_state == PB_STATE_TRANSPARENT) &&
       (pb_reconfigure_requested != 0U) &&
       (pb_tx_active == 0U) && (pb_tx_count == 0U) &&
       (pb_rx_active == 0U) && (pb_rx_locked == 0U) &&
       (pb_forward_pending == 0U) &&
       (PB03F_BLE_UART.TxBusy == 0U) &&
       (PB03F_BLE_UART.TxHead == PB03F_BLE_UART.TxTail))
    {
        pb_state = PB_STATE_ESCAPE_SEND;
        return;
    }

    if((pb_state != PB_STATE_TRANSPARENT) ||
       (pb_tx_count == 0U) ||
       (PB03F_BLE_UART.TxBusy != 0U) ||
       (PB03F_BLE_UART.TxHead != PB03F_BLE_UART.TxTail))
    {
        return;
    }

    slot = pb_tx_head;
    json_len = pb_tx_json_len[slot];
    if(pb_tx_active == 0U)
    {
        pb_tx_active = 1U;
        pb_tx_chunk_index = 0U;
        pb_tx_chunk_total =
            (json_len + PB03F_BLE_CHUNK_PAYLOAD_MAX - 1U) /
            PB03F_BLE_CHUNK_PAYLOAD_MAX;
        pb_tx_msg_id = pb_tx_next_msg_id++;
    }

    offset = pb_tx_chunk_index * PB03F_BLE_CHUNK_PAYLOAD_MAX;
    remaining = json_len - offset;
    payload_len = (remaining > PB03F_BLE_CHUNK_PAYLOAD_MAX) ?
                  PB03F_BLE_CHUNK_PAYLOAD_MAX : remaining;
    pb_tx_frame[0] = PB03F_BLE_FRAME_MAGIC_HIGH;
    pb_tx_frame[1] = PB03F_BLE_FRAME_MAGIC_LOW;
    pb_tx_frame[2] = PB03F_BLE_FRAME_VERSION;
    pb_tx_frame[3] =
        ((pb_tx_chunk_index + 1U) == pb_tx_chunk_total) ?
        PB03F_BLE_FRAME_FLAG_LAST : 0U;
    PbWriteBe16(&pb_tx_frame[4], pb_tx_msg_id);
    PbWriteBe16(&pb_tx_frame[6], pb_tx_chunk_index);
    PbWriteBe16(&pb_tx_frame[8], pb_tx_chunk_total);
    PbWriteBe16(&pb_tx_frame[10], payload_len);
    memcpy(&pb_tx_frame[PB03F_BLE_FRAME_HEADER_SIZE],
           &pb_tx_json[slot][offset], payload_len);
    crc_value = crc_16(pb_tx_frame,
                       PB03F_BLE_FRAME_HEADER_SIZE + payload_len);
    PbWriteBe16(&pb_tx_frame[PB03F_BLE_FRAME_HEADER_SIZE + payload_len],
                crc_value);
    frame_len = PB03F_BLE_FRAME_HEADER_SIZE + payload_len +
                PB03F_BLE_FRAME_CRC_SIZE;
    UartSendData(&PB03F_BLE_UART, pb_tx_frame, frame_len);

    pb_tx_chunk_index++;
    if(pb_tx_chunk_index >= pb_tx_chunk_total)
    {
        memset(pb_tx_json[slot], 0, json_len);
        pb_tx_json_len[slot] = 0U;
        pb_tx_active = 0U;
        pb_tx_head++;
        if(pb_tx_head >= PB_TX_QUEUE_DEPTH)
        {
            pb_tx_head = 0U;
        }
        pb_tx_count--;
    }
}

void Pb03fBleInit(void)
{
    memset(pb_mac, 0, sizeof(pb_mac));
    memset(pb_ble_id, 0, sizeof(pb_ble_id));
    memset(pb_ble_name, 0, sizeof(pb_ble_name));
    memset(pb_at_line, 0, sizeof(pb_at_line));
    memset(pb_rx_json, 0, sizeof(pb_rx_json));
    memset(pb_tx_json, 0, sizeof(pb_tx_json));
    memset(pb_tx_json_len, 0, sizeof(pb_tx_json_len));

    pb_mac_valid = 0U;
    pb_rx_locked = 0U;
    pb_forward_pending = 0U;
    pb_tx_head = 0U;
    pb_tx_tail = 0U;
    pb_tx_count = 0U;
    pb_tx_active = 0U;
    pb_tx_next_msg_id = 1U;
    PbResetRxAssembly();
    PbStartConfiguration(PB_POWER_DELAY_MS);
}

#endif /* bleV851_BRIDGE_ENABLED */

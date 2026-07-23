#include "v851_control_info.h"

#if bleV851_BRIDGE_ENABLED

#include "core_json.h"

#include <string.h>

static V851ControlDetails xdata v851_control_details[V851_CONTROL_COUNT];

static void V851ControlInfoWriteBe16(uint8_t *_data, uint16_t value)
{
    _data[0] = (uint8_t)(value >> 8);
    _data[1] = (uint8_t)value;
}

static uint8_t V851ControlInfoReadBool(const V851ControlCommand *command,
                                       const char *name,
                                       uint16_t name_len,
                                       uint16_t *value)
{
    const char *json_value;
    json_size_t json_value_len;
    JSONTypes_t json_type;

    if((command == NULL) || (name == NULL) || (value == NULL) ||
       (command->params_json == NULL) || (command->params_len == 0U))
    {
        return 0U;
    }

    if(JSON_SearchConst((const char *)command->params_json,
                        command->params_len,
                        name, name_len,
                        &json_value, &json_value_len,
                        &json_type) != JSONSuccess)
    {
        return 0U;
    }

    if(json_type == JSONTrue)
    {
        *value = 1U;
        return 1U;
    }
    if(json_type == JSONFalse)
    {
        *value = 0U;
        return 1U;
    }
    return 0U;
}

static uint8_t V851ControlInfoReadUint16(const V851ControlCommand *command,
                                         const char *name,
                                         uint16_t name_len,
                                         uint16_t *value)
{
    uint32_t number;

    if((command == NULL) || (name == NULL) || (value == NULL) ||
       (command->params_json == NULL) || (command->params_len == 0U) ||
       (JSONSearchToNumber(command->params_json,
                           command->params_len,
                           name, name_len,
                           &number) != JSONSuccess) ||
       (number > 0xFFFFUL))
    {
        return 0U;
    }

    *value = (uint16_t)number;
    return 1U;
}

static uint8_t V851ControlInfoReadText(const V851ControlCommand *command,
                                       const char *name,
                                       uint16_t name_len,
                                       uint8_t *out)
{
    char text[V851_CONTROL_DGUS_TEXT_BYTES + 1U];
    uint16_t text_len;

    if((command == NULL) || (name == NULL) || (out == NULL) ||
       (command->params_json == NULL) || (command->params_len == 0U) ||
       (JSONSearchToArray(command->params_json,
                          command->params_len,
                          name, name_len,
                          text, sizeof(text)) != JSONSuccess))
    {
        return 0U;
    }

    text_len = (uint16_t)strlen(text);
    if(text_len == 0U)
    {
        return 0U;
    }
    memcpy(out, text, text_len);
    return 1U;
}

static void V851ControlInfoDgusInvalid(V851ControlHandlerContext *context)
{
    context->result->status = V851_CONTROL_STATUS_FAILED;
    context->result->error_code = "INVALID_PARAMS";
    context->result->error_message = "invalid control parameters";
    context->result->applied_json = NULL;
    context->result->applied_len = 0U;
}

void V851ControlInfoDgusHandler(V851ControlHandlerContext *context)
{
    const V851ControlCommand *command;
    uint8_t record[V851_CONTROL_DGUS_SLOT_BYTES];
    uint16_t value0;
    uint16_t value1;
    uint16_t value2;
    uint32_t address;
    uint8_t valid = 0U;

    if((context == NULL) || (context->command == NULL) ||
       (context->result == NULL))
    {
        return;
    }

    command = context->command;
    memset(record, 0, sizeof(record));
    address = 0UL;

    switch(command->type)
    {
        case V851_CONTROL_EXHAUST:
            valid = V851ControlInfoReadBool(
                command, "enabled", sizeof("enabled") - 1U, &value0);
            valid &= V851ControlInfoReadUint16(
                command, "level", sizeof("level") - 1U, &value1);
            if(valid != 0U)
            {
                V851ControlInfoWriteBe16(&record[0], value0);
                V851ControlInfoWriteBe16(&record[2], value1);
                address = V851_CONTROL_DGUS_EXHAUST_ADDR;
            }
            break;

        case V851_CONTROL_CLIMATE:
            valid = V851ControlInfoReadBool(
                command, "enabled", sizeof("enabled") - 1U, &value0);
            valid &= V851ControlInfoReadUint16(
                command, "target_temperature",
                sizeof("target_temperature") - 1U, &value1);
            valid &= V851ControlInfoReadText(
                command, "mode", sizeof("mode") - 1U, &record[4]);
            if(valid != 0U)
            {
                V851ControlInfoWriteBe16(&record[0], value0);
                V851ControlInfoWriteBe16(&record[2], value1);
                address = V851_CONTROL_DGUS_CLIMATE_ADDR;
            }
            break;

        case V851_CONTROL_LIGHT:
            valid = V851ControlInfoReadBool(
                command, "enabled", sizeof("enabled") - 1U, &value0);
            valid &= V851ControlInfoReadUint16(
                command, "brightness", sizeof("brightness") - 1U, &value1);
            valid &= V851ControlInfoReadUint16(
                command, "color_temperature",
                sizeof("color_temperature") - 1U, &value2);
            if(valid != 0U)
            {
                V851ControlInfoWriteBe16(&record[0], value0);
                V851ControlInfoWriteBe16(&record[2], value1);
                V851ControlInfoWriteBe16(&record[4], value2);
                address = V851_CONTROL_DGUS_LIGHT_ADDR;
            }
            break;

        case V851_CONTROL_DEVICE_SETTINGS:
            valid = V851ControlInfoReadUint16(
                command, "volume", sizeof("volume") - 1U, &value0);
            valid &= V851ControlInfoReadUint16(
                command, "screen_brightness",
                sizeof("screen_brightness") - 1U, &value1);
            valid &= V851ControlInfoReadText(
                command, "language", sizeof("language") - 1U, &record[4]);
            if(valid != 0U)
            {
                V851ControlInfoWriteBe16(&record[0], value0);
                V851ControlInfoWriteBe16(&record[2], value1);
                address = V851_CONTROL_DGUS_SETTINGS_ADDR;
            }
            break;

        case V851_CONTROL_HUMIDIFIER:
            valid = V851ControlInfoReadBool(
                command, "enabled", sizeof("enabled") - 1U, &value0);
            valid &= V851ControlInfoReadUint16(
                command, "target_humidity",
                sizeof("target_humidity") - 1U, &value1);
            if(valid != 0U)
            {
                V851ControlInfoWriteBe16(&record[0], value0);
                V851ControlInfoWriteBe16(&record[2], value1);
                address = V851_CONTROL_DGUS_HUMIDIFIER_ADDR;
            }
            break;

        default:
            break;
    }

    if((valid == 0U) || (address == 0UL))
    {
        V851ControlInfoDgusInvalid(context);
        return;
    }

    write_dgus_vp(address, record, V851_CONTROL_DGUS_SLOT_WORDS);
    context->result->status = V851_CONTROL_STATUS_SUCCESS;
    context->result->error_code = NULL;
    context->result->error_message = NULL;
    context->result->applied_json = NULL;
    context->result->applied_len = 0U;
}

static void V851ControlInfoCopyText(char *out,
                                    uint16_t out_size,
                                    const char *text,
                                    uint16_t text_len)
{
    if((out == NULL) || (out_size == 0U))
    {
        return;
    }

    if(text == NULL)
    {
        out[0] = '\0';
        return;
    }

    if(text_len >= out_size)
    {
        text_len = out_size - 1U;
    }
    if(text_len != 0U)
    {
        memcpy(out, text, text_len);
    }
    out[text_len] = '\0';
}

void V851ControlInfoInit(void)
{
    memset(v851_control_details, 0, sizeof(v851_control_details));
}

uint8_t V851ControlInfoUpdate(const V851ControlCommand *command)
{
    V851ControlDetails *details;
    uint16_t stored_len;
    uint32_t revision;

    if((command == NULL) || (command->type >= V851_CONTROL_COUNT) ||
       (command->type == V851_CONTROL_INVALID) ||
       ((command->params_json == NULL) && (command->params_len != 0U)))
    {
        return 0U;
    }

    details = &v851_control_details[command->type];
    revision = details->revision + 1UL;
    if(revision == 0UL)
    {
        revision = 1UL;
    }

    memset(details, 0, sizeof(*details));
    details->type = command->type;
    details->valid = 1U;
    details->updated = 1U;
    details->revision = revision;
    details->received_tick = GetSysTick();
    details->expire_at = command->expire_at;
    details->params_len = command->params_len;

    V851ControlInfoCopyText(details->command_id,
                            sizeof(details->command_id),
                            command->command_id,
                            command->command_id_len);
    V851ControlInfoCopyText(details->server_msg_id,
                            sizeof(details->server_msg_id),
                            command->server_msg_id,
                            command->server_msg_id_len);
    V851ControlInfoCopyText(details->cmd,
                            sizeof(details->cmd),
                            command->cmd,
                            command->cmd_len);

    if(command->type == V851_CONTROL_DEVICE_PASSWORD)
    {
        details->params_redacted = 1U;
        return 1U;
    }

    stored_len = command->params_len;
    if(stored_len > V851_CONTROL_PARAMS_SNAPSHOT_MAX)
    {
        stored_len = V851_CONTROL_PARAMS_SNAPSHOT_MAX;
        details->params_truncated = 1U;
    }
    if(stored_len != 0U)
    {
        memcpy(details->params_json, command->params_json, stored_len);
    }
    details->params_json[stored_len] = '\0';
    details->stored_len = stored_len;
    return 1U;
}

uint8_t V851ControlInfoAnyUpdated(void)
{
    uint8_t i;

    for(i = 0U; i < V851_CONTROL_COUNT; i++)
    {
        if((v851_control_details[i].valid != 0U) &&
           (v851_control_details[i].updated != 0U))
        {
            return 1U;
        }
    }
    return 0U;
}

uint8_t V851ControlInfoIsUpdated(V851ControlType type)
{
    if((type >= V851_CONTROL_COUNT) || (type == V851_CONTROL_INVALID))
    {
        return 0U;
    }
    return ((v851_control_details[type].valid != 0U) &&
            (v851_control_details[type].updated != 0U)) ? 1U : 0U;
}

const V851ControlDetails *V851ControlInfoGet(V851ControlType type)
{
    if((type >= V851_CONTROL_COUNT) || (type == V851_CONTROL_INVALID) ||
       (v851_control_details[type].valid == 0U))
    {
        return NULL;
    }
    return &v851_control_details[type];
}

uint8_t V851ControlInfoClearUpdated(V851ControlType type)
{
    if((type >= V851_CONTROL_COUNT) || (type == V851_CONTROL_INVALID) ||
       (v851_control_details[type].valid == 0U))
    {
        return 0U;
    }
    v851_control_details[type].updated = 0U;
    return 1U;
}

uint8_t V851ControlInfoDgusInit(void)
{
    if(V851ProtocolRegisterControlHandler(
           V851_CONTROL_EXHAUST, V851ControlInfoDgusHandler) == 0U)
    {
        return 0U;
    }
    if(V851ProtocolRegisterControlHandler(
           V851_CONTROL_CLIMATE, V851ControlInfoDgusHandler) == 0U)
    {
        return 0U;
    }
    if(V851ProtocolRegisterControlHandler(
           V851_CONTROL_LIGHT, V851ControlInfoDgusHandler) == 0U)
    {
        return 0U;
    }
    if(V851ProtocolRegisterControlHandler(
           V851_CONTROL_DEVICE_SETTINGS, V851ControlInfoDgusHandler) == 0U)
    {
        return 0U;
    }
    if(V851ProtocolRegisterControlHandler(
           V851_CONTROL_HUMIDIFIER, V851ControlInfoDgusHandler) == 0U)
    {
        return 0U;
    }
    return 1U;
}

#endif /* bleV851_BRIDGE_ENABLED */

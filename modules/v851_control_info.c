#include "v851_control_info.h"

#if bleV851_BRIDGE_ENABLED

#include <string.h>

static V851ControlDetails xdata v851_control_details[V851_CONTROL_COUNT];

static void V851ControlInfoWriteBe16(uint8_t *_data, uint16_t value)
{
    _data[0] = (uint8_t)(value >> 8);
    _data[1] = (uint8_t)value;
}

static void V851ControlInfoWriteBe32(uint8_t *_data, uint32_t value)
{
    _data[0] = (uint8_t)(value >> 24);
    _data[1] = (uint8_t)(value >> 16);
    _data[2] = (uint8_t)(value >> 8);
    _data[3] = (uint8_t)value;
}

static V851ControlType V851ControlInfoDgusType(uint8_t slot)
{
    switch(slot)
    {
        case 0U:
            return V851_CONTROL_EXHAUST;

        case 1U:
            return V851_CONTROL_CLIMATE;

        case 2U:
            return V851_CONTROL_LIGHT;

        case 3U:
            return V851_CONTROL_DEVICE_SETTINGS;

        default:
            return V851_CONTROL_INVALID;
    }
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

void V851ControlInfoDgusTask(void)
{
    const V851ControlDetails *details;
    V851ControlType type;
    uint8_t record[V851_CONTROL_DGUS_RECORD_BYTES];
    uint8_t slot;
    uint8_t flags;
    uint16_t params_crc;
    uint32_t address;

    for(slot = 0U; slot < 4U; slot++)
    {
        type = V851ControlInfoDgusType(slot);
        if((type == V851_CONTROL_INVALID) ||
           (V851ControlInfoIsUpdated(type) == 0U))
        {
            continue;
        }

        details = V851ControlInfoGet(type);
        if(details == NULL)
        {
            continue;
        }

        memset(record, 0, sizeof(record));
        flags = 0U;
        if(details->valid != 0U) flags |= 0x01U;
        if(details->updated != 0U) flags |= 0x02U;
        if(details->params_truncated != 0U) flags |= 0x04U;
        if(details->params_redacted != 0U) flags |= 0x08U;

        params_crc = 0U;
        if((details->params_redacted == 0U) &&
           (details->stored_len != 0U))
        {
            params_crc = crc_16((uint8_t *)details->params_json,
                                details->stored_len);
        }

        record[0] = (uint8_t)type;
        record[1] = flags;
        V851ControlInfoWriteBe16(&record[2], details->params_len);
        V851ControlInfoWriteBe32(&record[4], details->revision);
        V851ControlInfoWriteBe32(&record[8], details->received_tick);
        V851ControlInfoWriteBe16(&record[12], details->stored_len);
        V851ControlInfoWriteBe16(&record[14], params_crc);

        address = V851_CONTROL_DGUS_BASE_ADDR +
                  ((uint32_t)slot * V851_CONTROL_DGUS_ADDR_STRIDE);
        write_dgus_vp(address, record, V851_CONTROL_DGUS_RECORD_WORDS);
        (void)V851ControlInfoClearUpdated(type);
    }
}

#endif /* bleV851_BRIDGE_ENABLED */

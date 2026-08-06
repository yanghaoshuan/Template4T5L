#include "v851_control_info.h"

#if v851PROTOCOL_ENABLED

#include <string.h>

#pragma optimize(8, size)

static uint8_t xdata v851_control_report_shadow[V851_CONTROL_COUNT]
                                                [V851_CONTROL_REPORT_MAX_BYTES];
static uint16_t v851_control_report_shadow_valid_mask;

static uint16_t V851ControlInfoReadBe16(const uint8_t *bytes)
{
    return (uint16_t)(((uint16_t)bytes[0] << 8) | bytes[1]);
}

static void V851ControlInfoWriteBe16(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)(value >> 8);
    bytes[1] = (uint8_t)value;
}

static uint8_t V851ControlInfoIndex(uint8_t struct_type)
{
    if((struct_type < V851_TLV_STRUCT_EXHAUST) ||
       (struct_type > V851_TLV_STRUCT_INLET_FAN))
    {
        return 0xFFU;
    }
    return (uint8_t)(struct_type - V851_TLV_STRUCT_EXHAUST);
}

static uint32_t V851ControlInfoCommandAddress(uint8_t struct_type)
{
    switch(struct_type)
    {
        case V851_TLV_STRUCT_EXHAUST:
            return V851_CONTROL_COMMAND_EXHAUST_ADDR;
        case V851_TLV_STRUCT_LIGHT:
            return V851_CONTROL_COMMAND_LIGHT_ADDR;
        case V851_TLV_STRUCT_UVB:
            return V851_CONTROL_COMMAND_UVB_ADDR;
        case V851_TLV_STRUCT_ANION:
            return V851_CONTROL_COMMAND_ANION_ADDR;
        case V851_TLV_STRUCT_PLASMA:
            return V851_CONTROL_COMMAND_PLASMA_ADDR;
        case V851_TLV_STRUCT_CLIMATE:
            return V851_CONTROL_COMMAND_CLIMATE_ADDR;
        case V851_TLV_STRUCT_HUMIDIFIER:
            return V851_CONTROL_COMMAND_HUMIDIFIER_ADDR;
        case V851_TLV_STRUCT_INLET_FAN:
            return V851_CONTROL_COMMAND_INLET_FAN_ADDR;
        default:
            return 0UL;
    }
}

static uint32_t V851ControlInfoReportAddress(uint8_t struct_type)
{
    switch(struct_type)
    {
        case V851_TLV_STRUCT_EXHAUST:
            return V851_CONTROL_REPORT_EXHAUST_ADDR;
        case V851_TLV_STRUCT_LIGHT:
            return V851_CONTROL_REPORT_LIGHT_ADDR;
        case V851_TLV_STRUCT_UVB:
            return V851_CONTROL_REPORT_UVB_ADDR;
        case V851_TLV_STRUCT_ANION:
            return V851_CONTROL_REPORT_ANION_ADDR;
        case V851_TLV_STRUCT_PLASMA:
            return V851_CONTROL_REPORT_PLASMA_ADDR;
        case V851_TLV_STRUCT_CLIMATE:
            return V851_CONTROL_REPORT_CLIMATE_ADDR;
        case V851_TLV_STRUCT_HUMIDIFIER:
            return V851_CONTROL_REPORT_HUMIDIFIER_ADDR;
        case V851_TLV_STRUCT_INLET_FAN:
            return V851_CONTROL_REPORT_INLET_FAN_ADDR;
        default:
            return 0UL;
    }
}

static uint8_t V851ControlInfoRelevantBytes(uint8_t struct_type)
{
    switch(struct_type)
    {
        case V851_TLV_STRUCT_EXHAUST:
        case V851_TLV_STRUCT_LIGHT:
        case V851_TLV_STRUCT_CLIMATE:
        case V851_TLV_STRUCT_INLET_FAN:
            return 4U;
        case V851_TLV_STRUCT_UVB:
        case V851_TLV_STRUCT_ANION:
        case V851_TLV_STRUCT_PLASMA:
        case V851_TLV_STRUCT_HUMIDIFIER:
            return 2U;
        default:
            return 0U;
    }
}

static uint8_t V851ControlInfoIsEnabledTag(uint8_t struct_type, uint8_t tag)
{
    if(tag != 0x01U)
    {
        return 0U;
    }
    return (uint8_t)(V851ControlInfoIndex(struct_type) != 0xFFU);
}

static uint8_t V851ControlInfoIsLevelTag(uint8_t struct_type, uint8_t tag)
{
    if(tag != 0x02U)
    {
        return 0U;
    }
    return (uint8_t)((struct_type == V851_TLV_STRUCT_EXHAUST) ||
                     (struct_type == V851_TLV_STRUCT_LIGHT) ||
                     (struct_type == V851_TLV_STRUCT_INLET_FAN));
}

uint8_t V851ControlInfoValidateSegment(uint8_t struct_type,
                                       const uint8_t *field_bytes,
                                       uint16_t length)
{
    V851TlvFieldCursor cursor;
    V851TlvField field;
    V851TlvIterResult result;
    uint8_t mapped_mask;
    uint8_t mapped_bit;
    uint16_t target;

    if((field_bytes == NULL) ||
       (V851ControlInfoIndex(struct_type) == 0xFFU))
    {
        return 0U;
    }

    mapped_mask = 0U;
    V851TlvFieldCursorInit(&cursor, field_bytes, length);
    for(;;)
    {
        result = V851TlvFieldNext(&cursor, &field);
        if(result == V851_TLV_ITER_END)
        {
            return 1U;
        }
        if(result != V851_TLV_ITER_FIELD)
        {
            return 0U;
        }

        mapped_bit = 0U;
        if(V851ControlInfoIsEnabledTag(struct_type, field.tag) != 0U)
        {
            mapped_bit = 0x01U;
            if((field.length != 1U) || (field.value[0] > 1U))
            {
                return 0U;
            }
        }
        else if(V851ControlInfoIsLevelTag(struct_type, field.tag) != 0U)
        {
            mapped_bit = 0x02U;
            if(field.length != 1U)
            {
                return 0U;
            }
        }
        else if((struct_type == V851_TLV_STRUCT_CLIMATE) &&
                (field.tag == V851_TLV_TAG_CLIMATE_TARGET))
        {
            mapped_bit = 0x02U;
            if(V851TlvReadBinary64Uint16(field.value, field.length,
                                         &target) == 0U)
            {
                return 0U;
            }
        }

        if(mapped_bit != 0U)
        {
            if((mapped_mask & mapped_bit) != 0U)
            {
                return 0U;
            }
            mapped_mask |= mapped_bit;
        }
    }
}

uint8_t V851ControlInfoApplySegment(uint8_t struct_type,
                                    const uint8_t *field_bytes,
                                    uint16_t length)
{
    V851TlvFieldCursor cursor;
    V851TlvField field;
    uint8_t record[V851_CONTROL_COMMAND_SLOT_BYTES];
    uint8_t changed;
    uint16_t target;
    uint32_t address;

    if((V851ControlInfoIndex(struct_type) == 0xFFU) ||
       (field_bytes == NULL))
    {
        return 0U;
    }

    address = V851ControlInfoCommandAddress(struct_type);
    read_dgus_vp(address, record, V851_CONTROL_COMMAND_SLOT_WORDS);
    changed = 0U;
    V851TlvFieldCursorInit(&cursor, field_bytes, length);
    while(V851TlvFieldNext(&cursor, &field) == V851_TLV_ITER_FIELD)
    {
        if(V851ControlInfoIsEnabledTag(struct_type, field.tag) != 0U)
        {
            V851ControlInfoWriteBe16(&record[0], field.value[0]);
            changed = 1U;
        }
        else if(V851ControlInfoIsLevelTag(struct_type, field.tag) != 0U)
        {
            V851ControlInfoWriteBe16(&record[2], field.value[0]);
            changed = 1U;
        }
        else if((struct_type == V851_TLV_STRUCT_CLIMATE) &&
                (field.tag == V851_TLV_TAG_CLIMATE_TARGET) &&
                (V851TlvReadBinary64Uint16(field.value, field.length,
                                            &target) != 0U))
        {
            V851ControlInfoWriteBe16(&record[2], target);
            changed = 1U;
        }
    }

    if(changed != 0U)
    {
        write_dgus_vp(address, record, V851_CONTROL_COMMAND_SLOT_WORDS);
    }
    return changed;
}

uint16_t V851ControlInfoBuildFields(uint8_t struct_type,
                                    uint8_t *field_buffer,
                                    uint16_t capacity)
{
    uint8_t record[V851_CONTROL_REPORT_MAX_BYTES];
    uint8_t bytes;
    uint16_t offset;
    uint16_t value;
    uint32_t address;

    address = V851ControlInfoReportAddress(struct_type);
    if((address == 0UL) || (field_buffer == NULL))
    {
        return 0U;
    }

    bytes = V851ControlInfoRelevantBytes(struct_type);
    if(bytes == 0U)
    {
        return 0U;
    }
    read_dgus_vp(address, record, (uint8_t)(bytes / 2U));
    offset = 0U;
    value = V851ControlInfoReadBe16(&record[0]);
    if(value <= 1U)
    {
        if(V851TlvWriteU8(field_buffer, capacity, &offset, 0x01U,
                           (uint8_t)value) == 0U)
        {
            return 0U;
        }
    }

    if(V851ControlInfoIsLevelTag(struct_type, 0x02U) != 0U)
    {
        value = V851ControlInfoReadBe16(&record[2]);
        if((value <= 0xFFU) &&
           (V851TlvWriteU8(field_buffer, capacity, &offset, 0x02U,
                            (uint8_t)value) == 0U))
        {
            return 0U;
        }
    }
    else if(struct_type == V851_TLV_STRUCT_CLIMATE)
    {
        value = V851ControlInfoReadBe16(&record[2]);
        if(V851TlvWriteBinary64Uint16(field_buffer, capacity, &offset,
                                      V851_TLV_TAG_CLIMATE_TARGET,
                                      value) == 0U)
        {
            return 0U;
        }
    }
    return offset;
}

uint16_t V851ControlInfoScanChanged(void)
{
    uint8_t record[V851_CONTROL_REPORT_MAX_BYTES];
    uint8_t index;
    uint8_t bytes;
    uint8_t struct_type;
    uint16_t valid_bit;
    uint16_t changed_mask;

    changed_mask = 0U;
    for(index = 0U; index < V851_CONTROL_COUNT; ++index)
    {
        struct_type = (uint8_t)(V851_TLV_STRUCT_EXHAUST + index);
        bytes = V851ControlInfoRelevantBytes(struct_type);
        read_dgus_vp(V851ControlInfoReportAddress(struct_type), record,
                     (uint8_t)(bytes / 2U));
        valid_bit = (uint16_t)1U << index;
        if((v851_control_report_shadow_valid_mask & valid_bit) == 0U)
        {
            /* The first scan establishes a baseline without reporting. */
            memcpy(v851_control_report_shadow[index], record, bytes);
            v851_control_report_shadow_valid_mask |= valid_bit;
        }
        else if(memcmp(record, v851_control_report_shadow[index], bytes) != 0)
        {
            changed_mask |= valid_bit;
            memcpy(v851_control_report_shadow[index], record, bytes);
        }
    }
    return changed_mask;
}

#endif /* v851PROTOCOL_ENABLED */

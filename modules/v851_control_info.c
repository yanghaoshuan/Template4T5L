#include "v851_control_info.h"

#if v851PROTOCOL_ENABLED

#include <string.h>

#pragma optimize(8, size)

static uint8_t xdata v851_control_shadow[V851_CONTROL_COUNT]
                                         [V851_CONTROL_DGUS_SLOT_BYTES];

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

static uint32_t V851ControlInfoAddress(uint8_t struct_type)
{
    switch(struct_type)
    {
        case V851_TLV_STRUCT_EXHAUST:
            return V851_CONTROL_DGUS_EXHAUST_ADDR;
        case V851_TLV_STRUCT_LIGHT:
            return V851_CONTROL_DGUS_LIGHT_ADDR;
        case V851_TLV_STRUCT_UVB:
            return V851_CONTROL_DGUS_UVB_ADDR;
        case V851_TLV_STRUCT_ANION:
            return V851_CONTROL_DGUS_ANION_ADDR;
        case V851_TLV_STRUCT_PLASMA:
            return V851_CONTROL_DGUS_PLASMA_ADDR;
        case V851_TLV_STRUCT_CLIMATE:
            return V851_CONTROL_DGUS_CLIMATE_ADDR;
        case V851_TLV_STRUCT_HUMIDIFIER:
            return V851_CONTROL_DGUS_HUMIDIFIER_ADDR;
        case V851_TLV_STRUCT_INLET_FAN:
            return V851_CONTROL_DGUS_INLET_FAN_ADDR;
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

void V851ControlInfoInit(void)
{
    uint8_t index;
    uint8_t struct_type;

    memset(v851_control_shadow, 0, sizeof(v851_control_shadow));
    for(index = 0U; index < V851_CONTROL_COUNT; ++index)
    {
        struct_type = (uint8_t)(V851_TLV_STRUCT_EXHAUST + index);
        read_dgus_vp(V851ControlInfoAddress(struct_type),
                     v851_control_shadow[index],
                     V851_CONTROL_DGUS_SLOT_WORDS);
    }
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

void V851ControlInfoApplySegment(uint8_t struct_type,
                                 const uint8_t *field_bytes,
                                 uint16_t length)
{
    V851TlvFieldCursor cursor;
    V851TlvField field;
    uint8_t record[V851_CONTROL_DGUS_SLOT_BYTES];
    uint8_t index;
    uint8_t changed;
    uint16_t target;
    uint32_t address;

    index = V851ControlInfoIndex(struct_type);
    if((index == 0xFFU) || (field_bytes == NULL))
    {
        return;
    }

    address = V851ControlInfoAddress(struct_type);
    read_dgus_vp(address, record, V851_CONTROL_DGUS_SLOT_WORDS);
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
        write_dgus_vp(address, record, V851_CONTROL_DGUS_SLOT_WORDS);
        memcpy(v851_control_shadow[index], record,
               V851_CONTROL_DGUS_SLOT_BYTES);
    }
}

uint16_t V851ControlInfoBuildFields(uint8_t struct_type,
                                    uint8_t *field_buffer,
                                    uint16_t capacity)
{
    uint8_t record[V851_CONTROL_DGUS_SLOT_BYTES];
    uint16_t offset;
    uint16_t value;
    uint32_t address;

    address = V851ControlInfoAddress(struct_type);
    if((address == 0UL) || (field_buffer == NULL))
    {
        return 0U;
    }

    read_dgus_vp(address, record, V851_CONTROL_DGUS_SLOT_WORDS);
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

    value = V851ControlInfoReadBe16(&record[2]);
    if(V851ControlInfoIsLevelTag(struct_type, 0x02U) != 0U)
    {
        if((value <= 0xFFU) &&
           (V851TlvWriteU8(field_buffer, capacity, &offset, 0x02U,
                            (uint8_t)value) == 0U))
        {
            return 0U;
        }
    }
    else if(struct_type == V851_TLV_STRUCT_CLIMATE)
    {
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
    uint8_t record[V851_CONTROL_DGUS_SLOT_BYTES];
    uint8_t index;
    uint8_t bytes;
    uint8_t struct_type;
    uint16_t changed_mask;

    changed_mask = 0U;
    for(index = 0U; index < V851_CONTROL_COUNT; ++index)
    {
        struct_type = (uint8_t)(V851_TLV_STRUCT_EXHAUST + index);
        bytes = V851ControlInfoRelevantBytes(struct_type);
        read_dgus_vp(V851ControlInfoAddress(struct_type), record,
                     V851_CONTROL_DGUS_SLOT_WORDS);
        if(memcmp(record, v851_control_shadow[index], bytes) != 0)
        {
            changed_mask |= (uint16_t)(1U << index);
            memcpy(v851_control_shadow[index], record,
                   V851_CONTROL_DGUS_SLOT_BYTES);
        }
    }
    return changed_mask;
}

#endif /* v851PROTOCOL_ENABLED */

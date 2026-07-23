#include "bridge_json.h"

#include <string.h>

void BridgeJsonWriterInit(BridgeJsonWriter *writer,
                          uint8_t *buffer,
                          uint16_t capacity)
{
    if(writer == NULL)
    {
        return;
    }

    writer->buffer = buffer;
    writer->capacity = capacity;
    writer->length = 0U;
    writer->valid = ((buffer != NULL) && (capacity != 0U)) ? 1U : 0U;
}

void BridgeJsonWriterRaw(BridgeJsonWriter *writer,
                         const uint8_t *_data,
                         uint16_t len)
{
    if((writer == NULL) || (writer->valid == 0U) ||
       ((_data == NULL) && (len != 0U)))
    {
        return;
    }

    if(((uint32_t)writer->length + len) > writer->capacity)
    {
        writer->valid = 0U;
        return;
    }

    if(len != 0U)
    {
        memcpy(&writer->buffer[writer->length], _data, len);
        writer->length += len;
    }
}

void BridgeJsonWriterText(BridgeJsonWriter *writer, const char *text)
{
    if(text == NULL)
    {
        if(writer != NULL)
        {
            writer->valid = 0U;
        }
        return;
    }

    BridgeJsonWriterRaw(writer, (const uint8_t *)text, (uint16_t)strlen(text));
}

void BridgeJsonWriterQuoted(BridgeJsonWriter *writer,
                            const char *text,
                            uint16_t len)
{
    uint16_t i;
    uint8_t value;
    uint8_t escaped[6];
    static const char hex_digits[] = "0123456789ABCDEF";

    if((writer == NULL) || ((text == NULL) && (len != 0U)))
    {
        return;
    }

    BridgeJsonWriterText(writer, "\"");
    for(i = 0U; (i < len) && (writer->valid != 0U); i++)
    {
        value = (uint8_t)text[i];
        if((value == '"') || (value == '\\'))
        {
            escaped[0] = '\\';
            escaped[1] = value;
            BridgeJsonWriterRaw(writer, escaped, 2U);
        }else if(value < 0x20U)
        {
            escaped[0] = '\\';
            escaped[1] = 'u';
            escaped[2] = '0';
            escaped[3] = '0';
            escaped[4] = (uint8_t)hex_digits[value >> 4];
            escaped[5] = (uint8_t)hex_digits[value & 0x0FU];
            BridgeJsonWriterRaw(writer, escaped, sizeof(escaped));
        }else
        {
            BridgeJsonWriterRaw(writer, &value, 1U);
        }
    }
    BridgeJsonWriterText(writer, "\"");
}

void BridgeJsonWriterUint32(BridgeJsonWriter *writer, uint32_t value)
{
    uint8_t digits[10];
    uint8_t count = 0U;

    do
    {
        digits[count++] = (uint8_t)('0' + (value % 10UL));
        value /= 10UL;
    }while((value != 0UL) && (count < sizeof(digits)));

    while((count != 0U) && (writer != NULL) && (writer->valid != 0U))
    {
        count--;
        BridgeJsonWriterRaw(writer, &digits[count], 1U);
    }
}

uint16_t BridgeJsonWriterFinish(BridgeJsonWriter *writer)
{
    if((writer == NULL) || (writer->valid == 0U))
    {
        return 0U;
    }

    if(writer->length < writer->capacity)
    {
        writer->buffer[writer->length] = 0U;
    }
    return writer->length;
}

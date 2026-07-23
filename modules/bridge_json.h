#ifndef BRIDGE_JSON_H
#define BRIDGE_JSON_H

#include "sys.h"

#define BRIDGE_JSON_MAX                         2000U

typedef struct
{
    uint8_t *buffer;
    uint16_t capacity;
    uint16_t length;
    uint8_t valid;
} BridgeJsonWriter;

void BridgeJsonWriterInit(BridgeJsonWriter *writer,
                          uint8_t *buffer,
                          uint16_t capacity);
void BridgeJsonWriterRaw(BridgeJsonWriter *writer,
                         const uint8_t *_data,
                         uint16_t len);
void BridgeJsonWriterText(BridgeJsonWriter *writer, const char *text);
void BridgeJsonWriterQuoted(BridgeJsonWriter *writer,
                            const char *text,
                            uint16_t len);
void BridgeJsonWriterUint32(BridgeJsonWriter *writer, uint32_t value);
uint16_t BridgeJsonWriterFinish(BridgeJsonWriter *writer);

#endif /* BRIDGE_JSON_H */

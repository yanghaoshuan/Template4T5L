#include "v851_protocol.h"

#if v851PROTOCOL_ENABLED

/*
 * Project extension point for document-defined structures and raw fields that
 * have no DGUS mapping in this template. Keep this as one direct hook so the
 * Keil C51 overlay analyser sees a deterministic call tree.
 */
void V851TlvApplicationSegment(uint8_t command,
                               const V851TlvSegment *segment)
{
    if((command == 0xFFU) && (segment == NULL))
    {
        __NOP();
    }
}

#endif /* v851PROTOCOL_ENABLED */

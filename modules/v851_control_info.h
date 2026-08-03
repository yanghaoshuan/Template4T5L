#ifndef V851_CONTROL_INFO_H
#define V851_CONTROL_INFO_H

#include "v851_protocol.h"

#if v851PROTOCOL_ENABLED

#define V851_CONTROL_COUNT                       8U
#define V851_CONTROL_DGUS_SLOT_WORDS             4U
#define V851_CONTROL_DGUS_SLOT_BYTES             8U

#define V851_CONTROL_DGUS_EXHAUST_ADDR           0x5100UL
#define V851_CONTROL_DGUS_INLET_FAN_ADDR         0x5104UL
#define V851_CONTROL_DGUS_UVB_ADDR               0x510CUL
#define V851_CONTROL_DGUS_HUMIDIFIER_ADDR        0x5110UL
#define V851_CONTROL_DGUS_LIGHT_ADDR             0x5114UL
#define V851_CONTROL_DGUS_CLIMATE_ADDR           0x5118UL
#define V851_CONTROL_DGUS_PLASMA_ADDR            0x5120UL
#define V851_CONTROL_DGUS_ANION_ADDR             0x5124UL

void V851ControlInfoInit(void);

/* Validate one segment without changing DGUS state. */
uint8_t V851ControlInfoValidateSegment(uint8_t struct_type,
                                       const uint8_t *field_bytes,
                                       uint16_t length);

/* Apply a segment that has already passed whole-frame validation. */
void V851ControlInfoApplySegment(uint8_t struct_type,
                                 const uint8_t *field_bytes,
                                 uint16_t length);

/* Encode the compatible fields of one actuator into field_buffer. */
uint16_t V851ControlInfoBuildFields(uint8_t struct_type,
                                    uint8_t *field_buffer,
                                    uint16_t capacity);

/* Return one bit per 0x61..0x68 actuator whose compatible VP fields changed. */
uint16_t V851ControlInfoScanChanged(void);

#endif /* v851PROTOCOL_ENABLED */

#endif /* V851_CONTROL_INFO_H */

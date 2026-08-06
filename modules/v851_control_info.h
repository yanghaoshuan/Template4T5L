#ifndef V851_CONTROL_INFO_H
#define V851_CONTROL_INFO_H

#include "v851_protocol.h"

#if v851PROTOCOL_ENABLED

#define V851_CONTROL_COUNT                       8U
#define V851_CONTROL_COMMAND_SLOT_WORDS          4U
#define V851_CONTROL_COMMAND_SLOT_BYTES          8U
#define V851_CONTROL_REPORT_MAX_WORDS            2U
#define V851_CONTROL_REPORT_MAX_BYTES            4U

/* Server command VPs: V851 -> T5L. */
#define V851_CONTROL_COMMAND_EXHAUST_ADDR        0x5100UL
#define V851_CONTROL_COMMAND_INLET_FAN_ADDR      0x5104UL
#define V851_CONTROL_COMMAND_UVB_ADDR            0x510CUL
#define V851_CONTROL_COMMAND_HUMIDIFIER_ADDR     0x5110UL
#define V851_CONTROL_COMMAND_LIGHT_ADDR          0x5114UL
#define V851_CONTROL_COMMAND_CLIMATE_ADDR        0x5118UL
#define V851_CONTROL_COMMAND_PLASMA_ADDR         0x5120UL
#define V851_CONTROL_COMMAND_ANION_ADDR          0x5124UL

/* Actual-state VPs: T5L -> V851 snapshot and incremental reports. */
#define V851_CONTROL_REPORT_EXHAUST_ADDR         0x301AUL
#define V851_CONTROL_REPORT_LIGHT_ADDR           0x301FUL
#define V851_CONTROL_REPORT_UVB_ADDR             0x3024UL
#define V851_CONTROL_REPORT_ANION_ADDR           0x3029UL
#define V851_CONTROL_REPORT_PLASMA_ADDR          0x302EUL
#define V851_CONTROL_REPORT_CLIMATE_ADDR         0x3033UL
#define V851_CONTROL_REPORT_HUMIDIFIER_ADDR      0x3038UL
#define V851_CONTROL_REPORT_INLET_FAN_ADDR       0x303DUL

/* Validate one segment without changing DGUS state. */
uint8_t V851ControlInfoValidateSegment(uint8_t struct_type,
                                       const uint8_t *field_bytes,
                                       uint16_t length);

/* Apply a validated segment; return 1 when at least one mapped field exists. */
uint8_t V851ControlInfoApplySegment(uint8_t struct_type,
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

#ifndef V851_CONTROL_INFO_H
#define V851_CONTROL_INFO_H

#include "v851_protocol.h"

#if v851PROTOCOL_ENABLED

#define V851_CONTROL_COUNT                       10U
#define V851_CONTROL_FULL_MASK                   0x03FFU
#define V851_CONTROL_ENVIRONMENT_INDEX           0U
#define V851_CONTROL_ACTUATOR_BASE_INDEX         1U
#define V851_CONTROL_COMMAND_MAX_WORDS           3U
#define V851_CONTROL_COMMAND_MAX_BYTES           6U
#define V851_CONTROL_REPORT_MAX_WORDS            5U
#define V851_CONTROL_REPORT_MAX_BYTES            10U
#define V851_CONTROL_FIELD_MAX_BYTES             34U

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
#define V851_CONTROL_REPORT_ENVIRONMENT_ADDR     0x3010UL
#define V851_CONTROL_REPORT_EXHAUST_ADDR         0x301AUL
#define V851_CONTROL_REPORT_LIGHT_ADDR           0x301FUL
#define V851_CONTROL_REPORT_UVB_ADDR             0x3024UL
#define V851_CONTROL_REPORT_ANION_ADDR           0x3029UL
#define V851_CONTROL_REPORT_PLASMA_ADDR          0x302EUL
#define V851_CONTROL_REPORT_CLIMATE_ADDR         0x3033UL
#define V851_CONTROL_REPORT_HUMIDIFIER_ADDR      0x3038UL
#define V851_CONTROL_REPORT_INLET_FAN_ADDR       0x303DUL
#define V851_CONTROL_REPORT_FILTER_ADDR          0x3042UL

/* Validate one segment without changing DGUS state. */
uint8_t V851ControlInfoValidateSegment(uint8_t struct_type,
                                       const uint8_t *field_bytes,
                                       uint16_t length);

/* Apply a validated segment; return 1 when at least one mapped field exists. */
uint8_t V851ControlInfoApplySegment(uint8_t struct_type,
                                    const uint8_t *field_bytes,
                                    uint16_t length);

/* Map report bit/index 0 to Environment and 1..9 to actuator 0x61..0x69. */
uint8_t V851ControlInfoReportStructType(uint8_t report_index);

/* Encode Environment or one actuator's report fields into field_buffer. */
uint16_t V851ControlInfoBuildFields(uint8_t struct_type,
                                    uint8_t *field_buffer,
                                    uint16_t capacity);

/* Return bit 0 for Environment and bits 1..9 for changed actuators. */
uint16_t V851ControlInfoScanChanged(void);

#endif /* v851PROTOCOL_ENABLED */

#endif /* V851_CONTROL_INFO_H */

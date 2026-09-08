#ifndef FW_PROTOCOL_H
#define FW_PROTOCOL_H
#ifndef FW_PROTOCOL_TEST
#include "sys.h"
#include "uart.h"
#endif
#define FW_MIRROR_VP       0x3600
#define FW_WRITE_ADDR_VP   0x3700
#define FW_WRITE_VALUE_VP  0x3701
#define FW_WRITE_TRIGGER_VP 0x3702
#define FW_WRITE_RESULT_VP 0x3703
#define FW_LINK_VP         0x3704
#define FW_ERROR_VP        0x3705
#define USB_VIDEO_VP       0x3780
#define USB_SOFTWARE_VP    0x3781
#define USB_COMPLETE_VP    0x3782
#define USB_VIDEO_GO_VP    0x3783
#define USB_SOFTWARE_GO_VP 0x3784
void FwProtocolInit(void);
void FwProtocolTask(void);
void FwProtocolFeed(uint8_t *bytes, uint16_t length);
void FwUsbProtocol(UART_TYPE *uart, uint8_t *frame, uint16_t length);
#endif

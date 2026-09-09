#ifndef APP_OTA_H
#define APP_OTA_H
#include "sys.h"
#include "uart.h"
#define APP_OTA_AVAILABLE_VP 0x3787U
#define APP_OTA_CONFIRM_VP 0x3788U
void AppOtaInit(void);
void AppOtaTask(void);
uint8_t AppOtaControl(UART_TYPE *uart, uint8_t *frame, uint16_t length);
#endif

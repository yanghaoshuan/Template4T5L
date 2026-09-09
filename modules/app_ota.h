#ifndef APP_OTA_H
#define APP_OTA_H
#include "sys.h"
#include "uart.h"
void AppOtaControl(UART_TYPE *uart, uint8_t *frame, uint16_t length);
#endif

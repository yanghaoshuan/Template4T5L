#ifndef _TA_PROTICAL_H_
#define _TA_PROTICAL_H_

#include "uart.h"
#include "T5LOSConfig.h"
#if uartTA_PROTOCOL_ENABLED
void UartStandardTAProtocal(UART_TYPE *uart, uint8_t *frame, uint16_t len);
#endif /* uartTA_PROTOCOL_ENABLED */
#endif /* _TA_PROTICAL_H_ */
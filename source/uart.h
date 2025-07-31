#ifndef __UART_H__
#define __UART_H__
#include "T5LOSConfig.h"

typedef struct UartxDefine
{
    uint16_t TxHead;
    uint16_t TxTail;

    uint16_t RxHead;
    uint16_t RxTail;


    uint8_t RxTimeout;

    uint8_t RxFlag:2;
    uint8_t TxBusy:1;
}UART_TYPE;


#define  UART_NON_REC    0
#define  UART_RECING     1


#if uartUART2_ENABLED
extern UART_TYPE Uart2;
void Uart2Init(const uint32_t bdt);
#endif

#if uartUART3_ENABLED
extern UART_TYPE Uart3;
void Uart3Init(const uint32_t bdt);
#endif

#if uartUART4_ENABLED
extern UART_TYPE Uart4;
void Uart4Init(const uint32_t bdt);
#endif

#if uartUART5_ENABLED
extern UART_TYPE Uart5;
void Uart5Init(const uint32_t bdt);
#endif

#define UartTimerDecrease()  \
    do { \
        #if uartUART2_ENABLED \
            DECREASE_IF_POSITIVE(Uart2.RxTimeout); \
        #endif \
        #if uartUART3_ENABLED \
            DECREASE_IF_POSITIVE(Uart3.RxTimeout); \
        #endif \
        #if uartUART4_ENABLED \
            DECREASE_IF_POSITIVE(Uart4.RxTimeout); \
        #endif \
        #if uartUART5_ENABLED \
            DECREASE_IF_POSITIVE(Uart5.RxTimeout); \
        #endif \
    } while(0)

void UartInit(void);
void UartSendData(UART_TYPE *uart, uint8_t *buf,uint16_t len);
void UartReadFrame(UART_TYPE *uart);

#endif

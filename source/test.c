#include "test.h"
#include "sys.h"
#include "uart.h"
#if uartMODBUS_PROTOCOL_ENABLED
#include "modbus.h" 
#endif

#if sysTEST_ENABLED
#define TESTUART Uart2
void UartTest(void)
{
    UartReadFrame(&Uart2);
    UartReadFrame(&Uart_R11);
}
#endif /* sysTEST_ENABLED */
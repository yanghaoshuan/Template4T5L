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
    UartReadFrame(&TESTUART);
    SendModbusReadHoldingRegistersFrame(&TESTUART, modbusSLAVE_ADDRESS, 0x0000, 10);
}
#endif /* sysTEST_ENABLED */
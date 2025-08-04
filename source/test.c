#include "test.h"
#include "sys.h"
#include "uart.h"
#if uartMODBUS_PROTOCOL_ENABLED
#include "modbus.h" 
#endif

#define TESTUART Uart2
void UartTest(void)
{
    const uint8_t buf[10] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A};
    UartReadFrame(&TESTUART);
    SendModbusReadHoldingRegistersFrame(&TESTUART, modbusSLAVE_ADDRESS, 0x0000, 10);
}
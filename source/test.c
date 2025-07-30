#include "test.h"
#include "sys.h"
#include "uart.h"
#if uartMODBUS_PROTOCOL_ENABLED
#include "modbus.h" 
#endif

#define TESTUART Uart2
void UartTest()
{
    const uint8_t buf[10] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A};
    UartReadFrame(&TESTUART);
    // UartSendData(&TESTUART, (uint8_t *)&"Hello,World!",sizeof("Hello,World!")-1);
    // delay_ms(100);
    // SendModbusReadCoilsFrame(&TESTUART, 0x01, 0x0000, 10);
    // delay_ms(100);
    // SendModbusReadHoldingRegistersFrame(&TESTUART, 0x01, 0x0000, 10);
    // delay_ms(100);
    // SendModbusReadInputRegistersFrame(&TESTUART, 0x01, 0x0000, 10);
    // delay_ms(100);
    // SendModbusWriteSingleCoilFrame(&TESTUART, 0x01, 0x0000, 0xFF00);
    // delay_ms(100);
    // SendModbusWriteSingleRegisterFrame(&TESTUART, 0x01, 0x0000, 0x1234);
    // delay_ms(100);
    // SendModbusWriteMultipleCoilsFrame(&TESTUART, 0x01, 0x0000, buf, 5);
    // delay_ms(100);
    // SendModbusWriteMultipleRegistersFrame(&TESTUART, 0x01, 0x0000, buf, 5);
    delay_ms(100);
}
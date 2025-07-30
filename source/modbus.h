#ifndef MODBUS_H
#define MODBUS_H

#include "sys.h"
#include "uart.h"

#define modbusSLAVE_ADDRESS 0xc9
#define modbusCMD_READ_COILS 0x01
#define modbusCMD_READ_DISCRETE_INPUTS 0x02
#define modbusCMD_READ_HOLDING_REGISTERS 0x03
#define modbusCMD_READ_INPUT_REGISTERS 0x04
#define modbusCMD_WRITE_SINGLE_COIL 0x05
#define modbusCMD_WRITE_SINGLE_REGISTER 0x06
#define modbusCMD_WRITE_MULTIPLE_COILS 0x0F
#define modbusCMD_WRITE_MULTIPLE_REGISTERS 0x10
#define modbusCMD_READ_EXCEPTION_STATUS 0x07
#define modbusCMD_DIAGNOSTICS 0x08

#define modbusMIN_FRAME_SIZE 5
#define modbusMAX_FRAME_SIZE 255
#define modbusHOLDING_REGISTERS_START 0x5000
#define modbusHOLDING_REGISTERS_END 0x50FF
#define modbusINPUT_REGISTERS_START modbusHOLDING_REGISTERS_START
#define modbusINPUT_REGISTERS_END modbusHOLDING_REGISTERS_END
#define modbusCOILS_START 0x5100
#define modbusCOILS_END 0x51FF


#define SendModbusReadCoilsFrame(uart, modbus_slave_address,modbus_start_address, modbus_register_quantity)     \
    SendModbusGenericFrame(uart, modbus_slave_address, modbusCMD_READ_COILS, modbus_start_address, NULL, modbus_register_quantity)

#define SendModbusReadHoldingRegistersFrame(uart, modbus_slave_address,modbus_start_address, modbus_register_quantity) \
    SendModbusGenericFrame(uart, modbus_slave_address, modbusCMD_READ_HOLDING_REGISTERS, modbus_start_address, NULL, modbus_register_quantity)

#define SendModbusReadInputRegistersFrame(uart, modbus_slave_address,modbus_start_address, modbus_register_quantity) \
    SendModbusGenericFrame(uart, modbus_slave_address, modbusCMD_READ_INPUT_REGISTERS, modbus_start_address, NULL, modbus_register_quantity)

#define SendModbusWriteSingleCoilFrame(uart, modbus_slave_address, modbus_start_address, modbus_value) \
    SendModbusGenericFrame(uart, modbus_slave_address, modbusCMD_WRITE_SINGLE_COIL, modbus_start_address, NULL, modbus_value)

#define SendModbusWriteSingleRegisterFrame(uart, modbus_slave_address, modbus_start_address, modbus_value) \
    SendModbusGenericFrame(uart, modbus_slave_address, modbusCMD_WRITE_SINGLE_REGISTER, modbus_start_address, NULL, modbus_value)

#define SendModbusWriteMultipleCoilsFrame(uart, modbus_slave_address, modbus_start_address, frame, modbus_register_quantity) \
    SendModbusGenericFrame(uart, modbus_slave_address, modbusCMD_WRITE_MULTIPLE_COILS, modbus_start_address, frame, modbus_register_quantity)

#define SendModbusWriteMultipleRegistersFrame(uart, modbus_slave_address, modbus_start_address, frame, modbus_register_quantity) \
    SendModbusGenericFrame(uart, modbus_slave_address, modbusCMD_WRITE_MULTIPLE_REGISTERS, modbus_start_address, frame, modbus_register_quantity)


typedef enum {
    modbusERR_NONE = 0,
    modbusERR_ILLEGAL_FUNCTION,
    modbusERR_ILLEGAL_DATA_ADDRESS,
    modbusERR_ILLEGAL_DATA_VALUE,
    modbusERR_SLAVE_DEVICE_FAILURE,
    modbusERR_ACKNOWLEDGE,
    modbusERR_SLAVE_DEVICE_BUSY,
    modbusERR_MEMORY_PARITY_ERROR,
    modbusERR_GATEWAY_PATH_UNAVAILABLE,
    modbusERR_GATEWAY_TARGET_DEVICE_FAILED_TO_RESPOND,
    modbusERR_CRC_RESULT, 
    modbusERR_SLAVE_ADDRESS,
} ModbusErrorCode;

void UartStandardModbusRTUProtocal(UART_TYPE *uart,uint8_t *frame, uint16_t len);

void SendModbusGenericFrame(UART_TYPE *uart, 
                            uint16_t modbus_slave_address,
                            uint8_t modbus_command, 
                            uint16_t modbus_start_address, 
                            uint8_t *frame, 
                            uint16_t modbus_register_quantity);





















#endif 
#include "modbus.h"

static uint8_t prvModbusCrcCheck(uint8_t *frame, uint16_t len)
{
    uint16_t crc16;
    if(len < modbusMIN_FRAME_SIZE) 
    {
        return 0; 
    }
    
    crc16 = crc_16(frame, len - 2);
    if(crc16 != (frame[len - 2] | (frame[len - 1] << 8)))
    {
        return 0; 
    }
    return 1; 
}


void UartStandardModbusRTUProtocal(UART_TYPE *uart,uint8_t *frame, uint16_t len)
{
    ModbusErrorCode ErrnoFlag = modbusERR_NONE;
    if(prvModbusCrcCheck(frame, len))
    {
        if(frame[0] == modbusSLAVE_ADDRESS)
        {
            if(frame[1] & 0x80) 
            {
                ErrnoFlag = (ModbusErrorCode)frame[2];
            }
            switch(frame[1])
            {
                case modbusCMD_READ_COILS:
                    write_dgus_vp(modbusCOILS_START, &frame[3], frame[2]>> 1);
                    UartSendData(uart, frame, frame[2] + 3);
                    __NOP();
                    break;
                case modbusCMD_READ_DISCRETE_INPUTS:
                    __NOP();
                    break;
                case modbusCMD_READ_HOLDING_REGISTERS:
                    write_dgus_vp(modbusHOLDING_REGISTERS_START, &frame[3], frame[2]>> 1);
                    UartSendData(uart, frame, frame[2] + 3);
                    break;
                case modbusCMD_READ_INPUT_REGISTERS:
                    write_dgus_vp(modbusINPUT_REGISTERS_START, &frame[3], frame[2]>> 1);
                    UartSendData(uart, frame, frame[2] + 3);
                    break;
                case modbusCMD_WRITE_SINGLE_COIL:
                    __NOP();
                    break;
                case modbusCMD_WRITE_SINGLE_REGISTER:
                    __NOP();
                    break;
                case modbusCMD_WRITE_MULTIPLE_COILS:
                    __NOP();
                    break;
                case modbusCMD_WRITE_MULTIPLE_REGISTERS:
                    __NOP();
                    break;
                default:
                    break;
            }
        }
    }
}



void SendModbusGenericFrame(UART_TYPE *uart, 
                            uint16_t modbus_slave_address,
                            uint8_t modbus_command, 
                            uint16_t modbus_start_address, 
                            uint8_t *frame, 
                            uint16_t modbus_register_quantity)
{
    uint8_t modbus_send_frame[modbusMAX_FRAME_SIZE];
    uint16_t crc,i;
    modbus_send_frame[0] = modbus_slave_address;
    modbus_send_frame[1] = modbus_command;
    modbus_send_frame[2] = (modbus_start_address >> 8) & 0xFF; 
    modbus_send_frame[3] = modbus_start_address & 0xFF; 
    modbus_send_frame[4] = (modbus_register_quantity >> 8) & 0xFF; 
    modbus_send_frame[5] = modbus_register_quantity & 0xFF; 
    if(NULL != frame)
    {
        modbus_send_frame[6] = modbus_register_quantity * 2; 
        for(i = 0; i < modbus_register_quantity; i++)
        {
            modbus_send_frame[7 + i * 2] = frame[i * 2]; 
            modbus_send_frame[8 + i * 2] = frame[i * 2 + 1]; 
        }
        crc = crc_16(modbus_send_frame, 7 + modbus_register_quantity * 2);
        modbus_send_frame[7 + modbus_register_quantity * 2] = crc & 0xFF; 
        modbus_send_frame[8 + modbus_register_quantity * 2] = (crc >> 8) & 0xFF; 
        UartSendData(uart, modbus_send_frame, 9 + modbus_register_quantity * 2);
    }else
    {
        crc = crc_16(modbus_send_frame, 6);
        modbus_send_frame[6] = crc & 0xFF; 
        modbus_send_frame[7] = (crc >> 8) & 0xFF; 
        UartSendData(uart, modbus_send_frame, 8);

    }
}

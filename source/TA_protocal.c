#include "TA_protocal.h"
#include "sys.h"
#if uartTA_PROTOCOL_ENABLED
void UartStandardTAProtocal(UART_TYPE *uart, uint8_t *frame, uint16_t len)
{
    uint16_t crc16;
    read_dgus_vp(sysDGUS_SYSTEM_CONFIG,(uint8_t*)&crc16,1);
    crc16 = crc16 & 0x0080;
    if(crc16 != 0)
    {
        crc16 = crc_16(frame,len-6);
        if(crc16 != ((frame[len-6] << 8) | frame[len-5]))
        {
            return; 
        }
    }
    if(frame[1] == 0x70)
    {
        SwitchPageById((uint16_t)frame[2]);
    }
}

#endif /* uartTA_PROTOCOL_ENABLED */
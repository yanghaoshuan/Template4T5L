
/**
 * @file    uart.c
 * @brief   UART通信接口驱动程序实现文件
 * @details 本文件实现了多路UART通信接口的完整功能，包括硬件初始化、
 *          中断处理、数据收发和多协议支持
 * @author  yangming
 * @version 1.0.0
 */

#include "uart.h"
#include "T5LOSConfig.h"
#include <string.h>
#include "sys.h"

#if uartMODBUS_PROTOCOL_ENABLED
#include "modbus.h"
#endif /* uartMODBUS_PROTOCOL_ENABLED */

#if sysBEAUTY_MODE_ENABLED
#include "r11_netskinAnalyze.h"
#if sysSET_FROM_LIB
uint16_t sys_2k_ratio;
uint32_t sysFOSC;
uint32_t sysFCLK;
#endif
#endif /* sysBEAUTY_MODE_ENABLED */

#if uartUART2_ENABLED
UART_TYPE Uart2;
uint8_t Uart2TxBuffer[uartUART2_TXBUF_SIZE+1];
uint8_t Uart2RxBuffer[uartUART2_RXBUF_SIZE+1];
void Uart2Init(const uint32_t bdt)
{
    uint32_t baud;
    memset((uint8_t *)&Uart2, 0, sizeof(UART_TYPE));
    memset((uint8_t *)Uart2TxBuffer, 0, uartUART2_TXBUF_SIZE);
    memset((uint8_t *)Uart2RxBuffer, 0, uartUART2_RXBUF_SIZE);

    MUX_SEL |= 0x40;
    P0MDOUT &= 0xCF;
    P0MDOUT |= 0x10;
    P0      |= 0x30;
    ADCON = 0x80;
    SCON0 = 0x50;
    PCON &= 0x7F;
    PCON |= 0x80;

    #if sysSET_FROM_LIB
    if(sys_2k_ratio)
    {
        PCON |= 0x80;
    }
    #else
    #if sys2K_RATIO
    PCON &= ~0x80;
    #endif /* sys2K_RATIO */
    #endif /* sysSET_FROM_LIB */
    
    if(PCON & 0x80)
    {
        baud = 1024- ( uint16_t)(sysFOSC/32/bdt);
    }else
    {
        baud = 1024 - ( uint16_t)(sysFOSC/64/bdt);
    }
    SREL0H = (baud>>8) & 0xff;
    SREL0L = baud & 0xff;

    #if uartUART2_485_ENABLED
    P0MDOUT |= 0x01;
    TR4 = 0;
    #endif  /* uartUART2_485_ENABLED */

    ES0 = 1;
    EA = 1;
}

/**
 * @brief UART2收发中断服务程序
 * @details 处理UART2的接收和发送中断，管理数据缓冲区和传输状态
 * @note 中断号4，处理RI0和TI0中断标志
 * @note 自动管理RS485方向控制（如果启用）
 */
void Uart2TxRxIsr()   interrupt 4
{
    if ( RI0 == 1 )
    {
        Uart2RxBuffer[Uart2.RxHead++] = SBUF0;
        Uart2.RxHead %= uartUART2_RXBUF_SIZE;
        Uart2.RxFlag = UART_RECING;
        #if uartUART2_TIMEOUT_ENABLED
        Uart2.RxTimeout = uartUART2_TIMEOUTSET;
        #else
        Uart2.RxTimeout = 0;
        #endif /* uartUART2_TIMEOUT_ENABLED */
        RI0 = 0; 
    } 
    if( TI0 == 1 )
    {
		TI0 = 0;
        if (Uart2.TxHead != Uart2.TxTail)
        {
            SBUF0 = Uart2TxBuffer[Uart2.TxTail++];
            Uart2.TxTail %= uartUART2_TXBUF_SIZE;
        }else
        {
            Uart2.TxBusy = 0;
            #if uartUART2_485_ENABLED
            {
                TR4 = 0; 
            }
            #endif /* uartUART2_485_ENABLED */
        } 
    }
}
#endif  /* uartUART2_ENABLED */

#if uartUART3_ENABLED
UART_TYPE Uart3;
uint8_t Uart3TxBuffer[uartUART3_TXBUF_SIZE+1];
uint8_t Uart3RxBuffer[uartUART3_RXBUF_SIZE+1];
void Uart3Init(const uint32_t bdt)
{
    uint32_t baud;
    memset((uint8_t *)&Uart3, 0, sizeof(UART_TYPE));
    memset((uint8_t *)Uart3TxBuffer, 0, uartUART3_TXBUF_SIZE);
    memset((uint8_t *)Uart3RxBuffer, 0, uartUART3_RXBUF_SIZE);

    MUX_SEL |= 0x20;
    P0MDOUT &=~0x80;
    P0MDOUT |= 0x40;
    SCON1 = 0x90;
    SCON1 = 0x90;
    
    baud = 1024- ( uint16_t ) (6451200.0f/bdt);
    SREL1H = (baud>>8) & 0xff;
    SREL1L = baud & 0xff;

    IEN2 |= 0x01;
    EA = 1;
}

/**
 * @brief UART3收发中断服务程序
 * @details 处理UART3的接收和发送中断，管理数据缓冲区和传输状态
 * @note 中断号16，处理SCON1的接收和发送标志位
 */
void Uart3TxRxIsr()   interrupt 16
{
    if ( (SCON1&0x01) == 0x01 )
    {
        Uart3RxBuffer[Uart3.RxHead++] = SBUF1;
        Uart3.RxHead %= uartUART3_RXBUF_SIZE;
        Uart3.RxFlag = UART_RECING;
        #if uartUART3_TIMEOUT_ENABLED
        Uart3.RxTimeout = uartUART3_TIMEOUTSET;
        #else
        Uart3.RxTimeout = 0;
        #endif
        SCON1 &= ~0x01;
        SCON1 &= ~0x01;
    } 
    if( (SCON1&0x02) == 0x02 )
    {
		SCON1 &= ~0x02;
        SCON1 &= ~0x02;
        if (Uart3.TxHead != Uart3.TxTail)
        {
            SBUF1 = Uart3TxBuffer[Uart3.TxTail++];
            Uart3.TxTail %= uartUART3_TXBUF_SIZE;
        }else
        {
            Uart3.TxBusy = 0;
        } 
    }
}
#endif /* uartUART3_ENABLED */

#if uartUART4_ENABLED
UART_TYPE Uart4;
uint8_t Uart4TxBuffer[uartUART4_TXBUF_SIZE+1];
uint8_t Uart4RxBuffer[uartUART4_RXBUF_SIZE+1];
void Uart4Init(const uint32_t bdt)
{
    uint32_t baud;
    memset((uint8_t *)&Uart4, 0, sizeof(UART_TYPE));
    memset((uint8_t *)Uart4TxBuffer, 0, uartUART4_TXBUF_SIZE);
    memset((uint8_t *)Uart4RxBuffer, 0, uartUART4_RXBUF_SIZE);

    P0MDOUT |= 0x01;
    #if uartUART4_485_ENABLED
    TR4 = 0;
    #endif /* uartUART4_485_ENABLED */

    SCON2T=0x80;
    SCON2R=0x80;
    
    #if sysSET_FROM_LIB
    if(sys_2k_ratio)
    {
        baud = (uint16_t)(sysFCLK/16/bdt);
    }else
    {
        baud = (uint16_t)(sysFCLK/8/bdt);
    }
    #else
    #if sys2K_RATIO
    baud = (uint16_t)(sysFCLK/16/bdt);
    #else
    baud = (uint16_t)(sysFCLK/8/bdt);
    #endif /* sys2K_RATIO */
    #endif /* sysSET_FROM_LIB */
    BODE2_DIV_H = (baud>>8) & 0xff;
    BODE2_DIV_L = baud & 0xff;

    ES2T = 1;
    ES2R = 1;
    EA   = 1;
}

/**
 * @brief UART4接收中断服务程序
 * @details 处理UART4的接收中断，管理接收数据缓冲区
 * @note 中断号11，处理SCON2R接收标志位
 */
void Uart4RxIsr()   interrupt 11
{
    if((SCON2R&0x01) == 0x01)
    {
        Uart4RxBuffer[Uart4.RxHead++] = SBUF2_RX;
        Uart4.RxHead %= uartUART4_RXBUF_SIZE;
        Uart4.RxFlag = UART_RECING;
        #if uartUART4_TIMEOUT_ENABLED
        Uart4.RxTimeout = uartUART4_TIMEOUTSET;
        #else
        Uart4.RxTimeout = 0;
        #endif
        SCON2R &= 0xFE;
    } 
}

/**
 * @brief UART4发送中断服务程序
 * @details 处理UART4的发送中断，管理发送数据缓冲区和传输状态
 * @note 中断号10，处理SCON2T发送标志位
 * @note 自动管理RS485方向控制（如果启用）
 */
void Uart4TxIsr()   interrupt 10
{
    if((SCON2T&0x01) == 0x01)
    {
        SCON2T &= 0xFE;
        if (Uart4.TxHead != Uart4.TxTail)
        {
            SBUF2_TX = Uart4TxBuffer[Uart4.TxTail++];
            Uart4.TxTail %= uartUART4_TXBUF_SIZE;
        }else
        {
            Uart4.TxBusy = 0;
            #if uartUART4_485_ENABLED
            {
                TR4 = 0; 
            }
            #endif /* uartUART4_485_ENABLED */
        } 
    }
}
#endif /* uartUART4_ENABLED */

#if uartUART5_ENABLED
UART_TYPE Uart5;
uint8_t Uart5TxBuffer[uartUART5_TXBUF_SIZE+1];
uint8_t Uart5RxBuffer[uartUART5_RXBUF_SIZE+1];
void Uart5Init(const uint32_t bdt)
{
    uint32_t baud;
    memset((uint8_t *)&Uart5, 0, sizeof(UART_TYPE));
    memset((uint8_t *)Uart5TxBuffer, 0, uartUART5_TXBUF_SIZE);
    memset((uint8_t *)Uart5RxBuffer, 0, uartUART5_RXBUF_SIZE);

    #if uartUART5_485_ENABLED
    P0MDOUT |= 0x02;
    TR5 = 0;
    #endif

    SCON3T=0x80;
	SCON3R=0x80;
    
    #if sysSET_FROM_LIB
    if(sys_2k_ratio)
    {
        baud = (uint16_t)(sysFCLK/16/bdt);
    }else
    {
        baud = (uint16_t)(sysFCLK/8/bdt);
    }
    #else
    #if sys2K_RATIO
    baud = (uint16_t)(sysFCLK/16/bdt);
    #else
    baud = (uint16_t)(sysFCLK/8/bdt);
    #endif /* sys2K_RATIO */
    #endif /* sysSET_FROM_LIB */
    BODE3_DIV_H = (baud>>8) & 0xff;
    BODE3_DIV_L = baud & 0xff;

    ES3T = 1;
    ES3R = 1;
    EA   = 1;
}

/**
 * @brief UART5接收中断服务程序
 * @details 处理UART5的接收中断，管理接收数据缓冲区
 * @note 中断号13，处理SCON3R接收标志位
 */
void Uart5RxIsr()   interrupt 13
{
    if((SCON3R&0x01) == 0x01)
    {
        Uart5RxBuffer[Uart5.RxHead++] = SBUF3_RX;
        Uart5.RxHead %= uartUART5_RXBUF_SIZE;
        Uart5.RxFlag = UART_RECING;
        #if uartUART5_TIMEOUT_ENABLED
        Uart5.RxTimeout = uartUART5_TIMEOUTSET;
        #else
        Uart5.RxTimeout = 0;
        #endif
        SCON3R &= 0xFE;
    } 
}

/**
 * @brief UART5发送中断服务程序
 * @details 处理UART5的发送中断，管理发送数据缓冲区和传输状态
 * @note 中断号12，处理SCON3T发送标志位
 * @note 自动管理RS485方向控制（如果启用）
 */
void Uart5TxIsr()   interrupt 12
{
    if((SCON3T&0x01) == 0x01)
    {
        SCON3T &= 0xFE;
        if (Uart5.TxHead != Uart5.TxTail)
        {
            SBUF3_TX = Uart5TxBuffer[Uart5.TxTail++];
            Uart5.TxTail %= uartUART5_TXBUF_SIZE;
        }else
        {
            Uart5.TxBusy = 0;
            #if uartUART5_485_ENABLED
            {
                TR5 = 0; 
            }
            #endif /* uartUART5_485_ENABLED */
        } 
    }
}
#endif /* uartUART5_ENABLED */

void UartInit(void)
{
    #if uartUART2_ENABLED
    Uart2Init(uartUART2_BAUDRATE);
    #endif /* uartUART2_ENABLED */

    #if uartUART3_ENABLED
    Uart3Init(uartUART3_BAUDRATE);
    #endif /* uartUART3_ENABLED */

    #if uartUART4_ENABLED
    Uart4Init(uartUART4_BAUDRATE);
    #endif /* uartUART4_ENABLED */

    #if uartUART5_ENABLED
    Uart5Init(uartUART5_BAUDRATE);
    #endif /* uartUART5_ENABLED */
}


void UartSendData(UART_TYPE *uart, uint8_t *buf, uint16_t len)
{
    uint16_t i;

    for(i=0; i<len; i++)
    {
        #if uartUART2_ENABLED
            if(uart == &Uart2)
            {
                Uart2TxBuffer[uart->TxHead++] = *buf++;
                uart->TxHead %= uartUART2_TXBUF_SIZE;
            }
        #endif

        #if uartUART3_ENABLED
            if(uart == &Uart3)
            {
                Uart3TxBuffer[uart->TxHead++] = *buf++;
                uart->TxHead %= uartUART3_TXBUF_SIZE;
            }
        #endif

        #if uartUART4_ENABLED
            if(uart == &Uart4)
            {
                Uart4TxBuffer[uart->TxHead++] = *buf++;
                uart->TxHead %= uartUART4_TXBUF_SIZE;
            }
        #endif

        #if uartUART5_ENABLED
            if(uart == &Uart5)
            {
                Uart5TxBuffer[uart->TxHead++] = *buf++;
                uart->TxHead %= uartUART5_TXBUF_SIZE;
            }
        #endif
        
    }

    if(uart->TxBusy == 0)
    {
        uart->TxBusy = 1;
        #if uartUART2_ENABLED
        {
            if(uart == &Uart2)
            {
                #if uartUART2_485_ENABLED
                {
                    TR4 = 1; 
                }
                #endif
                TI0 = 1;  
            }
        }
        #endif

        #if uartUART3_ENABLED
        {
            if(uart == &Uart3)
            {
                SCON1 |= 0x02;  
            }
        }
        #endif

        #if uartUART4_ENABLED
        {
            if(uart == &Uart4)
            {
                #if uartUART4_485_ENABLED
                {
                    TR4 = 1; 
                }
                #endif
                SCON2T |= 0x01; 
            }
        }
        #endif

        #if uartUART5_ENABLED
        {
            if(uart == &Uart5)
            {
                #if uartUART5_485_ENABLED
                {
                    TR5 = 1; 
                }
                #endif
                SCON3T |= 0x01; 
            }
        }
        #endif

    }
}



uint8_t prvDwin8283CrcCheck(uint8_t* frame,uint16_t len,uint16_t *CrcFlag)
{
    uint16_t crc16,min_frame_len;
    if(frame[2] == 0x83)
    {
        min_frame_len = 7;
    }else if(frame[2] == 0x82)
    {
        min_frame_len = 6;
    }
    read_dgus_vp(sysDGUS_SYSTEM_CONFIG,(uint8_t*)CrcFlag,1);
    *CrcFlag = *CrcFlag & 0x0080;
    if(*CrcFlag != 0)
    {
        min_frame_len += 2; 
        crc16 = crc_16(&frame[3],frame[2] - 2);
        if(crc16 != ((frame[frame[2] + 2] << 8) | frame[frame[2] + 1]))
        {
            return 0; 
        }
    }
    if(len < min_frame_len)
    {
        return 0; 
    }else
    {
        return 1;
    }
}

/**
 * @brief 标准Dwin8283协议处理函数
 * @details 处理接收到的Dwin8283协议帧，支持读写DGUS变量指针操作
 * @param[in] uart UART通信接口指针
 * @param[in,out] frame 协议帧数据缓冲区指针，会被修改用于响应
 * @param[in] len 帧数据长度
 * @return 无
 * @note 支持0x82写命令和0x83读命令
 * @note 自动处理CRC校验和响应帧生成
 * @note 0x82命令可选择是否返回确认帧
 * @warning frame缓冲区会被修改，调用者需注意
 */
static void UartStandardDwin8283Protocal(UART_TYPE *uart,uint8_t *frame, uint16_t len)
{
    uint16_t i=0,CrcFlag = 0,CrcResult = 0;
    if(frame[0] == 0x5a && frame[1] == 0xa5 && frame[3] == 0x82)
    {
        if(prvDwin8283CrcCheck(frame,len,&CrcFlag) == 0)
        {
            return;
        }
        if(len < frame[2] + 3) 
        {
            return; 
        }
        if(CrcFlag != 0)
        {
            frame[2] -= 2;
        }
        write_dgus_vp((frame[4] << 8) | frame[5], &frame[6], (frame[2] - 3) >> 1);
        #if uartUART_82CMD_RETURN
        i=0;
        frame[i++] = 0x5a;
        frame[i++] = 0xa5;
        frame[i++] = 0x03;
        frame[i++] = 0x82;  
        frame[i++] = 0x4f;
        frame[i++] = 0x4b;
        if(CrcFlag != 0)
        {
            CrcResult = crc_16(&frame[3], frame[2] - 2);
            frame[i++] = (uint8_t)CrcResult;
            frame[i++] = CrcResult >> 8;
        }
        UartSendData(uart, frame, i);
        #endif /* uartUART_82CMD_RETURN */
    }else if(frame[0] == 0x5a && frame[1] == 0xa5 && frame[3] == 0x83)
    {
        if(prvDwin8283CrcCheck(frame,len,&CrcFlag) == 0)
        {
            return;
        }
        if(len < frame[2] + 3) 
        {
            return; 
        }
        read_dgus_vp((frame[4] << 8) | frame[5], &frame[7], frame[6] >> 0);
        frame[i++] = 0x5a;  
        frame[i++] = 0xa5;
        frame[i++] = (frame[6] << 1) + 4;
        frame[i++] = 0x83;
        if(CrcFlag != 0)
        {
            frame[2] += 2;
            CrcResult = crc_16(&frame[3], frame[2] - 2);
            frame[frame[2]+1] = (uint8_t)CrcResult;
            frame[frame[2]+2] = CrcResult >> 8;
        }
        UartSendData(uart, frame, (frame[2]+3));
    }
}


void UartReadFrame(UART_TYPE *uart)
{
    uint8_t frame[uartUART_COMMON_FRAME_SIZE];
    uint16_t i;
    if(uart->RxFlag == UART_NON_REC)
        return;
    if(uart->RxTimeout == 0)
    {
			
        uart->RxFlag = UART_NON_REC;
        SysEnterCritical();
        i=0;
        while(uart->RxHead != uart->RxTail)
        {
            #if uartUART2_ENABLED
            if(uart == &Uart2)
            {
                frame[i++] = Uart2RxBuffer[uart->RxTail++];
                uart->RxTail %= uartUART2_RXBUF_SIZE;
            }
            #endif

            #if uartUART3_ENABLED
            if(uart == &Uart3)
            {
                frame[i++] = Uart3RxBuffer[uart->RxTail++];
                uart->RxTail %= uartUART3_RXBUF_SIZE;
            }
            #endif

            #if uartUART4_ENABLED
            if(uart == &Uart4)
            {
                frame[i++] = Uart4RxBuffer[uart->RxTail++];
                uart->RxTail %= uartUART4_RXBUF_SIZE;
            }
            #endif
        }
        SysExitCritical();
        UartStandardDwin8283Protocal(uart,frame, i); 
        #if uartMODBUS_PROTOCOL_ENABLED
        UartStandardModbusRTUProtocal(uart, frame, i);
        #endif /* uartMODBUS_PROTOCOL_ENABLED */
        #if sysBEAUTY_MODE_ENABLED
        UartR11UserBeautyProtocol(uart, frame, i);
        #endif /* sysBEAUTY_MODE_ENABLED */
    }
}



/**
 * @file    uart.c
 * @brief   UART通信接口驱动程序实现文件
 * @details 本文件实现了多路UART通信接口的完整功能，包括硬件初始化、
 *          中断处理、数据收发和多协议支持
 * @author  yangming
 * @version 1.0.0
 */

#include "uart.h"
#include <string.h>
#include "sys.h"

#if uartMODBUS_PROTOCOL_ENABLED
#include "modbus.h"
#endif /* uartMODBUS_PROTOCOL_ENABLED */

#if uartTA_PROTOCOL_ENABLED
#include "TA_protocal.h"
#endif /* uartTA_PROTOCOL_ENABLED */

#if otaOTA_ENABLED
#include "ota.h"
#endif /* otaOTA_ENABLED */

#if sysSET_FROM_LIB
uint16_t sys_2k_ratio;
uint32_t sysFOSC;
uint32_t sysFCLK;
#endif /* sysSET_FROM_LIB */

#if sysBEAUTY_MODE_ENABLED
#include "r11_netskinAnalyze.h"
#include "r11_common.h"
#endif /* sysBEAUTY_MODE_ENABLED */

#if sysN5CAMERA_MODE_ENABLED
#include "r11_common.h"
#include "r11_n5camera.h"
#endif /* sysN5CAMERA_MODE_ENABLED */

#if sysADVERTISE_MODE_ENABLED
#include "r11_common.h"
#include "r11_advertise.h"
#endif /* sysADVERTISE_MODE_ENABLED */


#if uartUART2_ENABLED
UART_TYPE Uart2;
uint8_t Uart2TxBuffer[uartUART2_TXBUF_SIZE+1];
uint8_t Uart2RxBuffer[uartUART2_RXBUF_SIZE+1];
void Uart2Init(const uint32_t bdt)
{
    uint32_t baud;
    memset((uint8_t *)&Uart2, 0, sizeof(UART_TYPE));
    memset((uint8_t *)Uart2TxBuffer, 0, uartUART2_TXBUF_SIZE);
    memset((uint8_t *)Uart2RxBuffer, 0, uartUART2_RXBUF_SIZE + 1U);

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
    uint16_t rx_head_next;
    uint8_t rx_data;

    if ( RI0 == 1 )
    {
        rx_data = SBUF0;
        rx_head_next = Uart2.RxHead + 1U;
        if(rx_head_next > uartUART2_RXBUF_SIZE)
        {
            rx_head_next = 0U;
        }
        if(rx_head_next != Uart2.RxTail)
        {
            Uart2RxBuffer[Uart2.RxHead] = rx_data;
            Uart2.RxHead = rx_head_next;
        }
        else if(Uart2.RxOverflowCount < 0xFFFFU)
        {
            Uart2.RxOverflowCount++;
        }
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
    memset((uint8_t *)Uart3RxBuffer, 0, uartUART3_RXBUF_SIZE + 1U);

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
    uint16_t rx_head_next;
    uint8_t rx_data;

    if ( (SCON1&0x01) == 0x01 )
    {
        rx_data = SBUF1;
        rx_head_next = Uart3.RxHead + 1U;
        if(rx_head_next > uartUART3_RXBUF_SIZE)
        {
            rx_head_next = 0U;
        }
        if(rx_head_next != Uart3.RxTail)
        {
            Uart3RxBuffer[Uart3.RxHead] = rx_data;
            Uart3.RxHead = rx_head_next;
        }
        else if(Uart3.RxOverflowCount < 0xFFFFU)
        {
            Uart3.RxOverflowCount++;
        }
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
    memset((uint8_t *)Uart4RxBuffer, 0, uartUART4_RXBUF_SIZE + 1U);

    P0MDOUT |= 0x01;
    #if uartUART4_485_ENABLED
    TR4 = 0;
    #endif /* uartUART4_485_ENABLED */

    #if CPU_TYPE==T5F0
    MUX_SEL1 |= 0x10; 
	P0MDOUT |= 0x01; 
	P0MDOUT &= 0xFD; 
    #endif /* CPU_TYPE==T5F0*/
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
    uint16_t rx_head_next;
    uint8_t rx_data;

    if((SCON2R&0x01) == 0x01)
    {
        rx_data = SBUF2_RX;
        rx_head_next = Uart4.RxHead + 1U;
        if(rx_head_next > uartUART4_RXBUF_SIZE)
        {
            rx_head_next = 0U;
        }
        if(rx_head_next != Uart4.RxTail)
        {
            Uart4RxBuffer[Uart4.RxHead] = rx_data;
            Uart4.RxHead = rx_head_next;
        }
        else if(Uart4.RxOverflowCount < 0xFFFFU)
        {
            Uart4.RxOverflowCount++;
        }
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
    memset((uint8_t *)Uart5RxBuffer, 0, uartUART5_RXBUF_SIZE + 1U);

    #if uartUART5_485_ENABLED
    P0MDOUT |= 0x02;
    TR5 = 0;
    #endif

    #if CPU_TYPE==T5F0
    MUX_SEL1 |= 0x20; 
	P0MDOUT |= 0x40; 
	P0MDOUT &= 0x7f; 
    #endif /* CPU_TYPE==T5F0*/
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
    uint16_t rx_head_next;
    uint8_t rx_data;

    if((SCON3R&0x01) == 0x01)
    {
        rx_data = SBUF3_RX;
        rx_head_next = Uart5.RxHead + 1U;
        if(rx_head_next > uartUART5_RXBUF_SIZE)
        {
            rx_head_next = 0U;
        }
        if(rx_head_next != Uart5.RxTail)
        {
            Uart5RxBuffer[Uart5.RxHead] = rx_data;
            Uart5.RxHead = rx_head_next;
        }
        else if(Uart5.RxOverflowCount < 0xFFFFU)
        {
            Uart5.RxOverflowCount++;
        }
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
    if((frame == NULL) || (CrcFlag == NULL) || (len < 4U))
    {
        return 0;
    }

    *CrcFlag = 0;
    if(frame[3] == 0x83)
    {
        if(frame[2] < 4U)
        {
            return 0;
        }
        min_frame_len = 7;
    }else if(frame[3] == 0x82)
    {
        if(frame[2] < 3U)
        {
            return 0;
        }
        min_frame_len = 6;
    }else
    {
        return 0;
    }

    if((len < min_frame_len) || (len < ((uint16_t)frame[2] + 3U)))
    {
        return 0;
    }

    read_dgus_vp(sysDGUS_SYSTEM_CONFIG,(uint8_t*)CrcFlag,1);
    *CrcFlag = *CrcFlag & 0x0080;
    if(*CrcFlag != 0)
    {
        min_frame_len += 2; 
        if(len < min_frame_len)
        {
            return 0;
        }
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
 */
static void UartStandardDwin8283Protocal(UART_TYPE *uart,uint8_t *frame, uint16_t len)
{
    uint16_t i=0,CrcFlag = 0,CrcResult = 0;
    uint8_t send_return_frame[256];
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
        if(frame[2] < 3U)
        {
            return;
        }
        write_dgus_vp((frame[4] << 8) | frame[5], &frame[6], (frame[2] - 3) >> 1);
        #if uartUART_82CMD_RETURN
        i=0;
        send_return_frame[i++] = 0x5a;
        send_return_frame[i++] = 0xa5;
        send_return_frame[i++] = 0x03;
        send_return_frame[i++] = 0x82;  
        send_return_frame[i++] = 0x4f;
        send_return_frame[i++] = 0x4b;
        if(CrcFlag != 0)
        {
            send_return_frame[2] += 2;
            CrcResult = crc_16(&send_return_frame[3], send_return_frame[2] - 2);
            send_return_frame[i++] = (uint8_t)CrcResult;
            send_return_frame[i++] = CrcResult >> 8;
        }
        UartSendData(uart, send_return_frame, i);
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
        if(frame[6] > ((sizeof(send_return_frame) - 9U) / 2U))
        {
            return;
        }
        read_dgus_vp((frame[4] << 8) | frame[5], &send_return_frame[7], frame[6] >> 0);
        i=0;
        send_return_frame[i++] = 0x5a;  
        send_return_frame[i++] = 0xa5;
        send_return_frame[i++] = (frame[6] << 1) + 4;
        send_return_frame[i++] = 0x83;
        send_return_frame[i++] = frame[4];
        send_return_frame[i++] = frame[5];
        send_return_frame[i++] = frame[6];
        if(CrcFlag != 0)
        {
            send_return_frame[2] += 2;
            CrcResult = crc_16(&send_return_frame[3], send_return_frame[2] - 2);
            send_return_frame[send_return_frame[2]+1] = (uint8_t)CrcResult;
            send_return_frame[send_return_frame[2]+2] = CrcResult >> 8;
        }
        UartSendData(uart, send_return_frame, (send_return_frame[2]+3));
    }
}


#define UART_FRAME_DWIN       1U
#define UART_FRAME_AA55       2U
#define UART_FRAME_OTA        3U
#define UART_FRAME_MODBUS     4U
#define UART_FRAME_TA         5U

static uint8_t *UartGetRxBuffer(UART_TYPE *uart, uint16_t *ring_size)
{
    *ring_size = 0U;
    #if uartUART2_ENABLED
    if(uart == &Uart2)
    {
        *ring_size = uartUART2_RXBUF_SIZE + 1U;
        return Uart2RxBuffer;
    }
    #endif /* uartUART2_ENABLED */
    #if uartUART3_ENABLED
    if(uart == &Uart3)
    {
        *ring_size = uartUART3_RXBUF_SIZE + 1U;
        return Uart3RxBuffer;
    }
    #endif /* uartUART3_ENABLED */
    #if uartUART4_ENABLED
    if(uart == &Uart4)
    {
        *ring_size = uartUART4_RXBUF_SIZE + 1U;
        return Uart4RxBuffer;
    }
    #endif /* uartUART4_ENABLED */
    #if uartUART5_ENABLED
    if(uart == &Uart5)
    {
        *ring_size = uartUART5_RXBUF_SIZE + 1U;
        return Uart5RxBuffer;
    }
    #endif /* uartUART5_ENABLED */
    return NULL;
}


static uint8_t UartRxInterruptDisable(UART_TYPE *uart)
{
    uint8_t enabled;

    #if uartUART2_ENABLED
    if(uart == &Uart2)
    {
        enabled = ES0;
        ES0 = 0;
        return enabled;
    }
    #endif /* uartUART2_ENABLED */
    #if uartUART3_ENABLED
    if(uart == &Uart3)
    {
        enabled = IEN2 & 0x01U;
        IEN2 &= 0xFEU;
        return enabled;
    }
    #endif /* uartUART3_ENABLED */
    #if uartUART4_ENABLED
    if(uart == &Uart4)
    {
        enabled = ES2R;
        ES2R = 0;
        return enabled;
    }
    #endif /* uartUART4_ENABLED */
    #if uartUART5_ENABLED
    if(uart == &Uart5)
    {
        enabled = ES3R;
        ES3R = 0;
        return enabled;
    }
    #endif /* uartUART5_ENABLED */
    return 0U;
}


static void UartRxInterruptRestore(UART_TYPE *uart, uint8_t enabled)
{
    if(enabled == 0U)
    {
        return;
    }

    #if uartUART2_ENABLED
    if(uart == &Uart2)
    {
        ES0 = 1;
        return;
    }
    #endif /* uartUART2_ENABLED */
    #if uartUART3_ENABLED
    if(uart == &Uart3)
    {
        IEN2 |= 0x01U;
        return;
    }
    #endif /* uartUART3_ENABLED */
    #if uartUART4_ENABLED
    if(uart == &Uart4)
    {
        ES2R = 1;
        return;
    }
    #endif /* uartUART4_ENABLED */
    #if uartUART5_ENABLED
    if(uart == &Uart5)
    {
        ES3R = 1;
    }
    #endif /* uartUART5_ENABLED */
}


static void UartRxSnapshot(UART_TYPE *uart, uint16_t *head, uint16_t *tail)
{
    uint8_t interrupt_enabled;

    interrupt_enabled = UartRxInterruptDisable(uart);
    *head = uart->RxHead;
    *tail = uart->RxTail;
    uart->RxFlag = (*head == *tail) ? UART_NON_REC : UART_RECING;
    UartRxInterruptRestore(uart, interrupt_enabled);
}


static uint16_t UartRxAvailable(uint16_t head, uint16_t tail, uint16_t ring_size)
{
    if(head >= tail)
    {
        return head - tail;
    }
    return ring_size - tail + head;
}


static uint8_t UartRxPeek(uint8_t *rx_buffer,
                          uint16_t ring_size,
                          uint16_t tail,
                          uint16_t offset)
{
    uint16_t index;

    index = tail + offset;
    while(index >= ring_size)
    {
        index -= ring_size;
    }
    return rx_buffer[index];
}


static void UartRxCopy(uint8_t *rx_buffer,
                       uint16_t ring_size,
                       uint16_t tail,
                       uint8_t *frame,
                       uint16_t len)
{
    uint16_t i;

    for(i = 0U; i < len; i++)
    {
        frame[i] = rx_buffer[tail++];
        if(tail >= ring_size)
        {
            tail = 0U;
        }
    }
}


static void UartRxConsume(UART_TYPE *uart, uint16_t ring_size, uint16_t len)
{
    uint8_t interrupt_enabled;
    uint16_t tail;

    interrupt_enabled = UartRxInterruptDisable(uart);
    tail = uart->RxTail + len;
    while(tail >= ring_size)
    {
        tail -= ring_size;
    }
    uart->RxTail = tail;
    uart->RxFlag = (uart->RxHead == uart->RxTail) ? UART_NON_REC : UART_RECING;
    UartRxInterruptRestore(uart, interrupt_enabled);
}


static void UartDispatchFrame(UART_TYPE *uart,
                              uint8_t *frame,
                              uint16_t frame_len,
                              uint8_t frame_type)
{
    switch(frame_type)
    {
        case UART_FRAME_DWIN:
            UartStandardDwin8283Protocal(uart, frame, frame_len);
            #if sysBEAUTY_MODE_ENABLED
            UartR11UserBeautyProtocol(uart, frame, frame_len);
            #endif /* sysBEAUTY_MODE_ENABLED */
            #if sysN5CAMERA_MODE_ENABLED
            UartR11UserN5CameraProtocol(uart, frame, frame_len);
            #endif /* sysN5CAMERA_MODE_ENABLED */
            break;

        case UART_FRAME_AA55:
            #if R11_WIFI_ENABLED
            UartR11UserWifiProtocol(uart, frame, frame_len);
            #endif /* R11_WIFI_ENABLED */
            #if sysBEAUTY_MODE_ENABLED
            UartR11UserVideoProtocol(uart, frame, frame_len);
            UartR11UserBeautyProtocol(uart, frame, frame_len);
            #endif /* sysBEAUTY_MODE_ENABLED */
            #if sysN5CAMERA_MODE_ENABLED
            UartR11UserVideoProtocol(uart, frame, frame_len);
            UartR11UserN5CameraProtocol(uart, frame, frame_len);
            #endif /* sysN5CAMERA_MODE_ENABLED */
            #if sysADVERTISE_MODE_ENABLED
            UartR11UserVideoProtocol(uart, frame, frame_len);
            UartR11UserAdvertiseProtocol(uart, frame, frame_len);
            #endif /* sysADVERTISE_MODE_ENABLED */
            break;

        #if otaOTA_ENABLED && (sysBEAUTY_MODE_ENABLED || sysN5CAMERA_MODE_ENABLED || sysADVERTISE_MODE_ENABLED)
        case UART_FRAME_OTA:
            if(uart == &Uart_R11)
            {
                OtaReceive(frame, frame_len);
            }
            break;
        #endif /* otaOTA_ENABLED && R11 mode */

        #if uartMODBUS_PROTOCOL_ENABLED
        case UART_FRAME_MODBUS:
            UartStandardModbusRTUProtocal(uart, frame, frame_len);
            break;
        #endif /* uartMODBUS_PROTOCOL_ENABLED */

        #if uartTA_PROTOCOL_ENABLED
        case UART_FRAME_TA:
            UartStandardTAProtocol(uart, frame, frame_len);
            break;
        #endif /* uartTA_PROTOCOL_ENABLED */

        default:
            break;
    }
}


void UartReadFrame(UART_TYPE *uart)
{
    uint8_t frame[uartUART_COMMON_FRAME_SIZE];
    uint8_t *rx_buffer;
    uint8_t frame_type;
    uint16_t ring_size;
    uint16_t rx_head;
    uint16_t rx_tail;
    uint16_t available;
    uint16_t payload_len;
    uint16_t frame_len;
    #if uartTA_PROTOCOL_ENABLED
    uint16_t ta_raw_len;
    uint16_t ta_tail_offset;
    #endif /* uartTA_PROTOCOL_ENABLED */

    if(uart == NULL)
    {
        return;
    }

    rx_buffer = UartGetRxBuffer(uart, &ring_size);
    if((rx_buffer == NULL) || (ring_size == 0U))
    {
        return;
    }

    while(1)
    {
        UartRxSnapshot(uart, &rx_head, &rx_tail);
        available = UartRxAvailable(rx_head, rx_tail, ring_size);
        if(available == 0U)
        {
            return;
        }

        if(available < 2U)
        {
            if(uart->RxTimeout != 0U)
            {
                return;
            }
            UartRxConsume(uart, ring_size, 1U);
            continue;
        }

        frame_type = 0U;
        frame_len = 0U;

        if((UartRxPeek(rx_buffer, ring_size, rx_tail, 0U) == 0x5AU) &&
           (UartRxPeek(rx_buffer, ring_size, rx_tail, 1U) == 0xA5U))
        {
            if(available < 3U)
            {
                if(uart->RxTimeout != 0U)
                {
                    return;
                }
                UartRxConsume(uart, ring_size, 1U);
                continue;
            }
            frame_type = UART_FRAME_DWIN;
            frame_len = (uint16_t)UartRxPeek(rx_buffer, ring_size, rx_tail, 2U) + 3U;
        }
        else if((UartRxPeek(rx_buffer, ring_size, rx_tail, 0U) == 0xAAU) &&
                (UartRxPeek(rx_buffer, ring_size, rx_tail, 1U) == 0x55U))
        {
            if(available < 4U)
            {
                if(uart->RxTimeout != 0U)
                {
                    return;
                }
                UartRxConsume(uart, ring_size, 1U);
                continue;
            }
            payload_len = ((uint16_t)UartRxPeek(rx_buffer, ring_size, rx_tail, 2U) << 8) |
                          UartRxPeek(rx_buffer, ring_size, rx_tail, 3U);
            if(payload_len > (uint16_t)(uartUART_COMMON_FRAME_SIZE - 4U))
            {
                UartRxConsume(uart, ring_size, 1U);
                continue;
            }
            frame_type = UART_FRAME_AA55;
            frame_len = payload_len + 4U;
        }
        #if otaOTA_ENABLED && (sysBEAUTY_MODE_ENABLED || sysN5CAMERA_MODE_ENABLED || sysADVERTISE_MODE_ENABLED)
        else if((UartRxPeek(rx_buffer, ring_size, rx_tail, 0U) == 0xABU) &&
                (UartRxPeek(rx_buffer, ring_size, rx_tail, 1U) == 0xCDU))
        {
            if(available < 4U)
            {
                if(uart->RxTimeout != 0U)
                {
                    return;
                }
                UartRxConsume(uart, ring_size, 1U);
                continue;
            }
            payload_len = ((uint16_t)UartRxPeek(rx_buffer, ring_size, rx_tail, 2U) << 8) |
                          UartRxPeek(rx_buffer, ring_size, rx_tail, 3U);
            if(payload_len > (uint16_t)(uartUART_COMMON_FRAME_SIZE - 4U))
            {
                UartRxConsume(uart, ring_size, 1U);
                continue;
            }
            frame_type = UART_FRAME_OTA;
            frame_len = payload_len + 4U;
        }
        #endif /* otaOTA_ENABLED && R11 mode */
        #if uartMODBUS_PROTOCOL_ENABLED
        else if(UartRxPeek(rx_buffer, ring_size, rx_tail, 0U) == modbusSLAVE_ADDRESS)
        {
            if(available < 3U)
            {
                if(uart->RxTimeout != 0U)
                {
                    return;
                }
                UartRxConsume(uart, ring_size, 1U);
                continue;
            }
            frame_type = UART_FRAME_MODBUS;
            frame_len = (uint16_t)UartRxPeek(rx_buffer, ring_size, rx_tail, 2U) + 5U;
        }
        #endif /* uartMODBUS_PROTOCOL_ENABLED */
        #if uartTA_PROTOCOL_ENABLED
        else if(UartRxPeek(rx_buffer, ring_size, rx_tail, 0U) == 0xAAU)
        {
            if(available < 4U)
            {
                if(uart->RxTimeout != 0U)
                {
                    return;
                }
                UartRxConsume(uart, ring_size, 1U);
                continue;
            }

            ta_raw_len = ((uint16_t)UartRxPeek(rx_buffer, ring_size, rx_tail, 1U) << 8) |
                         UartRxPeek(rx_buffer, ring_size, rx_tail, 2U);
            if(UartRxPeek(rx_buffer, ring_size, rx_tail, 3U) == 0x42U)
            {
                if((ta_raw_len < 2U) ||
                   (ta_raw_len > (uint16_t)(uartUART_COMMON_FRAME_SIZE - 5U)))
                {
                    UartRxConsume(uart, ring_size, 1U);
                    continue;
                }
                frame_len = ta_raw_len + 5U;
            }
            else
            {
                if((ta_raw_len < 4U) ||
                   (ta_raw_len > (uint16_t)(uartUART_COMMON_FRAME_SIZE - 3U)))
                {
                    UartRxConsume(uart, ring_size, 1U);
                    continue;
                }
                frame_len = ta_raw_len + 3U;
            }
            frame_type = UART_FRAME_TA;
        }
        #endif /* uartTA_PROTOCOL_ENABLED */
        else
        {
            UartRxConsume(uart, ring_size, 1U);
            continue;
        }

        if(available < frame_len)
        {
            if(uart->RxTimeout != 0U)
            {
                return;
            }
            UartRxConsume(uart, ring_size, 1U);
            continue;
        }

        UartRxCopy(rx_buffer, ring_size, rx_tail, frame, frame_len);

        #if uartTA_PROTOCOL_ENABLED
        if(frame_type == UART_FRAME_TA)
        {
            ta_tail_offset = frame_len - 4U;
            if((frame[ta_tail_offset] != 0xCCU) ||
               (frame[ta_tail_offset + 1U] != 0x33U) ||
               (frame[ta_tail_offset + 2U] != 0xC3U) ||
               (frame[ta_tail_offset + 3U] != 0x3CU))
            {
                UartRxConsume(uart, ring_size, 1U);
                continue;
            }
        }
        #endif /* uartTA_PROTOCOL_ENABLED */

        /* 帧已复制到本地缓冲，先释放环形缓冲空间，再执行可能较慢的协议处理。 */
        UartRxConsume(uart, ring_size, frame_len);
        UartDispatchFrame(uart, frame, frame_len, frame_type);
    }
}


void UartProtocalHandleTask(void)
{
    UartReadFrame(&Uart2);
    // UartReadFrame(&Uart4);
    #if sysBEAUTY_MODE_ENABLED || sysN5CAMERA_MODE_ENABLED || sysADVERTISE_MODE_ENABLED
    UartReadFrame(&Uart_R11);
    #endif /* sysBEAUTY_MODE_ENABLED || sysN5CAMERA_MODE_ENABLED || sysADVERTISE_MODE_ENABLED */
    #if uartTA_PROTOCOL_ENABLED
    TAProtocolUpload(&Uart2);
    #endif /* uartTA_PROTOCOL_ENABLED */
}


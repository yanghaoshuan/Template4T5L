
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

#if uartUART2_ENABLED && (uartUART2_RXBUF_SIZE > uartUART_COMMON_FRAME_SIZE)
#error "UART2 RX buffer must not exceed the protocol batch buffer"
#endif
#if uartUART3_ENABLED && (uartUART3_RXBUF_SIZE > uartUART_COMMON_FRAME_SIZE)
#error "UART3 RX buffer must not exceed the protocol batch buffer"
#endif
#if uartUART4_ENABLED && (uartUART4_RXBUF_SIZE > uartUART_COMMON_FRAME_SIZE)
#error "UART4 RX buffer must not exceed the protocol batch buffer"
#endif
#if uartUART5_ENABLED && (uartUART5_RXBUF_SIZE > uartUART_COMMON_FRAME_SIZE)
#error "UART5 RX buffer must not exceed the protocol batch buffer"
#endif


#if uartUART2_ENABLED
UART_TYPE Uart2;
uint8_t Uart2TxBuffer[uartUART2_TXBUF_SIZE+1];
volatile uint8_t Uart2RxBuffer[uartUART2_RXBUF_SIZE+1];
void Uart2Init(const uint32_t bdt)
{
    uint32_t baud;
    memset((uint8_t *)&Uart2, 0, sizeof(UART_TYPE));
    memset((uint8_t *)Uart2TxBuffer, 0, uartUART2_TXBUF_SIZE);
    memset((uint8_t *)Uart2RxBuffer, 0, sizeof(Uart2RxBuffer));

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
        uint8_t rx_data;
        uint16_t rx_head_next;

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
        else
        {
            if(Uart2.RxOverflow == 0U)
            {
                Uart2.RxOverflowCount++;
            }
            Uart2.RxOverflow = 1U;
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
volatile uint8_t Uart3RxBuffer[uartUART3_RXBUF_SIZE+1];
void Uart3Init(const uint32_t bdt)
{
    uint32_t baud;
    memset((uint8_t *)&Uart3, 0, sizeof(UART_TYPE));
    memset((uint8_t *)Uart3TxBuffer, 0, uartUART3_TXBUF_SIZE);
    memset((uint8_t *)Uart3RxBuffer, 0, sizeof(Uart3RxBuffer));

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
        uint8_t rx_data;
        uint16_t rx_head_next;

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
        else
        {
            if(Uart3.RxOverflow == 0U)
            {
                Uart3.RxOverflowCount++;
            }
            Uart3.RxOverflow = 1U;
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
volatile uint8_t Uart4RxBuffer[uartUART4_RXBUF_SIZE+1];
void Uart4Init(const uint32_t bdt)
{
    uint32_t baud;
    memset((uint8_t *)&Uart4, 0, sizeof(UART_TYPE));
    memset((uint8_t *)Uart4TxBuffer, 0, uartUART4_TXBUF_SIZE);
    memset((uint8_t *)Uart4RxBuffer, 0, sizeof(Uart4RxBuffer));

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
    if((SCON2R&0x01) == 0x01)
    {
        uint8_t rx_data;
        uint16_t rx_head_next;

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
        else
        {
            if(Uart4.RxOverflow == 0U)
            {
                Uart4.RxOverflowCount++;
            }
            Uart4.RxOverflow = 1U;
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
volatile uint8_t Uart5RxBuffer[uartUART5_RXBUF_SIZE+1];
void Uart5Init(const uint32_t bdt)
{
    uint32_t baud;
    memset((uint8_t *)&Uart5, 0, sizeof(UART_TYPE));
    memset((uint8_t *)Uart5TxBuffer, 0, uartUART5_TXBUF_SIZE);
    memset((uint8_t *)Uart5RxBuffer, 0, sizeof(Uart5RxBuffer));

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
    if((SCON3R&0x01) == 0x01)
    {
        uint8_t rx_data;
        uint16_t rx_head_next;

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
        else
        {
            if(Uart5.RxOverflow == 0U)
            {
                Uart5.RxOverflowCount++;
            }
            Uart5.RxOverflow = 1U;
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


static uint8_t UartIsValidInstance(UART_TYPE *uart)
{
    #if uartUART2_ENABLED
    if(uart == &Uart2)
    {
        return 1U;
    }
    #endif /* uartUART2_ENABLED */

    #if uartUART3_ENABLED
    if(uart == &Uart3)
    {
        return 1U;
    }
    #endif /* uartUART3_ENABLED */

    #if uartUART4_ENABLED
    if(uart == &Uart4)
    {
        return 1U;
    }
    #endif /* uartUART4_ENABLED */

    #if uartUART5_ENABLED
    if(uart == &Uart5)
    {
        return 1U;
    }
    #endif /* uartUART5_ENABLED */

    return 0U;
}


/**
 * @brief 尝试向指定UART发送环形缓冲区加入一个字节
 * @return 1=入队成功，0=缓冲区暂时已满
 * @note 头尾指针是16位共享变量，入队和启动发送必须在同一短临界区完成。
 */
static uint8_t UartTryQueueTxByte(UART_TYPE *uart, uint8_t data_byte)
{
    uint8_t queued;
    uint16_t tx_head_next;

    queued = 0U;
    SysEnterCritical();

    #if uartUART2_ENABLED
    if(uart == &Uart2)
    {
        tx_head_next = uart->TxHead + 1U;
        if(tx_head_next >= uartUART2_TXBUF_SIZE)
        {
            tx_head_next = 0U;
        }
        if(tx_head_next != uart->TxTail)
        {
            Uart2TxBuffer[uart->TxHead] = data_byte;
            uart->TxHead = tx_head_next;
            queued = 1U;
        }
        if((uart->TxBusy == 0U) && (uart->TxHead != uart->TxTail))
        {
            uart->TxBusy = 1U;
            #if uartUART2_485_ENABLED
            TR4 = 1;
            #endif /* uartUART2_485_ENABLED */
            TI0 = 1;
        }
    }
    #endif /* uartUART2_ENABLED */

    #if uartUART3_ENABLED
    if(uart == &Uart3)
    {
        tx_head_next = uart->TxHead + 1U;
        if(tx_head_next >= uartUART3_TXBUF_SIZE)
        {
            tx_head_next = 0U;
        }
        if(tx_head_next != uart->TxTail)
        {
            Uart3TxBuffer[uart->TxHead] = data_byte;
            uart->TxHead = tx_head_next;
            queued = 1U;
        }
        if((uart->TxBusy == 0U) && (uart->TxHead != uart->TxTail))
        {
            uart->TxBusy = 1U;
            SCON1 |= 0x02;
        }
    }
    #endif /* uartUART3_ENABLED */

    #if uartUART4_ENABLED
    if(uart == &Uart4)
    {
        tx_head_next = uart->TxHead + 1U;
        if(tx_head_next >= uartUART4_TXBUF_SIZE)
        {
            tx_head_next = 0U;
        }
        if(tx_head_next != uart->TxTail)
        {
            Uart4TxBuffer[uart->TxHead] = data_byte;
            uart->TxHead = tx_head_next;
            queued = 1U;
        }
        if((uart->TxBusy == 0U) && (uart->TxHead != uart->TxTail))
        {
            uart->TxBusy = 1U;
            #if uartUART4_485_ENABLED
            TR4 = 1;
            #endif /* uartUART4_485_ENABLED */
            SCON2T |= 0x01;
        }
    }
    #endif /* uartUART4_ENABLED */

    #if uartUART5_ENABLED
    if(uart == &Uart5)
    {
        tx_head_next = uart->TxHead + 1U;
        if(tx_head_next >= uartUART5_TXBUF_SIZE)
        {
            tx_head_next = 0U;
        }
        if(tx_head_next != uart->TxTail)
        {
            Uart5TxBuffer[uart->TxHead] = data_byte;
            uart->TxHead = tx_head_next;
            queued = 1U;
        }
        if((uart->TxBusy == 0U) && (uart->TxHead != uart->TxTail))
        {
            uart->TxBusy = 1U;
            #if uartUART5_485_ENABLED
            TR5 = 1;
            #endif /* uartUART5_485_ENABLED */
            SCON3T |= 0x01;
        }
    }
    #endif /* uartUART5_ENABLED */

    SysExitCritical();
    return queued;
}


void UartSendData(UART_TYPE *uart, uint8_t *buf, uint16_t len)
{
    uint16_t i;

    if((uart == NULL) || (buf == NULL) || (len == 0U) ||
       (UartIsValidInstance(uart) == 0U))
    {
        return;
    }

    for(i=0; i<len; i++)
    {
        while(UartTryQueueTxByte(uart, *buf) == 0U)
        {
            /* 保持中断开启，由发送ISR腾出缓冲区空间。 */
        }
        buf++;
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
 * @param[in] frame 协议帧数据缓冲区指针
 * @param[in] len 帧数据长度
 * @return 无
 * @note 支持0x82写命令和0x83读命令
 * @note 自动处理CRC校验和响应帧生成
 * @note 0x82命令可选择是否返回确认帧
 */
static void UartStandardDwin8283Protocal(UART_TYPE *uart,uint8_t *frame, uint16_t len)
{
    uint16_t i=0,CrcFlag = 0,CrcResult = 0;
    uint8_t frame_data_len;
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
        frame_data_len = frame[2];
        if(CrcFlag != 0)
        {
            frame_data_len -= 2U;
        }
        if(frame_data_len < 3U)
        {
            return;
        }
        write_dgus_vp((frame[4] << 8) | frame[5], &frame[6], (frame_data_len - 3U) >> 1);
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


void UartReadFrame(UART_TYPE *uart)
{
    uint8_t frame[uartUART_COMMON_FRAME_SIZE];
    uint8_t rx_overflow;
    uint16_t i,rx_head_bak,rx_tail_bak,one_frame_len,total_frame_len,frame_offset;
    #if uartTA_PROTOCOL_ENABLED
    uint16_t ta_raw_len,ta_frame_len,ta_tail_offset;
    #endif /* uartTA_PROTOCOL_ENABLED */
    if((uart == NULL) || (uart->RxFlag == UART_NON_REC))
        return;

    /**
     * 超时状态、接收头指针和溢出状态必须作为同一个快照读取。
     * 否则新字节可能在“超时判断”和“头指针备份”之间到达，
     * 从而把尚未超时的新批次提前交给协议层。
     */
    SysEnterCritical();
    if((uart->RxFlag == UART_NON_REC) || (uart->RxTimeout != 0U))
    {
        SysExitCritical();
        return;
    }
    rx_head_bak = uart->RxHead;
    rx_tail_bak = uart->RxTail;
    rx_overflow = uart->RxOverflow;
    uart->RxOverflow = 0U;
    uart->RxFlag = UART_NON_REC;
    if(rx_overflow != 0U)
    {
        /* 本批次已有字节丢失，整批丢弃，避免把残帧误判为有效命令。 */
        uart->RxTail = rx_head_bak;
    }
    SysExitCritical();

    if(rx_overflow != 0U)
    {
        return;
    }

    i=0;
    while(rx_head_bak != rx_tail_bak)
    {
        /* 每次仅在复制一个字节时短暂关中断，同时原子发布新的RxTail。 */
        SysEnterCritical();
        #if uartUART2_ENABLED
        if(uart == &Uart2)
        {
            frame[i++] = Uart2RxBuffer[rx_tail_bak++];
            if(rx_tail_bak > uartUART2_RXBUF_SIZE)
            {
                rx_tail_bak = 0U;
            }
        }
        #endif /* uartUART2_ENABLED */

        #if uartUART3_ENABLED
        if(uart == &Uart3)
        {
            frame[i++] = Uart3RxBuffer[rx_tail_bak++];
            if(rx_tail_bak > uartUART3_RXBUF_SIZE)
            {
                rx_tail_bak = 0U;
            }
        }
        #endif /* uartUART3_ENABLED */

        #if uartUART4_ENABLED
        if(uart == &Uart4)
        {
            frame[i++] = Uart4RxBuffer[rx_tail_bak++];
            if(rx_tail_bak > uartUART4_RXBUF_SIZE)
            {
                rx_tail_bak = 0U;
            }
        }
        #endif /* uartUART4_ENABLED */

        #if uartUART5_ENABLED
        if(uart == &Uart5)
        {
            frame[i++] = Uart5RxBuffer[rx_tail_bak++];
            if(rx_tail_bak > uartUART5_RXBUF_SIZE)
            {
                rx_tail_bak = 0U;
            }
        }
        #endif /* uartUART5_ENABLED */

        uart->RxTail = rx_tail_bak;
        SysExitCritical();
    }

    total_frame_len = i;
    while(i > 0)
    {
        frame_offset = total_frame_len - i;
        if(i < 2U)
        {
            break;
        }

        if(frame[frame_offset] == 0x5a && frame[frame_offset + 1] == 0xa5)
        {
            if(i < 3U)
            {
                break;
            }
            one_frame_len = frame[frame_offset + 2] + 3;
            if(i < one_frame_len)
            {
                i--;
                continue;
            }
            UartStandardDwin8283Protocal(uart, &frame[frame_offset], one_frame_len);
                #if sysBEAUTY_MODE_ENABLED
            UartR11UserBeautyProtocol(uart, &frame[frame_offset], one_frame_len);
                #endif /* sysBEAUTY_MODE_ENABLED */
                #if sysN5CAMERA_MODE_ENABLED
            UartR11UserN5CameraProtocol(uart, &frame[frame_offset], one_frame_len);
                #endif /* sysN5CAMERA_MODE_ENABLED */
            i -= one_frame_len;
        }else if(frame[frame_offset] == 0xaa && frame[frame_offset + 1] == 0x55)
        {
            if(i < 4U)
            {
                i--;
                continue;
            }
            one_frame_len = (frame[frame_offset + 2] << 8 | frame[frame_offset + 3]) + 4;
            if(i < one_frame_len)
            {
                i--;
                continue;
            }
                #if R11_WIFI_ENABLED
            UartR11UserWifiProtocol(uart, &frame[frame_offset], one_frame_len);
                #endif /* R11_WIFI_ENABLED */
                #if sysBEAUTY_MODE_ENABLED
            UartR11UserVideoProtocol(uart, &frame[frame_offset], one_frame_len);
            UartR11UserBeautyProtocol(uart, &frame[frame_offset], one_frame_len);
                #endif /* sysBEAUTY_MODE_ENABLED */
                #if sysN5CAMERA_MODE_ENABLED
            UartR11UserVideoProtocol(uart, &frame[frame_offset], one_frame_len);
            UartR11UserN5CameraProtocol(uart, &frame[frame_offset], one_frame_len);
                #endif /* sysN5CAMERA_MODE_ENABLED */
                #if sysADVERTISE_MODE_ENABLED
            UartR11UserVideoProtocol(uart, &frame[frame_offset], one_frame_len);
            UartR11UserAdvertiseProtocol(uart, &frame[frame_offset], one_frame_len);
                #endif /* sysADVERTISE_MODE_ENABLED */
            i -= one_frame_len;
        }
            #if otaOTA_ENABLED && (sysBEAUTY_MODE_ENABLED || sysN5CAMERA_MODE_ENABLED || sysADVERTISE_MODE_ENABLED)
            else if(frame[frame_offset] == 0xAB && frame[frame_offset + 1] == 0xCD)
            {
                /**
                 * @note OTA协议帧只允许从Uart_R11进入，避免普通串口误处理AB CD数据。
                 */
                if(i < 4U)
                {
                    break;
                }
                one_frame_len = (frame[frame_offset + 2] << 8 | frame[frame_offset + 3]) + 4;
                if(i < one_frame_len)
                {
                    i--;
                    continue;
                }
                if(uart == &Uart_R11)
                {
                    OtaReceive(&frame[frame_offset], one_frame_len);
                }
                i -= one_frame_len;
            }
            #endif /* otaOTA_ENABLED && R11 mode */
            #if uartMODBUS_PROTOCOL_ENABLED
            else if(frame[frame_offset] == modbusSLAVE_ADDRESS)
            {
                if(i < 3U)
                {
                    break;
                }
                one_frame_len = frame[frame_offset + 2] + 5;
                if(i < one_frame_len)
                {
                    i--;
                    continue;
                }
                UartStandardModbusRTUProtocal(uart, &frame[frame_offset], one_frame_len);
                i -= one_frame_len;
            }
            #endif /* uartMODBUS_PROTOCOL_ENABLED */
            #if uartTA_PROTOCOL_ENABLED
            else if(frame[frame_offset] == 0xAA)
            {
                /* C51 TA协议：AA lenH lenL cmd payload CC 33 C3 3C */
                if(i < 7U)
                {
                    i--;
                    continue;
                }

                ta_raw_len = ((uint16_t)frame[frame_offset + 1] << 8) | frame[frame_offset + 2];
                if(frame[frame_offset + 3] == 0x42U)
                {
                    /* 0x42写字符串命令沿用C51工程的len+5总长度规则。 */
                    if(ta_raw_len > (uint16_t)(uartUART_COMMON_FRAME_SIZE - 5U))
                    {
                        i--;
                        continue;
                    }
                    ta_frame_len = ta_raw_len + 5U;
                }
                else
                {
                    if(ta_raw_len > (uint16_t)(uartUART_COMMON_FRAME_SIZE - 3U))
                    {
                        i--;
                        continue;
                    }
                    ta_frame_len = ta_raw_len + 3U;
                }

                if(i < ta_frame_len)
                {
                    i--;
                    continue;
                }

                ta_tail_offset = ta_frame_len - 4U;
                /* 帧尾不匹配时只丢弃当前0xAA，继续扫描后续潜在帧头。 */
                if((frame[frame_offset + ta_tail_offset] == 0xCCU) &&
                   (frame[frame_offset + ta_tail_offset + 1U] == 0x33U) &&
                   (frame[frame_offset + ta_tail_offset + 2U] == 0xC3U) &&
                   (frame[frame_offset + ta_tail_offset + 3U] == 0x3CU))
                {
                    UartStandardTAProtocol(uart, &frame[frame_offset], ta_frame_len);
                    i -= ta_frame_len;
                }
                else
                {
                    i--;
                }
            }
            #endif /* uartTA_PROTOCOL_ENABLED */
            else
            {
                if(i>0)
                {
                    i--;
                }else{
                    break;
                }
            }
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


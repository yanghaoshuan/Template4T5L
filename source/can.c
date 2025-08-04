#include "can.h"
#include "gpio.h"
#include "uart.h"
#include <string.h>

#if canCAN_ENABLED

uint8_t can_message_rxhead = 0;  
uint8_t can_message_rxtail = 0; 


void CanInit(void)
{
    MUX_SEL |= 0x80;  
    PDMDOUT |= 0x04;        /**< 设置CAN_TX引脚为输出 */
    P02     =  1;           /**< 设置CAN_TX初始状态为高电平 */
    ADR_H = 0xFF;
	ADR_M = 0x00;
	ADR_L = 0x60;
	ADR_INC = 1;
	RAMMODE = 0x8F;		
	while(!APP_ACK)
        __NOP();
    DATA3 = canBAUD_RATE_250K >> 24;  /**< 设置波特率 */
    DATA2 = (canBAUD_RATE_250K >> 16) & 0xFF;
    DATA1 = (canBAUD_RATE_250K >> 8) & 0xFF;
    DATA0 = canBAUD_RATE_250K & 0xFF;
    APP_EN = 1;  
    while(APP_EN)
        __NOP();
    DATA3 = canCAN_ACR >> 24;  /**< 设置接收滤波器掩码 */
    DATA2 = (canCAN_ACR >> 16) & 0xFF;
    DATA1 = (canCAN_ACR >> 8) & 0xFF;
    DATA0 = canCAN_ACR & 0xFF;
    APP_EN = 1;
    while(APP_EN)
        __NOP();
    DATA3 = canCAN_AMR >> 24;  /**< 设置接收滤波器标识符 */
    DATA2 = (canCAN_AMR >> 16) & 0xFF;
    DATA1 = (canCAN_AMR >> 8) & 0xFF;
    DATA0 = canCAN_AMR & 0xFF;
    APP_EN = 1;
    while(APP_EN)
        __NOP();
    RAMMODE = 0x00;  
    CAN_CR = 0xA0;  
    while(CAN_CR&0x20);
    ECAN = 1;
    can_message_rxhead = 0;  /**< 初始化接收队列头指针 */
    can_message_rxtail = 0;  /**< 初始化接收队列尾指针 */
    EA = 1;
}


/**
 * @brief CAN任务处理函数
 * @details 处理接收到的CAN消息，并执行用户自定义协议处理
 */
void CanTask(void)
{
    CanErrorReset();
    if(can_message_rxtail != can_message_rxhead)
    {
        CanUesrCustonProtocol(&can_message[can_message_rxtail]);
        can_message_rxtail++;
        can_message_rxtail %= CAN_MESSAGE_QUEUE_SIZE; 
    }
}


/**
 * @brief CAN错误复位函数
 */
static void CanErrorReset(void)
{
    EA=0;
	if(CAN_ET&0X20)
	{
		CAN_ET &= 0XDF;
		CAN_CR |= 0X40;
		delay_ms(2);
		CAN_CR &= 0XBF;  
	}
	EA=1;
}


static void CanUesrCustonProtocol(CanMessage *message)
{
    __NOP();  /**< 用户自定义协议处理函数 */
}


void CanIsr(void)      interrupt 9
{
    EA = 0;
    if((CAN_IR&0x80) == 0x80)
    {
        CAN_IR &= ~0x80;  /**< 清除接收中断标志 */
    }
    if((CAN_IR&0x40) == 0x40)
    {
        CAN_IR &= ~0x40;  /**< 清除发送中断标志 */
        //接收数据
        ADR_H = 0xFF;
		ADR_M = 0x00;
		ADR_L = 0x68;
		ADR_INC = 1;
		RAMMODE = 0xAF;
        while(!APP_ACK)
            __NOP();
        APP_EN = 1;  
        while(APP_EN)
            __NOP();
        //第7位为IDE，第6位为RTR，3-0位为数据长度
        can_message[can_message_rxhead].frame_type  = DATA3 & 0x80 ? 1 : 0;  /**< 帧类型：标准帧/扩展帧 */
        can_message[can_message_rxhead].data_type   = DATA3 & 0x40 ? 1 : 0;  /**< 数据类型：数据帧/远程帧 */
        can_message[can_message_rxhead].dlc         = DATA3 & 0x0F;  /**< 数据长度代码（0-8字节） */
        APP_EN = 1;
        while(APP_EN)
            __NOP();
        if(can_message[can_message_rxhead].frame_type == 0)  /**< 标准帧 */
        {
            can_message[can_message_rxhead].id = (DATA3 & 0x1F) << 3;  /**< 标识符（11位标准） */
        }
        else  /**< 扩展帧 */
        {
            can_message[can_message_rxhead].id = ((DATA3 & 0x1F) << 21) | (DATA2 << 13) | (DATA1 << 5) | (DATA0 >> 3);
        }
        APP_EN = 1;
        while(APP_EN)
            __NOP();
        can_message[can_message_rxhead].data[0] = DATA3;  /**< 数据内容，最大8字节 */
        can_message[can_message_rxhead].data[1] = DATA2;
        can_message[can_message_rxhead].data[2] = DATA1;
        can_message[can_message_rxhead].data[3] = DATA0;
        APP_EN = 1;
        while(APP_EN)
            __NOP();
        can_message[can_message_rxhead].data[4] = DATA3;
        can_message[can_message_rxhead].data[5] = DATA2;
        can_message[can_message_rxhead].data[6] = DATA1;
        can_message[can_message_rxhead].data[7] = DATA0;
        RAMMODE = 0;
        can_message_rxhead++;
        can_message_rxhead %= CAN_MESSAGE_QUEUE_SIZE; 
    }
    if((CAN_IR&0x20) == 0x20)
    {
        CAN_IR &= ~0x20;  /**< 清除发送帧标志 */
    }
    if((CAN_IR&0x10) == 0x10)
    {
        CAN_IR &= ~0x10;  /**< 清除接收溢出标志 */
    }
    if((CAN_IR&0x08) == 0x08)
    {
        CAN_IR &= ~0x08;  /**< 清除错误标志 */
    }
    if((CAN_IR&0x04) == 0x04)
    {
        CAN_IR &= ~0x04;  /**< 清除仲裁失败标志 */
        CAN_CR |= 0x04;   /**< 重新开始发送 */
    }

    CAN_ET = 0x00; 
    EA = 1;  /**< 重新使能全局中断 */
}

#endif /* canCAN_ENABLED */
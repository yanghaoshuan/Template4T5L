/**
 * @file    timer.c
 * @brief   定时器驱动程序实现文件
 * @details 本文件实现了系统定时器的完整功能，包括硬件配置、中断处理
 *          和系统时钟节拍维护
 * @author  yangming
 * @date    2025-07-31
 * @version 1.0.0
 */

#include "timer.h"
#include "uart.h"

/**
 * @brief 系统任务定时器节拍计数器定义
 */
uint16_t SysTaskTimerTick = 0;

/**
 * @brief 启动定时器0函数
 * @details 配置并启动定时器0，用于产生系统时钟节拍
 * @param 无
 * @return 无
 * @post 定时器0已启动并开始产生周期性中断
 * @note 启动后会立即使能定时器0中断
 */
static void StartTimer0(void)
{
	TMOD |= 0x01;
	TH0   = timeT0_TICK >> 8;
	TL0   = timeT0_TICK;
	ET0   = 1;
	TR0   = 1;
}

void TimerInit(void)
{
    TCON  = 0x0F; 
    TMOD  = 0x11;
    
	TH0   = 0x00;
	TL0   = 0x00;
	TR0   = 0x00;

    TH1   = 0x00;
    TL1   = 0x00;
    TR1   = 0x00;

    T2CON = 0x70;  
    TH2   = 0x00; 
    TL2   = 0x00; 

    StartTimer0();
}

/**
 * @brief 定时器0中断服务程序
 * @details 处理定时器0中断，维护系统时钟节拍和UART超时计数
 * @note 中断号1，周期性执行系统维护任务
 * @note 自动更新所有启用UART的接收超时计数器
 * @note 维护系统任务调度时钟节拍
 */
void Timer0Isr() interrupt 1
{
    ET0 = 0;
    TH0 = timeT0_TICK >> 8;
    TL0 = timeT0_TICK;

	#if uartUART2_ENABLED
    DECREASE_IF_POSITIVE(Uart2.RxTimeout);
    #endif

    #if uartUART3_ENABLED
    DECREASE_IF_POSITIVE(Uart3.RxTimeout);  
    #endif

    #if uartUART4_ENABLED
    DECREASE_IF_POSITIVE(Uart4.RxTimeout);
    #endif

    #if uartUART5_ENABLED
    DECREASE_IF_POSITIVE(Uart5.RxTimeout);
    #endif

    SysTaskTimerTick++;

    ET0 = 1;
}

/* 定时器1支持 - 条件编译 */
#if timeTIMER1_ENABLED
/**
 * @brief 定时器1中断服务程序
 * @details 处理定时器1中断，执行用户自定义的定时任务
 * @note 中断号3
 */
void Timer1Isr() interrupt 3
{
    ET1 = 0;
    TH1 = timeT1_TICK >> 8;
    TL1 = timeT1_TICK;

    ET1 = 1;
}

/**
 * @brief 定时器1初始化函数
 * @details 配置并启动定时器1，用于用户自定义的定时任务
 * @param 无
 * @return 无
 * @post 定时器1已启动并开始产生周期性中断
 * @note 仅在timeTIMER1_ENABLED宏启用时编译
 */
void Timer1Init(void)
{
    TMOD |= 0x10;  
    TH1 = timeT1_TICK >> 8;
    TL1 = timeT1_TICK;
    ET1 = 1;       
    TR1 = 1;       
}
#endif /* timeTIMER1_ENABLED */
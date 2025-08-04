/**
 * @file    timer.h
 * @brief   定时器驱动程序头文件
 * @details 本文件提供系统定时器的管理和控制功能，包括定时器初始化、
 *          中断处理和系统时钟节拍管理
 * @author  yangming
 * @version 1.0.0
 */

#ifndef __TIMER_H__
#define __TIMER_H__
#include "sys.h"

/* 定时器0TMOD控制寄存器配置 */
#define timeT0_GATE_ENABLE                0x08
#define timeT0_GATE_DISABLE               0x00
#define timeT0_MODE_TIMER                 0x00
#define timeT0_MODE_COUNTER               0x04
#define timeT0_MODE_16BIT                 0x01
#define timeT0_MODE_8BIT                  0x03
#define timeT0_MODE_8BIT_AUTO_RELOAD      0x02
#define timeT0_MODE_13BIT                 0x00

/* 定时器1TMOD控制寄存器配置 */
#define timeT1_GATE_ENABLE                0x80
#define timeT1_GATE_DISABLE               0x00
#define timeT1_MODE_TIMER                 0x00
#define timeT1_MODE_COUNTER               0x40
#define timeT1_MODE_16BIT                 0x10
#define timeT1_MODE_8BIT                  0x30
#define timeT1_MODE_8BIT_AUTO_RELOAD      0x20
#define timeT1_MODE_13BIT                 0x00

#define GetSysTick()                (SysCurrentTick) /**< 获取当前系统滴答计时器值 */

extern uint16_t SysTaskTimerTick;    /**< 系统任务定时器节拍计数器 */

extern uint32_t SysCurrentTick;     /**< 当前系统滴答计时器值 */



/**
 * @brief 定时器模块初始化函数
 * @details 初始化系统中的所有定时器，配置定时器参数和启动必要的定时器
 * @param 无
 * @return 无
 * @post 定时器0已启动并开始产生中断
 * @post 其他定时器已初始化但处于停止状态
 * @note 函数会配置TCON、TMOD等定时器控制寄存器
 * @note 定时器0用于系统节拍，定时器1和定时器2保留备用
 * @note 初始化完成后会自动启动定时器0
 */
void TimerInit(void);

#endif /* __TIMER_H__ */
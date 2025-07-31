/**
 * @file    timer.h
 * @brief   定时器驱动程序头文件
 * @details 本文件提供系统定时器的管理和控制功能，包括定时器初始化、
 *          中断处理和系统时钟节拍管理
 * @author  yangming
 * @date    2025-07-31
 * @version 1.0.0
 */

#ifndef __TIMER_H__
#define __TIMER_H__
#include "sys.h"

/**
 * @brief 系统任务定时器节拍计数器
 * @details 由定时器0中断维护的系统时钟节拍计数，用于任务调度
 * @note 每次定时器0中断都会递增此计数器
 * @note 可用于实现软件定时器和任务调度
 */
extern uint16_t SysTaskTimerTick;

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

#if timeTIMER1_ENABLED
/**
 * @brief 定时器1初始化函数
 * @details 配置并启动定时器1，用于用户自定义的定时任务
 * @param 无
 * @return 无
 * @post 定时器1已启动并开始产生周期性中断
 */
void Timer1Init(void);
#endif

#endif /* __TIMER_H__ */
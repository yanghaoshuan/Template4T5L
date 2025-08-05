/**
 * @file    rtc.h
 * @brief   实时时钟(RTC)驱动程序头文件
 * @details 本文件提供RTC芯片的驱动接口，支持RX-8130和SD-2058两种RTC芯片，
 *          包括时间读写、初始化和周期性任务处理
 * @author  yangming
 * @version 1.0.0
 */

#ifndef RTC_H
#define RTC_H

/* RTC芯片型号选择 - 只能选择其中一种 */
/**
 * @brief 启用RX-8130 RTC芯片支持和SD-2058 RTC芯片支持
 * @details 定义此宏以启用RX-8130 RTC芯片的驱动代码
 * @note rtc芯片互斥，只能选择一种芯片类型
 */
#define rtcRX_8130
//#define rtcSD_2058

#include "sys.h"

#if defined(rtcRX_8130) || defined(rtcSD_2058)
#if !i2cI2C_ENABLED
#error "I2C must be enabled to use RTC driver"
#endif /* !i2cI2C_ENABLED */
#include "i2c.h"
#endif /* defined(rtcRX_8130) || defined(rtcSD_2058) */


/**
 * @brief RTC任务执行间隔时间
 * @details RTC周期性任务的执行间隔，单位为毫秒
 */
#define RTC_INTERVAL    500

/**
 * @brief RTC时间写入处理函数
 * @details 从DGUS显示屏读取时间设置命令，并更新RTC芯片时间
 * @param 无
 * @return 无
 * @post 如果检测到有效的时间设置命令，RTC时间将被更新
 * @note 函数会检查0x009C地址的命令标志
 * @note 自动计算星期值并进行BCD转换
 * @note 处理完成后会清除命令标志
 */
void RtcWriteTime(void);

/**
 * @brief RTC芯片初始化函数
 * @details 初始化RTC芯片硬件，配置I2C接口和芯片参数
 * @param 无
 * @return 无
 * @note 不同芯片有不同的初始化序列
 * @note 会检查芯片的复位状态并进行相应处理
 * @note 如果检测到芯片复位，会设置默认时间
 */
void RtcInit(void);

/**
 * @brief RTC时间设置函数
 * @details 将指定的时间数据写入RTC芯片
 * @param[in] prtc_set 时间数据缓冲区指针，不能为NULL
 *                     格式：[年(20xx), 月(1-12), 日(1-31), 星期(0-6), 时(0-23), 分(0-59), 秒(0-59)]
 * @return 无
 * @pre 时间数据必须在有效范围内
 * @post RTC芯片的时间寄存器已更新
 * @note 函数会根据芯片类型执行不同的写入序列
 * @note 时间格式为BCD码，函数内部会进行转换
 * @note 星期值会根据日期自动计算
 * @warning 设置错误的时间可能导致RTC功能异常
 */
void RtcSetTime(uint8_t *prtc_set);

/**
 * @brief RTC时间读取函数
 * @details 从RTC芯片读取当前时间并写入DGUS显示变量
 * @param 无
 * @return 无
 * @post 当前时间已写入DGUS地址0x0010
 * @note 函数会自动进行BCD到十六进制的转换
 * @note 时间格式：[年, 月, 日, 星期, 时, 分, 秒, 保留]
 * @note 星期值从0开始(0=周日, 1=周一, ..., 6=周六)
 */
void RtcReadTime(void);

/**
 * @brief RTC周期性任务函数
 * @details 执行RTC的周期性维护任务，包括时间读取和写入处理
 * @param 无
 * @return 无
 * @post 完成一次RTC时间同步和命令处理
 * @note 应在主循环中周期性调用，建议间隔RTC_INTERVAL毫秒
 * @note 函数内部调用RtcReadTime()和RtcWriteTime()
 */
void RtcTask(void);

#endif /* RTC_H */
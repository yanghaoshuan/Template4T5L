#ifndef GPIO_H
#define GPIO_H

#include "T5LOSConfig.h"

/**
 * @file    gpio.h
 * @brief   GPIO操作相关函数和宏定义
 * @details 本文件包含GPIO端口的操作函数和宏定义，用于设置GPIO端口的输入输出模式
 *          以及读写GPIO端口的状态
 * @author  [yangming]
 * @date    2025-07-31
 * @version 1.0.0
 */

 #define GPIO_BUTTON_PRESSED  0x00      /**< 按钮按下状态 */
 #define GPIO_BUTTON_RELEASED 0x01      /**< 按钮释放状态 */

typedef enum {
    BUTTON_IDLE = 0,        /**< 按钮空闲状态 */
    BUTTON_PRESSED,         /**< 按钮按下状态 */
    BUTTON_DEBOUNCED,       /**< 按钮去抖动状态 */
    BUTTON_LONG_PRESSED,    /**< 按钮长按状态 */
} ButtonState;

#define GPIO_DEBOUNCE_TIME          20          /**< 按钮去抖动时间，单位为毫秒 */
#define GPIO_LONG_PRESS_BONUS       50      /**< 按钮长按倍数 */
#define GPIO_LONG_PRESS_TIME        GPIO_DEBOUNCE_TIME*GPIO_LONG_PRESS_BONUS      /**< 按钮长按时间，单位为毫秒 */

#define GPIO_STATE(x)   ((x) ? 1 : 0)

/**
 * @brief 获取GPIO端口的状态
 * @param gpio_state: GPIO端口状态,使用GPIO_STATE宏获取
 * @return 按钮状态
 * @retval 按钮状态
 * @note 该函数用于获取按钮的当前状态，并进行去抖动处理
 */
ButtonState GetGpioState(uint16_t gpio_state);


/**
 * @brief GPIO任务函数
 * @details 该函数周期性检查GPIO端口状态，并根据按钮状态发送相应的UART消息
 * @note 需要在系统任务调度中调用
 * @note 运行周期为GPIO_DEBOUNCE_TIME毫秒
 * @note 在函数内定义测试GPIO端口为P15
 */
void GpioTask(void);
#endif /* GPIO_H */
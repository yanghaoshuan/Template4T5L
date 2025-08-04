#ifndef GPIO_H
#define GPIO_H

#include "T5LOSConfig.h"

/**
 * @file    gpio.h
 * @brief   GPIO操作相关函数和宏定义
 * @details 本文件包含GPIO端口的操作函数和宏定义，用于设置GPIO端口的输入输出模式
 *          以及读写GPIO端口的状态
 * @author  yangming
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

/* pwm使用定时器2生成 */
#if gpioPWM_ENABLE && timeTIMER2_ENABLED
#define GPIO_PWM_PORTMDOUT                 P1MDOUT                              /**< PWM端口模式寄存器 */
#define GPIO_PWM_PIN                        0                                   /**< PWM端口定义 */
#define GPIO_PWM_PORT                       P10                                 /**< PWM端口定义 */
#define GPIO_PWM_FREQUENCY                  1000                                /**< PWM频率，单位为Hz */
#define GPIO_PWM_DUTY_CYCLE                 50                                  /**< PWM占空比，单位为百分比 */
#define GPIO_PWM_MAX_DUTY                   100                                 /**< PWM最大占空比，单位为百分比 */
#define GPIO_PWM_MIN_DUTY                   0                                   /**< PWM最小占空比，单位为百分比 */
#define GPIO_PWM_DUTY_TO_VCC(duty)        ((duty) * 3.3f / GPIO_PWM_MAX_DUTY)   /**< 将占空比转换为VCC值 */
#define GPIO_PWM_VCC_TO_DUTY(vcc)        ((vcc) * GPIO_PWM_MAX_DUTY / 3.3f)     /**< 将VCC值转换为占空比 */
#endif /* gpioPWM_ENABLE */

/**
 * @brief 获取GPIO端口的状态
 * @param gpio_state: GPIO端口状态,使用GPIO_STATE宏获取
 * @return 按钮状态
 * @retval 按钮状态
 * @note 该函数用于获取按钮的当前状态，并进行去抖动处理
 */
ButtonState GetGpioState(uint16_t gpio_state);


#if gpioGPIO_ENABLE && timeTIMER2_ENABLED
/* 开始pwm，参数为频率和占空比，需要放在定时器2中开始执行 */
#define StartGpioPwm()   \
    do { \
        GPIO_BIT_SET_OUT(GPIO_PWM_PORTMDOUT, GPIO_PWM_PIN); /* 设置PWM端口为输出模式 */ \
        ET2 = 0; /* 停止定时器2 */ \
        TH2 = (65536UL - sysFOSC / 12 / GPIO_PWM_FREQUENCY) >> 8; /* 设置定时器2重装载值 */ \
        TL2 = (65536UL - sysFOSC / 12 / GPIO_PWM_FREQUENCY) & 0xFF; \
        ET2 = 1; /* 启动定时器2 */ \
    } while(0)

#define StopGpioPwm() \
    do { \
        ET2 = 0; /* 停止定时器2 */ \
        GPIO_BIT_SET_IN(GPIO_PWM_PORTMDOUT, GPIO_PWM_PIN); /* 设置PWM端口为输入模式 */ \
    } while(0)


#endif /* gpioGPIO_ENABLE && timeTIMER2_ENABLED */

#if gpioGPIO_ENABLE
/**
 * @brief GPIO任务函数
 * @details 该函数周期性检查GPIO端口状态，并根据按钮状态发送相应的UART消息
 * @note 需要在系统任务调度中调用
 * @note 运行周期为GPIO_DEBOUNCE_TIME毫秒
 * @note 在函数内定义测试GPIO端口为P15
 */
void GpioTask(void);
#endif /* gpioGPIO_ENABLE */
#endif /* GPIO_H */

/**
 * @file sys.h
 * @brief T5L系统核心功能头文件
 * @details 提供系统任务管理、延时、CRC计算、DGUS通信等核心功能接口
 * @author yangming
 * @version 1.0.0
 */

#ifndef SYS_H
#define SYS_H

#include "T5LOSConfig.h"

/* DGUS系统变量定义 */
#define sysDGUS_PIC_NOW       0x0014   

/**
 * @brief 系统任务定义结构体
 * @details 定义系统任务的基本属性，包括任务ID、执行间隔、计数器和函数指针
 */
typedef struct TaskDefine
{
    uint8_t taskID;                 /**< 任务唯一标识符 */
    uint16_t taskInterval;          /**< 任务执行间隔时间，单位为系统时钟节拍 */
    uint16_t taskCounter;           /**< 任务执行计数器，用于实现定时执行 */
    void (*taskFunction)(void);     /**< 任务执行函数指针 */
} SysTask;

/**
 * @brief 系统任务数组
 * @details 存储所有已注册系统任务的数组，最大容量由sysMAX_TASK_NUM定义
 */
static SysTask SysTasks[sysMAX_TASK_NUM];

/**
 * @brief 当前已注册任务数量
 * @details 记录系统中当前活跃任务的总数
 */
static uint8_t SysTaskCount = 0;

/**
 * @brief 计数任务执行间隔定义
 * @details 定义计数任务的执行周期，单位为毫秒
 */
#define COUNT_TASK_INTERVAL 100

/**
 * @brief 计数任务函数
 * @details 执行计数操作并更新DGUS显示的任务函数
 */
void CountTask(void);

/**
 * @brief 添加系统任务
 * @details 向系统任务调度器中添加一个新的任务
 * @param[in] taskID 任务唯一标识符
 * @param[in] taskInterval 任务执行间隔时间，单位为系统时钟节拍
 * @param[in] taskFunction 任务执行函数指针
 */
void SysTaskAdd(uint8_t taskID, uint16_t taskInterval, void (*taskFunction)(void));

/**
 * @brief 移除系统任务
 * @details 从系统任务调度器中移除指定ID的任务
 * @param[in] taskID 要移除的任务标识符
 */
void SysTaskRemove(uint8_t taskID);

/**
 * @brief 运行系统任务调度器
 * @details 执行任务调度逻辑，检查并运行到期的任务
 */
void SysTaskRun(void);

/**
 * @brief 微秒级延时函数
 * @details 提供精确的微秒级别延时功能
 * @param[in] us 延时时间，单位为微秒
 */
void delay_us(const uint16_t us);

/**
 * @brief 毫秒级延时函数
 * @details 提供毫秒级别延时功能
 * @param[in] ms 延时时间，单位为毫秒
 */
void delay_ms(const uint16_t ms);

/**
 * @brief CRC-16校验计算函数
 * @details 计算数据缓冲区的CRC-16校验值，采用标准多项式0xA001
 * @param[in] pBuf 指向待计算数据缓冲区的指针
 * @param[in] buf_len 数据缓冲区长度
 * @return 计算得到的CRC-16校验值
 */
uint16_t crc_16(uint8_t *pBuf, uint16_t buf_len);

/**
 * @brief 从DGUS读取VP数据
 * @details 从DGUS显示屏的指定VP地址读取数据到缓冲区
 * @param[in] addr VP地址
 * @param[out] buf 接收数据的缓冲区指针
 * @param[in] len 要读取的数据长度
 */
void read_dgus_vp(uint32_t addr, uint8_t *buf, uint8_t len);

/**
 * @brief 向DGUS写入VP数据
 * @details 将缓冲区数据写入到DGUS显示屏的指定VP地址
 * @param[in] addr VP地址
 * @param[in] buf 待写入数据的缓冲区指针
 * @param[in] len 要写入的数据长度
 */
void write_dgus_vp(uint32_t addr, uint8_t *buf, uint8_t len);


/**
 * @brief 读取当前页面ID
 * @details 从DGUS显示屏读取当前显示的页面ID
 * @return 当前页面ID
 */
uint16_t ReadPageId(void);


/**
 * @brief 切换到指定页面ID
 * @details 向DGUS发送命令切换到指定的页面ID
 * @param[in] page_id 要切换到的页面ID
 * @return 无
 */
void SwitchPageById(uint16_t page_id);


#if sysDGUS_AUTO_UPLOAD_ENABLED
void DgusAutoUpload(void);
#endif /* sysDGUS_AUTO_UPLOAD_ENABLED */

/**
 * @brief T5L CPU初始化函数
 * @details 完成T5L芯片的基本初始化，包括内核、GPIO、UART、定时器等模块
 */
void T5LCpuInit(void);

#endif /* SYS_H */
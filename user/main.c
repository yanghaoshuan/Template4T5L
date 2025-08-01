#include "sys.h"
#include "uart.h"
#include "timer.h"

#if uartMODBUS_PROTOCOL_ENABLED
#include "modbus.h" 
#endif /* uartMODBUS_PROTOCOL_ENABLED */

#if sysTEST_ENABLED
#include "test.h"
#endif /* sysTEST_ENABLED */

#include "rtc.h"

#if gpioGPIO_ENABLE
#include "gpio.h"
#endif /* gpioGPIO_ENABLE */

#define MAX_MONITORED_PAGES 2
/**
 * @brief 页面状态结构体
 * @details 用于维护每个页面的独立状态信息
 */
typedef struct {
    uint16_t last_exit_page_id;     /**< 最后离开的页面ID */
    uint16_t last_target_page_id;   /**< 上次的目标页面ID */
} PageState;

#define dgusMAX_MONITORED_PAGES 2
/* 将里面的数据全部初始化为UINT16_PORT_MAX */
static PageState page_states[dgusMAX_MONITORED_PAGES] = UINT16_PORT_MAX;

/**
 * @brief DGUS页面操作函数
 * @details 根据当前页面执行不同的操作，使用独立的页面状态避免状态混乱
 * @param[in] target_page_id 目标页面ID
 * @param[in] state 页面状态指针
 * @note 该函数会根据当前页面是否是目标页面ID执行不同的操作
 */
static void DgusPageAction(uint16_t target_page_id,PageState *state)
{
  
  uint16_t current_page_id;
  current_page_id = ReadPageId();

  if(current_page_id == target_page_id)
  {
    if(state->last_target_page_id != target_page_id)
    {
        state->last_target_page_id = target_page_id;
        /* 执行第一次进入页面的操作 */
        UartSendData(&Uart2,(uint8_t *)&target_page_id, sizeof(target_page_id));
        UartSendData(&Uart2, (uint8_t *)"Page First Entered", sizeof("Page First Entered") - 1);
    }else
    {
        /* 停留在目标页面，执行页面内操作 */
        UartSendData(&Uart2,(uint8_t *)&target_page_id, sizeof(target_page_id));
        UartSendData(&Uart2, (uint8_t *)"Page Repeatedly Entered", sizeof("Page Repeatedly Entered") - 1);
    }
  }else
  {
    if(state->last_exit_page_id == target_page_id)
    {
        /* 离开目标页面，执行离开页面的操作 */
        UartSendData(&Uart2,(uint8_t *)&target_page_id, sizeof(target_page_id));
        UartSendData(&Uart2, (uint8_t *)"Page Exited", sizeof("Page Exited") - 1);
    }
    state->last_exit_page_id = current_page_id;
  }
}


/**
 * @brief DGUS页面扫描任务
 * @details 定期扫描DGUS显示屏的页面ID并进行处理
 * @note 该任务会在系统任务调度中周期性执行，建议执行周期30ms
 * @note 该任务会根据当前页面是否是目标页面ID执行不同的操作
 * @warning 定义的页面切换数量不能超过dgusMAX_MONITORED_PAGES
 */
static void DgusPageScanTask(void)
{
  
  uint16_t page_state_location=0;
  DgusPageAction(1,&page_states[++page_state_location]); 
  DgusPageAction(0,&page_states[++page_state_location]); 
}


/**
 * @brief DGUS键值扫描任务
 * @details 定期扫描DGUS显示屏的键值并进行处理
 * @note 根据对应地址的对应键值进行操作
 * @note 该任务会在系统任务调度中周期性执行，建议执行周期30ms
 * 
 */
static void DgusValueScanTask(void)
{
  #define UINT16_PORT_ZERO     (uint16_t)0
  #define DGUS_SCAN_ADDRESS     (uint32_t)0x1000
  const uint16_t uint16_port_zero = 0;
  uint16_t dgus_value;
  
  #if sysDGUS_AUTO_UPLOAD_ENABLED
  DgusAutoUpload();
  #endif /* sysDGUS_AUTO_UPLOAD_ENABLED */

  read_dgus_vp(DGUS_SCAN_ADDRESS, (uint8_t *)&dgus_value, 1);
  if(dgus_value == 0x0001)
  {
    SwitchPageById(1);
    write_dgus_vp(DGUS_SCAN_ADDRESS, (uint8_t *)&uint16_port_zero, 1);
  }else if(dgus_value == 0x0002)
  {
    SysTaskRemove(1);
    write_dgus_vp(DGUS_SCAN_ADDRESS, (uint8_t *)&uint16_port_zero, 1);
  }else if(dgus_value == 0x0003)
  {
    SysTaskAdd(1, COUNT_TASK_INTERVAL, CountTask);
    write_dgus_vp(DGUS_SCAN_ADDRESS, (uint8_t *)&uint16_port_zero, 1);
  }
}


void main(void)
{
  T5LCpuInit();

  RtcInit();
  SysTaskAdd(0, RTC_INTERVAL, RtcTask);

  SysTaskAdd(1, COUNT_TASK_INTERVAL, CountTask);

  #if sysTEST_ENABLED
  SysTaskAdd(2,1000,UartTest);
  #endif /* sysTEST_ENABLED */

  #if gpioGPIO_ENABLE
  SysTaskAdd(3, GPIO_DEBOUNCE_TIME, GpioTask);
  #endif /* gpioGPIO_ENABLE */

  SysTaskAdd(4, 30, DgusValueScanTask);

  SysTaskAdd(5, 30, DgusPageScanTask);

  while(1)
  {

    SysTaskRun();

  }
    
}
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


void main(void)
{
  T5LCpuInit();

  #if flashDUAL_BACKUP_ENABLED
  T5lNorFlashInit();
  #endif /* flashDUAL_BACKUP_ENABLED */
  
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

  SysTaskAdd(6, sysDGUS_ADC_INTERVAL, AdcTask);

  while(1)
  {

    SysTaskRun();

  }
    
}
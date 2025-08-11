#include "sys.h"
#include "uart.h"
#include "timer.h"
#include "core_json.h"

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

char buffer[] = "{\"foo\":\"abc\",\"bar\":{\"foo\":\"xyz\"}}";
void main(void)
{
  uint16_t bufferLength = sizeof( buffer ) - 1;
  JSONStatus_t result;
  char query[] = "bar.foo";
  size_t queryLength = sizeof( query ) - 1;
  char value[32];
  size_t valueLength = sizeof(value) - 1;
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

  result = JSON_Validate( buffer, bufferLength );
  UartSendData(&Uart2, (uint8_t *)&result, sizeof(result));

  result = JSON_Search( buffer, bufferLength, query, queryLength, &(&value[0]), &valueLength );
  UartSendData(&Uart2, (uint8_t *)&result, sizeof(result));
  while(1)
  {

    SysTaskRun();
    UartReadFrame(&Uart2);
    UartReadFrame(&Uart4);

  }
    
}
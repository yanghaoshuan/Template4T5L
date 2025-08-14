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

#if sysBEAUTY_MODE_ENABLED
#include "r11_common.h"
#include "r11_netskinAnalyze.h"
#endif /* sysBEAUTY_MODE_ENABLED */

// char buffer[] = "{\"foo\":\"abc\",\"bar\":{\"foo\":\"xyz\"}}";
void main(void)
{
//  uint16_t bufferLength = sizeof( buffer ) - 1;
//  JSONStatus_t result;
//  char query[] = "bar.foo";
//  json_size_t queryLength = sizeof( query ) - 1;
//  char value[32];
//  json_size_t valueLength = sizeof(value) - 1;

  #if flashDUAL_BACKUP_ENABLED
  T5lNorFlashInit();
  #endif /* flashDUAL_BACKUP_ENABLED */

  T5LCpuInit();


  RtcInit();
  SysTaskAdd(0, RTC_INTERVAL, RtcTask);

  SysTaskAdd(1, COUNT_TASK_INTERVAL, CountTask);

  #if sysTEST_ENABLED
  SysTaskAdd(2,2,UartTest);
  #endif /* sysTEST_ENABLED */


  #if sysBEAUTY_MODE_ENABLED
  SysTaskAdd(3, R11_TASK_INTERVAL, R11NetskinAnalyzeTask);
  #endif /* sysBEAUTY_MODE_ENABLED */
  
  while(1)
  {
	/** 这个任务需要在主循环中运行，用来进行数据出错后的处理 */
	#if sysBEAUTY_MODE_ENABLED
	if(data_write_f > 2)
	{
	    EX0 = 0;
	    EX1 = 0;
		data_write_f = 0;
	    EX1_Start();
	}
	#endif /* sysBEAUTY_MODE_ENABLED */
    SysTaskRun();
	Display_Debug_Message();
  }
    
}
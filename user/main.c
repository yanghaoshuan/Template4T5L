#include "sys.h"
#include "uart.h"

#if uartMODBUS_PROTOCOL_ENABLED
#include "modbus.h" 
#endif

#if sysTEST_ENABLED
#include "test.h"
#endif

#include "rtc.h"

void main(void)
{
  T5LCpuInit();

  RtcInit();
  SysTaskAdd(0, RTC_INTERVAL, RtcTask);
  SysTaskAdd(1, COUNT_TASK_INTERVAL, CountTask);
  SysTaskAdd(2,2,UartTest);
  while(1)
  {

    SysTaskRun();

  }
    
}
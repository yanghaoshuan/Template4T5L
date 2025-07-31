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
  TaskAdd(0, RTC_INTERVAL, RtcTask);
  TaskAdd(1, COUNT_TASK_INTERVAL, CountTask);
  while(1)
  {

    TaskRun();

  }
    
}
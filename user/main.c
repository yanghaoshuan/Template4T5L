#include "sys.h"
#include "uart.h"

#if uartMODBUS_PROTOCOL_ENABLED
#include "modbus.h" 
#endif

#if sysTEST_ENABLED
#include "test.h"
#endif

void main(void)
{
  T5LCpuInit();

  while(1)
  {
    UartTest();
  }
    
}
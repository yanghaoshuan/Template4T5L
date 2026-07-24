#include "sys.h"
#include "uart.h"
#include "timer.h"
#include "core_json.h"

#if bleV851_BRIDGE_ENABLED
#include "pb03f_ble.h"
#include "v851_control_info.h"
#if v851CONTROL_MOCK_ENABLED
#include "v851_control_mock.h"
#endif /* v851CONTROL_MOCK_ENABLED */
#include "v851_protocol.h"
#endif /* bleV851_BRIDGE_ENABLED */

#if otaOTA_ENABLED
#include "ota.h"
#endif /* otaOTA_ENABLED */

#if uartMODBUS_PROTOCOL_ENABLED
#include "modbus.h" 
#endif /* uartMODBUS_PROTOCOL_ENABLED */


#if _4G_AIR780E_ENABLED
#include "4G_air780e.h"
#endif

#include "rtc.h"

#if gpioGPIO_ENABLE
#include "gpio.h"
#endif /* gpioGPIO_ENABLE */

#if sysBEAUTY_MODE_ENABLED
#include "r11_common.h"
#include "r11_netskinAnalyze.h"
#endif /* sysBEAUTY_MODE_ENABLED */

#if sysN5CAMERA_MODE_ENABLED
#include "r11_common.h"
#include "r11_n5camera.h"
#endif /* sysN5CAMERA_MODE_ENABLED */

#if sysADVERTISE_MODE_ENABLED
#include "r11_common.h"
#include "r11_advertise.h"
#endif /* sysADVERTISE_MODE_ENABLED */

void main(void)
{

	#if flashDUAL_BACKUP_ENABLED
	T5lNorFlashInit();
	#endif /* flashDUAL_BACKUP_ENABLED */


	T5LCpuInit();


	#if bleV851_BRIDGE_ENABLED
	V851ProtocolInit();
	Pb03fBleInit();
	(void)V851ControlInfoDgusInit();
	#endif /* bleV851_BRIDGE_ENABLED */

	RtcInit();
	SysTaskAdd(0, RTC_INTERVAL, RtcTask);

	SysTaskAdd(2, UART_TASK_INTERVAL, UartProtocalHandleTask);

	#if bleV851_BRIDGE_ENABLED
	SysTaskAdd(3, PB03F_BLE_TASK_INTERVAL, Pb03fBleTask);
	#endif /* bleV851_BRIDGE_ENABLED */

	#if bleV851_BRIDGE_ENABLED
	SysTaskAdd(5, V851_PROTOCOL_TASK_INTERVAL, V851ProtocolTask);
	#endif /* bleV851_BRIDGE_ENABLED */

	#if bleV851_BRIDGE_ENABLED && v851CONTROL_MOCK_ENABLED
	SysTaskAdd(6, V851_CONTROL_MOCK_TASK_INTERVAL, V851ControlMockTask);
	#endif /* bleV851_BRIDGE_ENABLED && v851CONTROL_MOCK_ENABLED */

	while(1)
	{
		SysTaskRun();
	}
}

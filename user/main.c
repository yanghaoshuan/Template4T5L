#include "sys.h"
#include "uart.h"
#include "timer.h"

#if pb03fBLE_ENABLED
#include "pb03f_ble.h"
#endif /* pb03fBLE_ENABLED */

#if v851PROTOCOL_ENABLED
#include "v851_protocol.h"
#include "v851_wifi.h"
#endif /* v851PROTOCOL_ENABLED */

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


	#if otaOTA_ENABLED
	OtaInit();
	OtaDataInit();
	#endif /* otaOTA_ENABLED */

	#if v851PROTOCOL_ENABLED
	V851ProtocolInit();
	V851WifiInit();
	#endif /* v851PROTOCOL_ENABLED */

	#if pb03fBLE_ENABLED
	Pb03fBleInit();
	#endif /* pb03fBLE_ENABLED */

	RtcInit();
	SysTaskAdd(0, RTC_INTERVAL, RtcTask);

	SysTaskAdd(2, UART_TASK_INTERVAL, UartProtocalHandleTask);

	#if v851PROTOCOL_ENABLED
	SysTaskAdd(3, V851_WIFI_TASK_INTERVAL, V851WifiTask);
	#endif /* v851PROTOCOL_ENABLED */

	#if pb03fBLE_ENABLED
	SysTaskAdd(4, PB03F_BLE_TASK_INTERVAL, Pb03fBleTask);
	#endif /* pb03fBLE_ENABLED */

	#if v851PROTOCOL_ENABLED
	SysTaskAdd(5, V851_PROTOCOL_TASK_INTERVAL, V851ProtocolTask);
	#endif /* v851PROTOCOL_ENABLED */

	#if otaOTA_ENABLED
	SysTaskAdd(6, otaTASK_INTERVAL, OtaTask);
	#endif /* otaOTA_ENABLED */

	while(1)
	{
		SysTaskRun();
	}
}

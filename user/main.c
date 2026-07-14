#include "sys.h"
#include "uart.h"
#include "timer.h"
#include "core_json.h"

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

	#if sysSET_FROM_LIB
	R11ConfigInitFormLib();
	#endif /* sysSET_FROM_LIB */

	T5LCpuInit();

	RtcInit();
	SysTaskAdd(0, RTC_INTERVAL, RtcTask);

	#if otaOTA_ENABLED
	/**
	 * @note OTA初始化依赖DGUS变量空间和Uart_R11，需在T5LCpuInit之后执行。
	 */
	OtaDataInit();
	OtaInit();
	SysTaskAdd(4, otaTASK_INTERVAL, OtaTask);
	#endif /* otaOTA_ENABLED */

	SysTaskAdd(1, COUNT_TASK_INTERVAL, CountTask);

	SysTaskAdd(2, UART_TASK_INTERVAL, UartProtocalHandleTask);

	#if _4G_AIR780E_ENABLED
	SysTaskAdd(6, AIR780E_TASK_INTERVAL, Air780E_Task);
	#endif

	#if sysBEAUTY_MODE_ENABLED
	SysTaskAdd(3, R11_TASK_INTERVAL, R11NetskinAnalyzeTask);
	#endif /* sysBEAUTY_MODE_ENABLED */

	#if sysN5CAMERA_MODE_ENABLED
	SysTaskAdd(3, R11_TASK_INTERVAL, R11N5CameraTask);
	#endif /* sysN5CAMERA_MODE_ENABLED */

	#if sysADVERTISE_MODE_ENABLED
	SysTaskAdd(3, R11_TASK_INTERVAL, R11AdvertiseTask);
	#endif /* sysADVERTISE_MODE_ENABLED */

	while(1)
	{
	/** 这个任务需要在主循环中运行，用来进行数据出错后的处理 */
	#if sysBEAUTY_MODE_ENABLED || sysN5CAMERA_MODE_ENABLED || sysADVERTISE_MODE_ENABLED
	if(data_write_f > 2)
	{
		EX0 = 0;
		EX1 = 0;
		data_write_f = 0;
		EX1_Start();
	}
	#endif /* sysBEAUTY_MODE_ENABLED||sysN5CAMERA_MODE_ENABLED || sysADVERTISE_MODE_ENABLED */
	SysTaskRun();
	}
}

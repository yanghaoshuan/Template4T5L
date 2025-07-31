#include "i2c.h"
#include "sys.h"

#define RTC_INTERVAL    500

#define rtcRX_8130
//#define rtcSD_2058

void RtcWriteTime(void);
void RtcInit(void);
void RtcSetTime(uint8_t *prtc_set);
void RtcReadTime(void);

void RtcTask(void);
#include "app_ota.h"
#include "timer.h"

static uint8_t pending;
static uint32_t notified_at;
static uint32_t AppOtaNow(void)
{
    uint32_t now;uint8_t enabled=ET0;
    ET0=0;now=GetSysTick();ET0=enabled;return now;
}
static void AppOtaSet(uint16_t vp,uint16_t value)
{
    uint8_t b[2];b[0]=value>>8;b[1]=value;write_dgus_vp(vp,b,1);
}
void AppOtaInit(void)
{
    pending=0;
    AppOtaSet(APP_OTA_AVAILABLE_VP,0);
    AppOtaSet(APP_OTA_CONFIRM_VP,0);
}
/* This command announces material; only the local UI may approve the reset. */
uint8_t AppOtaControl(UART_TYPE *uart, uint8_t *frame, uint16_t length)
{
#if sysBEAUTY_MODE_ENABLED || sysN5CAMERA_MODE_ENABLED || sysADVERTISE_MODE_ENABLED
    if(uart != &Uart_R11 || !frame || length != 10 || frame[0] != 0x5A || frame[1] != 0xA5 ||
       frame[2] != 7 || frame[3] != 0x82 || frame[4] != 0 || frame[5] != 0x20 ||
       frame[6] != 0x5A || frame[7] != 0xA5 || frame[8] != 0x5A || frame[9] != 0xA5) return 0;
    if(!pending)AppOtaSet(APP_OTA_CONFIRM_VP,0);
    pending=1;notified_at=AppOtaNow();
    AppOtaSet(APP_OTA_AVAILABLE_VP,1);
    return 1;
#else
    (void)uart;(void)frame;(void)length;return 0;
#endif
}
void AppOtaTask(void)
{
#if sysBEAUTY_MODE_ENABLED || sysN5CAMERA_MODE_ENABLED || sysADVERTISE_MODE_ENABLED
    uint8_t trigger[2],control[4];
    uint8_t upgrade[4]={0x5A,0xA5,0x5A,0xA5};
    uint8_t reset[4]={0x55,0xAA,0x5A,0xA5};
    if(pending && AppOtaNow()-notified_at>=5000UL)AppOtaInit();
    read_dgus_vp(APP_OTA_CONFIRM_VP,trigger,1);
    if(trigger[0] || trigger[1]) {
        AppOtaSet(APP_OTA_CONFIRM_VP,0);
        if(!pending || trigger[0]!=0 || trigger[1]!=1)return;
        AppOtaInit();
        write_dgus_vp(0x0020,upgrade,2);
        DgusToFlash(flashMAIN_BLOCK_ORDER,0x0020,0x0020,2);
        FlashToDgusWithData(flashMAIN_BLOCK_ORDER,0x0020,0x0020,control,2);
        if(control[0]!=0x5A || control[1]!=0xA5 || control[2]!=0x5A || control[3]!=0xA5)return;
        write_dgus_vp(0x0004,reset,2);
    }
#endif
}

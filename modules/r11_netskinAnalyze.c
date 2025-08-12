#include "r11_netskinAnalyze.h"



#if sysBEAUTY_MODE_ENABLED
CAMERA_MA camera_magnifier;
ENLARGE_P enlarge_mode,net_enlarge_mode,enl_enlarge_mode;
WECHAT_JSON Json_Wechat;
ADDR_S addr_st;
THUMBNAIL_S thumbnail;
MAINVIEW_S mainview;
PAGE_S page_st;
SCREEN_S screen_opt;
WIFI_PAGE_S wifi_page;
CAMERA_PROCESS_STATE camera_process_state = CAMERA_INSERT_DETECT;
NET_CONNECTED_STATE net_connected_state = NET_NOT_CONNECTED;
uint16_t r11_restart_flag = UINT16_PORT_MAX;
uint16_t r11_pic_capture_flag = 0; /* 摄像头拍照标志，0为未拍照，1为拍照成功 */
uint8_t r11_now_choose_pic = 0;

static void R11ConfigInitFormLib(void)
{

}


static void MagnifierKeyHandle(uint16_t key)
{
    if(key == 0xA501)
    {

    }
}

static void R11ValueScanTask(void)
{
    #define R11_SCAN_ADDRESS     (uint32_t)0x0600
    const uint16_t uint16_port_zero = 0;
    uint16_t dgus_value;
  
    #if sysDGUS_AUTO_UPLOAD_ENABLED
    DgusAutoUpload();
    #endif /* sysDGUS_AUTO_UPLOAD_ENABLED */

    read_dgus_vp(R11_SCAN_ADDRESS, (uint8_t *)&dgus_value, 1);
    if((dgus_value>>8) == 0xA5)
    {
        MagnifierKeyHandle(dgus_value);
        write_dgus_vp(R11_SCAN_ADDRESS, (uint8_t *)&uint16_port_zero, 1);
    }
}

static void R11FlagBitInit(void)
{

}

static void R11VideoPlayerInit(void)
{

}

static void R11CameraPixelsInit(void)
{

}

static void R11RestartInit(void)
{
    R11ConfigInitFormLib();
    R11FlagBitInit();
    R11VideoPlayerInit();
    R11CameraPixelsInit();
}


void UartR11UserBeautyProtocol(UART_TYPE *uart,uint8_t *frame, uint16_t len)
{
    uint16_t i=0,CrcFlag = 0,CrcResult = 0;
    if(frame[0] == 0x5a && frame[1] == 0xa5 && frame[3] == 0x82)
    {
        if(prvDwin8283CrcCheck(frame,len,&CrcFlag) == 0)
        {
            return;
        }
        if(len < frame[2] + 3) 
        {
            return; 
        }
        if(CrcFlag != 0)
        {
            frame[2] -= 2;
        }
        if(frame[4] == 0x04 && frame[5] == 0x82 && uart == &Uart_R11)
        {
            r11_restart_flag = 1;  /* 设置重启标志 */
        }
    }
}

void R11NetskinAnalyzeTask(void)
{
    
    if(r11_restart_flag == 1)
    {
        R11RestartInit();
        r11_restart_flag = UINT16_PORT_MAX;  /* 重置重启标志 */
    }
    R11ValueScanTask();
}

#endif /* sysBEAUTY_MODE_ENABLED */
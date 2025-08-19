#include "r11_advertise.h"

#if sysADVERTISE_MODE_ENABLED

SCREEN_S screen_opt;
MAINVIEW_S mainview;
WIFI_PAGE_S wifi_page;
R11_STATE r11_state = {UINT16_PORT_MAX}; // 初始化R11状态


void R11ConfigInitFormLib(void)
{
    uint16_t read_param[30];
    FlashToDgus(flashMAIN_BLOCK_ORDER,PIXELS_SET_ADDR,PIXELS_SET_ADDR,0x48);
    	/** 1.进行分辨率的初始化，针对2k分辨率需要修改主频 */
	read_dgus_vp(PIXELS_SET_ADDR,(uint8_t*)&read_param[0],1);
	memcpy(&screen_opt, &read_param[0], 2);
	if(screen_opt.screen_ratio == 0)  //19201080
	{
		sys_2k_ratio = 1;
		sysFOSC = 383385600;
		sysFCLK = 383385600;
	}else{
		sys_2k_ratio = 0;
		sysFOSC = 206438400;
		sysFCLK = 206438400;
	}
	/** 2.进行屏幕设置的初始化，包括分频系数，显示质量，截图张数 */
    Icon_Overlay_SP_VP[0] = Icon_Overlay_SP_VP[2] = 0x10000;
	Icon_Overlay_SP_VP[1] = Icon_Overlay_SP_VP[3] = 0x28000;
    Icon_Overlay_SP_VP[4] = Icon_Overlay_SP_VP[5] = Icon_Overlay_SP_VP[6] = 0x3f000;
	Icon_Overlay_SP_VP[7] = Icon_Overlay_SP_VP[8] = Icon_Overlay_SP_VP[9] = Icon_Overlay_SP_VP[10] = 0x3f000;


	/* 3.针对主显示位置进行初始化 */
	read_dgus_vp(MAIN_HIGH_ADDR,(uint8_t*)&read_param[0],8);
	memcpy(&mainview, &read_param[0], 16);

	Icon_Overlay_SP_X[0] = Icon_Overlay_SP_X[1] = mainview.main_x_point;
	Icon_Overlay_SP_X[2] = Icon_Overlay_SP_X[3] = mainview.detail_x_point;
	Icon_Overlay_SP_Y[0] = Icon_Overlay_SP_Y[1] = mainview.main_y_point;
	Icon_Overlay_SP_Y[2] = Icon_Overlay_SP_Y[3] = mainview.detail_y_point;
	
	Icon_Overlay_SP_L[0] = Icon_Overlay_SP_L[1] = mainview.main_high;
	Icon_Overlay_SP_L[2] = Icon_Overlay_SP_L[3] = mainview.detail_high;
	Icon_Overlay_SP_H[0] = Icon_Overlay_SP_H[1] = mainview.main_weight;
	Icon_Overlay_SP_H[2] = Icon_Overlay_SP_H[3] = mainview.detail_high;
	
	Locate_arr[0] = mainview.main_x_point;     
	Locate_arr[1] = mainview.main_y_point;
	Locate_arr[2] = mainview.detail_x_point;
	Locate_arr[3] = mainview.detail_y_point;


	/** 4.针对页面切换进行初始化 */
	read_dgus_vp(MENU_SCREEN_ADDR,(uint8_t*)&read_param[0],7);
	memcpy(&page_st, &read_param[0], 14);
	/** 5.针对wifi相关过渡页进行初始化 */
	read_dgus_vp(WIFI_SCAN_PAGE_ADDR,(uint8_t*)&read_param[0],7);
	memcpy(&wifi_page, &read_param[0], 14);

	read_dgus_vp(WIFI_START_ADDR,(uint8_t*)&wifi_page.wifi_start_addr,1);
	read_dgus_vp(VIDEO_FULL_ADDR,(uint8_t*)&read_param[0],1);
	page_st.fullvideo_flag = read_param[0] >> 8;
	page_st.fullvideo_page = read_param[0] & 0xff;
}


/*
 * @brief 页面切换时调整图片显示位置。
 */
static void R11PageInitChange()
{
	uint16_t now_pic;
    read_dgus_vp(sysDGUS_PIC_NOW, (uint8_t *)&now_pic, 1);
    if(now_pic == (uint16_t)page_st.detail_page)
	{
		R11ChangePictureLocate(mainview.detail_x_point,mainview.detail_y_point,mainview.detail_high,mainview.detail_weight,0x02);
	}else if(now_pic == (uint16_t)page_st.main_page)
	{
		R11ChangePictureLocate(mainview.main_x_point,mainview.main_y_point,mainview.main_high,mainview.main_weight,0x02);
	}else
	{
		R11ChangePictureLocate(mainview.main_x_point,mainview.main_y_point,mainview.main_high,mainview.main_weight,0x02);
	}
}


static void R11RestartInit()
{
    R11PageInitChange();
}



static void R11ValueScanTask(void)
{
    const uint16_t uint16_port_zero = 0;
    uint16_t dgus_value;
  
    #if sysDGUS_AUTO_UPLOAD_ENABLED
    DgusAutoUpload();
    #endif /* sysDGUS_AUTO_UPLOAD_ENABLED */

    read_dgus_vp(R11_SCAN_ADDRESS, (uint8_t *)&dgus_value, 1);
    if((dgus_value>>8) == 0x00 && (dgus_value & 0x00FF) != 0x00)
	{
		R11VideoValueHandle(dgus_value);
		write_dgus_vp(R11_SCAN_ADDRESS,(uint8_t*)&uint16_port_zero,1);
	}else if((dgus_value>>8) == 0x01)
	{
		R11DebugValueHandle(dgus_value);
		write_dgus_vp(R11_SCAN_ADDRESS,(uint8_t*)&uint16_port_zero,1);
	}else if((dgus_value>>8) >= 0xaa && (dgus_value>>8) <= 0xaf)
	{
		R11WifiValueHandle(dgus_value);
		write_dgus_vp(R11_SCAN_ADDRESS,(uint8_t*)&uint16_port_zero,1);
	}
}

void R11AdvertiseTask(void)
{
    if(r11_state.restart_flag == 1)
    {
        R11RestartInit();
        r11_state.restart_flag = 2;  /* 重置重启标志 */
    }else if(r11_state.restart_flag == 2)
	{
        R11VideoPlayerProcess();
		R11ValueScanTask();
	}
}


void UartR11UserAdvertiseProtocol(UART_TYPE *uart,uint8_t *frame, uint16_t len)
{
	static uint16_t prev_pic = 0;
	uint16_t write_param[10],now_pic;
    if(frame[0] == 0xAA && frame[1] == 0x55)
    {
        if(len < 6 || len < ((frame[2]<<8|frame[3])+4))
        {
            return;
        }
        if(frame[4] == 0x82 && frame[5] == 0x04 && frame[6] == 0x82 && uart == &Uart_R11)
        {
            r11_state.restart_flag = 1;  /* 设置重启标志 */
        }
    }
}

#endif /* sysADVERTISE_MODE_ENABLED */
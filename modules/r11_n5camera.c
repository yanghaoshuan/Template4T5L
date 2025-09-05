#include "r11_n5camera.h"

#if sysN5CAMERA_MODE_ENABLED

SCREEN_S screen_opt;
MAINVIEW_S mainview;
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
	read_dgus_vp(VIDEO_HIGH_ADDR,(uint8_t*)&read_param[0],16);
	memcpy(&mainview, &read_param[0], 32);

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
		R11ChangePictureLocate(mainview.detail_x_point,mainview.detail_y_point,mainview.detail_high,mainview.detail_weight,0x01);
	}else if(now_pic == (uint16_t)page_st.main_page)
	{
		R11ChangePictureLocate(mainview.main_x_point,mainview.main_y_point,mainview.main_high,mainview.main_weight,0x01);
	}else
	{
		R11ChangePictureLocate(mainview.main_x_point,mainview.main_y_point,mainview.main_high,mainview.main_weight,0x01);
	}
}


static void R11N5CameraModeSet()
{
    uint16_t mode = 0;
    uint8_t write_param[9];
    read_dgus_vp(N5_CAMERA_MODE_ADDR, (uint8_t *)&mode, 1);
    write_param[0] = 0xAA;
    write_param[1] = 0x55;
    write_param[2] = 0x00;
    write_param[3] = 0x02;
    write_param[4] = cmdN5_CAMERA_MODE;
    write_param[5] = (uint8_t)mode;
    T5lSendUartDataToR11(cmdN5_CAMERA_MODE, write_param);

}


static void R11RestartInit()
{
    R11N5CameraModeSet();
}


static void N5CameraKeyHandle(uint16_t dgus_value)
{
    uint8_t r11_send_buf[100];
	const uint16_t uin16_port_zero = 0;
    if(dgus_value == 0xA501)
    {
        /** 打开摄像头 */
        r11_send_buf[0] = 0xaa;
        r11_send_buf[1] = 0x55;
        r11_send_buf[2] = 0x00;
        r11_send_buf[3] = 0x02;
        r11_send_buf[4] = cmdN5_CAMERA_AV1_OPEN;
        r11_send_buf[5] = 0x00;
        T5lSendUartDataToR11(cmdN5_CAMERA_AV1_OPEN,r11_send_buf);
        if(page_st.main_flag == 0x5a)
        {
            SwitchPageById((uint16_t)page_st.main_page); 
        }
        R11PageInitChange();
        write_dgus_vp(R11_SCAN_ADDRESS, (uint8_t *)&uin16_port_zero, 1);
    }else if(dgus_value == 0xA50A)
    {
        /** 关闭摄像头 */
        r11_send_buf[0] = 0xaa;
        r11_send_buf[1] = 0x55;
        r11_send_buf[2] = 0x00;
        r11_send_buf[3] = 0x02;
        r11_send_buf[4] = cmdN5_CAMERA_CLOSE;
        r11_send_buf[5] = 0x00;
        T5lSendUartDataToR11(cmdN5_CAMERA_CLOSE,r11_send_buf);
        R11ClearPicture(0);
        if(page_st.menu_flag == 0x5a)
        {
            SwitchPageById((uint16_t)page_st.menu_page); 
        }
        write_dgus_vp(R11_SCAN_ADDRESS, (uint8_t *)&uin16_port_zero, 1);
    }else if(dgus_value == 0xA530)
    {
        r11_send_buf[0] = 0xaa;
        r11_send_buf[1] = 0x55;
        r11_send_buf[2] = 0x00;
        r11_send_buf[3] = 0x02;
        r11_send_buf[4] = cmdN5_CAMERA_MODE;
        r11_send_buf[5] = SD_H720_NT;
        T5lSendUartDataToR11(cmdN5_CAMERA_MODE,r11_send_buf);
        write_dgus_vp(R11_SCAN_ADDRESS, (uint8_t *)&uin16_port_zero, 1);
    }else if(dgus_value == 0xA531)
    {
        r11_send_buf[0] = 0xaa;
        r11_send_buf[1] = 0x55;
        r11_send_buf[2] = 0x00;
        r11_send_buf[3] = 0x02;
        r11_send_buf[4] = cmdN5_CAMERA_MODE;
        r11_send_buf[5] = SD_H720_PAL;
        T5lSendUartDataToR11(cmdN5_CAMERA_MODE,r11_send_buf);
        write_dgus_vp(R11_SCAN_ADDRESS, (uint8_t *)&uin16_port_zero, 1);
    }
    else if(dgus_value == 0xA532)
    {
        r11_send_buf[0] = 0xaa;
        r11_send_buf[1] = 0x55;
        r11_send_buf[2] = 0x00;
        r11_send_buf[3] = 0x02;
        r11_send_buf[4] = cmdN5_CAMERA_MODE;
        r11_send_buf[5] = SD_960H_NT;
        T5lSendUartDataToR11(cmdN5_CAMERA_MODE,r11_send_buf);
        write_dgus_vp(R11_SCAN_ADDRESS, (uint8_t *)&uin16_port_zero, 1);
    }
    else if(dgus_value == 0xA533)
    {
        r11_send_buf[0] = 0xaa;
        r11_send_buf[1] = 0x55;
        r11_send_buf[2] = 0x00;
        r11_send_buf[3] = 0x02;
        r11_send_buf[4] = cmdN5_CAMERA_MODE;
        r11_send_buf[5] = SD_960H_PAL;
        T5lSendUartDataToR11(cmdN5_CAMERA_MODE,r11_send_buf);
        write_dgus_vp(R11_SCAN_ADDRESS, (uint8_t *)&uin16_port_zero, 1);
    }
    else if(dgus_value == 0xA534)
    {
        r11_send_buf[0] = 0xaa;
        r11_send_buf[1] = 0x55;
        r11_send_buf[2] = 0x00;
        r11_send_buf[3] = 0x02;
        r11_send_buf[4] = cmdN5_CAMERA_MODE;
        r11_send_buf[5] = AHD_720P_25P;
        T5lSendUartDataToR11(cmdN5_CAMERA_MODE,r11_send_buf);
        write_dgus_vp(R11_SCAN_ADDRESS, (uint8_t *)&uin16_port_zero, 1);
    }
    else if(dgus_value == 0xA535)
    {
        r11_send_buf[0] = 0xaa;
        r11_send_buf[1] = 0x55;
        r11_send_buf[2] = 0x00;
        r11_send_buf[3] = 0x02;
        r11_send_buf[4] = cmdN5_CAMERA_MODE;
        r11_send_buf[5] = AHD_1080P_25P;
        T5lSendUartDataToR11(cmdN5_CAMERA_MODE,r11_send_buf);
        write_dgus_vp(R11_SCAN_ADDRESS, (uint8_t *)&uin16_port_zero, 1);
    }
    else if(dgus_value == 0xA536)
    {
        r11_send_buf[0] = 0xaa;
        r11_send_buf[1] = 0x55;
        r11_send_buf[2] = 0x00;
        r11_send_buf[3] = 0x02;
        r11_send_buf[4] = cmdN5_CAMERA_MODE;
        r11_send_buf[5] = AHD_1080P_15P;
        T5lSendUartDataToR11(cmdN5_CAMERA_MODE,r11_send_buf);
        write_dgus_vp(R11_SCAN_ADDRESS, (uint8_t *)&uin16_port_zero, 1);
    }
}


static void R11ValueScanTask(void)
{
    const uint16_t uint16_port_zero = 0;
    uint16_t dgus_value;
  
    #if sysDGUS_AUTO_UPLOAD_ENABLED
    DgusAutoUpload();
    #endif /* sysDGUS_AUTO_UPLOAD_ENABLED */

    read_dgus_vp(R11_SCAN_ADDRESS, (uint8_t *)&dgus_value, 1);
    if((dgus_value>>8) == 0xA5)
    {
        N5CameraKeyHandle(dgus_value);
    }else if((dgus_value>>8) == 0x00 && (dgus_value & 0x00FF) != 0x00)
	{
		R11VideoValueHandle(dgus_value);
		write_dgus_vp(R11_SCAN_ADDRESS,(uint8_t*)&uint16_port_zero,1);
	}else if((dgus_value>>8) == 0x01)
	{
		R11DebugValueHandle(dgus_value);
		write_dgus_vp(R11_SCAN_ADDRESS,(uint8_t*)&uint16_port_zero,1);
	}
}

void R11N5CameraTask(void)
{
    if(r11_state.restart_flag == 1)
    {
        R11RestartInit();
        T5lJpegInit();
        r11_state.restart_flag = 2;  /* 重置重启标志 */
    }else if(r11_state.restart_flag == 2)
	{
		R11ValueScanTask();
	}
}


void UartR11UserN5CameraProtocol(UART_TYPE *uart,uint8_t *frame, uint16_t len)
{
	static uint16_t prev_pic = 0;
	uint16_t write_param[10],now_pic;
    if(frame[0] == 0x5a && frame[1] == 0xa5)
    {
        if(len < frame[2] + 3) 
        {
            return; 
        }
        if(frame[4] == 0x04 && frame[5] == 0x11 && uart == &Uart_R11)
        {
            r11_state.restart_flag = 1;  /* 设置重启标志 */
        }
    }else if(frame[0] == 0xAA && frame[1] == 0x55)
    {
        if(len < 6 || len < ((frame[2]<<8|frame[3])+4))
        {
            return;
        }
        switch (frame[4])
        {
            case cmdN5_CAMERA_AV1_OPEN:
            case cmdN5_CAMERA_AV2_OPEN:
                break;
            case cmdN5_CAMERA_CLOSE:
                break;
            default:
                break;
        }
    }
}

#endif /* sysN5CAMERA_MODE_ENABLED */
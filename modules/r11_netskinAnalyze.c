#include "r11_netskinAnalyze.h"
#include <string.h>


#if sysBEAUTY_MODE_ENABLED
CAMERA_MA camera_magnifier;
ENLARGE_P enl_enlarge_mode;
WECHAT_JSON Json_Wechat;
ADDR_S addr_st;
THUMBNAIL_S thumbnail;
MAINVIEW_S mainview;
SCREEN_S screen_opt;
WIFI_PAGE_S wifi_page;
CAMERA_PROCESS_STATE camera_process_state = CAMERA_INSERT_DETECT;
NET_CONNECTED_STATE net_connected_state = NET_NOT_CONNECTED;
R11_STATE r11_state = {UINT16_PORT_MAX,0,0}; // 初始化R11状态



void R11ConfigInitFormLib(void)
{
    uint16_t read_param[30];
    FlashToDgus(flashMAIN_BLOCK_ORDER,PIXELS_SET_ADDR,PIXELS_SET_ADDR,0x48);
    	/** 1.进行分辨率的初始化，针对2k分辨率需要修改主频 */
	read_dgus_vp(PIXELS_SET_ADDR,(uint8_t*)&read_param[0],6);
	memcpy(&screen_opt, &read_param[0], 12);
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
	if(screen_opt.fclk_div <7||screen_opt.fclk_div>=12)
	{
		screen_opt.fclk_div = 12;
		write_dgus_vp(FCLK_DIV_ADDR,(uint8_t*)&screen_opt.fclk_div,1);
	}
	if(screen_opt.quality_set < 50||screen_opt.quality_set>100)
	{
		screen_opt.quality_set = 80;
		write_dgus_vp(QUANITY_SET_ADDR,(uint8_t*)&screen_opt.quality_set,1);
	}
	if(screen_opt.thumbnail_num < 1||screen_opt.thumbnail_num> 6)
	{
		screen_opt.thumbnail_num = 3;
		write_dgus_vp(CAMERA_NUM_ADDR,(uint8_t*)&screen_opt.thumbnail_num,1);
	}
	if(screen_opt.thumbnail_num <= 3)
	{
		Icon_Overlay_SP_VP[0] = Icon_Overlay_SP_VP[2] = 0x07000;
		Icon_Overlay_SP_VP[1] = Icon_Overlay_SP_VP[3] = 0x1d000;
		Icon_Overlay_SP_VP[4] = 0x33000;
		Icon_Overlay_SP_VP[5] = 0x37000;
		Icon_Overlay_SP_VP[6] = 0x3b000;
		Icon_Overlay_SP_VP[7] = Icon_Overlay_SP_VP[8] = Icon_Overlay_SP_VP[9] = Icon_Overlay_SP_VP[10] = 0x3f000;
	}else if(screen_opt.thumbnail_num == 4)
	{
		Icon_Overlay_SP_VP[0] = Icon_Overlay_SP_VP[2] = 0x07000;
		Icon_Overlay_SP_VP[1] = Icon_Overlay_SP_VP[3] = 0x1f000;
		Icon_Overlay_SP_VP[4] = 0x37000;
		Icon_Overlay_SP_VP[5] = 0x39000;
		Icon_Overlay_SP_VP[6] = 0x3b000;
		Icon_Overlay_SP_VP[7] = 0x3d000;
		Icon_Overlay_SP_VP[8] = Icon_Overlay_SP_VP[9] = Icon_Overlay_SP_VP[10] = 0x3f000;
	}else if(screen_opt.thumbnail_num == 5)
	{
		Icon_Overlay_SP_VP[0] = Icon_Overlay_SP_VP[2] = 0x07000;
		Icon_Overlay_SP_VP[1] = Icon_Overlay_SP_VP[3] = 0x1e000;
		Icon_Overlay_SP_VP[4] = 0x35000;
		Icon_Overlay_SP_VP[5] = 0x37000;
		Icon_Overlay_SP_VP[6] = 0x39000;
		Icon_Overlay_SP_VP[7] = 0x3b000;
		Icon_Overlay_SP_VP[8] = 0x3d000;
		Icon_Overlay_SP_VP[9] = Icon_Overlay_SP_VP[10] = 0x3f000;
		
	}else if(screen_opt.thumbnail_num == 6)
	{
		Icon_Overlay_SP_VP[0] = Icon_Overlay_SP_VP[2] = 0x07000;
		Icon_Overlay_SP_VP[1] = Icon_Overlay_SP_VP[3] = 0x1d000;
		Icon_Overlay_SP_VP[4] = 0x33000;
		Icon_Overlay_SP_VP[5] = 0x35000;
		Icon_Overlay_SP_VP[6] = 0x37000;
		Icon_Overlay_SP_VP[7] = 0x39000;
		Icon_Overlay_SP_VP[8] = 0x3b000;
		Icon_Overlay_SP_VP[9] = 0x3d000;
		Icon_Overlay_SP_VP[10] = 0x3f000;
	}

	if(screen_opt.jpeg_type> 4)
	{
		screen_opt.jpeg_type = 0;
		write_dgus_vp(JPEG_TYPE_ADDR,(uint8_t*)&screen_opt.jpeg_type,1);
	}

	if(screen_opt.camera_pix == 0)  /** 1920*1080 */
	{
		camera_magnifier.camera_col_high = 1920;
		camera_magnifier.camera_col_width = 1080;

	}else if(screen_opt.camera_pix == 1)   /** 1024*600 */
	{
		camera_magnifier.camera_col_high = 1024;
		camera_magnifier.camera_col_width = 600;
	}else if(screen_opt.camera_pix == 2)   /** 800*600 */
	{
		camera_magnifier.camera_col_high = 800;
		camera_magnifier.camera_col_width = 600;
	}else if(screen_opt.camera_pix == 3)
	{
		camera_magnifier.camera_col_high = 800;
		camera_magnifier.camera_col_width = 480;
	}else if(screen_opt.camera_pix == 4)
	{
		camera_magnifier.camera_col_high = 640;
		camera_magnifier.camera_col_width = 480;
	}else if(screen_opt.camera_pix == 5)
	{
		camera_magnifier.camera_col_high = 320;
		camera_magnifier.camera_col_width = 240;
	}else if(screen_opt.camera_pix == 6)
	{
		camera_magnifier.camera_col_high = 1280;
		camera_magnifier.camera_col_width = 720;
	}else{
		camera_magnifier.camera_col_high = 640;
		camera_magnifier.camera_col_width = 480;
	}

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
	
	Locate_arr[4] = mainview.crop_x_point;
	Locate_arr[5] = mainview.crop_y_point;
	Locate_arr[6] = mainview.video_x_point;
	Locate_arr[7] = mainview.video_y_point;

	/* 4.针对缩略图进行初始化 */
	read_dgus_vp(PIC_HIGH_ADDR,(uint8_t*)&read_param[0],14);
	memcpy(&thumbnail, &read_param[0], 28);
	camera_magnifier.camera_cap_high = thumbnail.high;
	camera_magnifier.camera_cap_width = thumbnail.weight;
	Icon_Overlay_SP_L[4] = Icon_Overlay_SP_L[5] = Icon_Overlay_SP_L[6] = Icon_Overlay_SP_L[7] = Icon_Overlay_SP_L[8] = Icon_Overlay_SP_L[9] = thumbnail.high;
	Icon_Overlay_SP_H[4] = Icon_Overlay_SP_H[5] = Icon_Overlay_SP_H[6] = Icon_Overlay_SP_H[7] = Icon_Overlay_SP_H[8] = Icon_Overlay_SP_H[9] = thumbnail.weight;
	Icon_Overlay_SP_X[4] = thumbnail.pic1_x_point;
	Icon_Overlay_SP_X[5] = thumbnail.pic2_x_point;
	Icon_Overlay_SP_X[6] = thumbnail.pic3_x_point;
	Icon_Overlay_SP_X[7] = thumbnail.pic4_x_point;
	Icon_Overlay_SP_X[8] = thumbnail.pic5_x_point;
	Icon_Overlay_SP_X[9] = thumbnail.pic6_x_point;
	
	Icon_Overlay_SP_Y[4] = thumbnail.pic1_y_point;
	Icon_Overlay_SP_Y[5] = thumbnail.pic2_y_point;
	Icon_Overlay_SP_Y[6] = thumbnail.pic3_y_point;
	Icon_Overlay_SP_Y[7] = thumbnail.pic4_y_point;
	Icon_Overlay_SP_Y[8] = thumbnail.pic5_y_point;
	Icon_Overlay_SP_Y[9] = thumbnail.pic6_y_point;

	/** 5.针对页面切换进行初始化 */
	read_dgus_vp(MENU_SCREEN_ADDR,(uint8_t*)&read_param[0],7);
	memcpy(&page_st, &read_param[0], 14);
	/** 6.针对显示必要数据的地址进行初始化 */
	read_dgus_vp(USER_TEL_ADDR,(uint8_t*)&read_param[0],11);
	memcpy(&addr_st, &read_param[0], 22);

	read_dgus_vp(CMD_82_RETURN_ADDR,(uint8_t*)&screen_opt.cmd_82_return_flag,1);
	//7.针对wifi相关过渡页进行初始化
	read_dgus_vp(WIFI_SCAN_PAGE_ADDR,(uint8_t*)&read_param[0],7);
	memcpy(&wifi_page, &read_param[0], 14);

	read_dgus_vp(WIFI_START_ADDR,(uint8_t*)&wifi_page.wifi_start_addr,1);
	read_dgus_vp(CAMERA_OPEN_ADDR,(uint8_t*)&screen_opt.camera_open_sta,1);
	read_dgus_vp(VIDEO_FULL_ADDR,(uint8_t*)&read_param[0],1);
	page_st.fullvideo_flag = read_param[0] >> 8;
	page_st.fullvideo_page = read_param[0] & 0xff;
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
    }else if((dgus_value>>8) == 0x00 && (dgus_value & 0x00FF) != 0x00)
	{
		R11VideoValueScanTask(dgus_value);
		write_dgus_vp(R11_SCAN_ADDRESS, (uint8_t *)&uint16_port_zero, 1);
	}
}

static void R11FlagBitInit(void)
{
    uint16_t now_pic;
    /** 1.针对当前页面调整显示大小 */
    read_dgus_vp(sysDGUS_PIC_NOW, (uint8_t *)&now_pic, 1);
    if(now_pic == (uint16_t)page_st.detail_page)
	{
		change_pic_locate(mainview.detail_x_point,mainview.detail_y_point,mainview.detail_high,mainview.detail_weight,0x00);
	}else if(now_pic == (uint16_t)page_st.main_page)
	{
		change_pic_locate(mainview.main_x_point,mainview.main_y_point,mainview.main_high,mainview.main_weight,0x00);
	}
    /** 3.针对摄像头显示参数进行初始化 */
    camera_magnifier.camera_type = 0;
    camera_magnifier.camera_way = 0;
    camera_magnifier.camera_show_high = mainview.main_high;
    camera_magnifier.camera_show_width = mainview.main_weight;
    camera_magnifier.camera_send_high = 640;
    camera_magnifier.camera_send_width = 480;
    camera_magnifier.camera_local = 0;
    camera_magnifier.camera_status = 1;
    camera_magnifier.camera_process = 0;
	camera_magnifier.cap_status = 0;

    /** 4.针对缩放参数进行初始化 */
    enl_enlarge_mode.enlarge_x_acc = 16;
    enl_enlarge_mode.enlarge_y_acc = 12;
    enl_enlarge_mode.enlarge_start_X = 0;
    enl_enlarge_mode.enlarge_start_Y = 0;
    enl_enlarge_mode.enlarge_cap_high = 640;
    enl_enlarge_mode.enlarge_cap_width = 480;
	enl_enlarge_mode.enlarge_start_X_last = 0;
    enl_enlarge_mode.enlarge_start_Y_last = 0;
    enl_enlarge_mode.enlarge_cap_high_last = 640;
    enl_enlarge_mode.enlarge_cap_width_last = 480;
    enl_enlarge_mode.enlarge_show_high = mainview.main_high;
    enl_enlarge_mode.enlarge_show_width = mainview.main_weight;
}


static void R11CameraInit(void)
{

}

static void R11StartPowerInit(void)
{
	uint8_t send_r11_buf[10];
	uint16_t quanity_set,fclk_div;
	send_r11_buf[0] = 0xAA;
	send_r11_buf[1] = 0x55;
	send_r11_buf[2] = 0x00;
	send_r11_buf[3] = 0x04; 
	send_r11_buf[4] = 0xd0; 
	read_dgus_vp(QUANITY_SET_ADDR,(uint8_t*)&quanity_set,1);
	send_r11_buf[5]=(uint8_t)quanity_set;
	read_dgus_vp(FCLK_DIV_ADDR,(uint8_t*)&fclk_div,1);
	send_r11_buf[6]=(uint8_t)fclk_div;
	send_r11_buf[7]=Bit8_16_Flag*8;
	UartSendData(&Uart_R11,send_r11_buf,8);
}

static void R11RestartInit(void)
{
    R11FlagBitInit();
    R11CameraInit();
	R11StartPowerInit();
}


void UartR11UserBeautyProtocol(UART_TYPE *uart,uint8_t *frame, uint16_t len)
{
    if(frame[0] == 0x5a && frame[1] == 0xa5 && frame[3] == 0x82)
    {
        if(len < frame[2] + 3) 
        {
            return; 
        }
        if(frame[4] == 0x04 && frame[5] == 0x82 && uart == &Uart_R11)
        {
            r11_state.restart_flag = 1;  /* 设置重启标志 */
        }
    }
}

void R11NetskinAnalyzeTask(void)
{
    /** 在重启之后，R11RestartInit只运行一次，R11VideoPlayerProcess会一直运行直到完成 */
    if(r11_state.restart_flag == 1)
    {
        R11RestartInit();
		video_init_process = VIDEO_PROCESS_UNINIT;
        r11_state.restart_flag = 2;  /* 重置重启标志 */
    }else if(r11_state.restart_flag == 2)
	{
		R11VideoPlayerProcess();
		R11ValueScanTask();
	}
}

#endif /* sysBEAUTY_MODE_ENABLED */
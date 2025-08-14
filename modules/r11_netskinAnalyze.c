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
CAMERA_PROCESS_STATE camera_process_state = CAMERA_INSERT_CHECK;
NET_CONNECTED_STATE net_connected_state = NET_NOT_CONNECTED;
R11_STATE r11_state = {UINT16_PORT_MAX,0,0,0}; // 初始化R11状态



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


static void R11CameraOpenThreadCtrl(uint8_t camera_mode,uint8_t camera_status,uint8_t camera_type)
{
	uint8_t r11_send_buf[15];
	r11_send_buf[0] = 0xaa;
	r11_send_buf[1] = 0x55;
	r11_send_buf[2] = 0x00;
	r11_send_buf[3] = 0x09;
	r11_send_buf[4] = cameraOPEN_THREAD;
	if(camera_mode == 0)
	{
		/** not use */
		__NOP();
	}else if(camera_mode == cameraMAGNIFIER_MODE)
	{
		r11_send_buf[5] = camera_magnifier.camera_way;
        r11_send_buf[6] = camera_magnifier.camera_type;
        r11_send_buf[7] = camera_status;
		r11_send_buf[8] = camera_type;
		r11_send_buf[9] = camera_magnifier.camera_col_high>>8;
		r11_send_buf[10] = (uint8_t)camera_magnifier.camera_col_high;
		r11_send_buf[11] = camera_magnifier.camera_col_width>>8;
		r11_send_buf[12] = (uint8_t)camera_magnifier.camera_col_width;
		UartSendData(&Uart_R11,r11_send_buf,13);
	}
}


static void R11CameraSendT5lCtrl(uint8_t camera_mode,uint8_t send_flag)
{
    uint8_t r11_send_buf[25];
	r11_send_buf[0] = 0xaa;
    r11_send_buf[1] = 0x55;
    r11_send_buf[2] = 0x00;
    r11_send_buf[3] = 0x11;
    r11_send_buf[4] = cameraSEND_T5L;
    if(camera_mode == 0)
    {
		/** not use */
        __NOP();
    }else if(camera_mode == cameraMAGNIFIER_MODE)
	{
        r11_send_buf[5] = camera_magnifier.camera_type;
        r11_send_buf[6] = camera_magnifier.camera_local;
        r11_send_buf[7] = send_flag;
		r11_send_buf[8] = Icon_Overlay_SP_L[0]>>8;
        r11_send_buf[9] = (uint8_t)Icon_Overlay_SP_L[0];
        r11_send_buf[10] =Icon_Overlay_SP_H[0]>>8;
        r11_send_buf[11] =(uint8_t)Icon_Overlay_SP_H[0];
        r11_send_buf[12] = 0;
        r11_send_buf[13] = Icon_Overlay_SP_X[0]>>8;
        r11_send_buf[14] = (uint8_t)Icon_Overlay_SP_X[0];
        r11_send_buf[15] = Icon_Overlay_SP_Y[0]>>8;
        r11_send_buf[16] = (uint8_t)Icon_Overlay_SP_Y[0];
        r11_send_buf[17] = Icon_Overlay_SP_L[0]>>8;
        r11_send_buf[18] = (uint8_t)Icon_Overlay_SP_L[0];
        r11_send_buf[19] = Icon_Overlay_SP_H[0]>>8;
        r11_send_buf[20] = (uint8_t)Icon_Overlay_SP_H[0];
		UartSendData(&Uart_R11,r11_send_buf,21);
    }
}


static void R11CameraUploadPicture(void)
{
	/** not use */
	__NOP();
}


static void R11MagnifierEnlarge(uint8_t enlarge_type,uint8_t* item_en_name,ENLARGE_P* source2)
{
    uint8_t r11_send_buf[20];
    r11_send_buf[0] = 0xaa;
    r11_send_buf[1] = 0x55;
    r11_send_buf[2] = 0x00;
    r11_send_buf[3] = 14;
    r11_send_buf[4] = enlarge_type;
    r11_send_buf[5] = 0x00;
    r11_send_buf[6] = source2->enlarge_start_X>>8;
    r11_send_buf[7] = (uint8_t)source2->enlarge_start_X;
    r11_send_buf[8] = source2->enlarge_start_Y>>8;
    r11_send_buf[9] = (uint8_t)source2->enlarge_start_Y;
    r11_send_buf[10] = source2->enlarge_cap_high>>8;
    r11_send_buf[11] = (uint8_t)source2->enlarge_cap_high;
    r11_send_buf[12] = source2->enlarge_cap_width>>8;
    r11_send_buf[13] = (uint8_t)source2->enlarge_cap_width;
    r11_send_buf[14] = source2->enlarge_show_high>>8;
    r11_send_buf[15] = (uint8_t)source2->enlarge_show_high;
    r11_send_buf[16] = source2->enlarge_show_width>>8;
    r11_send_buf[17] = (uint8_t)source2->enlarge_show_width;
    if(enlarge_type == cameraENL_PICTURE)
    {
        r11_send_buf[18] = (uint8_t)r11_state.now_choose_pic;
        r11_send_buf[3] += 1;
        UartSendData(&Uart_R11,r11_send_buf,19);
    }
}


static void R11CameraEnlargeHandle(uint16_t dgus_value)
{
	uint8_t curr_enlarge_type;
	uint16_t curr_cap_high,curr_cap_width;
	const uint16_t uint16_port_zero = 0;
	ENLARGE_P *source;
	if((dgus_value>>8) == 0x50)
	{
		/** not use */
		__NOP();
	}else if((dgus_value>>8) == 0x51)
	{
		/** not use */
		__NOP();
	}else if((dgus_value>>8) == 0x52)
	{
		if(r11_state.delay_count == 5)
		{
			curr_cap_high = camera_magnifier.camera_show_high;
        	curr_cap_width = camera_magnifier.camera_show_width;
        	curr_enlarge_type = cameraENL_PICTURE;
        	source = &enl_enlarge_mode;
			r11_state.delay_count = 0;
		}else
		{
			r11_state.delay_count++;
			return;
		}
	}
	(*source).enlarge_start_X_last = (*source).enlarge_start_X;
    (*source).enlarge_start_Y_last = (*source).enlarge_start_Y;
    (*source).enlarge_cap_high_last = (*source).enlarge_cap_high;
    (*source).enlarge_cap_width_last = (*source).enlarge_cap_width;
	if((uint8_t)dgus_value == 0x01)
	{
		/** left */
		if((*source).enlarge_start_X >=(*source).enlarge_x_acc)
		{
			(*source).enlarge_start_X -=(*source).enlarge_x_acc;
		}
		R11MagnifierEnlarge(curr_enlarge_type,NULL,source);
		write_dgus_vp(R11_SCAN_ADDRESS,(uint8_t*)&uint16_port_zero,1);
	}else if((uint8_t)dgus_value == 0x02)
	{
		/** right */
		if((*source).enlarge_start_X+(*source).enlarge_x_acc+(*source).enlarge_cap_high <= curr_cap_high)
		{
			(*source).enlarge_start_X +=(*source).enlarge_x_acc;
		}
		R11MagnifierEnlarge(curr_enlarge_type,NULL,source);
		write_dgus_vp(R11_SCAN_ADDRESS,(uint8_t*)&uint16_port_zero,1);
	}else if((uint8_t)dgus_value == 0x03)
	{
		/** up */
		if((*source).enlarge_start_X+(*source).enlarge_x_acc+(*source).enlarge_cap_high <= curr_cap_high)
		{
			(*source).enlarge_start_X +=(*source).enlarge_x_acc;
		}
		R11MagnifierEnlarge(curr_enlarge_type,NULL,source);
		write_dgus_vp(R11_SCAN_ADDRESS,(uint8_t*)&uint16_port_zero,1);
	}else if((uint8_t)dgus_value == 0x04)
	{
		/** down */
		if((*source).enlarge_start_Y+(*source).enlarge_y_acc+(*source).enlarge_cap_width <= curr_cap_width)
		{
			(*source).enlarge_start_Y +=(*source).enlarge_y_acc;
		}
		R11MagnifierEnlarge(curr_enlarge_type,NULL,source);
		write_dgus_vp(R11_SCAN_ADDRESS,(uint8_t*)&uint16_port_zero,1);
	}else if((uint8_t)dgus_value == 0x05)
	{
		/** zoom */
		if((*source).enlarge_cap_high > (MIN_HIGH+(*source).enlarge_x_acc*2))
		{
			(*source).enlarge_cap_high -=((*source).enlarge_x_acc*2);
			if((*source).enlarge_start_X+(*source).enlarge_x_acc+(*source).enlarge_cap_high <= curr_cap_high)
			{
				(*source).enlarge_start_X +=(*source).enlarge_x_acc;
			}
		}
		if((*source).enlarge_cap_width > (MIN_WIDTH+(*source).enlarge_y_acc*2))
		{
			(*source).enlarge_cap_width -=((*source).enlarge_y_acc*2);
			if((*source).enlarge_start_Y+(*source).enlarge_y_acc+(*source).enlarge_cap_width <= curr_cap_width)
			{
				(*source).enlarge_start_Y +=(*source).enlarge_y_acc;
			}
		}
		R11MagnifierEnlarge(curr_enlarge_type,NULL,source);
		write_dgus_vp(R11_SCAN_ADDRESS,(uint8_t*)&uint16_port_zero,1);
	}else if((uint8_t)dgus_value == 0x06)
	{
		/** narrow */
		if(((*source).enlarge_cap_high+(*source).enlarge_x_acc*2) <= curr_cap_high)
		{
			(*source).enlarge_cap_high +=((*source).enlarge_x_acc*2);
		}
		if((*source).enlarge_cap_high+(*source).enlarge_start_X > curr_cap_high)
		{
			(*source).enlarge_start_X = curr_cap_high - (*source).enlarge_cap_high;
			
		}
		if((*source).enlarge_start_X >=(*source).enlarge_x_acc)
		{
			(*source).enlarge_start_X -=(*source).enlarge_x_acc;
		}
		if(((*source).enlarge_cap_width+(*source).enlarge_y_acc*2) <= curr_cap_width)
		{
			(*source).enlarge_cap_width +=((*source).enlarge_y_acc*2);
		}
		if((*source).enlarge_cap_width+(*source).enlarge_start_Y > curr_cap_width)
		{
			(*source).enlarge_start_Y = curr_cap_width - (*source).enlarge_cap_width;
		}
		if((*source).enlarge_start_Y >=(*source).enlarge_y_acc)
		{
			(*source).enlarge_start_Y -=(*source).enlarge_y_acc;
		}
		R11MagnifierEnlarge(curr_enlarge_type,NULL,source);
		write_dgus_vp(R11_SCAN_ADDRESS,(uint8_t*)&uint16_port_zero,1);
	}else if((uint8_t)dgus_value == 0x07)
	{
		/** reduction */
		(*source).enlarge_start_X=0;
		(*source).enlarge_start_Y=0;
		if(curr_enlarge_type == cameraENL_PICTURE)
		{
			(*source).enlarge_cap_high = camera_magnifier.camera_show_high;
			(*source).enlarge_cap_width = camera_magnifier.camera_show_width;
		}
		R11MagnifierEnlarge(curr_enlarge_type,NULL,source);
		write_dgus_vp(R11_SCAN_ADDRESS,(uint8_t*)&uint16_port_zero,1);
	}
}


static void MagnifierKeyHandle(uint16_t dgus_value)
{
	uint8_t r11_send_buf[100];
	const uint16_t uin16_port_zero = 0;
	uint16_t rotate_angle;
    if(dgus_value == 0xA501)
    {
		/**
		 * @brief 打开摄像头
		 * @note 首先发送0xb8指令检查摄像头是否插入，之后使用0xb5开启摄像头进程，之后使用0xa0指令发送给t5l进行显示
		 * @note 在这个过程不会清零变量地址的值，下一个周期会继续执行，直到摄像头打开完成，执行周期建议为为R11_TASK_INTERVAL
		 *  
		 * */
		if(camera_process_state == CAMERA_INSERT_CHECK)
		{
			r11_send_buf[0] = 0xaa;
			r11_send_buf[1] = 0x55;
			r11_send_buf[2] = 0x00;
			r11_send_buf[3] = 0x02;
			r11_send_buf[4] = cameraINSERT_CHECK;
			r11_send_buf[5] = camera_magnifier.camera_type;
			UartSendData(&Uart_R11, r11_send_buf, 6);
			camera_process_state = CAMERA_INSERT_WAITING;
			r11_state.delay_count++;
			if(r11_state.delay_count == cameraRETRY_MAX_COUNT)
			{
				r11_state.delay_count = 0;
			}
		}else if(camera_process_state == CAMERA_INSERT_WAITING)
		{
			r11_state.delay_count++;
			if(r11_state.delay_count == cameraRETRY_MAX_COUNT)
			{
				r11_state.delay_count = 0;
				camera_process_state = CAMERA_INSERT_CHECK;
			}
		}else if(camera_process_state == CAMERA_OPEN_THREAD)
		{
			R11ClearPicture(0);
			R11CameraOpenThreadCtrl(cameraMAGNIFIER_MODE,cameraOPEN_STATUS,(uint8_t)screen_opt.jpeg_type);
			R11PageInitChange();
			camera_process_state = CAMERA_OPEN_WAITING;
		}else if(camera_process_state == CAMERA_OPEN_WAITING)
		{
			r11_state.delay_count++;
			if(r11_state.delay_count == cameraRETRY_MAX_COUNT)
			{
				r11_state.delay_count = 0;
				camera_process_state = CAMERA_OPEN_THREAD;
			}
		}else if(camera_process_state == CAMERA_SEND_T5L)
		{
			R11CameraSendT5lCtrl(cameraMAGNIFIER_MODE,cameraOPEN_STATUS);
			camera_process_state = CAMERA_SEND_T5L_WAITING;
		}else if(camera_process_state == CAMERA_SEND_T5L_WAITING)
		{
			r11_state.delay_count++;
			if(r11_state.delay_count == cameraRETRY_MAX_COUNT)
			{
				r11_state.delay_count = 0;
				camera_process_state = CAMERA_SEND_T5L;
			}
		}else if(camera_process_state == CAMERA_PROCESS_END)
		{
			if(page_st.main_flag == 0x5a)
			{
				SwitchPageById((uint16_t)page_st.main_page); 
			}
			write_dgus_vp(R11_SCAN_ADDRESS, (uint8_t *)&uin16_port_zero, 1);
		}
	}else if(dgus_value == 0xA502)
	{
		/** 摄像头截图 */
		if(r11_state.pic_capture_flag == 1)
		{
			camera_magnifier.camera_num[r11_state.now_choose_pic] = 1;
			if(r11_state.now_choose_pic<(screen_opt.thumbnail_num-1))
			{
				r11_state.now_choose_pic++;
				write_dgus_vp(cameraNOW_NUM_ADDR,(uint8_t*)&r11_state.now_choose_pic,1);
			}
			write_dgus_vp(R11_SCAN_ADDRESS,(uint8_t*)&uin16_port_zero,1);
			r11_state.pic_capture_flag = 0;
			return;
		}
		if(r11_state.delay_count == 5)
		{
			write_dgus_vp(Icon_Overlay_SP_VP[4+r11_state.now_choose_pic],"\x5b\x5b\x5b\x5b",2);
			r11_send_buf[0] = 0xaa;
			r11_send_buf[1] = 0x55;
			r11_send_buf[2] = 0x00;
			r11_send_buf[3] = 0x07;
			r11_send_buf[4] = cameraCAP_PICTURE;
			r11_send_buf[5] = r11_state.now_choose_pic;
			r11_send_buf[6] = camera_magnifier.camera_cap_high>>8;
			r11_send_buf[7] = (uint8_t)camera_magnifier.camera_cap_high;
			r11_send_buf[8] = camera_magnifier.camera_cap_width >> 8;
			r11_send_buf[9] = (uint8_t)camera_magnifier.camera_cap_width;
			r11_send_buf[10] = 50;
			UartSendData(&Uart_R11,r11_send_buf,11);
			r11_state.delay_count = 0;
		}else
		{
			r11_state.delay_count++;
		}
	}else if(dgus_value == 0xA503)
	{
		/** 摄像头删除 */
		if(camera_magnifier.camera_num[r11_state.now_choose_pic] == 1)   
		{
			write_dgus_vp(Icon_Overlay_SP_VP[4+r11_state.now_choose_pic],"\x5b\x5b\x5b\x5b",2);
			camera_magnifier.camera_num[r11_state.now_choose_pic] = 0;
			r11_send_buf[0] = 0xaa;
			r11_send_buf[1] = 0x55;
			r11_send_buf[2] = 0x00;
			r11_send_buf[3] = 0x02;
			r11_send_buf[4] = cameraDEL_PICTURE;
			r11_send_buf[5] = (uint8_t)r11_state.now_choose_pic;
			UartSendData(&Uart_R11,r11_send_buf,6);
			write_dgus_vp(R11_SCAN_ADDRESS,(uint8_t*)&uin16_port_zero,1);
		}else
		{
			write_dgus_vp(R11_SCAN_ADDRESS,(uint8_t*)&uin16_port_zero,1);
            return;
		}
	}else if(dgus_value == 0xA504)
	{
		/** 摄像头选择上一个图片 */
		if(r11_state.now_choose_pic>0)
        {
            r11_state.now_choose_pic--;
            write_dgus_vp(cameraNOW_NUM_ADDR,(uint8_t*)&r11_state.now_choose_pic,1);
        }
        write_dgus_vp(R11_SCAN_ADDRESS,(uint8_t*)&uin16_port_zero,1);
	}else if(dgus_value == 0xA505)  
    {
		/** 摄像头选择下一个图片 */
        if(r11_state.now_choose_pic<(screen_opt.thumbnail_num-1))
        {
            r11_state.now_choose_pic++;
            write_dgus_vp(cameraNOW_NUM_ADDR,(uint8_t*)&r11_state.now_choose_pic,1);
        }
        write_dgus_vp(R11_SCAN_ADDRESS,(uint8_t*)&uin16_port_zero,1);
    }else if(dgus_value == 0xA506)  
    {
		/** 进入摄像头详情页 */
        R11ChangePictureLocate(mainview.detail_x_point,mainview.detail_y_point,mainview.detail_high,mainview.detail_weight,0x00);
		R11CameraSendT5lCtrl(cameraMAGNIFIER_MODE,cameraOPEN_STATUS);
        if(page_st.detail_flag == 0x5a)
		{
			SwitchPageById((uint16_t)page_st.detail_page); 
		}
        write_dgus_vp(R11_SCAN_ADDRESS,(uint8_t*)&uin16_port_zero,1);
    }else if(dgus_value == 0xA507)   
    {
		/** 返回摄像头主页面 */
        R11ChangePictureLocate(mainview.main_x_point,mainview.main_y_point,mainview.main_high,mainview.main_weight,0x00);
		R11CameraSendT5lCtrl(cameraMAGNIFIER_MODE,cameraOPEN_STATUS);
		if(page_st.main_flag == 0x5a)
		{
			SwitchPageById((uint16_t)page_st.main_page); 
		}
        write_dgus_vp(R11_SCAN_ADDRESS,(uint8_t*)&uin16_port_zero,1);
    }else if(dgus_value == 0xA508)
	{
		R11CameraUploadPicture();
		write_dgus_vp(R11_SCAN_ADDRESS,(uint8_t*)&uin16_port_zero,1);
	}else if(dgus_value == 0xA509)  
    {
		/** 从摄像头放大页面返回到详情页 */
		R11ClearPicture(0);
		R11ChangePictureLocate(mainview.detail_x_point,mainview.detail_y_point,mainview.detail_high,mainview.detail_weight,0x00);
		R11CameraSendT5lCtrl(cameraMAGNIFIER_MODE,cameraOPEN_STATUS);
        if(page_st.detail_flag == 0x5a)
		{
			SwitchPageById((uint16_t)page_st.detail_page); 
		}
		write_dgus_vp(R11_SCAN_ADDRESS,(uint8_t*)&uin16_port_zero,1);
    }else if(dgus_value == 0xA50A)
	{
		/** 
		 * @brief 关闭摄像头
		 * @note 首先发送0xa0指令关闭发送给t5l显示，再发送0xb5指令关闭摄像头进程
		 * @note 在这个过程不会清零变量地址的值，下一个周期会继续执行，直到摄像头打开完成，执行周期建议为为R11_TASK_INTERVAL
		 */
		if(screen_opt.camera_open_sta)
		{
			if(page_st.menu_flag == 0x5a)
			{
				SwitchPageById((uint16_t)page_st.menu_page); 
			}
			write_dgus_vp(R11_SCAN_ADDRESS, (uint8_t *)&uin16_port_zero, 1);
		}else
		{
			if(camera_process_state == CAMERA_PROCESS_END)
			{
				R11CameraSendT5lCtrl(cameraMAGNIFIER_MODE,cameraCLOSE_STATUS);
				camera_process_state = CAMERA_SEND_T5L_WAITING;
			}else if(camera_process_state == CAMERA_SEND_T5L_WAITING)
			{
				r11_state.delay_count++;
				if(r11_state.delay_count == cameraRETRY_MAX_COUNT)
				{
					r11_state.delay_count = 0;
					camera_process_state = CAMERA_PROCESS_END;
				}
			}else if(camera_process_state == CAMERA_SEND_T5L)
			{
				R11CameraOpenThreadCtrl(cameraMAGNIFIER_MODE,cameraCLOSE_STATUS,(uint8_t)screen_opt.jpeg_type);
				camera_process_state = CAMERA_OPEN_WAITING;
			}else if(camera_process_state == CAMERA_OPEN_WAITING)
			{
				r11_state.delay_count++;
				if(r11_state.delay_count == cameraRETRY_MAX_COUNT)
				{
					r11_state.delay_count = 0;
					camera_process_state = CAMERA_SEND_T5L;
				}
			}else if(camera_process_state == CAMERA_INSERT_CHECK)
			{
				R11ClearPicture(0);
				if(page_st.menu_flag == 0x5a)
				{
					SwitchPageById((uint16_t)page_st.menu_page); 
				}
				write_dgus_vp(R11_SCAN_ADDRESS, (uint8_t *)&uin16_port_zero, 1);
			}
		}
    }else if(dgus_value == 0xA50B)
	{
		/** 从热插拔页面返回 */
		r11_send_buf[0] = 0xA5;
		r11_send_buf[1] = 0x01;
		write_dgus_vp(R11_SCAN_ADDRESS, r11_send_buf, 1);
	}else if(dgus_value == 0xA50E)
	{
		/** 摄像头画面放大 */
		read_dgus_vp(sysDGUS_SYSTEM_CONFIG, (uint8_t *)&rotate_angle, 1);
		if((rotate_angle & 0x0003) == 0x00 || (rotate_angle & 0x0003) == 0x02)
		{
			R11ChangePictureLocate(0,0,pixels_arr_h2[screen_opt.screen_ratio],pixels_arr_l2[screen_opt.screen_ratio],0x02);
		}else if((rotate_angle & 0x0003) == 0x01 || (rotate_angle & 0x0003) == 0x03)
		{
			R11ChangePictureLocate(0,0,pixels_arr_l2[screen_opt.screen_ratio],pixels_arr_h2[screen_opt.screen_ratio],0x02);
		}
		R11CameraSendT5lCtrl(cameraMAGNIFIER_MODE,cameraOPEN_STATUS);
		write_dgus_vp(R11_SCAN_ADDRESS, (uint8_t *)&uin16_port_zero, 1);
	}else if(dgus_value == 0xA50f)
	{
		/** 摄像头画面还原 */
		R11ChangePictureLocate(mainview.main_x_point,mainview.main_y_point,mainview.main_high,mainview.main_weight,0x02);
		R11CameraSendT5lCtrl(cameraMAGNIFIER_MODE,cameraCLOSE_STATUS);
		write_dgus_vp(R11_SCAN_ADDRESS, (uint8_t *)&uin16_port_zero, 1);
	}else if(dgus_value >= 0xA510 && dgus_value < 0xA516)
	{
		/** 选择哪一个缩略图进行放大 */
		r11_state.now_choose_pic = dgus_value-0xa510;
        write_dgus_vp(cameraNOW_NUM_ADDR,(uint8_t*)&r11_state.now_choose_pic,1);
        if(camera_magnifier.camera_num[r11_state.now_choose_pic] == 0)
        {
            write_dgus_vp(R11_SCAN_ADDRESS, (uint8_t *)&uin16_port_zero, 1);
            return;
        }
        if(page_st.crop_flag == 0x5a)
		{
			SwitchPageById((uint16_t)page_st.crop_page); 
		}
		R11ChangePictureLocate(mainview.crop_x_point,mainview.crop_y_point,mainview.crop_high,mainview.crop_weight,0x02);
        enl_enlarge_mode.enlarge_start_X = 0;
        enl_enlarge_mode.enlarge_start_Y = 0;
        enl_enlarge_mode.enlarge_cap_high = camera_magnifier.camera_show_high;
        enl_enlarge_mode.enlarge_cap_width = camera_magnifier.camera_show_width;
        R11MagnifierEnlarge(cameraENL_PICTURE,NULL,&enl_enlarge_mode);
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
        MagnifierKeyHandle(dgus_value);
    }
	else if((dgus_value>>8) >= 0x50 && (dgus_value>>8) <= 0x52)
	{
		R11CameraEnlargeHandle(dgus_value);
	}
	else if((dgus_value>>8) == 0x00 && (dgus_value & 0x00FF) != 0x00)
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



static void R11FlagBitInit(void)
{
	/** 1.针对当前页面调整显示大小 */
    R11PageInitChange();
    /** 2.针对摄像头显示参数进行初始化 */
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

    /** 3.针对缩放参数进行初始化 */
    enl_enlarge_mode.enlarge_x_acc = ACC_X;
    enl_enlarge_mode.enlarge_y_acc = ACC_Y;
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
	uint8_t r11_send_buf[10];
	uint16_t quanity_set,fclk_div;
	r11_send_buf[0] = 0xAA;
	r11_send_buf[1] = 0x55;
	r11_send_buf[2] = 0x00;
	r11_send_buf[3] = 0x04; 
	r11_send_buf[4] = 0xd0; 
	read_dgus_vp(QUANITY_SET_ADDR,(uint8_t*)&quanity_set,1);
	r11_send_buf[5]=(uint8_t)quanity_set;
	read_dgus_vp(FCLK_DIV_ADDR,(uint8_t*)&fclk_div,1);
	r11_send_buf[6]=(uint8_t)fclk_div;
	r11_send_buf[7]=Bit8_16_Flag*8;
	UartSendData(&Uart_R11,r11_send_buf,8);
}

static void R11RestartInit(void)
{
    R11FlagBitInit();
    R11CameraInit();
	R11StartPowerInit();
}


void UartR11UserBeautyProtocol(UART_TYPE *uart,uint8_t *frame, uint16_t len)
{
	uint16_t write_param[10],now_pic;
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
    }else if(frame[0] == 0xAA && frame[1] == 0x55)
    {
        if(len < 6 || len < ((frame[2]<<8|frame[3])+4))
        {
            return;
        }
        switch (frame[4])
        {
			case cameraSUPPORT_FORMAT:
				write_dgus_vp(addr_st.camera_para_addr,&frame[8], ( frame[7]+1 ) /2 );
				break;
			case cameraOPEN_THREAD:
				if(frame[5] == R11_RECV_OK&&frame[6] == camera_magnifier.camera_type&&frame[7] == cameraCLOSE_STATUS)
				{
					camera_process_state = CAMERA_INSERT_CHECK;
					if(page_st.hotplug_flag == 0x5a)
					{
						SwitchPageById((uint16_t)page_st.hotplug_page); 
					}
					r11_state.delay_count = 0;
				}else if(frame[5] == R11_RECV_OK&&frame[6] == camera_magnifier.camera_type&&frame[7] == cameraOPEN_STATUS)
				{
					camera_process_state = CAMERA_SEND_T5L;
					r11_state.delay_count = 0;
				}
				break;
			case cameraINSERT_CHECK:
				if(frame[5] == cameraCLOSE_STATUS)
				{
					camera_process_state = CAMERA_PROCESS_END;
					read_dgus_vp(sysDGUS_PIC_NOW,(uint8_t*)&now_pic,1);
					if(page_st.hotplug_flag == 0x5a&&(now_pic == page_st.main_page||now_pic == page_st.detail_page))
					{
						SwitchPageById((uint16_t)page_st.hotplug_page); 
					}
					r11_state.delay_count = 0;
				}else if(frame[5] == cameraOPEN_STATUS)
				{
					camera_process_state = CAMERA_OPEN_THREAD;
					r11_state.delay_count = 0;
				}
				break;
			case cameraSEND_T5L:
				if(frame[5] == R11_RECV_OK&&frame[6] == camera_magnifier.camera_type)
				{
					if(camera_process_state == CAMERA_SEND_T5L_WAITING)
					{
						read_dgus_vp(R11_SCAN_ADDRESS,(uint8_t*)&write_param[0],1);
						if(write_param[0] == 0xa501)
						{
							camera_process_state = CAMERA_PROCESS_END;
						}else if(write_param[0] == 0xa50a)
						{
							camera_process_state = CAMERA_SEND_T5L;
						}
					}
					r11_state.delay_count = 0;
				}
				break;
			case cameraENL_PICTURE:
				if ( frame[5] == R11_RECV_OK )
				{

				}else
				{
					enl_enlarge_mode.enlarge_start_X = enl_enlarge_mode.enlarge_start_X_last;
					enl_enlarge_mode.enlarge_start_Y = enl_enlarge_mode.enlarge_start_Y_last;
					enl_enlarge_mode.enlarge_cap_high = enl_enlarge_mode.enlarge_cap_high_last;
					enl_enlarge_mode.enlarge_cap_width = enl_enlarge_mode.enlarge_cap_width_last;
				}
				break;
			case cameraCAP_PICTURE:
				__NOP();
				break;
			case cameraCAP_BY_BUTTON:
				if (camera_process_state == CAMERA_PROCESS_END )
				{
					/** 截图一次 */
					write_param[0] = 0xa502;
					write_dgus_vp(R11_SCAN_ADDRESS,(uint8_t*)&write_param[0],1);
				}
				break;
			default:
				break;
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
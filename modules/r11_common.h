#ifndef R11_COMMON_H
#define R11_COMMON_H
#include "sys.h"
#include "uart.h"
#include "T5LOSConfig.h"
#include "core_json.h"             

#if sysBEAUTY_MODE_ENABLED

typedef struct 
{
	uint8_t menu_flag;           /* 菜单页面标志 */
	uint8_t menu_page;           /* 菜单页面编号 */
	
	uint8_t main_flag;           /* 主页面标志,广告屏不包含，设置为00 */
	uint8_t main_page;           /* 主页面编号,广告屏不包含，设置为00 */
	
	uint8_t detail_flag;         /* 详细页面标志,广告屏不包含，设置为00 */
	uint8_t detail_page;         /* 详细页面编号,广告屏不包含，设置为00 */
	
	uint8_t hotplug_flag;        /* 热插拔页面标志,广告屏不包含，设置为00 */
	uint8_t hotplug_page;        /* 热插拔页面编号,广告屏不包含，设置为00 */
	
	uint8_t crop_flag;           /* 裁剪页面标志,广告屏不包含，设置为00 */
	uint8_t crop_page;           /* 裁剪页面编号,广告屏不包含，设置为00 */
	
	uint8_t video_flag;          /* 视频页面标志 */
	uint8_t video_page;          /* 视频页面编号 */
	
	uint8_t upload_flag;         /* 上传页面标志,广告屏不包含，设置为00 */
	uint8_t upload_page;         /* 上传页面编号,广告屏不包含，设置为00 */
	
	uint8_t fullvideo_flag;      /* 全屏视频页面标志 */
	uint8_t fullvideo_page;      /* 全屏视频页面编号 */
} PAGE_S;
extern PAGE_S page_st;

typedef enum
{
	/* 视频初始化进程，0为未初始化，1为调整音量，2为设置大小，3为查询循环模式，4为查询视频，5为播放视频，6为设置循环，7为初始化完成 */
	VIDEO_PROCESS_UNINIT = 0,
	VIDEO_PROCESS_VOLUME,
	VIDEO_PROCESS_SIZE,
	VIDEO_PROCESS_SEARCH_LOOP,
	VIDEO_PROCESS_QUERY,
	VIDEO_PROCESS_PLAY,
	VIDEO_PROCESS_SET_LOOP,
	VIDEO_PROCESS_COMPLETE
}VIDEO_INIT_PROCESS;

/** 图标叠加相关控件定义区域 */
#define Icon_Overlay_SP_Mode 0xFF00
extern code uint16_t Icon_Overlay_SP[11];
extern uint32_t Icon_Overlay_SP_VP[11];
extern uint16_t Icon_Overlay_SP_X[10];  
extern uint16_t Icon_Overlay_SP_Y[10];
extern uint16_t Icon_Overlay_SP_L[10];
extern uint16_t Icon_Overlay_SP_H[10]; 
extern uint16_t Locate_arr[10]; 
extern VIDEO_INIT_PROCESS video_init_process; 


/** 视频播放相关定义区域 */
#define SDCARD 1
#define exUDISK 2
#define UDISK 3

#define MP4 0
#define MP3 1
#define JPG 2

#define MAX_MP3_NAME_LEN  		32
#define MAX_WIFI_NAME_LEN 		32
#define MP4_FILENAME_VP_LIST1       0x0610
#define MP4_FILENAME_VP_LIST2       0x0620
#define MP4_FILENAME_VP_LIST3       0x0630
#define MP4_FILENAME_VP_LIST4       0x0640
#define MP4_FILENAME_VP_LIST5       0x0650
#define VOLUME_SET_ADDR          	0x06D0
#define PLAY_STATUS_ADDR		 	0x06D1
#define LOOP_MODE_ADDR          	0x06D2
#define AUTOPLAY_MODE_ADDR      	0x06D3
#define R11_RESTART_FLAG_ADDR      	0x06D4
#define BIG_SMALL_FLAG_ADDR      	0x06D5
#define MP4_NOW_PLAY_NAME_ADDR	  	0x3400

/** 视频播放指令定义区域 */
#define cmdMP4_UPDATEFILE         	0x0061
#define cmdMP4_PREVFILE           	0x0062
#define cmdMP4_NEXTFILE           	0x0063
#define cmdMP4_PLAY                	0x0064
#define cmdMP4_PAUSE               	0x0065
#define cmdMP4_REPLAY              	0x0066
#define cmdMP4_STOP					0x0067
#define cmdMP4_AUX_SET              0x006b
#define cmdMP4_IMG_SET              0x0076    /* 0x76为小图播放，0x77为大图播放*/
#define cmdMP4_ROTATE_ANGLE         0x0079
#define cmdMP4_LOOP_MODE_SET	   	0x0090

/** 视频播放按键定义区域 */
#define keyMP4_PAUSE				0x0004
#define keyMP4_STOP                 0x0005
#define keyMP4_AUX_CLOSE			0x000a
#define keyMP4_AUX_ADD				0x000b
#define keyMP4_AUX_SUB				0x000c
#define keyMP4_AUX_SET				0x000f
#define keyMP4_EXIT				 	0x000d
#define keyMP4_REPLAY               0x0013
#define keyMP4_UPDATE_SDCARD       	0x0020
#define keyMP4_UPDATE_exUDISK      	0x002a
#define keyMP4_UPDATE_UDISK        	0x002b
#define keyMP4_1FILE                0x0021
#define keyMP4_2FILE                0x0022
#define keyMP4_3FILE                0x0023
#define keyMP4_4FILE                0x0024
#define keyMP4_5FILE                0x0025
#define keyMP4_IMG_SET_BIG			0x0026
#define keyMP4_IMG_SET_SMALL		0x0027
#define keyMP4_ROTATE_ANGLE			0x0029
#define keyMP4_NONE_LOOP_MODE 	 	0x0036
#define keyMP4_SINGLE_LOOP_MODE   	0x0037
#define keyMP4_ALL_LOOP_MODE      	0x0038
#define keyMP4_NEXTFILE            	0x0002
#define keyMP4_PREVFILE            	0x0001


#define DirPath(Index) ((Index==1)?("url:/mnt/SDCARD/VIDEO/"):\
					   ((Index==2)?("url:/mnt/exUDISK/VIDEO/"):\
                       ((Index==3)?("url:/mnt/UDISK/VIDEO/"):("url:/mnt/exUDISK/VIDEO/"))))

typedef struct play_state
{
    uint8_t r11_volunme; 
	uint8_t r11_volunme_Bak;

    uint8_t state:3; 
    uint8_t rotate:2;
	uint8_t save:3;      /* 保留字段*/


    uint8_t store_type;       
	uint8_t Document_type;    /* 0:MP4 1:MP3 2:JPG */
    uint8_t page_mp4_nums; 

    uint16_t serial; 
} PLAYER_T;

extern uint16_t mp4_name_len[5];
extern uint8_t mp4_name[5][MAX_MP3_NAME_LEN];
extern PLAYER_T r11_player;

extern volatile uint8_t data data_write_f;
extern volatile uint8_t data Bit8_16_Flag;

#define Init_Jpeg_Parament();	\
                buf_os_tail         = 0x8000;\
				buf_tail            = 0x8000;\
				Packet_Count        = 0;\
				Packet_TotalCount   = 0;\
				END_CRC             = 0xFF;


#define EX0_Start();	\
		IE0 = 0;\
		EX0 = 1; \
        pic_time_cnt = 50;

#define EX0_Stop();	\
		EX0 = 0;

#define EX1_Start();	\
		IE1 = 0;\
		EX1 = 1;

#define EX1_Stop();	\
		EX1 = 0;


void T5lJpegInit(void);

void change_pic_locate(uint16_t x_point,uint16_t y_point,uint16_t high,uint16_t weight,uint8_t big_small_flag);

void R11VideoPlayerProcess(void);

void UartR11UserVideoProtocal(UART_TYPE *uart,uint8_t *frame, uint16_t len);

void R11VideoValueScanTask(uint16_t dgus_value);

void T5lSendUartDataToR11( uint8_t cmd, uint8_t *buf);

void Judge_Packet_Count(void);

void Display_Debug_Message(void);

#endif /* sysBEAUTY_MODE_ENABLED */

#endif /* R11_COMMON_H */
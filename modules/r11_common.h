#ifndef R11_COMMON_H
#define R11_COMMON_H
#include "sys.h"
#include "T5LOSConfig.h"
#include "core_json.h"             

#if sysBEAUTY_MODE_ENABLED
/** 图标叠加相关控件定义区域 */
#define Icon_Overlay_SP_Mode 0xFF00
extern code uint16_t Icon_Overlay_SP[11];
extern uint32_t Icon_Overlay_SP_VP[11];
extern uint16_t Icon_Overlay_SP_X[10];  
extern uint16_t Icon_Overlay_SP_Y[10];
extern uint16_t Icon_Overlay_SP_L[10];
extern uint16_t Icon_Overlay_SP_H[10]; 
extern uint16_t Locate_arr[10]; 
#define Judge_Packet_Count()   


/** 视频播放相关宏定义区域 */
#define MAX_MP3_NAME_LEN  		32
#define MAX_WIFI_NAME_LEN 		32

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

void T5lSendUartDataToR11( uint8_t cmd, uint8_t *buf);
#endif /* sysBEAUTY_MODE_ENABLED */

#endif /* R11_COMMON_H */
#ifndef R11_N5CAMERA_H
#define R11_N5CAMERA_H

#include "sys.h"
#include "T5LOSConfig.h"
#include <string.h>
#include "uart.h"
#include "r11_common.h"

#if sysN5CAMERA_MODE_ENABLED
#define PIXELS_SET_ADDR         0x0580      


#define VIDEO_HIGH_ADDR         0x0586
#define VIDEO_WIDTH_ADDR        0x0587
#define VIDEO_XPOINT_ADDR       0x0588
#define VIDEO_YPOINT_ADDR       0x0589

#define MAIN_HIGH_ADDR          0x058A
#define MAIN_WIDTH_ADDR         0x058B
#define MAIN_XPOINT_ADDR        0x058C
#define MAIN_YPOINT_ADDR        0x058D

#define DETAIL_HIGH_ADDR        0x058E
#define DETAIL_WIDTH_ADDR       0x058F
#define DETAIL_XPOINT_ADDR      0x0590
#define DETAIL_YPOINT_ADDR      0x0591

#define CROP_HIGH_ADDR          0x0592
#define CROP_WIDTH_ADDR         0x0593
#define CROP_XPOINT_ADDR        0x0594
#define CROP_YPOINT_ADDR        0x0595

#define MENU_SCREEN_ADDR        0x05a4   
#define MAIN_SCREEN_ADDR        0x05a5   
#define DETAIL_SCREEN_ADDR      0x05a6
#define HOT_PLUG_ADDR           0x05a7
#define CROP_SCREEN_ADDR        0x05a8
#define VIDEO_SCREEN_ADDR       0x05a9
#define UPLOAD_SCREEN_ADDR      0x05aa


#define VIDEO_FULL_ADDR         0x05c0


#define R11_TASK_INTERVAL       100
#define R11_SCAN_ADDRESS     	(uint32_t)0x0600

#define cmdN5_CAMERA_OPEN        0x71
#define cmdN5_CAMERA_CLOSE       0x7b


typedef struct
{
	uint16_t screen_ratio;           /* 屏幕分辨率 */
}SCREEN_S;

typedef struct 
{
    uint16_t video_high;         /* 视频显示高度 */
	uint16_t video_weight;       /* 视频显示宽度 */
	uint16_t video_x_point;      /* 视频显示X坐标 */
	uint16_t video_y_point;      /* 视频显示Y坐标 */
	
	uint16_t main_high;          /* 主视图高度 */
	uint16_t main_weight;        /* 主视图宽度 */
	uint16_t main_x_point;       /* 主视图X坐标 */
	uint16_t main_y_point;       /* 主视图Y坐标 */
	
	uint16_t detail_high;        /* 详细视图高度 */
	uint16_t detail_weight;      /* 详细视图宽度 */
	uint16_t detail_x_point;     /* 详细视图X坐标 */
	uint16_t detail_y_point;     /* 详细视图Y坐标 */
	
	uint16_t crop_high;          /* 裁剪区域高度 */
	uint16_t crop_weight;        /* 裁剪区域宽度 */
	uint16_t crop_x_point;       /* 裁剪区域X坐标 */
	uint16_t crop_y_point;       /* 裁剪区域Y坐标 */
} MAINVIEW_S;


typedef struct 
{
	uint16_t restart_flag;		  /* 重启标志，0为未重启，1为重启 */
}R11_STATE;

extern MAINVIEW_S mainview;
extern SCREEN_S screen_opt;
extern R11_STATE r11_state;

void R11ConfigInitFormLib(void);
void R11N5CameraTask(void);
void UartR11UserN5CameraProtocol(UART_TYPE *uart,uint8_t *frame, uint16_t len);
#endif  /* sysN5CAMERA_MODE_ENABLED */

#endif /* R11_N5CAMERA_H */
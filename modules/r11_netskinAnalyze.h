#ifndef R11_NETSKINANALYZE_H
#define R11_NETSKINANALYZE_H

#include "sys.h"
#include "T5LOSConfig.h"
#include "core_json.h"
#include <string.h>
#include "uart.h"
#include "r11_common.h"


#if sysBEAUTY_MODE_ENABLED
/** 从lib文件读取的定义区域 */
#define PIXELS_SET_ADDR         0x0580      
#define FCLK_DIV_ADDR           0x0581
#define QUANITY_SET_ADDR        0x0582
#define CAMERA_NUM_ADDR         0x0583

#define JPEG_TYPE_ADDR          0x0584      
#define CAMERA_PIX_ADDR         0x0585


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

#define PIC_HIGH_ADDR           0x0596
#define PIC_WEIGHT_ADDR         0x0597
#define PIC1_XP_ADDR            0x0598
#define PIC1_YP_ADDR            0x0599
#define PIC2_XP_ADDR            0x059a
#define PIC2_YP_ADDR            0x059b
#define PIC3_XP_ADDR            0x059c
#define PIC3_YP_ADDR            0x059d
#define PIC4_XP_ADDR            0x059e
#define PIC4_YP_ADDR            0x059f
#define PIC5_XP_ADDR            0x05a0
#define PIC5_YP_ADDR            0x05a1
#define PIC6_XP_ADDR            0x05a2
#define PIC6_YP_ADDR            0x05a3

#define MENU_SCREEN_ADDR        0x05a4   
#define MAIN_SCREEN_ADDR        0x05a5   
#define DETAIL_SCREEN_ADDR      0x05a6
#define HOT_PLUG_ADDR           0x05a7
#define CROP_SCREEN_ADDR        0x05a8
#define VIDEO_SCREEN_ADDR       0x05a9
#define UPLOAD_SCREEN_ADDR      0x05aa


#define USER_TEL_ADDR           0x05ab
#define USER_NAME_ADDR          0x05ac
#define APP_QR_ADDR             0x05ad
#define IP_ADDR                 0x05ae
#define MAC_ADDR                0x05af
#define CLERK_QR_ADDR           0x05b0
#define BROADCAST_QR_ADDR       0x05b1
#define OFFLINE_QR_ADDR         0x05b2
#define CAMERA_PARA_ADDR        0x05b3         
#define T5L_VER_ADDR            0x05b4
#define R11_VER_ADDR            0x05b5
#define CMD_82_RETURN_ADDR      0x05b6   
#define RTC_FLAG_ADDR           0x05b7   
#define WIFI_SCAN_PAGE_ADDR     0x05b8
#define WIFI_DETAIL_PAGE_ADDR   0x05b9
#define WIFI_SCAN_TRANSIT_ADDR  0x05ba
#define WIFI_DTAL_TRANSIT_ADDR  0x05bb
#define WIFI_COMIC_FLAG_ADDR    0x05bc
#define STORE_PAGE_ADDR         0x05bd
#define WIFI_START_ADDR         0x05be
#define CAMERA_OPEN_ADDR        0x05bf
#define VIDEO_FULL_ADDR         0x05c0



/** 美容屏相关结构体定义区域 */
typedef struct
{
	uint8_t camera_type;   /* 摄像头类型，0是魔镜分析，1是放大镜 */
	uint8_t camera_way;    /* 摄像头路，默认单路0 */
	uint16_t camera_show_high;  /* 摄像头显示高 */
	uint16_t camera_show_width;  /* 摄像头显示宽 */
    uint16_t camera_cap_high;  /* 摄像头拍照高，这个是针对缩略图的大小 */
	uint16_t camera_cap_width;  /* 摄像头拍照宽 */
	uint16_t camera_col_high;  /* 摄像头采集高，这个是针对r11采集的大图的分辨率 */
	uint16_t camera_col_width;  /* 摄像头采集宽 */
    uint16_t camera_send_high;   /* 发给r11采集的大小，这个是针对r11发给云端的图像大小 */
    uint16_t camera_send_width;  /* 发给r11采集的宽度 */
    uint8_t camera_local;        /* 摄像头显示位置，从0开始 */
    uint8_t camera_status;       /* 是否发给摄像头显示，0不显示，1显示 */
    uint8_t camera_num[6];        /* 当前摄像头显示的编号，默认0-5 */
    uint8_t camera_process;    /* 当前摄像头发送指令状态，0为没发送b5指令，1为已经发送b5指令没发送a0指令，2为已经发送a0指令 */
    uint8_t cap_status;          /* 摄像头拍照状态标志 */
} CAMERA_MA;


typedef struct 
{
    char   send_cmd[32];               /* api的接口名 */
    char   send_ip[32];                /* 发送的ip地址 */
    char   send_mac[32];               /* mac地址 */
    char   cpuinfo[32];                /* r11的cpuinfo */
    char   websocket_url[64];          /* 发送的websocket的地址 */
    char   weixin_url[128];          /* 微信小程序的二维码 */
    char   store_url[128];           /* 店铺的二维码 */
    char   weixin_id[32];           /* 微信的id */
    char   open_id[32];             /* 微信的openid */
    char   weixin_tel[32];          /* 微信的电话 */
    char   weixin_name[32];         /* 微信的名字 */
    uint16_t remove_flag;           /* 清除成功的标记 */
} WECHAT_JSON;


typedef struct
{
	uint8_t enlarge_x_acc;   /* 放大的x平移步进参数 */
    uint8_t enlarge_y_acc;   /* 放大的y平移步进参数 */
	uint16_t enlarge_start_X;    /* 放大起始位置的x坐标 */
	uint16_t enlarge_start_Y;  /* 放大起始位置的y坐标 */
    
	uint16_t enlarge_cap_high;  /* 放大的截取高，每次减少为步进参数 */
    uint16_t enlarge_cap_width;  /* 放大的截取宽，每次减少为步进参数 */
    
    uint16_t enlarge_start_X_last;    /* 放大起始位置的x坐标备份 */
	uint16_t enlarge_start_Y_last;  /* 放大起始位置的y坐标备份 */
    
	uint16_t enlarge_cap_high_last;  /* 放大的截取高，每次减少为步进参数备份 */
    uint16_t enlarge_cap_width_last;  /* 放大的截取宽，每次减少为步进参数备份 */
    
    uint16_t enlarge_max_high;  /* 放大的边界最大高 */
    uint16_t enlarge_max_width;  /* 放大的边界最大宽 */
    
    uint16_t enlarge_min_high;  /* 放大的边界最小高 */
    uint16_t enlarge_min_width;  /* 放大的边界最小宽 */
    
    uint16_t enlarge_show_high;  /* 放大的显示高，当前固定 */
    uint16_t enlarge_show_width;  /* 放大的显示宽，当前固定 */
} ENLARGE_P;

typedef struct 
{
	uint16_t user_tel_addr;      /* 用户电话号码地址 */
	uint16_t user_name_addr;     /* 用户姓名地址 */
	uint16_t app_qr_addr;        /* APP二维码地址 */
	uint16_t ip_addr;            /* IP地址 */
	uint16_t mac_addr;           /* MAC地址 */
	uint16_t clerk_qr_addr;      /* 店员二维码地址 */
	uint16_t broadcast_qr_addr;  /* 广播二维码地址 */
	uint16_t offline_qr_addr;    /* 离线二维码地址 */
	uint16_t camera_para_addr;   /* 摄像头参数地址 */
	uint16_t t5l_ver_addr;       /* T5L版本地址 */
	uint16_t r11_ver_addr;       /* R11版本地址 */
} ADDR_S;


typedef struct 
{
	uint16_t high;               /* 缩略图高度 */
	uint16_t weight;             /* 缩略图宽度 */
	uint16_t pic1_x_point;       /* 图片1的X坐标 */
	uint16_t pic1_y_point;       /* 图片1的Y坐标 */
	uint16_t pic2_x_point;       /* 图片2的X坐标 */
	uint16_t pic2_y_point;       /* 图片2的Y坐标 */
	uint16_t pic3_x_point;       /* 图片3的X坐标 */
	uint16_t pic3_y_point;       /* 图片3的Y坐标 */
	uint16_t pic4_x_point;       /* 图片4的X坐标 */
	uint16_t pic4_y_point;       /* 图片4的Y坐标 */
	uint16_t pic5_x_point;       /* 图片5的X坐标 */
	uint16_t pic5_y_point;       /* 图片5的Y坐标 */
	uint16_t pic6_x_point;       /* 图片6的X坐标 */
	uint16_t pic6_y_point;       /* 图片6的Y坐标 */
} THUMBNAIL_S;


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
	uint8_t menu_flag;           /* 菜单页面标志 */
	uint8_t menu_page;           /* 菜单页面编号 */
	
	uint8_t main_flag;           /* 主页面标志 */
	uint8_t main_page;           /* 主页面编号 */
	
	uint8_t detail_flag;         /* 详细页面标志 */
	uint8_t detail_page;         /* 详细页面编号 */
	
	uint8_t hotplug_flag;        /* 热插拔页面标志 */
	uint8_t hotplug_page;        /* 热插拔页面编号 */
	
	uint8_t crop_flag;           /* 裁剪页面标志 */
	uint8_t crop_page;           /* 裁剪页面编号 */
	
	uint8_t video_flag;          /* 视频页面标志 */
	uint8_t video_page;          /* 视频页面编号 */
	
	uint8_t upload_flag;         /* 上传页面标志 */
	uint8_t upload_page;         /* 上传页面编号 */
	
	uint8_t fullvideo_flag;      /* 全屏视频页面标志 */
	uint8_t fullvideo_page;      /* 全屏视频页面编号 */
} PAGE_S;

typedef struct 
{
	uint8_t scan_flag;           /* WiFi扫描页面标志 */
	uint8_t scan_page;           /* WiFi扫描页面编号 */
	
	uint8_t detail_flag;         /* WiFi详情页面标志 */
	uint8_t detail_page;         /* WiFi详情页面编号 */
	
	uint8_t tr_scan_flag;        /* WiFi扫描传输页面标志 */
	uint8_t tr_scan_page;        /* WiFi扫描传输页面编号 */
	
	uint8_t tr_detail_flag;      /* WiFi详情传输页面标志 */
	uint8_t tr_detail_page;      /* WiFi详情传输页面编号 */
	
	uint8_t comic_flag;          /* WiFi连接页面标志 */
	uint8_t comic_page;          /* WiFi连接页面编号 */
	
	uint8_t store_flag;          /* 商店页面标志 */
	uint8_t store_page;          /* 商店页面编号 */
	
	uint8_t start_flag;          /* WiFi启动页面标志 */
	uint8_t start_page;          /* WiFi启动页面编号 */
	
	uint16_t wifi_start_addr;    /* WiFi启动地址 */
} WIFI_PAGE_S;


typedef struct
{
	uint16_t screen_ratio;           /* 屏幕分辨率 */
	uint16_t fclk_div;               /* 分频系数 */
	uint16_t quality_set;            /* 摄像头质量 */
	uint16_t thumbnail_num;          /* 缩略图数量 */
	uint16_t jpeg_type;              /* 摄像头支持格式 */
	uint16_t camera_pix;             /* 摄像头支持分辨率 */
	uint16_t cmd_82_return_flag;     /* 8283日志返回开关 */
	uint16_t r11_ver[32];            /* r11固件版本号 */
	uint16_t t5l_ver[32];            /* t5l程序版本号 */
	uint16_t camera_open_sta;        /* 摄像头默认打开关闭 */
}SCREEN_S;

typedef enum
{
    CAMERA_INSERT_DETECT = 0,      /* 摄像头插入检测，0xb8指令 */
    CAMERA_INSERT_WAITING,         /* 摄像头已经发送0xb8指令，等待回复 */
    CAMERA_OPEN_PROCESS,           /* 摄像头打开进程，0xb5指令 */
    CAMERA_OPEN_WAITING,          /* 摄像头打开进程等待回复 */
    CAMERA_SEND_T5L,            /* 摄像头发送给t5l的指令，0xa0指令 */
    CAMERA_SEND_T5L_WAITING,    /* 摄像头发送给t5l的指令等待回复 */
    CAMERA_PROCESS_END,         /* 摄像头打开完成 */
}CAMERA_PROCESS_STATE;

typedef enum
{
    NET_NOT_CONNECTED = 0,    /* 没有连上互联网 */
    NET_CONNECTED_IP,        /* 获取到ip */
    NET_CONNECTED_CLOUD,      /* 连上云端 */
}NET_CONNECTED_STATE;

extern CAMERA_MA camera_magnifier;
extern ENLARGE_P enlarge_mode,net_enlarge_mode,enl_enlarge_mode;
extern ADDR_S addr_st;
extern THUMBNAIL_S thumbnail;
extern MAINVIEW_S mainview;
extern PAGE_S page_st;
extern WECHAT_JSON Json_Wechat;
extern SCREEN_S screen_opt;
extern WIFI_PAGE_S wifi_page;
extern CAMERA_PROCESS_STATE camera_process_state;
extern NET_CONNECTED_STATE net_connected_state;
extern uint16_t r11_restart_flag;
extern uint16_t r11_pic_capture_flag; /* 摄像头拍照标志，0为未拍照，1为拍照成功 */
extern uint8_t r11_now_choose_pic;

void R11ConfigInitFormLib(void);

void R11ValueScanTask(void);

void R11FlagBitInit(void);

void R11RestartInit(void);

void R11NetskinAnalyzeTask(void);

void UartR11UserBeautyProtocol(UART_TYPE *uart,uint8_t *frame, uint16_t len);
#endif /* sysBEAUTY_MODE_ENABLED */

#endif /* R11_NETSKINANALYZE_H */
#ifndef __MQTTSSL_WORKER_H__
#define __MQTTSSL_WORKER_H__

#include "config.h"

#include <mosquitto.h>
#include <stdint.h>

/*

{
	"code": 0,
	"message": "OK",
	"data": {
		"server_time": 1784868753,
		"cloud_region": "CN",
		"api_endpoint": "https:\/\/test-api-cn.mcqx.pet",
		"endpoint_config": {
			"sales_country_code": "CN",
			"cloud_region": "CN",
			"api_endpoint": "https:\/\/test-api-cn.mcqx.pet",
			"mqtt_endpoint": "mqtts:\/\/test-mqtt-cn.mcqx.pet:8883",
			"first_api_endpoint": "https:\/\/test-api-cn.mcqx.pet",
			"endpoint_version": 1,
			"endpoint_ttl_seconds": 86400
		},
		"bind_status": "BOUND",
		"device_status": "BOUND",
		"mqtt": {
			"endpoint": "mqtts:\/\/test-mqtt-cn.mcqx.pet:8883",
			"client_id": "device:MQXQ-PET-CN-2026-000002",
			"username": "device:MQXQ-PET-CN-2026-000002",
			"password": "gauNdASEbfkxm0DLOGMjuwQSUJZtVcHA6_tsS2y8I4k",
			"keepalive_seconds": 60,
			"credential_expire_at": 1785473553
		},
		"report_policy": {
			"heartbeat_interval_seconds": 60,
			"snapshot_interval_seconds": 3600,
			"property_report_min_interval_seconds": 5,
			"event_upload_enabled": true,
			"log_upload_enabled": false
		},
		"required_actions": []
	},
	"request_id": "REQ20260724125232b26bc890"
}



*/


/* endpoint_config 节点(从 hello 响应 data.endpoint_config 解析) */
typedef struct _endpoint_config {
	char sales_country_code[8];    /* 销售国家代码, 如 CN */
	char cloud_region[8];          /* 云区域, 如 CN */
	char api_endpoint[256];        /* API 端点地址 */
	char mqtt_endpoint[256];       /* MQTT 端点地址 */
	char first_api_endpoint[256];  /* 首选 API 端点地址 */
	int  endpoint_version;         /* 端点版本号 */
	int  endpoint_ttl_seconds;     /* 端点 TTL(秒) */
} EndpointConfig;

/* MQTT 连接配置信息(从 hello 响应 data.mqtt 节点解析) */
typedef struct _mqtt_config {
	char host[128];                /* MQTT broker 主机地址, 如 test-mqtt-cn.mcqx.pet */
	int  port;                     /* MQTT broker 端口, 如 8883 */
	char client_id[128];           /* MQTT client_id */
	char username[128];            /* MQTT 用户名 */
	char password[128];            /* MQTT 密码 */
	int  keepalive_seconds;        /* 心跳间隔(秒) */
	int64_t credential_expire_at;  /* 凭证过期时间(Unix 时间戳) */
	char api_endpoint[256];        /* API 端点地址(从 data.endpoint_config.api_endpoint) */
	EndpointConfig endpoint_config; /* 端点配置(完整保留) */
	char bind_status[32];          /* 绑定状态: BOUND / UNBOUND (从 data.bind_status) */
	char device_status[32];        /* 设备状态: BOUND / UNBOUND (从 data.device_status) */
	int  hello_getnew_flag;        /* 0=未获取, 1=hello 已获取配置, MQTT 可连接 */
	int  mqtt_connected;           /* 0=未连接, 1=已连接 */
} MqttConfig;

/* Bootstrap 配置信息(从 bootstrap 响应解析, 设备自举后持久化)
 * 屏幕自配网设备出厂仅烧 MAC + device_secret, 联网后调 bootstrap 取回身份。
 * 保存 device_sn / ble_id / endpoint / bind_status 四个核心状态。 */
typedef struct _bootstrap_config {
	char device_sn[64];           /* 设备 SN(服务端领取生成, hello 签名主体) */
	char ble_id[32];              /* BLE ID(6位短码, 屏幕二维码用) */
	char ble_name[64];            /* 蓝牙名称, 格式 MCQX-{ble_id前4位} */
	char qr_url[256];             /* 二维码 URL, https://b.mcqx.pet?b={ble_id} */
	char mac[16];                 /* MAC 地址(12位大写hex, 签名主体) */
	char product_key[64];         /* 产品 key */
	char sales_country_code[8];   /* 销售国家代码: CN / US / JP / KR */
	char cloud_region[8];         /* 云区域 */
	char api_endpoint[256];       /* API 端点地址(权威值, 后续请求走这里) */
	char mqtt_endpoint[256];      /* MQTT 端点地址(仅展示, 未绑定不可接入) */
	char first_api_endpoint[256]; /* 兜底首次接触地址 */
	int  endpoint_version;        /* 端点版本号 */
	char bind_status[32];         /* 绑定状态: UNBOUND / BINDING / BOUND */
	char bind_session_id[128];    /* 绑定会话 ID(仅 BINDING 时返回) */
	int  bootstrap_done;          /* 0=未自举, 1=已自举获取身份 */
} BootstrapConfig;

/* Bootstrap 自举结果 TLV (V851 → D5/LCD 串口下发)
 * 每次 bootstrap 轮询后, 将 device_sn / ble_id / endpoint / bind_status
 * 打包为 TLV 帧发送到串口, D5/LCD 据此渲染屏幕二维码和绑定状态 */
typedef struct _bootstrap_result_tlv {
	uint8_t struct_type;                /* 类型标识: TLV_STRUCT_BOOTSTRAP_RESULT (0x6C) */
	char    device_sn[64];             /* 设备 SN */
	char    ble_id[32];                /* BLE ID(6位短码) */
	char    api_endpoint[256];         /* API 端点地址 */
	char    bind_status[32];           /* 绑定状态: UNBOUND / BINDING / BOUND */
	char    qr_url[256];               /* 二维码 URL */
} BootstrapResultTlv;

/* 将 BootstrapConfig 转换为 BootstrapResultTlv 并打包 TLV 帧发送到串口(queue_2_uart)
 * D5/LCD 收到后解析 struct_type=0x6C, 显示二维码和绑定状态
 * 返回 0 成功, -1 失败 */
int bootstrap_result_send_uart(const BootstrapConfig *cfg);

/* ============================================================================
 * 状态字段字典 — 对应 device.snapshot 中 data.state 节点
 * 依据: ShowDoc §54 状态字段字典 (54.1 ~ 54.8)
 *
 * state 节点结构:
 *   "state": {
 *     "device":      { bind_status, work_status, online_status, fault_summary, faults[], lifecycle_status },
 *     "network":     { network_type, rssi, ip, mqtt_connected },
 *     "power":       { power_mode, battery_percent, charging },
 *     "door":        { door_status, lock_status, last_open_time },
 *     "environment": { temperature, humidity, pm25, co2 },
 *     "actuators":   { exhaust, light, uvb, anion, plasma, climate, humidifier, inlet_fan, filter },
 *     "camera":      { camera_status, stream_status, provider, video_codec, resolution, fps },
 *     "settings":    { temperature_unit, display, volume_percent, language, local_password_enabled }
 *   }
 * ============================================================================ */

/* ---- 54.1 设备基础状态 ---- */

/* 绑定状态: UNBOUND / BINDING / BOUND */
typedef enum {
    BIND_STATUS_UNBOUND = 0,
    BIND_STATUS_BINDING = 1,
    BIND_STATUS_BOUND   = 2
} BindStatus;

/* 工作状态: IDLE / WORKING / SLEEP / ERROR */
typedef enum {
    WORK_STATUS_IDLE    = 0,
    WORK_STATUS_WORKING = 1,
    WORK_STATUS_SLEEP   = 2,
    WORK_STATUS_ERROR   = 3
} WorkStatus;

/* 在线状态: ONLINE / OFFLINE (仅存档, App 在线态由服务端计算) */
typedef enum {
    ONLINE_STATUS_OFFLINE = 0,
    ONLINE_STATUS_ONLINE  = 1
} OnlineStatus;

/* 异常汇总等级: NORMAL / WARN / ERROR / CRITICAL (服务端按 faults[] 最高等级重算) */
typedef enum {
    FAULT_LEVEL_NORMAL   = 0,
    FAULT_LEVEL_WARN     = 1,
    FAULT_LEVEL_ERROR    = 2,
    FAULT_LEVEL_CRITICAL = 3
} FaultLevel;

/* 设备生命周期状态: ACTIVE / INACTIVE / REPLACED / SCRAPPED (服务端维护) */
typedef enum {
    LIFECYCLE_ACTIVE   = 0,
    LIFECYCLE_INACTIVE = 1,
    LIFECYCLE_REPLACED = 2,
    LIFECYCLE_SCRAPPED = 3
} LifecycleStatus;

/* ---- 54.1.1 device.faults 异常元素结构 ---- */
/* 约束: 不得包含 message/display_message/error_message 等自然语言字段 */
typedef struct _fault_entry {
    char    code[65];          /* 稳定机器码, regex ^[A-Z][A-Z0-9_]{0,63}$ */
    FaultLevel level;          /* 异常等级: INFO / WARN / ERROR / CRITICAL */
    char    module[33];        /* 功能模块, 如 TEMPERATURE/HUMIDIFIER/LIGHT/EXHAUST/ANION/UVB/PLASMA/FILTER/SYSTEM */
    char    source[33];        /* 异常来源, 如 DEVICE/SENSOR/MOTOR/NETWORK/SYSTEM */
    int64_t occurred_at;       /* 发生时间(Unix 秒) */
    int64_t cleared_at;        /* 恢复时间(Unix 秒, 0 表示未恢复) */
} FaultEntry;

/* ---- 54.2 网络状态 ---- */

/* 网络类型: wifi / 4g / ethernet */
typedef enum {
    NETWORK_TYPE_UNKNOWN  = 0,
    NETWORK_TYPE_WIFI     = 1,
    NETWORK_TYPE_4G       = 2,
    NETWORK_TYPE_ETHERNET = 3
} NetworkType;

/* ---- 54.3 电源状态 ---- */

typedef enum {
    POWER_MODE_AC      = 0,   /* 市电供电 */
    POWER_MODE_BATTERY = 1    /* 电池供电 */
} PowerMode;

/* ---- 54.4 门锁状态 ---- */

typedef enum {
    DOOR_STATUS_OPEN    = 0,
    DOOR_STATUS_CLOSED  = 1,
    DOOR_STATUS_UNKNOWN = 2
} DoorStatus;

typedef enum {
    LOCK_STATUS_LOCKED   = 0,
    LOCK_STATUS_UNLOCKED = 1,
    LOCK_STATUS_JAMMED   = 2,
    LOCK_STATUS_UNKNOWN  = 3
} LockStatus;

/* ---- 54.6 执行器状态 ---- */

/* 温控状态: OFF / HEATING / HOLDING / COOLING / UNKNOWN */
typedef enum {
    CLIMATE_CTRL_OFF      = 0,
    CLIMATE_CTRL_HEATING  = 1,
    CLIMATE_CTRL_HOLDING  = 2,
    CLIMATE_CTRL_COOLING  = 3,
    CLIMATE_CTRL_UNKNOWN  = 4
} ClimateControlStatus;

/* 雾化液位状态: NORMAL / LOW_WARNING / EMPTY / DRY_BURN */
typedef enum {
    LIQUID_NORMAL      = 0,
    LIQUID_LOW_WARNING = 1,
    LIQUID_EMPTY       = 2,
    LIQUID_DRY_BURN    = 3
} LiquidStatus;

/* ---- 54.7 摄像头状态 ---- */

typedef enum {
    CAMERA_STATUS_OFFLINE  = 0,
    CAMERA_STATUS_ONLINE   = 1,
    CAMERA_STATUS_ERROR    = 2,
    CAMERA_STATUS_DISABLED = 3
} CameraStatus;

typedef enum {
    STREAM_STATUS_IDLE      = 0,
    STREAM_STATUS_STARTING  = 1,
    STREAM_STATUS_STREAMING = 2,
    STREAM_STATUS_STOPPING  = 3,
    STREAM_STATUS_ERROR     = 4
} StreamStatus;

typedef enum {
    VIDEO_CODEC_H264 = 0,
    VIDEO_CODEC_H265 = 1
} VideoCodec;

typedef enum {
    RESOLUTION_480P  = 0,
    RESOLUTION_720P  = 1,
    RESOLUTION_1080P = 2
} Resolution;

/* ---- 54.8 设备本地设置 ---- */

typedef enum {
    SCREEN_MODE_SCREEN_SAVER = 0,
    SCREEN_MODE_STANDBY      = 1
} ScreenMode;

/* ---- command_ack 通用状态 ---- */

typedef enum {
    CMD_ACK_STATUS_FAILED  = 0,
    CMD_ACK_STATUS_SUCCESS = 1
} CmdAckStatus;

/* ============================================================================
 * 结构体类型标签 — 用于 TLV 增量上报时标识结构体类型
 * ============================================================================ */
#define TLV_STRUCT_DEVICE            0x01
#define TLV_STRUCT_NETWORK           0x02
#define TLV_STRUCT_POWER             0x03
#define TLV_STRUCT_DOOR              0x04
#define TLV_STRUCT_ENVIRONMENT       0x05
#define TLV_STRUCT_ACTUATOR          0x06
#define TLV_STRUCT_CAMERA            0x07
#define TLV_STRUCT_SETTINGS          0x08
#define TLV_STRUCT_FIRMWARE          0x09
#define TLV_STRUCT_STORAGE           0x0A

#define TLV_STRUCT_EXHAUST           0x61
#define TLV_STRUCT_LIGHT             0x62
#define TLV_STRUCT_UVB               0x63
#define TLV_STRUCT_ANION             0x64
#define TLV_STRUCT_PLASMA            0x65
#define TLV_STRUCT_CLIMATE           0x66
#define TLV_STRUCT_HUMIDIFIER        0x67
#define TLV_STRUCT_INLET_FAN         0x68
#define TLV_STRUCT_FILTER            0x69
#define TLV_STRUCT_FACTORY_DEVICE   0x6A  /* 工厂设备信息(出厂烧录上报) */
#define TLV_STRUCT_FACTORY_REJECTION 0x6B  /* 工厂设备上报被拒结果(V851 → D5) */
#define TLV_STRUCT_BOOTSTRAP_RESULT 0x6C  /* Bootstrap 自举结果(V851 → D5/LCD) */

/* TLV 消息命令字 */
#define TLV_CMD_PROPERTY            0x35  /* 设备属性上报(D5/LCD → V851) */
#define TLV_CMD_FACTORY_DEVICE      0x36  /* 工厂设备信息上报(D5/LCD → V851) */
#define TLV_CMD_BOOTSTRAP_RESULT    0x37  /* Bootstrap 自举结果下发(V851 → D5/LCD) */

/* ============================================================================
 * 各节点结构体
 * ============================================================================ */

/* 54.1 设备基础状态 */
typedef struct _device_state {
    uint8_t         struct_type;        /* 类型标识: TLV_STRUCT_DEVICE (0x01) */
    BindStatus      bind_status;        /* 绑定状态 */
    WorkStatus      work_status;        /* 工作状态 */
    OnlineStatus    online_status;      /* 在线状态(仅存档) */
    FaultLevel      fault_summary;      /* 异常汇总等级(服务端重算) */
    FaultEntry     *faults;             /* 异常列表(动态数组, 最多 50 条) */
    int             faults_count;       /* 异常数量 */
    LifecycleStatus lifecycle_status;   /* 生命周期状态(服务端维护) */
} DeviceState;

/* 54.2 网络状态 */
typedef struct _network_state {
    uint8_t     struct_type;            /* 类型标识: TLV_STRUCT_NETWORK (0x02) */
    NetworkType network_type;           /* 网络类型 */
    char        ifname[16];             /* 接口名(内部用, 如 wlan0/eth0) */
    int         rssi;                   /* 信号强度(-100 ~ 0, dBm) */
    char        ip[64];                 /* 本机 IPv4 地址 */
    int         mqtt_connected;         /* MQTT 连接: 0=断开, 1=已连接 */
} NetworkState;

/* 54.3 电源状态 */
typedef struct _power_state {
    uint8_t  struct_type;            /* 类型标识: TLV_STRUCT_POWER (0x03) */
    PowerMode power_mode;               /* AC / BATTERY */
    int       battery_percent;          /* 电池电量 0~100 */
    int       charging;                 /* 是否充电: 0/1 */
} PowerState;

/* 54.4 门锁状态 */
typedef struct _door_state {
    uint8_t    struct_type;             /* 类型标识: TLV_STRUCT_DOOR (0x04) */
    DoorStatus door_status;             /* OPEN / CLOSED / UNKNOWN */
    LockStatus lock_status;             /* LOCKED / UNLOCKED / JAMMED / UNKNOWN */
    double     last_open_time;          /* 最近开门时间(Unix 秒) */
} DoorState;

/* 54.5 环境状态 */
typedef struct _environment_state {
    uint8_t struct_type;                /* 类型标识: TLV_STRUCT_ENVIRONMENT (0x05) */
    double temperature;                 /* 温度(℃) */
    double humidity;                    /* 湿度(%RH) */
    double pm25;                        /* PM2.5(μg/m³) */
    double co2;                         /* CO2(ppm) */
} EnvironmentState;

/* 54.6 执行器状态 — 各控制项 */
typedef struct _actuator_exhaust {
    uint8_t struct_type;                /* 类型标识: TLV_STRUCT_EXHAUST (0x61) */
    int    enabled;                     /* 启用: 0/1 */
    int    speed_level;                 /* 排风档位 1..6 */
    double interval_hours;              /* 间隔小时: 0.5/1/2/4/6/8 */
    int    running;                     /* 当前是否运行: 0/1 */
} ActuatorExhaust;

typedef struct _actuator_light {
    uint8_t struct_type;                /* 类型标识: TLV_STRUCT_LIGHT (0x62) */
    int enabled;                        /* 启用: 0/1 */
    int brightness_level;               /* 亮度档位 1..4 (1弱/2正常/3-4强) */
    int running;                        /* 当前是否运行: 0/1 */
} ActuatorLight;

typedef struct _actuator_uvb {
    uint8_t struct_type;                /* 类型标识: TLV_STRUCT_UVB (0x63) */
    int enabled;                        /* 启用: 0/1 */
    int level;                          /* UVB 档位 1..4 */
    int daily_hours;                    /* 每天工作小时: 2/4/6/8 */
    int running;                        /* 当前是否运行: 0/1 */
} ActuatorUvb;

typedef struct _actuator_anion {
    uint8_t struct_type;                /* 类型标识: TLV_STRUCT_ANION (0x64) */
    int enabled;                        /* 启用: 0/1 */
    int running;                        /* 当前是否运行: 0/1 */
} ActuatorAnion;

typedef struct _actuator_plasma {
    uint8_t struct_type;                /* 类型标识: TLV_STRUCT_PLASMA (0x65) */
    int enabled;                        /* 启用: 0/1 */
    int running;                        /* 当前是否运行: 0/1 */
} ActuatorPlasma;

typedef struct _actuator_climate {
    uint8_t              struct_type;   /* 类型标识: TLV_STRUCT_CLIMATE (0x66) */
    int                  enabled;       /* 加热启用: 0/1 */
    double               target_celsius;/* 目标温度(℃), 传感器不可用时为 -1 表示 null */
    ClimateControlStatus control_status;/* OFF/HEATING/HOLDING/COOLING/UNKNOWN */
    int                  running;       /* 当前是否加热: 0/1 */
} ActuatorClimate;

typedef struct _actuator_humidifier {
    uint8_t      struct_type;           /* 类型标识: TLV_STRUCT_HUMIDIFIER (0x67) */
    int          enabled;               /* 雾化加湿启用: 0/1 */
    double       interval_hours;        /* 间隔小时: 2/4/8/12 */
    double       running_hours;         /* 每次运行时长: 0.5/1/2 */
    int          running;               /* 当前是否运行: 0/1 */
    LiquidStatus liquid_status;         /* 液位状态 */
} ActuatorHumidifier;

typedef struct _actuator_inlet_fan {
    uint8_t struct_type;                /* 类型标识: TLV_STRUCT_INLET_FAN (0x68) */
    int enabled;                        /* 启用: 0/1 */
    int speed_level;                    /* 进风档位 1..4 */
    int running;                        /* 当前是否运行: 0/1 */
} ActuatorInletFan;

typedef struct _actuator_filter {
    uint8_t struct_type;                /* 类型标识: TLV_STRUCT_FILTER (0x69) */
    int life_percent;                   /* 滤芯剩余寿命 0..100 */
    int need_replace;                   /* 是否需要更换: 0/1 */
} ActuatorFilter;

/* 工厂设备信息(出厂烧录上报, 不含 mac/device_secret — mac 由 DEVICE_SN 提供, secret 由 generate_device_secret 生成) */
typedef struct _factory_device_tlv {
    uint8_t struct_type;                /* 类型标识: TLV_STRUCT_FACTORY_DEVICE (0x6A) */
    char sales_country_code[8];         /* CN / US / JP / KR */
    char product_key[64];
    char model[64];
    char hardware_version[32];
    char firmware_version[32];
} FactoryDeviceTlv;

/* 工厂设备上报被拒结果 (V851 → D5, 0x36 命令字下发)
 * 包含 mac 和 reason, 用于 D5/LCD 显示烧录失败原因 */
typedef struct _factory_device_rejection_tlv {
    uint8_t struct_type;                /* 类型标识: TLV_STRUCT_FACTORY_REJECTION (0x6B) */
    char mac[16];                       /* 被拒设备 MAC (12 位大写 hex) */
    char reason[64];                    /* 被拒原因: DUPLICATE_MAC / SECRET_FORMAT_INVALID 等 */
} FactoryDeviceRejectionTlv;

typedef struct _actuator_state {
    uint8_t           struct_type;      /* 类型标识: TLV_STRUCT_ACTUATOR (0x06) */
    ActuatorExhaust    exhaust;
    ActuatorLight      light;
    ActuatorUvb        uvb;
    ActuatorAnion      anion;
    ActuatorPlasma     plasma;
    ActuatorClimate    climate;
    ActuatorHumidifier humidifier;
    ActuatorInletFan   inlet_fan;
    ActuatorFilter     filter;
} ActuatorState;

/* 54.7 摄像头状态 */
typedef struct _camera_state {
    uint8_t      struct_type;            /* 类型标识: TLV_STRUCT_CAMERA (0x07) */
    CameraStatus camera_status;         /* OFFLINE / ONLINE / ERROR / DISABLED */
    StreamStatus stream_status;         /* IDLE / STARTING / STREAMING / STOPPING / ERROR */
    char         provider[32];          /* TRTC / TANGE / UNKNOWN */
    VideoCodec   video_codec;           /* H264 / H265 */
    Resolution   resolution;            /* 720p / 1080p */
    double       fps;                   /* 帧率 1~30 */
    int          audio_enabled;         /* 音频启用: 0/1 */
    int          privacy_mode;          /* 隐私模式: 0/1 */
    char         last_session_id[128];  /* 上次会话 ID(无则空) */
    char         last_error_code[32];   /* 上次错误码(无则空) */
} CameraState;

/* 54.8 设备本地设置 */
typedef struct _display_settings {
    int        brightness_percent;      /* 屏幕亮度 0..100 */
    int        screen_timeout_minutes;  /* 屏幕超时分钟 0..60 */
    ScreenMode screen_mode;             /* SCREEN_SAVER / STANDBY */
} DisplaySettings;

typedef struct _settings_state {
    uint8_t         struct_type;        /* 类型标识: TLV_STRUCT_SETTINGS (0x08) */
    char            temperature_unit[4];        /* C / F */
    DisplaySettings display;
    int             volume_percent;             /* 音量 0..100 */
    char            language[16];               /* 语言标签, 如 zh-CN */
    int             local_password_enabled;     /* 是否启用本地管理密码: 0/1 */
} SettingsState;

/* 固件信息(§51.4 请求示例中 state.firmware) */
typedef struct _firmware_state {
    uint8_t struct_type;                /* 类型标识: TLV_STRUCT_FIRMWARE (0x09) */
    char firmware_version[32];          /* 固件版本 */
    char hardware_version[32];          /* 硬件版本 */
    int  config_version;                /* 配置版本号 */
} FirmwareState;

/* 存储信息(§51.4 请求示例中 state.storage) */
typedef struct _storage_state {
    uint8_t struct_type;                /* 类型标识: TLV_STRUCT_STORAGE (0x0A) */
    int storage_total_mb;               /* 总存储(MB) */
    int storage_free_mb;                /* 可用存储(MB) */
} StorageState;

/* 完整状态字段字典 — 对应 data.state 节点 */
typedef struct _device_full_state {
    DeviceState      device;
    NetworkState     network;
    PowerState       power;
    DoorState        door;
    EnvironmentState environment;
    ActuatorState    actuators;
    CameraState      camera;
    SettingsState    settings;
    FirmwareState    firmware;
    StorageState     storage;
} DeviceFullState;

/* ============================================================================
 * 状态枚举 ↔ 字符串转换
 * ============================================================================ */
const char *bind_status_to_str(BindStatus v);
BindStatus  bind_status_from_str(const char *s);
const char *work_status_to_str(WorkStatus v);
WorkStatus  work_status_from_str(const char *s);
const char *online_status_to_str(OnlineStatus v);
OnlineStatus online_status_from_str(const char *s);
const char *fault_level_to_str(FaultLevel v);
FaultLevel  fault_level_from_str(const char *s);
const char *lifecycle_status_to_str(LifecycleStatus v);
LifecycleStatus lifecycle_status_from_str(const char *s);
const char *network_type_to_str(NetworkType v);
NetworkType  network_type_from_str(const char *s);
const char *power_mode_to_str(PowerMode v);
PowerMode  power_mode_from_str(const char *s);
const char *door_status_to_str(DoorStatus v);
DoorStatus  door_status_from_str(const char *s);
const char *lock_status_to_str(LockStatus v);
LockStatus  lock_status_from_str(const char *s);
const char *climate_control_status_to_str(ClimateControlStatus v);
ClimateControlStatus climate_control_status_from_str(const char *s);
const char *liquid_status_to_str(LiquidStatus v);
LiquidStatus  liquid_status_from_str(const char *s);
const char *camera_status_to_str(CameraStatus v);
CameraStatus camera_status_from_str(const char *s);
const char *stream_status_to_str(StreamStatus v);
StreamStatus stream_status_from_str(const char *s);
const char *video_codec_to_str(VideoCodec v);
VideoCodec  video_codec_from_str(const char *s);
const char *resolution_to_str(Resolution v);
Resolution  resolution_from_str(const char *s);
const char *screen_mode_to_str(ScreenMode v);
ScreenMode  screen_mode_from_str(const char *s);

/* 从 MqttConfig 填充 DeviceFullState(绑定/在线/MQTT连接等) */
void device_full_state_from_mqtt_config(const MqttConfig *cfg, DeviceFullState *state);

/* 从 DeviceFullState 构建 data.state JSON 字符串(调用者 free) */
char *device_full_state_to_json(const DeviceFullState *state);

/* 从 JSON 字符串解析 data.state, 填充 DeviceFullState */
int device_full_state_parse_from_json(const char *json_str, DeviceFullState *state);

/* 从 JSON 字符串解析 data.mqtt 节点, 填充 MqttConfig 结构体 */
int mqtt_config_parse_from_json(const char *json_str, MqttConfig *cfg);

/* 从 MqttConfig 构建 state.device 节点的 JSON 字符串:
 *   device_status → "bind_status"
 *   mqtt_connected → "online_status" (1→ONLINE, 0→OFFLINE)
 * 调用者 free 返回值。 */
char *mqtt_config_build_device_json(MqttConfig *cfg);

/* 从本机网络信息构建 state.network 节点的 JSON 字符串:
 *   IP/getifaddrs → "ip"/"network_type"
 *   /proc/net/wireless → "rssi"
 *   mqtt_connected → "mqtt_connected"
 * 调用者 free 返回值。 */
char *mqtt_config_build_network_json(MqttConfig *cfg);

/* MqttConfig 保存到文件 / 从文件读取 */
#define MQTT_CONFIG_SAVE_PATH  "/mnt/UDISK/mqtt_save_config.txt"
int mqtt_config_save(MqttConfig *cfg);
int mqtt_config_load(MqttConfig *cfg);

/* BootstrapConfig 保存到文件 / 从文件读取 */
#define BOOTSTRAP_CONFIG_SAVE_PATH "/mnt/UDISK/bootstrap_config.txt"
int bootstrap_config_save(BootstrapConfig *cfg);
int bootstrap_config_load(BootstrapConfig *cfg);

/* 从 JSON 字符串解析 bootstrap 响应, 填充 BootstrapConfig 结构体 */
int bootstrap_config_parse_from_json(const char *json_str, BootstrapConfig *cfg);

/* 发送 /api/device/v1/bootstrap 请求获取设备身份
 * 凭 MAC + device_secret HMAC-SHA256 签名, 返回 device_sn / ble_id / endpoint / bind_status
 * 返回 0 成功, -1 失败 */
int http_post_bootstrap(void);

/* device.snapshot 消息结构体
 * data = { "state": <DeviceFullState>, "reported_at": <Unix秒> } */
typedef struct _snapshot_message {
    char msg_id[128];            /* 消息 ID */
    char msg_type[64];           /* 消息类型, 固定 "device.snapshot" */
    char protocol_version[16];   /* 协议版本, 如 "1.0" */
    char device_sn[64];          /* 设备 SN */
    char ble_id[32];             /* BLE ID */
    char product_key[64];        /* 产品 key */
    int64_t timestamp;           /* 顶层时间戳(Unix 秒) */
    int seq;                     /* 序列号 */
    int64_t reported_at;         /* data.reported_at 上报时间戳 */
} SnapshotMessage;

/* server.command 下行指令结构体(从 /down 主题接收) */
typedef struct _server_command {
    /* 顶层字段 */
    char msg_id[128];            /* 消息 ID */
    char msg_type[64];           /* 消息类型, 固定 "server.command" */
    char protocol_version[16];   /* 协议版本 */
    char device_sn[64];          /* 设备 SN */
    char product_key[64];        /* 产品 key */
    int64_t timestamp;           /* 消息时间戳(Unix 秒) */

    /* data 字段 */
    char command_id[128];        /* 命令 ID */
    char cmd[64];                /* 命令名称, 如 video.start_session */
    int64_t data_expire_at;      /* 命令过期时间(data.expire_at) */

    /* data.params 字段 */
    char session_id[128];        /* 会话 ID */
    char provider[32];           /* 流媒体提供商, 如 TANGE */
    int64_t params_expire_at;    /* 参数过期时间(params.expire_at) */
    char stream_type[16];        /* 流类型, 如 main / sub */
    char video_profile[16];      /* 视频规格, 如 720p */
    int  audio_enabled;          /* 是否启用音频: 0/1 */

    /* data.params.provider_params 字段 */
    char provider_app_id[128];   /* 提供商 app_id */
    char provider_device_id[64]; /* 提供商 device_id */

    /* data.params 原始 JSON 字符串 (供执行器指令解析用) */
    char params_json[1024];
} ServerCommand;

/* 从 JSON 字符串解析 server.command 消息, 填充 ServerCommand 结构体 */
int server_command_parse_from_json(const char *json_str, ServerCommand *cmd);

/* device.command_ack 指令应答结构体(对 video.start_session 等命令的回应) */
typedef struct _command_ack {
    /* 顶层字段 */
    char msg_id[128];            /* 消息 ID */
    char msg_type[64];           /* 消息类型, 固定 "device.command_ack" */
    char device_sn[64];          /* 设备 SN */
    char product_key[64];        /* 产品 key */
    int64_t timestamp;           /* 消息时间戳(Unix 秒) */

    /* data 字段 */
    char command_id[128];        /* 原始命令 ID */
    char server_msg_id[128];     /* 服务端消息 ID */
    char status[32];             /* 执行状态: SUCCESS / FAILED */

    /* data.result 字段 */
    char session_id[128];        /* 视频会话 ID */
    char provider[32];           /* 流媒体提供商 */
    char camera_status[32];      /* 摄像头状态: STREAMING / IDLE / ERROR */
} CommandAck;

/* 从 JSON 字符串解析 device.command_ack 消息, 填充 CommandAck 结构体 */
int command_ack_parse_from_json(const char *json_str, CommandAck *ack);

/* command_ack 错误码(对应 data.error_code, 仅 FAILED 时携带) */
#define CMD_ACK_ERROR_NONE           ""
#define CMD_ACK_ERROR_EXPIRED        "EXPIRED"
#define CMD_ACK_ERROR_INVALID_PARAM  "INVALID_PARAM"
#define CMD_ACK_ERROR_STREAM_INIT    "STREAM_INIT_FAILED"
#define CMD_ACK_ERROR_BUSY           "BUSY"
#define CMD_ACK_ERROR_UNSUPPORTED    "UNSUPPORTED"

/* 对 video.start_session 命令构建 device.command_ack 应答并发布到 MQTT up 主题
 * error_code: 仅 status=="FAILED" 时写入 data.error_code, 成功时传 NULL */
int command_ack_publish(struct mosquitto *mosq, const ServerCommand *cmd,
                        const char *status, const char *session_id,
                        const char *provider, const char *camera_status,
                        const char *error_code);

/* ============================================================================
 * 执行器控制指令处理 (server.command → 更新 g_device_state → TLV 打包 → 串口下发)
 * ============================================================================ */

/* 构建 TLV 帧 (AA 55 帧头 + 0x35 命令字) 并推送到 queue_2_uart 串口队列
 * @param struct_type 结构体类型 (TLV_STRUCT_EXHAUST 等)
 * @param tlv_data    TLV 序列化后的数据
 * @param data_len    TLV 数据长度
 * @return 0 成功, -1 失败 */
int actuator_command_send_uart(uint8_t struct_type, const uint8_t *tlv_data, int data_len);

/* 9 条执行器控制指令处理函数 */
void handle_exhaust_set(struct mosquitto *mosq, const ServerCommand *cmd);
void handle_plasma_set(struct mosquitto *mosq, const ServerCommand *cmd);
void handle_anion_set(struct mosquitto *mosq, const ServerCommand *cmd);
void handle_climate_set(struct mosquitto *mosq, const ServerCommand *cmd);
void handle_humidifier_set(struct mosquitto *mosq, const ServerCommand *cmd);
void handle_uvb_set(struct mosquitto *mosq, const ServerCommand *cmd);
void handle_light_set(struct mosquitto *mosq, const ServerCommand *cmd);
void handle_inlet_fan_set(struct mosquitto *mosq, const ServerCommand *cmd);
void handle_camera_privacy_set(struct mosquitto *mosq, const ServerCommand *cmd);

/* 3 条系统指令处理函数 */
void handle_device_settings_set(struct mosquitto *mosq, const ServerCommand *cmd);
void handle_device_password_set(struct mosquitto *mosq, const ServerCommand *cmd);
void handle_ota_upgrade(struct mosquitto *mosq, const ServerCommand *cmd);

/* 构建完整 device.snapshot JSON 并通过 MQTT 发布到 /mcqx/device/{device_sn}/up */
int snapshot_publish(struct mosquitto *mosq, const SnapshotMessage *snap,
                     const DeviceFullState *state);

/* 在 dwin_nettplayer.c 中实现: 根据 server.command 启动/确认 TIRTC 推流就绪
 * 返回 0 表示已就绪, 非 0 表示启动失败 */
int tirtc_stream_start_session(const ServerCommand *cmd);

/* 停止 TIRTC 推流并退出播放线程。返回 0 表示成功, 非 0 表示失败。 */
int tirtc_stream_stop_session(const ServerCommand *cmd);

/* 视频 License 信息(从 /api/device/v1/video/tange_tirtc/license 接口解析) */
typedef struct _video_license_info {
	char provider[32];          /* 探歌/TRTC 等, 如 TANGE */
	char license[256];          /* video license */
	char tange_device_id[64];   /* 探歌设备 ID */
	char tange_device_key[128]; /* 探歌设备 Key / Secret */
	int64_t expire_at;          /* 授权过期时间(Unix 时间戳) */
	int fetched;                /* 0=未获取, 1=已成功获取 */
} VideoLicenseInfo;

/* 发送 /api/device/v1/video/tange_tirtc/license 请求获取视频 License 信息
 * session_id: 视频会话 ID(可传 NULL 或具体 session_id)
 * provider: 流媒体提供商(如 "TANGE")
 * out_info: 输出解析到的 VideoLicenseInfo 结构体
 * 返回 0 成功, -1 失败 */
int http_post_video_license(const char *session_id, const char *provider, VideoLicenseInfo *out_info);

/* 工厂设备上报单条记录(对应 OpenAPI FactoryDeviceItem) */
typedef struct _factory_device_item {
	char mac[16];                 /* 12 位大写 hex, 如 A0B1C2D3E4F5 */
	char sales_country_code[8];   /* CN / US / JP / KR */
	char device_secret[64];       /* 43 字符 Base64URL */
	char product_key[64];
	char model[64];
	char hardware_version[32];
	char firmware_version[32];    /* 可选, 空字符串表示不上报 */
	char camera_vendor[32];       /* 摄像头供应商, 如 tange_tirtc; 空字符串表示不上报 */
} FactoryDeviceItem;

/* POST /api/device/v1/factory-devices 请求体(对应 JSON 顶层) */
typedef struct _factory_device_submission_request {
	char submission_batch_id[72];         /* 工厂批次号, 如 FACTORY-20260720-001 */
	FactoryDeviceItem *devices;           /* 动态数组, 调用者管理内存 */
	int  device_count;                    /* 设备数量(固定为1, 单条 TLV 消息只上报一台设备) */
} FactoryDeviceSubmissionRequest;

/* 工厂设备上报被拒条目 */
typedef struct _factory_device_rejection {
	char mac[16];
	char reason[64];              /* DUPLICATE_MAC / SECRET_FORMAT_INVALID 等 */
} FactoryDeviceRejection;

#define FACTORY_DEVICE_MAX_REJECTIONS 1

/* /api/device/v1/factory-devices 响应 data 节点 */
typedef struct _factory_device_submission_result {
	char submission_batch_id[72];
	int total;
	int accepted;
	int rejected;
	FactoryDeviceRejection rejections[FACTORY_DEVICE_MAX_REJECTIONS];
	int rejection_count;
} FactoryDeviceSubmissionResult;

/* 发送 /api/device/v1/factory-devices 批量上报设备密钥
 * factory_api_key: Bearer FACTORY_API_KEY(非设备 HMAC 签名)
 * req: 请求体(submission_batch_id + devices 数组), 调用者管理内存
 * out_result: 输出解析结果(可传 NULL)
 * 返回 0 成功(HTTP 200 且 code=0), -1 失败 */
int http_post_factory_devices(const char *factory_api_key,
                              const FactoryDeviceSubmissionRequest *req,
                              FactoryDeviceSubmissionResult *out_result);

/* 便捷封装: 出厂烧录 — generate_device_secret 生成密钥 → 落盘 → 上报云端
 * 若本地已有密钥则复用(支持重试上报) */
int http_post_factory_device_self(const char *factory_api_key,
                                  const char *mac,
                                  const char *sales_country_code,
                                  FactoryDeviceSubmissionResult *out_result);

/* TLV 工厂设备信息处理: mac 从 WiFi 获取, 其余字段从 TLV 数据填充,
 * 组装 FactoryDeviceSubmissionRequest 后调用 /api/device/v1/factory-devices 上报 */
int handle_factory_device_tlv(const FactoryDeviceTlv *tlv);

/* 将工厂设备上报被拒结果打包为 TLV (0x36 命令字) 发送到串口 (queue_2_uart) */
int factory_device_result_send_uart(const FactoryDeviceSubmissionResult *result);

/* 生成 43 字符 Base64URL device_secret(32 字节密码学随机数) */
int generate_device_secret(char *out, size_t out_size);

/* 设备上测试工厂设备上报 API — 获取本机 MAC + 生成/加载 device_secret → 发送请求 → 打印结果
 * factory_api_key: Bearer token(可传 NULL, 使用宏 FACTORY_API_KEY)
 * 返回 0 成功, -1 失败 */
int test_factory_device_self(const char *factory_api_key);

#define DEVICE_SECRET_SAVE_PATH "/mnt/UDISK/device_secret.txt"
int device_secret_save(const char *secret);
int device_secret_load(char *out, size_t out_size);

/* VideoLicenseInfo 保存到文件 / 从文件读取 */
#define VIDEO_LICENSE_SAVE_PATH "/mnt/UDISK/video_license_config.txt"
int video_license_save(VideoLicenseInfo *info);
int video_license_load(VideoLicenseInfo *info);

/* 全局 MQTT 配置与 Video License 配置 */
extern MqttConfig g_mqtt_config;
extern DeviceFullState g_device_state;
extern VideoLicenseInfo g_video_license;
extern BootstrapConfig g_bootstrap_config;

int mqttssl_thread_task(  );
int mqtt_thread_exit(void);

#endif

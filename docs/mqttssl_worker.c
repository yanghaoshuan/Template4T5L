#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
//#include <netinet/tcp.h>

#include <linux/tcp.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <errno.h>
//access、unlink函数头文件
#include<unistd.h>

#include "mqttssl_worker.h"
#include "queue_worker.h"
#include "ws_core.h"
#include "cirbuffer.h"

#include "cJSON/inc/cJSON.h"
#include "utils/inc/conversion.h"
#include "utils/inc/op_config.h"
#include "config.h"
#include "create_json.h"
 
 
 
#include "tlv_worker.h"
#include "tlv_codec.h"
#include "wifi_worker_sh.h"

static int tlv_thread_exit = 0;

int mqtt_thread_exit(void)
{
	tlv_thread_exit = 0;
	return 0;
} 
 
 
 
 
 
 /* ============================================================================
 * 配置 — 根据实际设备信息修改这些值
 * ============================================================================ */



#define DEVICE_SN       "MQXQ-PET-CN-2026-000002"

#define BLE_ID          "AVUU69"
#define BLE_NAME        "MQXQ-AVUU69"
#define PRODUCT_KEY     "MCQX_QXY03"
#define MODEL           "QXY03"
#define HW_VERSION      "HW-V1.0"
#define FW_VERSION      "FW-V2.0.1"
#define PROTOCOL_VER    "1.0"

/* 测试环境：跳过 HTTPS 对端证书校验（生产环境应配置 CA 证书并移除本宏） */
//#define SKIP_PEER_VERIFY
 
 
 // 探歌系统  
#include "tirtc/include/tirtc/basedef.h" 
#include "tirtc/include/tirtc/tiRTC.h" 




 

// 探歌 发送h264句柄
static tirtc_conn_t current_hconn;
static const uint8_t kAudioStreamId = 10;
static const uint8_t kVideoStreamId = 11;

/* 全局 MQTT 配置(由 hello 响应填充) */
MqttConfig g_mqtt_config;
DeviceFullState g_device_state = {0};
VideoLicenseInfo g_video_license = {0};
BootstrapConfig g_bootstrap_config = {0};


int http_post_hello(void);
int http_post_bootstrap(void);

/* 前向声明: get_wifi_mac 定义在文件后方, http_post_bootstrap 需要调用 */
static int get_wifi_mac(char *out, size_t out_size);
 
static void tirtc_on_event(int event, const void *data, int len)
{
    (void)data;
    (void)len;

    if (event == TIRTC_EVENT_SYS_STARTED) {
        printf_info("[tirtc] SDK started\r\n");
    } else if (event == TIRTC_EVENT_SYS_STOPPED) {
        printf_info("[tirtc] SDK stopped\r\n");
    }
}

static void tirtc_on_conn_accepted(tirtc_conn_t hconn)
{
    printf_info("[tirtc] conn accepted hconn=%p\r\n", hconn);
    current_hconn = hconn;
}

static void tirtc_on_conn_error(tirtc_conn_t hconn, int error)
{
    printf_info("[tirtc] conn error hconn=%p error=%d\r\n", hconn, error);
    if (current_hconn == hconn) {
        current_hconn = NULL;
    }
}

static void tirtc_on_disconnected(tirtc_conn_t hconn)
{
    printf_info("[tirtc] disconnected hconn=%p\r\n", hconn);
    if (current_hconn == hconn) {
        current_hconn = NULL;
    }
}

static void tirtc_on_audio(tirtc_conn_t hconn, const TIRTCFRAMEINFO *pFi, void *data)
{
    (void)hconn;
    (void)pFi;
    (void)data;
}

static void tirtc_on_video(tirtc_conn_t hconn, const TIRTCFRAMEINFO *pFi, void *data)
{
    (void)hconn;
    (void)pFi;
    (void)data;
}

static void tirtc_on_message(tirtc_conn_t hconn, const TIRTCFRAMEINFO *pFi, void *data)
{
    if (!pFi) return;
    printf_info("[tirtc] message hconn=%p stream_id=%u len=%u\r\n",
                hconn, pFi->stream_id, pFi->length);
    (void)data;
}

static void tirtc_on_command(tirtc_conn_t hconn, uint32_t cmdw,
                             const void *data, uint32_t len)
{
    printf_info("[tirtc] command hconn=%p cmdw=0x%08x len=%u\r\n",
                hconn, (unsigned int)cmdw, len);
    (void)data;
}

static void tirtc_on_request_key_frame(tirtc_conn_t hconn, uint8_t stream_id)
{
    printf_info("[tirtc] request key frame hconn=%p stream_id=%u\r\n",
                hconn, stream_id);
}

static int tirtc_on_subscribe_video(tirtc_conn_t hconn, uint8_t stream_id)
{
    printf_info("[tirtc] subscribe video hconn=%p stream_id=%u\r\n",
                hconn, stream_id);
    return 0;
}

static void tirtc_on_unsubscribe_video(tirtc_conn_t hconn, uint8_t stream_id)
{
    printf_info("[tirtc] unsubscribe video hconn=%p stream_id=%u\r\n",
                hconn, stream_id);
}

static int tirtc_on_subscribe_audio(tirtc_conn_t hconn, uint8_t stream_id)
{
    printf_info("[tirtc] subscribe audio hconn=%p stream_id=%u\r\n",
                hconn, stream_id);
    return 0;
}

static void tirtc_on_unsubscribe_audio(tirtc_conn_t hconn, uint8_t stream_id)
{
    printf_info("[tirtc] unsubscribe audio hconn=%p stream_id=%u\r\n",
                hconn, stream_id);
}

static const TIRTCCALLBACKS kCallbacks = {
    .on_event = tirtc_on_event,
    .on_conn_accepted = tirtc_on_conn_accepted,
    .on_conn_error = tirtc_on_conn_error,
    .on_disconnected = tirtc_on_disconnected,
    .on_audio = tirtc_on_audio,
    .on_video = tirtc_on_video,
    .on_message = tirtc_on_message,
    .on_command = tirtc_on_command,
    .on_request_key_frame = tirtc_on_request_key_frame,
    .on_subscribe_video = tirtc_on_subscribe_video,
    .on_unsubscribe_video = tirtc_on_unsubscribe_video,
    .on_subscribe_audio = tirtc_on_subscribe_audio,
    .on_unsubscribe_audio = tirtc_on_unsubscribe_audio,
};

/* 将视频规格字符串映射为分辨率枚举 */
static Resolution video_profile_to_resolution(const char *profile)
{
    if (!profile) return RESOLUTION_720P;
    if (strcmp(profile, "1080p") == 0) return RESOLUTION_1080P;
    if (strcmp(profile, "720p") == 0)  return RESOLUTION_720P;
    if (strcmp(profile, "480p") == 0)  return RESOLUTION_720P; /* 480p 按设备能力降级为 720p */
    return RESOLUTION_720P;
}

/* 处理 video.start_session 下行命令 */
static void handle_video_start_session(struct mosquitto *mosq, const ServerCommand *cmd)
{
    int64_t now = (int64_t)time(NULL);

    /* ---- 参数校验 ---- */
    if (!cmd->session_id[0] || !cmd->provider[0]) {
        command_ack_publish(mosq, cmd, "FAILED", NULL, NULL, NULL,
                            CMD_ACK_ERROR_INVALID_PARAM);
        return;
    }
    if (strcmp(cmd->provider, "TANGE") != 0) {
        command_ack_publish(mosq, cmd, "FAILED", NULL, NULL, NULL,
                            CMD_ACK_ERROR_UNSUPPORTED);
        return;
    }
    if ((cmd->data_expire_at > 0 && cmd->data_expire_at < now) ||
        (cmd->params_expire_at > 0 && cmd->params_expire_at < now)) {
        command_ack_publish(mosq, cmd, "FAILED", NULL, NULL, NULL,
                            CMD_ACK_ERROR_EXPIRED);
        return;
    }
    if (cmd->stream_type[0] &&
        strcmp(cmd->stream_type, "main") != 0 &&
        strcmp(cmd->stream_type, "sub") != 0) {
        command_ack_publish(mosq, cmd, "FAILED", NULL, NULL, NULL,
                            CMD_ACK_ERROR_INVALID_PARAM);
        return;
    }
    if (cmd->video_profile[0] &&
        strcmp(cmd->video_profile, "480p") != 0 &&
        strcmp(cmd->video_profile, "720p") != 0 &&
        strcmp(cmd->video_profile, "1080p") != 0) {
        command_ack_publish(mosq, cmd, "FAILED", NULL, NULL, NULL,
                            CMD_ACK_ERROR_INVALID_PARAM);
        return;
    }
    if (!cmd->provider_app_id[0] || !cmd->provider_device_id[0]) {
        command_ack_publish(mosq, cmd, "FAILED", NULL, NULL, NULL,
                            CMD_ACK_ERROR_INVALID_PARAM);
        return;
    }

    /* ---- 更新状态 ---- */
    g_device_state.camera.camera_status  = CAMERA_STATUS_ONLINE;
    g_device_state.camera.stream_status  = STREAM_STATUS_STARTING;
    g_device_state.camera.provider[0]    = '\0';
    g_device_state.camera.video_codec    = VIDEO_CODEC_H264;
    g_device_state.camera.resolution     = video_profile_to_resolution(cmd->video_profile);
    g_device_state.camera.fps            = 15;
    g_device_state.camera.audio_enabled  = cmd->audio_enabled;
    g_device_state.camera.privacy_mode   = 0;
    strncpy(g_device_state.camera.last_session_id, cmd->session_id,
            sizeof(g_device_state.camera.last_session_id) - 1);
    g_device_state.camera.last_error_code[0] = '\0';

    /* ---- 启动流媒体 ---- */
    int rc = tirtc_stream_start_session(cmd);
    if (rc == 0) {
        g_device_state.camera.stream_status = STREAM_STATUS_STREAMING;
        strncpy(g_device_state.camera.provider, cmd->provider,
                sizeof(g_device_state.camera.provider) - 1);
        command_ack_publish(mosq, cmd, "SUCCESS",
                            cmd->session_id, cmd->provider, "STREAMING",
                            CMD_ACK_ERROR_NONE);
    } else {
        g_device_state.camera.stream_status = STREAM_STATUS_ERROR;
        strncpy(g_device_state.camera.last_error_code, "STREAM_INIT_FAILED",
                sizeof(g_device_state.camera.last_error_code) - 1);
        command_ack_publish(mosq, cmd, "FAILED", NULL, NULL, NULL,
                            CMD_ACK_ERROR_STREAM_INIT);
    }
}

/* 处理 video.stop_session 下行命令
 * 停止推流 + 退出播放线程，即使 TiRtcStop 失败也继续退出。 */
static void handle_video_stop_session(struct mosquitto *mosq, const ServerCommand *cmd)
{
    printf_info("[tirtc] handle_video_stop_session: stopping session %s\n", cmd->session_id);

    /* 同步: 回 RECEIVED */
    command_ack_publish(mosq, cmd, "RECEIVED", NULL, NULL, NULL,
                        CMD_ACK_ERROR_NONE);

    /* 执行停止: 停止 TiRTC SDK + 退出播放线程 */
    int rc = tirtc_stream_stop_session(cmd);
    if (rc == 0) {
        command_ack_publish(mosq, cmd, "SUCCESS",
                            cmd->session_id, NULL, "STOPPED",
                            CMD_ACK_ERROR_NONE);
    } else {
        g_device_state.camera.stream_status = STREAM_STATUS_ERROR;
        strncpy(g_device_state.camera.last_error_code, "STOP_FAILED",
                sizeof(g_device_state.camera.last_error_code) - 1);
        command_ack_publish(mosq, cmd, "FAILED", NULL, NULL, NULL,
                            CMD_ACK_ERROR_STREAM_INIT);
    }
}

/* ============================================================================
 * 执行器控制指令 — 辅助函数
 * ============================================================================ */

/* 构建 TLV 帧 (AA 55 帧头 + 0x35 命令字) 并推送到 queue_2_uart 串口队列
 * 与 factory_device_result_send_uart 同模式, 但命令字为 0x35 (TLV_CMD_PROPERTY) */
int actuator_command_send_uart(uint8_t struct_type, const uint8_t *tlv_data, int data_len)
{
    if (!tlv_data || data_len <= 0 || data_len > 65532) return -1;

    /* 结构体段 = 1B type + 2B seg_len + tlv_data */
    int seg_total = 3 + data_len;
    int total = 5 + seg_total;
    char *msg = (char *)malloc(total);
    if (!msg) return -1;

    msg[0] = 0xAA;
    msg[1] = 0x55;
    msg[2] = (char)((seg_total >> 8) & 0xFF);
    msg[3] = (char)(seg_total & 0xFF);
    msg[4] = TLV_CMD_PROPERTY;                   /* 命令字 0x35 */
    msg[5] = (char)struct_type;
    msg[6] = (char)((data_len >> 8) & 0xFF);
    msg[7] = (char)(data_len & 0xFF);
    memcpy(msg + 8, tlv_data, data_len);

    printf_info("[ACTUATOR] send to uart: struct_type=0x%02X tlv_len=%d total=%d\n",
                struct_type, data_len, total);

    if (tiny_queue_push(queue_2_uart, msg) == 0) {
        printf_info("[ACTUATOR] queue_2_uart push ok\n");
    } else {
        printf_info("[ACTUATOR] queue_2_uart push fail\n");
        free(msg);
        return -1;
    }
    return 0;
}

/* 解析 params JSON 中指定 key 的 int 值, 不存在返回 defval */
static int params_get_int(const char *params_json, const char *key, int defval)
{
    if (!params_json || !params_json[0] || !key) return defval;
    cJSON *root = cJSON_Parse(params_json);
    if (!root) return defval;
    cJSON *item = cJSON_GetObjectItem(root, key);
    int val = defval;
    if (cJSON_IsNumber(item)) val = (int)item->valuedouble;
    else if (cJSON_IsBool(item)) val = cJSON_IsTrue(item) ? 1 : 0;
    cJSON_Delete(root);
    return val;
}

/* 解析 params JSON 中指定 key 的 double 值, 不存在返回 defval */
static double params_get_double(const char *params_json, const char *key, double defval)
{
    if (!params_json || !params_json[0] || !key) return defval;
    cJSON *root = cJSON_Parse(params_json);
    if (!root) return defval;
    cJSON *item = cJSON_GetObjectItem(root, key);
    double val = defval;
    if (cJSON_IsNumber(item)) val = item->valuedouble;
    cJSON_Delete(root);
    return val;
}

/* 解析 params JSON 中指定 key 的字符串值, 不存在返回 defval
 * 使用轮转缓冲区池 (4 个 256B 缓冲区), 避免连续调用时结果被覆盖 */
static const char *params_get_str(const char *params_json, const char *key, const char *defval)
{
    if (!params_json || !params_json[0] || !key) return defval;
    cJSON *root = cJSON_Parse(params_json);
    if (!root) return defval;
    cJSON *item = cJSON_GetObjectItem(root, key);
    if (!cJSON_IsString(item)) { cJSON_Delete(root); return defval; }
    /* 轮转缓冲区池: 4 个槽位, 单线程 MQTT 回调安全 */
    static char buf[4][256];
    static int buf_idx = 0;
    buf_idx = (buf_idx + 1) & 3;  /* 等价于 % 4, 更快 */
    char *out = buf[buf_idx];
    strncpy(out, item->valuestring, 255);
    out[255] = '\0';
    cJSON_Delete(root);
    return out;
}

/* ============================================================================
 * 9 条执行器控制指令 handler
 * 每条指令流程: 解析 params → 校验 → 更新 g_device_state → TLV 打包 → 串口下发 → ACK
 *
 * 串口帧格式: AA 55 | lenH lenL | 0x35 | struct_type[1B] seg_len[2B] | tlv_payload
 * ============================================================================ */

/*
 * ---- 1. exhaust.set TLV 编码示例 ----
 * 输入: {enabled=1, speed_level=3, interval_hours=2.0, running=1}
 *
 * ---- 逐字段 TLV 编码 ----
 * enabled = 1 (U8):
 *   01 00 01 01
 *   │  │     └─ val = 0x01 = 1
 *   │  └─ len = 0x0001
 *   └─ tag = 0x01 (TLV_TAG_EXH_ENABLED)
 *
 * speed_level = 3 (U8):
 *   02 00 01 03
 *   │  │     └─ val = 0x03 = 3
 *   │  └─ len = 0x0001
 *   └─ tag = 0x02 (TLV_TAG_EXH_SPEED_LEVEL)
 *
 * interval_hours = 2.0 (IEEE754 double):
 *   03 00 08 40 00 00 00 00 00 00 00
 *   │  │     └─ 0x4000000000000000 = 2.0
 *   │  └─ len = 0x0008 = 8
 *   └─ tag = 0x03 (TLV_TAG_EXH_INTERVAL_HOURS)
 *
 * running = 1 (U8):
 *   04 00 01 01
 *   │  │     └─ val = 0x01 = 1
 *   │  └─ len = 0x0001
 *   └─ tag = 0x04 (TLV_TAG_EXH_RUNNING)
 *
 * ---- 完整 TLV 段 (struct_type 0x61) ----
 * seg_len = 0x0017 = 23 字节
 *
 * 61 00 17
 * │  └───── seg_len = 23
 * └─ struct_type = 0x61 (ActuatorExhaust)
 *
 * 01 00 01 01 02 00 01 03 03 00 08 40 00 00 00 00 00 00 00 04 00 01 01
 *
 * ---- 完整消息 (含帧头) ----
 * AA 55  00 1A  35  61  00 17
 * │      │      │  │   └───── seg_len = 0x0017 = 23
 * │      │      │  └─ struct_type = 0x61
 * │      │      └─ 命令字 0x35
 * │      └─ lenH|lenL = 0x001A = 26 (3 + 23)
 * └─ 帧头 AA 55
 *
 * + 23 字节 TLV payload → 共 31 字节
 * 一行 hex: AA 55 00 1A 35 61 00 17 01 00 01 01 02 00 01 03 03 00 08 40 00 00 00 00 00 00 00 04 00 01 01
 */
void handle_exhaust_set(struct mosquitto *mosq, const ServerCommand *cmd)
{
    int64_t now = (int64_t)time(NULL);

    /* 过期校验 */
    if (cmd->data_expire_at > 0 && cmd->data_expire_at < now) {
        command_ack_publish(mosq, cmd, "FAILED", NULL, NULL, NULL,
                            CMD_ACK_ERROR_EXPIRED);
        return;
    }

    /* 解析参数 */
    int enabled = params_get_int(cmd->params_json, "enabled", -1);
    if (enabled < 0) {
        command_ack_publish(mosq, cmd, "FAILED", NULL, NULL, NULL,
                            CMD_ACK_ERROR_INVALID_PARAM);
        return;
    }

    int speed_level = params_get_int(cmd->params_json, "speed_level", 1);
    double interval_hours = params_get_double(cmd->params_json, "interval_hours", 0.0);

    /* 更新全局状态 */
    g_device_state.actuators.exhaust.enabled        = enabled;
    g_device_state.actuators.exhaust.speed_level    = speed_level;
    g_device_state.actuators.exhaust.interval_hours = interval_hours;
    g_device_state.actuators.exhaust.running        = enabled ? 1 : 0;

    /* TLV 打包 → 串口下发 */
    uint8_t tlv_buf[256];
    ActuatorExhaust *ae = &g_device_state.actuators.exhaust;
    int tlv_len = actuator_exhaust_tlv_pack(ae, tlv_buf, sizeof(tlv_buf));
    if (tlv_len > 0) {
        actuator_command_send_uart(TLV_STRUCT_EXHAUST, tlv_buf, tlv_len);
    }

    printf_info("[ACTUATOR] exhaust.set: enabled=%d speed=%d interval=%.1f\n",
                enabled, speed_level, interval_hours);
    command_ack_publish(mosq, cmd, "SUCCESS", NULL, NULL, NULL, CMD_ACK_ERROR_NONE);
}

/*
 * ---- 2. plasma.set TLV 编码示例 ----
 * 输入: {enabled=1, running=1}
 *
 * ---- 逐字段 TLV 编码 ----
 * enabled = 1 (U8):
 *   01 00 01 01
 *   │  │     └─ val = 0x01 = 1
 *   │  └─ len = 0x0001
 *   └─ tag = 0x01 (TLV_TAG_PLASMA_ENABLED)
 *
 * running = 1 (U8):
 *   02 00 01 01
 *   │  │     └─ val = 0x01 = 1
 *   │  └─ len = 0x0001
 *   └─ tag = 0x02 (TLV_TAG_PLASMA_RUNNING)
 *
 * ---- 完整 TLV 段 (struct_type 0x65) ----
 * seg_len = 0x0008 = 8 字节
 *
 * 65 00 08
 * │  └───── seg_len = 8
 * └─ struct_type = 0x65 (ActuatorPlasma)
 *
 * 01 00 01 01 02 00 01 01
 *
 * ---- 完整消息 (含帧头) ----
 * AA 55  00 0B  35  65  00 08
 * │      │      │  │   └───── seg_len = 0x0008 = 8
 * │      │      │  └─ struct_type = 0x65
 * │      │      └─ 命令字 0x35
 * │      └─ lenH|lenL = 0x000B = 11 (3 + 8)
 * └─ 帧头 AA 55
 *
 * + 8 字节 TLV payload → 共 16 字节
 * 一行 hex: AA 55 00 0B 35 65 00 08 01 00 01 01 02 00 01 01
 */
void handle_plasma_set(struct mosquitto *mosq, const ServerCommand *cmd)
{
    int64_t now = (int64_t)time(NULL);
    if (cmd->data_expire_at > 0 && cmd->data_expire_at < now) {
        command_ack_publish(mosq, cmd, "FAILED", NULL, NULL, NULL, CMD_ACK_ERROR_EXPIRED);
        return;
    }

    int enabled = params_get_int(cmd->params_json, "enabled", -1);
    if (enabled < 0) {
        command_ack_publish(mosq, cmd, "FAILED", NULL, NULL, NULL, CMD_ACK_ERROR_INVALID_PARAM);
        return;
    }

    g_device_state.actuators.plasma.enabled = enabled;
    g_device_state.actuators.plasma.running = enabled ? 1 : 0;

    uint8_t tlv_buf[128];
    int tlv_len = actuator_plasma_tlv_pack(&g_device_state.actuators.plasma, tlv_buf, sizeof(tlv_buf));
    if (tlv_len > 0) {
        actuator_command_send_uart(TLV_STRUCT_PLASMA, tlv_buf, tlv_len);
    }

    printf_info("[ACTUATOR] plasma.set: enabled=%d\n", enabled);
    command_ack_publish(mosq, cmd, "SUCCESS", NULL, NULL, NULL, CMD_ACK_ERROR_NONE);
}

/*
 * ---- 3. anion.set TLV 编码示例 ----
 * 输入: {enabled=1, running=1}
 *
 * ---- 逐字段 TLV 编码 ----
 * enabled = 1 (U8):
 *   01 00 01 01
 *   │  │     └─ val = 0x01 = 1
 *   │  └─ len = 0x0001
 *   └─ tag = 0x01 (TLV_TAG_ANION_ENABLED)
 *
 * running = 1 (U8):
 *   02 00 01 01
 *   │  │     └─ val = 0x01 = 1
 *   │  └─ len = 0x0001
 *   └─ tag = 0x02 (TLV_TAG_ANION_RUNNING)
 *
 * ---- 完整 TLV 段 (struct_type 0x64) ----
 * seg_len = 0x0008 = 8 字节
 *
 * 64 00 08
 * │  └───── seg_len = 8
 * └─ struct_type = 0x64 (ActuatorAnion)
 *
 * 01 00 01 01 02 00 01 01
 *
 * ---- 完整消息 (含帧头) ----
 * AA 55  00 0B  35  64  00 08
 * │      │      │  │   └───── seg_len = 0x0008 = 8
 * │      │      │  └─ struct_type = 0x64
 * │      │      └─ 命令字 0x35
 * │      └─ lenH|lenL = 0x000B = 11 (3 + 8)
 * └─ 帧头 AA 55
 *
 * + 8 字节 TLV payload → 共 16 字节
 * 一行 hex: AA 55 00 0B 35 64 00 08 01 00 01 01 02 00 01 01
 */
void handle_anion_set(struct mosquitto *mosq, const ServerCommand *cmd)
{
    int64_t now = (int64_t)time(NULL);
    if (cmd->data_expire_at > 0 && cmd->data_expire_at < now) {
        command_ack_publish(mosq, cmd, "FAILED", NULL, NULL, NULL, CMD_ACK_ERROR_EXPIRED);
        return;
    }

    int enabled = params_get_int(cmd->params_json, "enabled", -1);
    if (enabled < 0) {
        command_ack_publish(mosq, cmd, "FAILED", NULL, NULL, NULL, CMD_ACK_ERROR_INVALID_PARAM);
        return;
    }

    g_device_state.actuators.anion.enabled = enabled;
    g_device_state.actuators.anion.running = enabled ? 1 : 0;

    uint8_t tlv_buf[128];
    int tlv_len = actuator_anion_tlv_pack(&g_device_state.actuators.anion, tlv_buf, sizeof(tlv_buf));
    if (tlv_len > 0) {
        actuator_command_send_uart(TLV_STRUCT_ANION, tlv_buf, tlv_len);
    }

    printf_info("[ACTUATOR] anion.set: enabled=%d\n", enabled);
    command_ack_publish(mosq, cmd, "SUCCESS", NULL, NULL, NULL, CMD_ACK_ERROR_NONE);
}

/*
 * ---- 4. climate.set TLV 编码示例 ----
 * 输入: {enabled=1, target_celsius=30.0, control_status=HEATING(1), running=1}
 *
 * ---- 逐字段 TLV 编码 ----
 * enabled = 1 (U8):
 *   01 00 01 01
 *   │  │     └─ val = 0x01 = 1
 *   │  └─ len = 0x0001
 *   └─ tag = 0x01 (TLV_TAG_CLIMATE_ENABLED)
 *
 * target_celsius = 30.0 (IEEE754 double):
 *   02 00 08 40 3E 00 00 00 00 00 00
 *   │  │     └─ 0x403E000000000000 = 30.0
 *   │  └─ len = 0x0008 = 8
 *   └─ tag = 0x02 (TLV_TAG_CLIMATE_TARGET)
 *
 * control_status = 1 / HEATING (U8):
 *   03 00 01 01
 *   │  │     └─ val = 0x01 = 1 (HEATING)
 *   │  └─ len = 0x0001
 *   └─ tag = 0x03 (TLV_TAG_CLIMATE_CTRL_STATUS)
 *
 * running = 1 (U8):
 *   04 00 01 01
 *   │  │     └─ val = 0x01 = 1
 *   │  └─ len = 0x0001
 *   └─ tag = 0x04 (TLV_TAG_CLIMATE_RUNNING)
 *
 * ---- 完整 TLV 段 (struct_type 0x66) ----
 * seg_len = 0x0018 = 24 字节
 *
 * 66 00 18
 * │  └───── seg_len = 24
 * └─ struct_type = 0x66 (ActuatorClimate)
 *
 * 01 00 01 01 02 00 08 40 3E 00 00 00 00 00 00 03 00 01 01 04 00 01 01
 *
 * ---- 完整消息 (含帧头) ----
 * AA 55  00 1B  35  66  00 18
 * │      │      │  │   └───── seg_len = 0x0018 = 24
 * │      │      │  └─ struct_type = 0x66
 * │      │      └─ 命令字 0x35
 * │      └─ lenH|lenL = 0x001B = 27 (3 + 24)
 * └─ 帧头 AA 55
 *
 * + 24 字节 TLV payload → 共 32 字节
 * 一行 hex: AA 55 00 1B 35 66 00 18 01 00 01 01 02 00 08 40 3E 00 00 00 00 00 00 03 00 01 01 04 00 01 01
 */
void handle_climate_set(struct mosquitto *mosq, const ServerCommand *cmd)
{
    int64_t now = (int64_t)time(NULL);
    if (cmd->data_expire_at > 0 && cmd->data_expire_at < now) {
        command_ack_publish(mosq, cmd, "FAILED", NULL, NULL, NULL, CMD_ACK_ERROR_EXPIRED);
        return;
    }

    int enabled = params_get_int(cmd->params_json, "enabled", -1);
    if (enabled < 0) {
        command_ack_publish(mosq, cmd, "FAILED", NULL, NULL, NULL, CMD_ACK_ERROR_INVALID_PARAM);
        return;
    }

    double target = params_get_double(cmd->params_json, "target_temperature_c", -1.0);

    g_device_state.actuators.climate.enabled        = enabled;
    g_device_state.actuators.climate.target_celsius = target;
    g_device_state.actuators.climate.control_status = enabled ? CLIMATE_CTRL_HEATING : CLIMATE_CTRL_OFF;
    g_device_state.actuators.climate.running        = enabled ? 1 : 0;

    uint8_t tlv_buf[256];
    int tlv_len = actuator_climate_tlv_pack(&g_device_state.actuators.climate, tlv_buf, sizeof(tlv_buf));
    if (tlv_len > 0) {
        actuator_command_send_uart(TLV_STRUCT_CLIMATE, tlv_buf, tlv_len);
    }

    printf_info("[ACTUATOR] climate.set: enabled=%d target=%.1f\n", enabled, target);
    command_ack_publish(mosq, cmd, "SUCCESS", NULL, NULL, NULL, CMD_ACK_ERROR_NONE);
}

/*
 * ---- 5. humidifier.set TLV 编码示例 ----
 * 输入: {enabled=1, interval_hours=4.0, running_hours=1.0, running=1, liquid_status=NORMAL(0)}
 *
 * ---- 逐字段 TLV 编码 ----
 * enabled = 1 (U8):
 *   01 00 01 01
 *   │  │     └─ val = 0x01 = 1
 *   │  └─ len = 0x0001
 *   └─ tag = 0x01 (TLV_TAG_HUMI_ENABLED)
 *
 * interval_hours = 4.0 (IEEE754 double):
 *   02 00 08 40 10 00 00 00 00 00 00
 *   │  │     └─ 0x4010000000000000 = 4.0
 *   │  └─ len = 0x0008 = 8
 *   └─ tag = 0x02 (TLV_TAG_HUMI_INTERVAL_HOURS)
 *
 * running_hours = 1.0 (IEEE754 double):
 *   03 00 08 3F F0 00 00 00 00 00 00
 *   │  │     └─ 0x3FF0000000000000 = 1.0
 *   │  └─ len = 0x0008 = 8
 *   └─ tag = 0x03 (TLV_TAG_HUMI_RUNNING_HOURS)
 *
 * running = 1 (U8):
 *   04 00 01 01
 *   │  │     └─ val = 0x01 = 1
 *   │  └─ len = 0x0001
 *   └─ tag = 0x04 (TLV_TAG_HUMI_RUNNING)
 *
 * liquid_status = 0 / NORMAL (U8):
 *   05 00 01 00
 *   │  │     └─ val = 0x00 = 0 (NORMAL)
 *   │  └─ len = 0x0001
 *   └─ tag = 0x05 (TLV_TAG_HUMI_LIQUID_STATUS)
 *
 * ---- 完整 TLV 段 (struct_type 0x67) ----
 * seg_len = 0x0022 = 34 字节
 *
 * 67 00 22
 * │  └───── seg_len = 34
 * └─ struct_type = 0x67 (ActuatorHumidifier)
 *
 * 01 00 01 01 02 00 08 40 10 00 00 00 00 00 00 03 00 08 3F F0 00 00 00 00 00 00 04 00 01 01 05 00 01 00
 *
 * ---- 完整消息 (含帧头) ----
 * AA 55  00 25  35  67  00 22
 * │      │      │  │   └───── seg_len = 0x0022 = 34
 * │      │      │  └─ struct_type = 0x67
 * │      │      └─ 命令字 0x35
 * │      └─ lenH|lenL = 0x0025 = 37 (3 + 34)
 * └─ 帧头 AA 55
 *
 * + 34 字节 TLV payload → 共 42 字节
 * 一行 hex: AA 55 00 25 35 67 00 22 01 00 01 01 02 00 08 40 10 00 00 00 00 00 00 03 00 08 3F F0 00 00 00 00 00 00 04 00 01 01 05 00 01 00
 */
void handle_humidifier_set(struct mosquitto *mosq, const ServerCommand *cmd)
{
    int64_t now = (int64_t)time(NULL);
    if (cmd->data_expire_at > 0 && cmd->data_expire_at < now) {
        command_ack_publish(mosq, cmd, "FAILED", NULL, NULL, NULL, CMD_ACK_ERROR_EXPIRED);
        return;
    }

    int enabled = params_get_int(cmd->params_json, "enabled", -1);
    if (enabled < 0) {
        command_ack_publish(mosq, cmd, "FAILED", NULL, NULL, NULL, CMD_ACK_ERROR_INVALID_PARAM);
        return;
    }

    double interval_hours = params_get_double(cmd->params_json, "interval_hours", 0.0);
    double running_hours  = params_get_double(cmd->params_json, "running_hours", 0.0);

    g_device_state.actuators.humidifier.enabled        = enabled;
    g_device_state.actuators.humidifier.interval_hours = interval_hours;
    g_device_state.actuators.humidifier.running_hours  = running_hours;
    g_device_state.actuators.humidifier.running        = enabled ? 1 : 0;

    uint8_t tlv_buf[256];
    int tlv_len = actuator_humidifier_tlv_pack(&g_device_state.actuators.humidifier, tlv_buf, sizeof(tlv_buf));
    if (tlv_len > 0) {
        actuator_command_send_uart(TLV_STRUCT_HUMIDIFIER, tlv_buf, tlv_len);
    }

    printf_info("[ACTUATOR] humidifier.set: enabled=%d interval=%.1f running=%.1f\n",
                enabled, interval_hours, running_hours);
    command_ack_publish(mosq, cmd, "SUCCESS", NULL, NULL, NULL, CMD_ACK_ERROR_NONE);
}

/*
 * ---- 6. uvb.set TLV 编码示例 ----
 * 输入: {enabled=1, level=2, daily_hours=4, running=1}
 *
 * ---- 逐字段 TLV 编码 ----
 * enabled = 1 (U8):
 *   01 00 01 01
 *   │  │     └─ val = 0x01 = 1
 *   │  └─ len = 0x0001
 *   └─ tag = 0x01 (TLV_TAG_UVB_ENABLED)
 *
 * level = 2 (U8):
 *   02 00 01 02
 *   │  │     └─ val = 0x02 = 2
 *   │  └─ len = 0x0001
 *   └─ tag = 0x02 (TLV_TAG_UVB_LEVEL)
 *
 * daily_hours = 4 (U8):
 *   03 00 01 04
 *   │  │     └─ val = 0x04 = 4
 *   │  └─ len = 0x0001
 *   └─ tag = 0x03 (TLV_TAG_UVB_DAILY_HOURS)
 *
 * running = 1 (U8):
 *   04 00 01 01
 *   │  │     └─ val = 0x01 = 1
 *   │  └─ len = 0x0001
 *   └─ tag = 0x04 (TLV_TAG_UVB_RUNNING)
 *
 * ---- 完整 TLV 段 (struct_type 0x63) ----
 * seg_len = 0x0010 = 16 字节
 *
 * 63 00 10
 * │  └───── seg_len = 16
 * └─ struct_type = 0x63 (ActuatorUvb)
 *
 * 01 00 01 01 02 00 01 02 03 00 01 04 04 00 01 01
 *
 * ---- 完整消息 (含帧头) ----
 * AA 55  00 13  35  63  00 10
 * │      │      │  │   └───── seg_len = 0x0010 = 16
 * │      │      │  └─ struct_type = 0x63
 * │      │      └─ 命令字 0x35
 * │      └─ lenH|lenL = 0x0013 = 19 (3 + 16)
 * └─ 帧头 AA 55
 *
 * + 16 字节 TLV payload → 共 24 字节
 * 一行 hex: AA 55 00 13 35 63 00 10 01 00 01 01 02 00 01 02 03 00 01 04 04 00 01 01
 */
void handle_uvb_set(struct mosquitto *mosq, const ServerCommand *cmd)
{
    int64_t now = (int64_t)time(NULL);
    if (cmd->data_expire_at > 0 && cmd->data_expire_at < now) {
        command_ack_publish(mosq, cmd, "FAILED", NULL, NULL, NULL, CMD_ACK_ERROR_EXPIRED);
        return;
    }

    int enabled = params_get_int(cmd->params_json, "enabled", -1);
    if (enabled < 0) {
        command_ack_publish(mosq, cmd, "FAILED", NULL, NULL, NULL, CMD_ACK_ERROR_INVALID_PARAM);
        return;
    }

    int level       = params_get_int(cmd->params_json, "level", 1);
    int daily_hours = params_get_int(cmd->params_json, "daily_hours", 0);

    g_device_state.actuators.uvb.enabled     = enabled;
    g_device_state.actuators.uvb.level       = level;
    g_device_state.actuators.uvb.daily_hours = daily_hours;
    g_device_state.actuators.uvb.running     = enabled ? 1 : 0;

    uint8_t tlv_buf[256];
    int tlv_len = actuator_uvb_tlv_pack(&g_device_state.actuators.uvb, tlv_buf, sizeof(tlv_buf));
    if (tlv_len > 0) {
        actuator_command_send_uart(TLV_STRUCT_UVB, tlv_buf, tlv_len);
    }

    printf_info("[ACTUATOR] uvb.set: enabled=%d level=%d daily_hours=%d\n",
                enabled, level, daily_hours);
    command_ack_publish(mosq, cmd, "SUCCESS", NULL, NULL, NULL, CMD_ACK_ERROR_NONE);
}

/*
 * ---- 7. light.set TLV 编码示例 ----
 * 输入: {enabled=1, brightness_level=2, running=1}
 *
 * ---- 逐字段 TLV 编码 ----
 * enabled = 1 (U8):
 *   01 00 01 01
 *   │  │     └─ val = 0x01 = 1
 *   │  └─ len = 0x0001
 *   └─ tag = 0x01 (TLV_TAG_LIGHT_ENABLED)
 *
 * brightness_level = 2 (U8):
 *   02 00 01 02
 *   │  │     └─ val = 0x02 = 2
 *   │  └─ len = 0x0001
 *   └─ tag = 0x02 (TLV_TAG_LIGHT_BRIGHTNESS)
 *
 * running = 1 (U8):
 *   03 00 01 01
 *   │  │     └─ val = 0x01 = 1
 *   │  └─ len = 0x0001
 *   └─ tag = 0x03 (TLV_TAG_LIGHT_RUNNING)
 *
 * ---- 完整 TLV 段 (struct_type 0x62) ----
 * seg_len = 0x000C = 12 字节
 *
 * 62 00 0C
 * │  └───── seg_len = 12
 * └─ struct_type = 0x62 (ActuatorLight)
 *
 * 01 00 01 01 02 00 01 02 03 00 01 01
 *
 * ---- 完整消息 (含帧头) ----
 * AA 55  00 0F  35  62  00 0C
 * │      │      │  │   └───── seg_len = 0x000C = 12
 * │      │      │  └─ struct_type = 0x62
 * │      │      └─ 命令字 0x35
 * │      └─ lenH|lenL = 0x000F = 15 (3 + 12)
 * └─ 帧头 AA 55
 *
 * + 12 字节 TLV payload → 共 20 字节
 * 一行 hex: AA 55 00 0F 35 62 00 0C 01 00 01 01 02 00 01 02 03 00 01 01
 */
void handle_light_set(struct mosquitto *mosq, const ServerCommand *cmd)
{
    int64_t now = (int64_t)time(NULL);
    if (cmd->data_expire_at > 0 && cmd->data_expire_at < now) {
        command_ack_publish(mosq, cmd, "FAILED", NULL, NULL, NULL, CMD_ACK_ERROR_EXPIRED);
        return;
    }

    int enabled = params_get_int(cmd->params_json, "enabled", -1);
    if (enabled < 0) {
        command_ack_publish(mosq, cmd, "FAILED", NULL, NULL, NULL, CMD_ACK_ERROR_INVALID_PARAM);
        return;
    }

    int brightness_level = params_get_int(cmd->params_json, "brightness_level", 1);

    g_device_state.actuators.light.enabled         = enabled;
    g_device_state.actuators.light.brightness_level = brightness_level;
    g_device_state.actuators.light.running         = enabled ? 1 : 0;

    uint8_t tlv_buf[128];
    int tlv_len = actuator_light_tlv_pack(&g_device_state.actuators.light, tlv_buf, sizeof(tlv_buf));
    if (tlv_len > 0) {
        actuator_command_send_uart(TLV_STRUCT_LIGHT, tlv_buf, tlv_len);
    }

    printf_info("[ACTUATOR] light.set: enabled=%d brightness=%d\n", enabled, brightness_level);
    command_ack_publish(mosq, cmd, "SUCCESS", NULL, NULL, NULL, CMD_ACK_ERROR_NONE);
}

/*
 * ---- 8. inlet_fan.set TLV 编码示例 ----
 * 输入: {enabled=1, speed_level=2, running=1}
 *
 * ---- 逐字段 TLV 编码 ----
 * enabled = 1 (U8):
 *   01 00 01 01
 *   │  │     └─ val = 0x01 = 1
 *   │  └─ len = 0x0001
 *   └─ tag = 0x01 (TLV_TAG_INLET_ENABLED)
 *
 * speed_level = 2 (U8):
 *   02 00 01 02
 *   │  │     └─ val = 0x02 = 2
 *   │  └─ len = 0x0001
 *   └─ tag = 0x02 (TLV_TAG_INLET_SPEED_LEVEL)
 *
 * running = 1 (U8):
 *   03 00 01 01
 *   │  │     └─ val = 0x01 = 1
 *   │  └─ len = 0x0001
 *   └─ tag = 0x03 (TLV_TAG_INLET_RUNNING)
 *
 * ---- 完整 TLV 段 (struct_type 0x68) ----
 * seg_len = 0x000C = 12 字节
 *
 * 68 00 0C
 * │  └───── seg_len = 12
 * └─ struct_type = 0x68 (ActuatorInletFan)
 *
 * 01 00 01 01 02 00 01 02 03 00 01 01
 *
 * ---- 完整消息 (含帧头) ----
 * AA 55  00 0F  35  68  00 0C
 * │      │      │  │   └───── seg_len = 0x000C = 12
 * │      │      │  └─ struct_type = 0x68
 * │      │      └─ 命令字 0x35
 * │      └─ lenH|lenL = 0x000F = 15 (3 + 12)
 * └─ 帧头 AA 55
 *
 * + 12 字节 TLV payload → 共 20 字节
 * 一行 hex: AA 55 00 0F 35 68 00 0C 01 00 01 01 02 00 01 02 03 00 01 01
 */
void handle_inlet_fan_set(struct mosquitto *mosq, const ServerCommand *cmd)
{
    int64_t now = (int64_t)time(NULL);
    if (cmd->data_expire_at > 0 && cmd->data_expire_at < now) {
        command_ack_publish(mosq, cmd, "FAILED", NULL, NULL, NULL, CMD_ACK_ERROR_EXPIRED);
        return;
    }

    int enabled = params_get_int(cmd->params_json, "enabled", -1);
    if (enabled < 0) {
        command_ack_publish(mosq, cmd, "FAILED", NULL, NULL, NULL, CMD_ACK_ERROR_INVALID_PARAM);
        return;
    }

    int speed_level = params_get_int(cmd->params_json, "speed_level", 1);

    g_device_state.actuators.inlet_fan.enabled     = enabled;
    g_device_state.actuators.inlet_fan.speed_level = speed_level;
    g_device_state.actuators.inlet_fan.running     = enabled ? 1 : 0;

    uint8_t tlv_buf[128];
    int tlv_len = actuator_inlet_fan_tlv_pack(&g_device_state.actuators.inlet_fan, tlv_buf, sizeof(tlv_buf));
    if (tlv_len > 0) {
        actuator_command_send_uart(TLV_STRUCT_INLET_FAN, tlv_buf, tlv_len);
    }

    printf_info("[ACTUATOR] inlet_fan.set: enabled=%d speed=%d\n", enabled, speed_level);
    command_ack_publish(mosq, cmd, "SUCCESS", NULL, NULL, NULL, CMD_ACK_ERROR_NONE);
}

/*
 * ---- 9. camera_privacy.set TLV 编码示例 ----
 * 输入: {enabled=1} → 隐私开启, 将 CameraState 整体打包
 * 典型 CameraState: {camera_status=ONLINE(1), stream_status=IDLE(0), provider="",
 *   video_codec=H264(0), resolution=720p(1), fps=15.0, audio_enabled=0,
 *   privacy_mode=1, last_session_id="", last_error_code=""}
 *
 * ---- 逐字段 TLV 编码 ----
 * camera_status = 1 / ONLINE (U8):
 *   01 00 01 01
 *   │  │     └─ val = 0x01 = 1 (ONLINE)
 *   │  └─ len = 0x0001
 *   └─ tag = 0x01 (TLV_TAG_CAMERA_STATUS)
 *
 * stream_status = 0 / IDLE (U8):
 *   02 00 01 00
 *   │  │     └─ val = 0x00 = 0 (IDLE)
 *   │  └─ len = 0x0001
 *   └─ tag = 0x02 (TLV_TAG_STREAM_STATUS)
 *
 * provider = "" (STR, 空):
 *   03 00 00
 *   │  └─ len = 0x0000 = 0
 *   └─ tag = 0x03 (TLV_TAG_PROVIDER)
 *
 * video_codec = 0 / H264 (U8):
 *   04 00 01 00
 *   │  │     └─ val = 0x00 = 0 (H264)
 *   │  └─ len = 0x0001
 *   └─ tag = 0x04 (TLV_TAG_VIDEO_CODEC)
 *
 * resolution = 1 / 720p (U8):
 *   05 00 01 01
 *   │  │     └─ val = 0x01 = 1 (720p)
 *   │  └─ len = 0x0001
 *   └─ tag = 0x05 (TLV_TAG_RESOLUTION)
 *
 * fps = 15.0 (IEEE754 double):
 *   06 00 08 40 2E 00 00 00 00 00 00
 *   │  │     └─ 0x402E000000000000 = 15.0
 *   │  └─ len = 0x0008 = 8
 *   └─ tag = 0x06 (TLV_TAG_FPS)
 *
 * audio_enabled = 0 (U8):
 *   07 00 01 00
 *   │  │     └─ val = 0x00 = 0
 *   │  └─ len = 0x0001
 *   └─ tag = 0x07 (TLV_TAG_AUDIO_ENABLED)
 *
 * privacy_mode = 1 (U8):
 *   08 00 01 01
 *   │  │     └─ val = 0x01 = 1 (隐私开启)
 *   │  └─ len = 0x0001
 *   └─ tag = 0x08 (TLV_TAG_PRIVACY_MODE)
 *
 * last_session_id = "" (STR, 空):
 *   09 00 00
 *   │  └─ len = 0x0000 = 0
 *   └─ tag = 0x09 (TLV_TAG_LAST_SESSION_ID)
 *
 * last_error_code = "" (STR, 空):
 *   0A 00 00
 *   │  └─ len = 0x0000 = 0
 *   └─ tag = 0x0A (TLV_TAG_LAST_ERROR_CODE)
 *
 * ---- 完整 TLV 段 (struct_type 0x07) ----
 * seg_len = 0x002C = 44 字节
 *
 * 07 00 2C
 * │  └───── seg_len = 44
 * └─ struct_type = 0x07 (CameraState)
 *
 * 01 00 01 01 02 00 01 00 03 00 00 04 00 01 00 05 00 01 01 06 00 08 40 2E 00 00 00 00 00 00 07 00 01 00 08 00 01 01 09 00 00 0A 00 00
 *
 * ---- 完整消息 (含帧头) ----
 * AA 55  00 2F  35  07  00 2C
 * │      │      │  │   └───── seg_len = 0x002C = 44
 * │      │      │  └─ struct_type = 0x07
 * │      │      └─ 命令字 0x35
 * │      └─ lenH|lenL = 0x002F = 47 (3 + 44)
 * └─ 帧头 AA 55
 *
 * + 44 字节 TLV payload → 共 52 字节
 * 一行 hex: AA 55 00 2F 35 07 00 2C 01 00 01 01 02 00 01 00 03 00 00 04 00 01 00 05 00 01 01 06 00 08 40 2E 00 00 00 00 00 00 07 00 01 00 08 00 01 01 09 00 00 0A 00 00
 */
void handle_camera_privacy_set(struct mosquitto *mosq, const ServerCommand *cmd)
{
    int64_t now = (int64_t)time(NULL);
    if (cmd->data_expire_at > 0 && cmd->data_expire_at < now) {
        command_ack_publish(mosq, cmd, "FAILED", NULL, NULL, NULL, CMD_ACK_ERROR_EXPIRED);
        return;
    }

    int enabled = params_get_int(cmd->params_json, "enabled", -1);
    if (enabled < 0) {
        command_ack_publish(mosq, cmd, "FAILED", NULL, NULL, NULL, CMD_ACK_ERROR_INVALID_PARAM);
        return;
    }

    /* 隐私模式: enabled=1 表示开启隐私 (关摄像头), enabled=0 表示关闭隐私 (开摄像头) */
    g_device_state.camera.privacy_mode = enabled ? 1 : 0;

    /* 将 CameraState 整体打包为 TLV 下发到串口 */
    uint8_t tlv_buf[512];
    int tlv_len = camera_state_tlv_pack(&g_device_state.camera, tlv_buf, sizeof(tlv_buf));
    if (tlv_len > 0) {
        actuator_command_send_uart(TLV_STRUCT_CAMERA, tlv_buf, tlv_len);
    }

    printf_info("[ACTUATOR] camera_privacy.set: enabled=%d (privacy_mode=%d)\n",
                enabled, g_device_state.camera.privacy_mode);
    command_ack_publish(mosq, cmd, "SUCCESS", NULL, NULL, NULL, CMD_ACK_ERROR_NONE);
}

/* ============================================================================
 * 3 条系统指令 handler
 * 每条指令流程: 解析 params → 校验 → 更新 g_device_state → TLV 打包 → 串口下发 → ACK
 *
 * 串口帧格式: AA 55 | lenH lenL | 0x35 | struct_type[1B] seg_len[2B] | tlv_payload
 * ============================================================================ */

/*
 * ---- 10. device_settings.set TLV 编码示例 ----
 * 输入: {settings: {temperature_unit="C", display:{brightness_percent=80,
 *        screen_timeout_minutes=10, screen_mode="SCREEN_SAVER"},
 *        volume_percent=60, language="zh-CN"}}
 *
 * ---- 逐字段 TLV 编码 ----
 * temperature_unit = "C" (STR, 1B):
 *   01 00 01 43
 *   │  │     └─ "C" = {0x43}
 *   │  └─ len = 0x0001
 *   └─ tag = 0x01 (TLV_TAG_TEMP_UNIT)
 *
 * display = {brightness=80, screen_timeout=10, screen_mode=SCREEN_SAVER(0)} (嵌套 TLV):
 *   02 00 0A 01 00 01 50 02 00 01 0A 03 00 01 00
 *   │  │     └┈┈┈┈┈┈ DisplaySettings 嵌套段 (10B)
 *   │  │          01 00 01 50 → brightness_percent=80
 *   │  │          02 00 01 0A → screen_timeout_minutes=10
 *   │  │          03 00 01 00 → screen_mode=0 (SCREEN_SAVER)
 *   │  └─ len = 0x000A = 10
 *   └─ tag = 0x02 (TLV_TAG_DISPLAY)
 *
 * volume_percent = 60 (U8):
 *   03 00 01 3C
 *   │  │     └─ val = 0x3C = 60
 *   │  └─ len = 0x0001
 *   └─ tag = 0x03 (TLV_TAG_VOLUME_PERCENT)
 *
 * language = "zh-CN" (STR, 5B):
 *   04 00 05 7A 68 2D 43 4E
 *   │  │     └─ "zh-CN" = {0x7A,0x68,0x2D,0x43,0x4E}
 *   │  └─ len = 0x0005
 *   └─ tag = 0x04 (TLV_TAG_LANGUAGE)
 *
 * local_password_enabled = 0 (U8):
 *   05 00 01 00
 *   │  │     └─ val = 0x00 = 0
 *   │  └─ len = 0x0001
 *   └─ tag = 0x05 (TLV_TAG_LOCAL_PASSWORD)
 *
 * ---- 完整 TLV 段 (struct_type 0x08) ----
 * seg_len = 0x0023 = 35 字节
 *
 * 08 00 23
 * │  └───── seg_len = 35
 * └─ struct_type = 0x08 (SettingsState)
 *
 * 01 00 01 43 02 00 0A 01 00 01 50 02 00 01 0A 03 00 01 00 03 00 01 3C 04 00 05 7A 68 2D 43 4E 05 00 01 00
 *
 * ---- 完整消息 (含帧头) ----
 * AA 55  00 26  35  08  00 23
 * │      │      │  │   └───── seg_len = 0x0023 = 35
 * │      │      │  └─ struct_type = 0x08
 * │      │      └─ 命令字 0x35
 * │      └─ lenH|lenL = 0x0026 = 38 (3 + 35)
 * └─ 帧头 AA 55
 *
 * + 35 字节 TLV payload → 共 43 字节
 * 一行 hex: AA 55 00 26 35 08 00 23 01 00 01 43 02 00 0A 01 00 01 50 02 00 01 0A 03 00 01 00 03 00 01 3C 04 00 05 7A 68 2D 43 4E 05 00 01 00
 */
void handle_device_settings_set(struct mosquitto *mosq, const ServerCommand *cmd)
{
    int64_t now = (int64_t)time(NULL);

    if (cmd->data_expire_at > 0 && cmd->data_expire_at < now) {
        command_ack_publish(mosq, cmd, "FAILED", NULL, NULL, NULL, CMD_ACK_ERROR_EXPIRED);
        return;
    }

    /* 解析 params.settings 子对象 — 拷贝到本地缓冲区, 避免后续 params_get_str
     * 轮转覆盖该指针 (settings_json 作为后续多次 params_get_str 的输入参数) */
    const char *settings_raw = params_get_str(cmd->params_json, "settings", NULL);
    if (!settings_raw) {
        command_ack_publish(mosq, cmd, "FAILED", NULL, NULL, NULL, CMD_ACK_ERROR_INVALID_PARAM);
        return;
    }
    char settings_buf[1024];
    strncpy(settings_buf, settings_raw, sizeof(settings_buf) - 1);
    settings_buf[sizeof(settings_buf) - 1] = '\0';
    const char *settings_json = settings_buf;

    /* 逐个解析 settings 字段 */
    const char *temp_unit = params_get_str(settings_json, "temperature_unit", "");
    if (temp_unit[0]) strncpy(g_device_state.settings.temperature_unit, temp_unit,
                               sizeof(g_device_state.settings.temperature_unit) - 1);

    const char *display_json = params_get_str(settings_json, "display", NULL);
    if (display_json) {
        int brightness = params_get_int(display_json, "brightness_percent", -1);
        if (brightness >= 0) g_device_state.settings.display.brightness_percent = brightness;

        int timeout = params_get_int(display_json, "screen_timeout_minutes", -1);
        if (timeout >= 0) g_device_state.settings.display.screen_timeout_minutes = timeout;

        const char *smode = params_get_str(display_json, "screen_mode", "");
        if (smode[0]) g_device_state.settings.display.screen_mode = screen_mode_from_str(smode);
    }

    int volume = params_get_int(settings_json, "volume_percent", -1);
    if (volume >= 0) g_device_state.settings.volume_percent = volume;

    const char *lang = params_get_str(settings_json, "language", "");
    if (lang[0]) strncpy(g_device_state.settings.language, lang,
                          sizeof(g_device_state.settings.language) - 1);

    /* TLV 打包 → 串口下发 */
    uint8_t tlv_buf[512];
    int tlv_len = settings_state_tlv_pack(&g_device_state.settings, tlv_buf, sizeof(tlv_buf));
    if (tlv_len > 0) {
        actuator_command_send_uart(TLV_STRUCT_SETTINGS, tlv_buf, tlv_len);
    }

    printf_info("[SYS] device_settings.set: temp_unit=%s brightness=%d timeout=%d volume=%d lang=%s\n",
                temp_unit, g_device_state.settings.display.brightness_percent,
                g_device_state.settings.display.screen_timeout_minutes,
                g_device_state.settings.volume_percent,
                g_device_state.settings.language);
    command_ack_publish(mosq, cmd, "SUCCESS", NULL, NULL, NULL, CMD_ACK_ERROR_NONE);
}

/*
 * ---- 11. device_password.set TLV 编码示例 ----
 * 输入: {old_password="123456", new_password="654321"}
 *
 * 注意: 密码本身不进入 TLV, 仅更新 local_password_enabled 标志后打包 SettingsState。
 * 典型 SettingsState: {temperature_unit="C", display:{brightness=80, timeout=10,
 *   screen_mode=SCREEN_SAVER(0)}, volume_percent=60, language="zh-CN",
 *   local_password_enabled=1}
 *
 * ---- 逐字段 TLV 编码 (仅关键字段) ----
 * temperature_unit = "C" (STR, 1B):
 *   01 00 01 43
 * display = {brightness=80, timeout=10, mode=0} (嵌套 TLV, 10B):
 *   02 00 0A 01 00 01 50 02 00 01 0A 03 00 01 00
 * volume_percent = 60 (U8):
 *   03 00 01 3C
 * language = "zh-CN" (STR, 5B):
 *   04 00 05 7A 68 2D 43 4E
 * local_password_enabled = 1 (U8) ← 密码修改后置 1:
 *   05 00 01 01
 *   │  │     └─ val = 0x01 = 1 (密码已启用)
 *   │  └─ len = 0x0001
 *   └─ tag = 0x05 (TLV_TAG_LOCAL_PASSWORD)
 *
 * ---- 完整 TLV 段 (struct_type 0x08) ----
 * seg_len = 0x0023 = 35 字节
 *
 * 08 00 23
 * │  └───── seg_len = 35
 * └─ struct_type = 0x08 (SettingsState)
 *
 * 01 00 01 43 02 00 0A 01 00 01 50 02 00 01 0A 03 00 01 00 03 00 01 3C 04 00 05 7A 68 2D 43 4E 05 00 01 01
 *
 * ---- 完整消息 (含帧头) ----
 * AA 55  00 26  35  08  00 23
 * │      │      │  │   └───── seg_len = 0x0023 = 35
 * │      │      │  └─ struct_type = 0x08
 * │      │      └─ 命令字 0x35
 * │      └─ lenH|lenL = 0x0026 = 38 (3 + 35)
 * └─ 帧头 AA 55
 *
 * + 35 字节 TLV payload → 共 43 字节
 * 一行 hex: AA 55 00 26 35 08 00 23 01 00 01 43 02 00 0A 01 00 01 50 02 00 01 0A 03 00 01 00 03 00 01 3C 04 00 05 7A 68 2D 43 4E 05 00 01 01
 */
void handle_device_password_set(struct mosquitto *mosq, const ServerCommand *cmd)
{
    int64_t now = (int64_t)time(NULL);

    if (cmd->data_expire_at > 0 && cmd->data_expire_at < now) {
        command_ack_publish(mosq, cmd, "FAILED", NULL, NULL, NULL, CMD_ACK_ERROR_EXPIRED);
        return;
    }

    const char *old_pwd = params_get_str(cmd->params_json, "old_password", NULL);
    const char *new_pwd = params_get_str(cmd->params_json, "new_password", NULL);
    if (!old_pwd || !new_pwd) {
        command_ack_publish(mosq, cmd, "FAILED", NULL, NULL, NULL, CMD_ACK_ERROR_INVALID_PARAM);
        return;
    }

    /* 密码明文仅出现在本次 MQTT payload, 不记录到日志/数据库 */
    printf_info("[SYS] device_password.set: old=%s new=%s\n", old_pwd, new_pwd);

    /* 更新本地密码启用标志 */
    g_device_state.settings.local_password_enabled = 1;

    /* TLV 打包 SettingsState → 串口下发 */
    uint8_t tlv_buf[512];
    int tlv_len = settings_state_tlv_pack(&g_device_state.settings, tlv_buf, sizeof(tlv_buf));
    if (tlv_len > 0) {
        actuator_command_send_uart(TLV_STRUCT_SETTINGS, tlv_buf, tlv_len);
    }

    command_ack_publish(mosq, cmd, "SUCCESS", NULL, NULL, NULL, CMD_ACK_ERROR_NONE);
}

/*
 * ---- 12. ota.upgrade TLV 编码示例 ----
 * 输入: {firmware_version="2.2.0", firmware_url="https://ota.mcqx.pet/fw.bin",
 *        sha256="a1b2c3...64hex", size_bytes=4194304, force=false}
 *
 * ---- 逐字段 TLV 编码 ----
 * firmware_version = "2.2.0" (STR, 5B):
 *   01 00 05 32 2E 32 2E 30
 *   │  │     └─ "2.2.0" = {0x32,0x2E,0x32,0x2E,0x30}
 *   │  └─ len = 0x0005 = 5
 *   └─ tag = 0x01 (TLV_TAG_FW_VERSION)
 *
 * hardware_version = "R1" (STR, 2B, 保持不变):
 *   02 00 02 52 31
 *   │  │     └─ "R1" = {0x52,0x31}
 *   │  └─ len = 0x0002 = 2
 *   └─ tag = 0x02 (TLV_TAG_HW_VERSION)
 *
 * config_version = 0 (U32, 与固件升级无关, 保持原值):
 *   03 00 04 00 00 00 00
 *   │  │     └─ 0x00000000 = 0
 *   │  └─ len = 0x0004 = 4
 *   └─ tag = 0x03 (TLV_TAG_CONFIG_VERSION)
 *
 * ---- 完整 TLV 段 (struct_type 0x09) ----
 * seg_len = 0x0018 = 24 字节
 *
 * 09 00 18
 * │  └───── seg_len = 24
 * └─ struct_type = 0x09 (FirmwareState)
 *
 * 01 00 05 32 2E 32 2E 30 02 00 02 52 31 03 00 04 00 00 00 00
 *
 * ---- 完整消息 (含帧头) ----
 * AA 55  00 1B  35  09  00 18
 * │      │      │  │   └───── seg_len = 0x0018 = 24
 * │      │      │  └─ struct_type = 0x09
 * │      │      └─ 命令字 0x35
 * │      └─ lenH|lenL = 0x001B = 27 (3 + 24)
 * └─ 帧头 AA 55
 *
 * + 24 字节 TLV payload → 共 32 字节
 * 一行 hex: AA 55 00 1B 35 09 00 18 01 00 05 32 2E 32 2E 30 02 00 02 52 31 03 00 04 00 00 00 00
 */
void handle_ota_upgrade(struct mosquitto *mosq, const ServerCommand *cmd)
{
    int64_t now = (int64_t)time(NULL);

    if (cmd->data_expire_at > 0 && cmd->data_expire_at < now) {
        command_ack_publish(mosq, cmd, "FAILED", NULL, NULL, NULL, CMD_ACK_ERROR_EXPIRED);
        return;
    }

    /* 解析 OTA 参数 */
    const char *fw_ver   = params_get_str(cmd->params_json, "firmware_version", "");
    const char *fw_url   = params_get_str(cmd->params_json, "firmware_url", "");
    const char *sha256   = params_get_str(cmd->params_json, "sha256", "");
    int         size_mb  = params_get_int(cmd->params_json, "size_bytes", 0);
    int         force    = params_get_int(cmd->params_json, "force", 0);

    if (!fw_ver[0] || !fw_url[0] || !sha256[0] || size_mb <= 0) {
        printf_info("[SYS] ota.upgrade: invalid params ver=%s url=%s sha256=%s size=%d\n",
                    fw_ver, fw_url, sha256, size_mb);
        command_ack_publish(mosq, cmd, "FAILED", NULL, NULL, NULL, CMD_ACK_ERROR_INVALID_PARAM);
        return;
    }

    /* 更新 firmare_version (硬件版本保持不变, config_version 暂不修改) */
    strncpy(g_device_state.firmware.firmware_version, fw_ver,
            sizeof(g_device_state.firmware.firmware_version) - 1);

    printf_info("[SYS] ota.upgrade: ver=%s url=%s sha256=%s size=%d force=%d\n",
                fw_ver, fw_url, sha256, (int)size_mb, force);

    /* TLV 打包 FirmwareState → 串口下发 */
    uint8_t tlv_buf[256];
    int tlv_len = firmware_state_tlv_pack(&g_device_state.firmware, tlv_buf, sizeof(tlv_buf));
    if (tlv_len > 0) {
        actuator_command_send_uart(TLV_STRUCT_FIRMWARE, tlv_buf, tlv_len);
    }

    command_ack_publish(mosq, cmd, "SUCCESS", NULL, NULL, NULL, CMD_ACK_ERROR_NONE);
}


/* Callback called when the client receives a CONNACK message from the broker. */
void on_connect(struct mosquitto *mosq, void *obj, int reason_code)
{
	printf_info("on_connect: %s\n", mosquitto_connack_string(reason_code));
	if(reason_code != 0){
		g_mqtt_config.mqtt_connected = 0;
		mosquitto_disconnect(mosq);
	} else {
		g_mqtt_config.mqtt_connected = 1;
		printf_info("MQTT connected successfully\n");

		/* 订阅下行 Topic: /mcqx/device/{device_sn}/down */
		{
			char down_topic[128];
			snprintf(down_topic, sizeof(down_topic), "/mcqx/device/%s/down", DEVICE_SN);
			int sub_rc = mosquitto_subscribe(mosq, NULL, down_topic, 1);
			if (sub_rc == MOSQ_ERR_SUCCESS) {
				printf_info("subscribed: %s\n", down_topic);
			} else {
				printf_info("subscribe %s failed: %s\n", down_topic, mosquitto_strerror(sub_rc));
			}
		}
	}
}

/* Callback called when the client disconnects from the broker. */
void on_disconnect(struct mosquitto *mosq, void *obj, int reason_code)
{
	g_mqtt_config.mqtt_connected = 0;
	printf_info("on_disconnect: %s (reason_code=%d)\n", mosquitto_reason_string(reason_code), reason_code);
}


/* Callback called when the client knows to the best of its abilities that a
 * PUBLISH has been successfully sent. For QoS 0 this means the message has
 * been completely written to the operating system. For QoS 1 this means we
 * have received a PUBACK from the broker. For QoS 2 this means we have
 * received a PUBCOMP from the broker. */
void on_publish(struct mosquitto *mosq, void *obj, int mid)
{
	printf_info("Message with mid %d has been published.\n", mid);
}





/* Callback called when the broker sends a SUBACK in response to a SUBSCRIBE. */
void on_subscribe(struct mosquitto *mosq, void *obj, int mid, int qos_count, const int *granted_qos)
{
	int i;
	bool have_subscription = false;

	/* In this example we only subscribe to a single topic at once, but a
	 * SUBSCRIBE can contain many topics at once, so this is one way to check
	 * them all. */
	for(i=0; i<qos_count; i++){
		printf_info("on_subscribe: %d:granted qos = %d\n", i, granted_qos[i]);
		if(granted_qos[i] <= 2){
			have_subscription = true;
		}
	}
	if(have_subscription == false){
		/* The broker rejected all of our subscriptions, we know we only sent
		 * the one SUBSCRIBE, so there is no point remaining connected. */
		printf_info(  "Error: All subscriptions rejected.\n");
		mosquitto_disconnect(mosq);
	}
}


/* Callback called when the client receives a message. */
void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg)
{
	printf_info(" --recv mqtt mess: [topic]%s [payloadlen]%d [qos]%d [payload]%s\n", msg->topic, msg->payloadlen, msg->qos, (char *)msg->payload);

	/* 将收到的 MQTT 消息封装为 Inner_cmd_msg 推入队列
	 * 数据布局: Inner_cmd_msg | payload | topic(含'\\0') */
	if (queue_from_mqtt && msg->payload && msg->payloadlen > 0) {
		char *new_payload = NULL;
		int payloadLen = msg->payloadlen;
		int topicLen = (int)strlen(msg->topic) + 1;
		int headerlen = sizeof(Inner_cmd_msg);
		int totalLen = headerlen + payloadLen + topicLen;
		int ret = mallocArray("mqtt_recv", &new_payload, totalLen);
		if (ret == 0) {
			Inner_cmd_msg *header = (Inner_cmd_msg *)new_payload;
			header->cmdid = 0xE0;
			header->datalen = payloadLen;
			header->pic_buffer_identify = NULL;
			memcpy(new_payload + headerlen, msg->payload, payloadLen);
			memcpy(new_payload + headerlen + payloadLen, msg->topic, topicLen);
			packet_data_to_queue(new_payload, "mqtt_recv", queue_from_mqtt, false);
		}
	}
}

int get_temperature(void)
{
	sleep(1); /* Prevent a storm of messages - this pretend sensor works at 1Hz */
	return random()%100;
}

/* This function pretends to read some data from a sensor and publish it.*/
void publish_sensor_data(struct mosquitto *mosq)
{
	char payload[20];
	int temp;
	int rc;

	/* Get our pretend data */
	temp = get_temperature();
	/* Print it to a string for easy human reading - payload format is highly
	 * application dependent. */
	snprintf(payload, sizeof(payload), "%d", temp);

	/* Publish the message
	 * mosq - our client instance
	 * *mid = NULL - we don't want to know what the message id for this message is
	 * topic = "example/temperature" - the topic on which this message will be published
	 * payloadlen = strlen(payload) - the length of our payload in bytes
	 * payload - the actual payload
	 * qos = 2 - publish with QoS 2 for this example
	 * retain = false - do not use the retained message feature for this message
	 */
	rc = mosquitto_publish(mosq, NULL, "example/temperature", strlen(payload), payload, 2, false);
	if(rc != MOSQ_ERR_SUCCESS){
		printf_info(  "Error publishing: %s\n", mosquitto_strerror(rc));
	}
}



void my_log_callback(struct mosquitto *mosq, void *obj, int level, const char *str)
{
 

	printf_info(" mqttssl debug: %s\n", str);
}



int mqttssl_thread_task(void)
{
	struct mosquitto *mosq;
	int rc;

	tlv_thread_exit = 1;  /* 进入主循环 */

	while (tlv_thread_exit) {
		/* ================================================================ */
		/* 阶段 1: Bootstrap 自举轮询 (hello 之前)                          */
		/* 设备联网后先 bootstrap 获取 device_sn / ble_id / endpoint,       */
		/* 每 10 秒轮询一次, TLV 下发结果到串口, 绑定后进入 hello 阶段       */
		/* ================================================================ */
		if (!g_bootstrap_config.bootstrap_done) {
			printf_info("=== Bootstrap phase: waiting for network & device identity ===\n");

			while (tlv_thread_exit && !g_bootstrap_config.bootstrap_done) {
				/* 1. 等待网络就绪 (网卡 up + 获取到 IP) */
				char ip_str[32] = {0};
				if (_check_network_state() != 1 || _get_ip(ip_str) != 1) {
					printf_info("bootstrap: network not ready, waiting...\n");
					sleep(3);
					continue;
				}
				printf_info("bootstrap: network ready, ip=%s\n", ip_str);

				/* 2. 调用 bootstrap 接口获取设备身份 */
				if (http_post_bootstrap() == 0) {
					/* 3. 将 bootstrap 结果 TLV 下发到串口 (D5/LCD) */
					bootstrap_result_send_uart(&g_bootstrap_config);

					/* 4. 检查绑定状态 */
					if (strcmp(g_bootstrap_config.bind_status, "UNBOUND") == 0) {
						/* 未绑定: 间隔 10 秒后重新轮询 */
						printf_info("bootstrap: bind_status=UNBOUND, retry in 10s...\n");
						sleep(10);
						/* 重置 bootstrap_done 以便下一轮重新请求 */
						g_bootstrap_config.bootstrap_done = 0;
					} else {
						/* BINDING 或 BOUND: 已绑定(或已有绑定会话), 进入 hello 阶段 */
						printf_info("bootstrap: bind_status=%s, proceeding to hello...\n",
							g_bootstrap_config.bind_status);
						/* bootstrap_done 保持 1, 跳出轮询循环 */
					}
				} else {
					/* bootstrap 失败: 10 秒后重试 */
					printf_info("bootstrap: failed, retry in 10s...\n");
					sleep(10);
				}
			}

			if (!tlv_thread_exit) break;   /* 退出信号 */
			printf_info("=== Bootstrap phase done: device_sn=%s ===\n",
				g_bootstrap_config.device_sn);
		}

		/* ================================================================ */
		/* 阶段 2: Hello — 获取 MQTT 凭证                                    */
		/* ================================================================ */
		/* 等待 hello 请求完成并获取配置后才可连接 MQTT */
		while (!g_mqtt_config.hello_getnew_flag && tlv_thread_exit) {
			printf_info("http_post_hello retry...\n");
			http_post_hello();
			if (!g_mqtt_config.hello_getnew_flag && tlv_thread_exit) {
				sleep(3);
			}
		}
		if (!tlv_thread_exit) break;   /* 退出信号, 跳出外层 while */
		printf_info("hello_getnew_flag=1, ready to connect MQTT\n");

		/* 确保在 hello 之后已获取 Video License 信息 */
		if (!g_video_license.fetched) {
			printf_info("Video license empty, fetching now...\n");
			if (http_post_video_license(NULL, "TANGE", &g_video_license) == 0) {
				video_license_save(&g_video_license);
			}
		}

		/* 检查 credential 是否过期, 过期则重新 hello */
		{
			int64_t now = (int64_t)time(NULL);
			if (g_mqtt_config.credential_expire_at > 0 && now >= g_mqtt_config.credential_expire_at) {
				printf_info("credential expired (expire_at=%lld, now=%lld), re-hello...\n",
					(long long)g_mqtt_config.credential_expire_at, (long long)now);
				g_mqtt_config.hello_getnew_flag = 0;
				while (!g_mqtt_config.hello_getnew_flag && tlv_thread_exit) {
					printf_info("re-hello retry...\n");
					http_post_hello();
					if (!g_mqtt_config.hello_getnew_flag && tlv_thread_exit) {
						sleep(3);
					}
				}
				if (!tlv_thread_exit) break;   /* 退出信号, 跳出外层 while */
				printf_info("re-hello done, credential renewed expire_at=%lld\n",
					(long long)g_mqtt_config.credential_expire_at);
			}
		}

		/* Required before calling other mosquitto functions */
		mosquitto_lib_init();

		/* Create a new client instance.
		 * id = NULL -> ask the broker to generate a client id for us
		 * clean session = true -> the broker should remove old sessions when we connect
		 * obj = NULL -> we aren't passing any of our private data for callbacks
		 */
		mosq = mosquitto_new(g_mqtt_config.client_id, true, NULL);
		if (mosq == NULL) {
			printf_info("Error: Out of memory.\n");
			continue;
		}

		/* 配置 TLS (关键步骤：连接 MQTTS 必须) */
		rc = mosquitto_tls_set(mosq, NULL, "/etc/ssl/certs/", NULL, NULL, NULL);
		if (rc != MOSQ_ERR_SUCCESS) {
			printf_info("TLS setup failed\n");
			mosquitto_destroy(mosq);
			continue;
		}

		mosquitto_log_callback_set(mosq, my_log_callback);

		/* Configure callbacks. This should be done before connecting ideally. */
		mosquitto_connect_callback_set(mosq, on_connect);
		mosquitto_disconnect_callback_set(mosq, on_disconnect);
		mosquitto_publish_callback_set(mosq, on_publish);
		mosquitto_subscribe_callback_set(mosq, on_subscribe);
		mosquitto_message_callback_set(mosq, on_message);

		mosquitto_username_pw_set(mosq, g_mqtt_config.username, g_mqtt_config.password);

		/* Last Will and Testament: 设备异常断线时 broker 自动发布遗嘱消息 */
		{
			char will_topic[128];
			snprintf(will_topic, sizeof(will_topic), "/mcqx/device/%s/up", DEVICE_SN);
			const char *will_payload =
				"{"
					"\"msg_type\":\"device.event\","
					"\"data\":{"
						"\"event_type\":\"device.offline\","
						"\"reason\":\"mqtt_lost\""
					"}"
				"}";
			int will_qos = 1;
			bool will_retain = false;
			rc = mosquitto_will_set(mosq, will_topic,
				(int)strlen(will_payload), will_payload,
				will_qos, will_retain);
			if (rc != MOSQ_ERR_SUCCESS) {
				printf_info("Will set failed: %s\n", mosquitto_strerror(rc));
			} else {
				printf_info("Will set: topic=%s qos=%d retain=%d\n", will_topic, will_qos, will_retain);
			}
		}

		/* Connect to broker */
		rc = mosquitto_connect(mosq, g_mqtt_config.host, g_mqtt_config.port, g_mqtt_config.keepalive_seconds);
		if (rc != MOSQ_ERR_SUCCESS) {
			mosquitto_destroy(mosq);
			printf_info("Error: %s\n", mosquitto_strerror(rc));
			continue;
		}

		/* Run the network loop in a background thread, this call returns quickly. */
		rc = mosquitto_loop_start(mosq);
		if (rc != MOSQ_ERR_SUCCESS) {
			mosquitto_destroy(mosq);
			printf_info("Error: %s\n", mosquitto_strerror(rc));
			continue;
		}

		/* At this point the client is connected to the network socket, but may not
		 * have completed CONNECT/CONNACK.
		 * It is fairly safe to start queuing messages at this point, but if you
		 * want to be really sure you should wait until after a successful call to
		 * the connect callback.
		 * In this case we know it is 1 second before we start publishing.
		 */
		while (tlv_thread_exit)
		{
			/* device.heartbeat 心跳: 每 60 秒发送一次 */
			{
				static time_t last_heartbeat = 0;
				static int heartbeat_seq = 0;
				static time_t start_time = 0;
				time_t now = time(NULL);
				if (start_time == 0) start_time = now;

				if (g_mqtt_config.mqtt_connected) {
					if (now - last_heartbeat >= 60) {
						last_heartbeat = now;
						heartbeat_seq++;

						char up_topic[128];
						snprintf(up_topic, sizeof(up_topic), "/mcqx/device/%s/up", DEVICE_SN);

						/* msg_id: HEARTBEAT-{device_sn}-{timestamp} */
						char msg_id[128];
						snprintf(msg_id, sizeof(msg_id), "HEARTBEAT-%s-%lld", DEVICE_SN, (long long)now);

						char heartbeat_json[512];
						snprintf(heartbeat_json, sizeof(heartbeat_json),
							"{"
								"\"msg_id\":\"%s\","
								"\"msg_type\":\"device.heartbeat\","
								"\"protocol_version\":\"%s\","
								"\"device_sn\":\"%s\","
								"\"ble_id\":\"%s\","
								"\"product_key\":\"%s\","
								"\"timestamp\":%lld,"
								"\"seq\":%d,"
								"\"data\":{"
									"\"uptime_seconds\":%ld,"
									"\"rssi\":0,"
									"\"free_memory_kb\":0"
								"}"
							"}",
							msg_id,
							PROTOCOL_VER,
							DEVICE_SN,
							BLE_ID,
							PRODUCT_KEY,
							(long long)now,
							heartbeat_seq,
							(long)(now - start_time)
						);

						int hb_rc = mosquitto_publish(mosq, NULL, up_topic,
							(int)strlen(heartbeat_json), heartbeat_json, 1, false);
						if (hb_rc == MOSQ_ERR_SUCCESS) {
							printf_info("heartbeat sent: seq=%d\n", heartbeat_seq);
						} else {
							printf_info("heartbeat publish failed: %s\n", mosquitto_strerror(hb_rc));
						}
					}
				}
			}

			/* 断线且 credential 过期时重新 hello 获取凭证 */
			if (!g_mqtt_config.mqtt_connected && g_mqtt_config.credential_expire_at > 0) {
				int64_t now = (int64_t)time(NULL);
				if (now >= g_mqtt_config.credential_expire_at) {
					printf_info("mqtt disconnected & credential expired, re-hello...\n");
					g_mqtt_config.hello_getnew_flag = 0;
					while (!g_mqtt_config.hello_getnew_flag && tlv_thread_exit) {
						printf_info("re-hello retry...\n");
						http_post_hello();
						if (!g_mqtt_config.hello_getnew_flag && tlv_thread_exit) {
							sleep(3);
						}
					}
					if (!tlv_thread_exit) break;   /* 退出信号, 跳出内层 while */
					/* 用新凭证重连 MQTT */
					mosquitto_disconnect(mosq);
					mosquitto_username_pw_set(mosq, g_mqtt_config.username, g_mqtt_config.password);
					rc = mosquitto_connect(mosq, g_mqtt_config.host, g_mqtt_config.port, g_mqtt_config.keepalive_seconds);
					if (rc == MOSQ_ERR_SUCCESS) {
						mosquitto_loop_start(mosq);
					}
					printf_info("re-hello & reconnect done\n");
				}
			}

			/* 处理 queue_from_mqtt 队列中的消息 */
			{
				char *recvdata = (char *)tiny_queue_pop_waittime(queue_from_mqtt, 0);
				if (recvdata) {
					Inner_cmd_msg *header = (Inner_cmd_msg *)recvdata;
					char *payload = recvdata + sizeof(Inner_cmd_msg);
					char *topic = recvdata + sizeof(Inner_cmd_msg) + header->datalen;
					printf_info("queue_from_mqtt: cmdid=0x%02X topic=%s datalen=%d payload=%s\n",
						header->cmdid, topic, header->datalen, payload);

					/* 解析 JSON, 判断 msg_type */
					cJSON *root = cJSON_Parse(payload);
					if (root) {
						cJSON *msg_type = cJSON_GetObjectItem(root, "msg_type");
						if (msg_type && cJSON_IsString(msg_type)
							&& strcmp(msg_type->valuestring, "server.command") == 0) {
							/* server.command → 解析并回复 command_ack */
							ServerCommand cmd;
							if (server_command_parse_from_json(payload, &cmd) == 0) {
								printf_info("server.command received: cmd=%s session=%s\n",
									cmd.cmd, cmd.session_id);

								/* 按命令名分发处理 */
								if (strcmp(cmd.cmd, "video.start_session") == 0) {
									handle_video_start_session(mosq, &cmd);
								} else if (strcmp(cmd.cmd, "video.stop_session") == 0) {
									handle_video_stop_session(mosq, &cmd);
								} else if (strcmp(cmd.cmd, "exhaust.set") == 0) {
									handle_exhaust_set(mosq, &cmd);
								} else if (strcmp(cmd.cmd, "plasma.set") == 0) {
									handle_plasma_set(mosq, &cmd);
								} else if (strcmp(cmd.cmd, "anion.set") == 0) {
									handle_anion_set(mosq, &cmd);
								} else if (strcmp(cmd.cmd, "climate.set") == 0) {
									handle_climate_set(mosq, &cmd);
								} else if (strcmp(cmd.cmd, "humidifier.set") == 0) {
									handle_humidifier_set(mosq, &cmd);
								} else if (strcmp(cmd.cmd, "uvb.set") == 0) {
									handle_uvb_set(mosq, &cmd);
								} else if (strcmp(cmd.cmd, "light.set") == 0) {
									handle_light_set(mosq, &cmd);
								} else if (strcmp(cmd.cmd, "inlet_fan.set") == 0) {
									handle_inlet_fan_set(mosq, &cmd);
								} else if (strcmp(cmd.cmd, "camera_privacy.set") == 0) {
									handle_camera_privacy_set(mosq, &cmd);
								} else if (strcmp(cmd.cmd, "device_settings.set") == 0) {
									handle_device_settings_set(mosq, &cmd);
								} else if (strcmp(cmd.cmd, "device_password.set") == 0) {
									handle_device_password_set(mosq, &cmd);
								} else if (strcmp(cmd.cmd, "ota.upgrade") == 0) {
									handle_ota_upgrade(mosq, &cmd);
								} else {
									command_ack_publish(mosq, &cmd,
										"UNSUPPORTED", NULL, NULL, NULL,
										CMD_ACK_ERROR_UNSUPPORTED);
								}
							}
						}
						cJSON_Delete(root);
					} else {
						/* 非 JSON 消息, 按原样转发 */
						rc = mosquitto_publish(mosq, NULL, topic, header->datalen, payload, 1, false);
						if (rc != MOSQ_ERR_SUCCESS) {
							printf_info("Error publishing from queue: %s\n", mosquitto_strerror(rc));
						}
					}
					dw_free(&recvdata);
				}
			}

			/* TLV 数据处理 */
			{
				process_tlv_worker_task();
			}

			usleep(30);
		}

		/* 退出内层循环后清理 MQTT 连接 */
		mosquitto_disconnect(mosq);
		mosquitto_destroy(mosq);
		mosquitto_lib_cleanup();
	}

	return 0;
}






//////////////////////////////////////////////////////////////
/**
 * ============================================================================
 * 设备 Hello 请求 — C 语言参考实现
 * ============================================================================
 *
 * 用途: 给嵌入式设备端参考,展示如何构建 /api/device/v1/hello 请求的完整流程:
 *   JSON 构造 → Canonical JSON → body_hash(SHA-256) → nonce 生成
 *   → sign_string 拼接 → HMAC-SHA256 签名 → 发 HTTP → 解析响应
 *
 * 编译:
 *   gcc -o hello_example c_hello_example.c -lssl -lcrypto -lcurl -Wall -Wextra
 *
 * 运行:
 *   ./hello_example
 *
 * 依赖: libssl / libcrypto (OpenSSL) + libcurl
 *   嵌入式环境可参照核心逻辑自行替换网络层。
 *
 * 设备信息(示例):
 *   device_sn:    MQXQ-BL-2026-00002
 *   ble_id:       B2K8E4
 *   product_key:  MCQX_PET_CABIN
 *   device_secret: d6f3e8a1c9b25e7a4f0d3c8b6a2e9f1d
 *
 * 相关服务端代码:
 *   - CanonicalJson      app/Util/CanonicalJson.php
 *   - DeviceSignatureService  app/Service/DeviceSignatureService.php
 *   - DeviceHelloRequest  app/Request/Device/DeviceHelloRequest.php
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <curl/curl.h>

/* ============================================================================
 * 配置 — 根据实际设备信息修改这些值
 * ============================================================================ */

 

/* 设备认证密钥(出厂写入,服务端同时保存加密版本)
   测试/dev 回退值; 量产后由 generate_device_secret 生成并写入 DEVICE_SECRET_SAVE_PATH */
#define DEVICE_SECRET   "-Ro6nkz8varO_xKx8XrGtMAZAHWO0UtLL4Z2aqNalZ4"

/* 工厂 API 密钥(Bearer FACTORY_API_KEY, 由服务端管理员分发; 烧录/产测时填写) */
#define FACTORY_API_KEY "5175ae0f522bc323f539490d308eb000a1dc382d25d4c0c12a1883e620b42d9c"

/* 绑定会话 ID(首次配网时由 App 发起绑定后获得,从 App 端传入;已绑定设备裸 hello 不传) */
#define BIND_SESSION_ID ""

/* 默认 API 基础地址(固件内置区域域名, 首次 bootstrap 前使用) */
#define DEFAULT_API_BASE "https://test-api-cn.mcqx.pet"

/* Hello 接口 URL(按实际环境修改) */
#define HELLO_URL       "https://test-api-cn.mcqx.pet/api/device/v1/hello"

/* Bootstrap 接口 URL(设备自举取回身份, 按实际环境修改) */
#define BOOTSTRAP_URL   "https://test-api-cn.mcqx.pet/api/device/v1/bootstrap"

/*
 * 获取当前 API 基础地址(用于拼接各接口 URL)
 * 优先级: g_bootstrap_config.first_api_endpoint > api_endpoint
 *         > g_mqtt_config.api_endpoint > DEFAULT_API_BASE
 * bootstrap 成功后 first_api_endpoint 被持久化, 后续所有 API 均走此地址
 */
static const char *get_api_base(void)
{
	if (g_bootstrap_config.first_api_endpoint[0] != '\0')
		return g_bootstrap_config.first_api_endpoint;
	if (g_bootstrap_config.api_endpoint[0] != '\0')
		return g_bootstrap_config.api_endpoint;
	if (g_mqtt_config.api_endpoint[0] != '\0')
		return g_mqtt_config.api_endpoint;
	return DEFAULT_API_BASE;
}

/* ============================================================================
 * 辅助: 动态字符串(避免在嵌入式里手写大量 realloc,仅供参考)
 * 嵌入式环境通常有固定 buffer,可替换为 sprintf 到静态数组。
 * ============================================================================ */

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} String;

static void string_init(String *s, size_t initial)
{
    s->buf = (char *)malloc(initial);
    s->len = 0;
    s->cap = initial;
    if (s->buf) s->buf[0] = '\0';
}

static void string_append(String *s, const char *data, size_t len)
{
    if (len == 0) return;
    if (s->len + len + 1 > s->cap) {
        while (s->len + len + 1 > s->cap) s->cap *= 2;
        s->buf = (char *)realloc(s->buf, s->cap);
    }
    memcpy(s->buf + s->len, data, len);
    s->len += len;
    s->buf[s->len] = '\0';
}

static void string_append_str(String *s, const char *str)
{
    string_append(s, str, strlen(str));
}

/* ============================================================================
 * 字节 → 小写十六进制
 * ============================================================================ */

static void bytes_to_hex(const unsigned char *bytes, size_t len, char *out)
{
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        out[i * 2]     = hex[bytes[i] >> 4];
        out[i * 2 + 1] = hex[bytes[i] & 0x0f];
    }
    out[len * 2] = '\0';
}

/*
 * 注: 本示例直接通过 sprintf 手动构建 Canonical JSON(因为 key 顺序固
 * 定,无需运行时排序)。实际项目中若动态构造 JSON,需确保 Object key 按
 * ASCII 升序排列,具体规则:
 *   - Object key 按 ASCII 升序排序,递归处理嵌套 object
 *   - Array 保持原始顺序
 *   - 紧凑输出,无空格/换行/缩进
 *   - Number 十进制,Boolean true/false,null 参与签名
 *   - auth 字段不参与 body_hash 计算
 * 嵌入式项目可用 cJSON 构造后排序,或使用固定模板。
 */

/* ============================================================================
 * SHA-256 工具
 * ============================================================================ */

static void sha256_hex(const char *data, size_t len, char *out_hex)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char *)data, len, hash);
    bytes_to_hex(hash, SHA256_DIGEST_LENGTH, out_hex);
}

/* ============================================================================
 * HMAC-SHA256 工具
 * ============================================================================ */

static void hmac_sha256_hex(
    const char *key, size_t key_len,
    const char *data, size_t data_len,
    char *out_hex)
{
    unsigned char result[32];
    unsigned int result_len = 32;

    HMAC(EVP_sha256(), key, (int)key_len,
         (const unsigned char *)data, data_len,
         result, &result_len);
    bytes_to_hex(result, result_len, out_hex);
}

/* ============================================================================
 * Nonce 生成(安全随机,16 字节 → 32 hex 字符)
 * ============================================================================ */

static void generate_nonce(char *out_hex)
{
    unsigned char bytes[16];
    if (RAND_bytes(bytes, (int)sizeof(bytes)) != 1) {
        /* 回退: 用时间 + 计数器(仅供演示,生产用硬件随机数或 TRNG) */
        for (int i = 0; i < (int)sizeof(bytes); i++) {
            bytes[i] = (unsigned char)(rand() ^ (time(NULL) >> (i * 8)));
        }
    }
    bytes_to_hex(bytes, sizeof(bytes), out_hex);
}

/* ============================================================================
 * 核心: 构建 hello 请求 JSON + 计算签名
 *
 * 步骤:
 *   1. 构造完整 JSON body(含 auth 占位)
 *   2. 去掉 auth 字段 → Canonical JSON → SHA-256 → body_hash
 *   3. 生成 nonce
 *   4. 计算 sign_string = device_sn + "\n" + timestamp + "\n" + nonce + "\n" + body_hash
 *   5. HMAC-SHA256(sign_string, device_secret) → signature
 *   6. 组装最终 JSON(含 auth)
 *
 * 注意: timestamp 使用 Unix 秒,服务器校验 ±300 秒窗口。
 * ============================================================================ */

/**
 * 构造 Hello 请求(不含 auth),返回 Canonical JSON 字符串和当前 timestamp。
 * 调用者负责 free() 返回的 canonical_json。
 */
static char *build_hello_body_canonical(int64_t *out_timestamp)
{
    int64_t ts = (int64_t)time(NULL);
    *out_timestamp = ts;

    /*
     * 手动构建 Canonical JSON。
     *
     * 注意 key 的 ASCII 顺序:
     *   ble_id < data < device_sn < msg_id < msg_type < product_key < protocol_version < seq < timestamp
     *
     * 最外层 object 的 key 必须按 ASCII 升序排列。
     * 嵌套 object 内部也按同样规则。
     * 本示例直接手写排序后的 JSON。
     *
     * 实际嵌入式项目建议用 cJSON + 排序函数,或编译期模板。
     */
    /* bind_session_id 片段: BIND_SESSION_ID 为空时输出 JSON null(裸 hello, 不走配网);
       非空时输出字符串。服务端 isset() 对 null / 缺省均判为 null, 二者等价。
       ★ 不能输出空串 "": (string)"" 仍是空串而非 null, 语义不同。 */
    char bind_session_fragment[128];
    if (BIND_SESSION_ID[0] == '\0') {
        snprintf(bind_session_fragment, sizeof(bind_session_fragment),
            "\"bind_session_id\":null,");
    } else {
        snprintf(bind_session_fragment, sizeof(bind_session_fragment),
            "\"bind_session_id\":\"%s\",", BIND_SESSION_ID);
    }

    const char *fmt =
        "{"
          "\"ble_id\":\"%s\","
          "\"data\":{"
            "%s"
            "\"device_identity\":{"
              "\"ble_id\":\"%s\","
              "\"ble_name\":\"%s\","
              "\"device_sn\":\"%s\","
              "\"firmware_version\":\"%s\","
              "\"hardware_version\":\"%s\","
              "\"model\":\"%s\","
              "\"product_key\":\"%s\""
            "}"
          "},"
          "\"device_sn\":\"%s\","
          "\"msg_id\":\"%s\","
          "\"msg_type\":\"device.hello\","
          "\"product_key\":\"%s\","
          "\"protocol_version\":\"%s\","
          "\"seq\":1,"
          "\"timestamp\":%lld"
        "}";

    /* msg_id 建议用设备 SN + 时间戳保证唯一 */
    char msg_id[128];
    snprintf(msg_id, sizeof(msg_id), "HELLO-%s-%lld", DEVICE_SN, (long long)ts);

    int len = snprintf(NULL, 0, fmt,
        BLE_ID,
        bind_session_fragment,
        BLE_ID, BLE_NAME, DEVICE_SN, FW_VERSION, HW_VERSION, MODEL, PRODUCT_KEY,
        DEVICE_SN,
        msg_id,
        PRODUCT_KEY,
        PROTOCOL_VER,
        (long long)ts
    );

    if (len < 0) return NULL;

    char *json = (char *)malloc((size_t)len + 1);
    if (!json) return NULL;
    snprintf(json, (size_t)len + 1, fmt,
        BLE_ID,
        bind_session_fragment,
        BLE_ID, BLE_NAME, DEVICE_SN, FW_VERSION, HW_VERSION, MODEL, PRODUCT_KEY,
        DEVICE_SN,
        msg_id,
        PRODUCT_KEY,
        PROTOCOL_VER,
        (long long)ts
    );

    return json;
}

/**
 * 构造 Bootstrap 请求(不含 auth), 返回 Canonical JSON 字符串和当前 timestamp。
 * 调用者负责 free() 返回的 canonical_json。
 *
 * 与 hello 的差异:
 *   - 身份字段用 mac(设备此时不知道 device_sn), 没有 ble_id
 *   - msg_type 为 "device.bootstrap"
 *   - sign_string 第一行用 mac(而非 device_sn)
 *   - data 可选携带 sales_country_code / first_api_endpoint
 */
static char *build_bootstrap_body_canonical(const char *mac, int64_t *out_timestamp)
{
    int64_t ts = (int64_t)time(NULL);
    *out_timestamp = ts;

    /*
     * Canonical JSON key 升序规则:
     *   data < mac < msg_id < msg_type < product_key < protocol_version < seq < timestamp
     * 内部 data 节点: first_api_endpoint < sales_country_code (f < s)
     */

    const char *fmt =
        "{"
          "\"data\":{"
            "\"sales_country_code\":\"CN\""
          "},"
          "\"mac\":\"%s\","
          "\"msg_id\":\"%s\","
          "\"msg_type\":\"device.bootstrap\","
          "\"product_key\":\"%s\","
          "\"protocol_version\":\"%s\","
          "\"seq\":1,"
          "\"timestamp\":%lld"
        "}";

    /* msg_id 用 MAC + 时间戳保证唯一 */
    char msg_id[128];
    snprintf(msg_id, sizeof(msg_id), "BOOT-%s-%lld", mac, (long long)ts);

    int len = snprintf(NULL, 0, fmt,
        mac,
        msg_id,
        PRODUCT_KEY,
        PROTOCOL_VER,
        (long long)ts
    );

    if (len < 0) return NULL;

    char *json = (char *)malloc((size_t)len + 1);
    if (!json) return NULL;
    snprintf(json, (size_t)len + 1, fmt,
        mac,
        msg_id,
        PRODUCT_KEY,
        PROTOCOL_VER,
        (long long)ts
    );

    return json;
}

/**
 * 构建带 auth 的完整请求 JSON。
 *
 * body_canonical 是不含 auth 的 Canonical JSON,形如:
 *   {"ble_id":"...","data":{...},"device_sn":"...",...}
 *
 * 最终 JSON 在 body_canonical 基础上插入 auth 字段作为第一个 key。
 * 因为 "auth" < "ble_id"(ASCII a < b),所以排序仍是正确的。
 */
static char *build_signed_request(
    const char *body_canonical,
    const char *nonce,
    const char *signature)
{
    /*
     * body_canonical = {"key1":...,"key2":...,...}
     * 去掉首 '{' 和尾 '}',得到:
     *   "key1":...,"key2":...,...
     * 然后拼装:
     *   {"auth":{...},              ← auth 插入
     *    "key1":...,"key2":...,...} ← body 原本内容
     */
    size_t body_len = strlen(body_canonical);
    /* 跳过首字符 '{',尾部 -1 去掉 '}' */
    const char *inner = body_canonical + 1;
    size_t inner_len = body_len - 2;

    /* 预估总长度 */
    size_t auth_prefix_len = strlen(nonce) + strlen(signature) + 80;
    size_t total_len = auth_prefix_len + inner_len + 2; /* +2 for {} */

    String result;
    string_init(&result, total_len);

    string_append_str(&result,
        "{\"auth\":{"
        "\"nonce\":\"");
    string_append_str(&result, nonce);
    string_append_str(&result,
        "\","
        "\"sign_method\":\"HMAC-SHA256\","
        "\"signature\":\"");
    string_append_str(&result, signature);
    string_append_str(&result, "\"},");
    /* 插入 body 内部内容 */
    string_append(&result, inner, inner_len);
    string_append_str(&result, "}");

    return result.buf;
}

/* ============================================================================
 * HTTP POST 发送(sync 阻塞版本,使用 libcurl)
 * ============================================================================ */

struct MemoryBuffer {
    char *data;
    size_t size;
};

static size_t write_callback(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    struct MemoryBuffer *buf = (struct MemoryBuffer *)userdata;
    size_t total = size * nmemb;
    buf->data = (char *)realloc(buf->data, buf->size + total + 1);
    memcpy(buf->data + buf->size, ptr, total);
    buf->size += total;
    buf->data[buf->size] = '\0';
    return total;
}

static int http_post_json(const char *url, const char *json_body,
                          char **response_out, size_t *response_len_out)
{
    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "[ERROR] curl_easy_init() failed\n");
        return -1;
    }

    struct MemoryBuffer resp = { NULL, 0 };
    struct curl_slist *headers = NULL;

    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(json_body));
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);

    /* 超时(毫秒) */
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 10000L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 5000L);

#ifdef SKIP_PEER_VERIFY
    /* 测试环境跳过证书校验(生产环境不要设置!) */
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
#endif

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "[ERROR] curl failed: %s\n", curl_easy_strerror(res));
        free(resp.data);
        return -1;
    }

    *response_out = resp.data;
    *response_len_out = resp.size;

    printf("[HTTP] %ld\n", http_code);
    return (int)http_code;
}

/* HTTP POST JSON, Authorization: Bearer {token} (工厂 API 专用) */
static int http_post_json_bearer(const char *url, const char *json_body,
                                 const char *bearer_token,
                                 char **response_out, size_t *response_len_out)
{
    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "[ERROR] curl_easy_init() failed\n");
        return -1;
    }

    struct MemoryBuffer resp = { NULL, 0 };
    struct curl_slist *headers = NULL;
    char auth_header[512];

    headers = curl_slist_append(headers, "Content-Type: application/json");
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", bearer_token);
    headers = curl_slist_append(headers, auth_header);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(json_body));
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 10000L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 5000L);

#ifdef SKIP_PEER_VERIFY
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
#endif

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "[ERROR] curl failed: %s\n", curl_easy_strerror(res));
        free(resp.data);
        return -1;
    }

    *response_out = resp.data;
    *response_len_out = resp.size;
    printf("[HTTP] %ld\n", http_code);
    return (int)http_code;
}

/*
 * 从 hello 响应 JSON 中解析 data.mqtt 节点, 填充 MqttConfig 结构体。
 * 返回 0 成功, -1 失败。
 */
int mqtt_config_parse_from_json(const char *json_str, MqttConfig *cfg)
{
	if (!json_str || !cfg) return -1;
	memset(cfg, 0, sizeof(MqttConfig));

	cJSON *root = cJSON_Parse(json_str);
	if (!root) {
		printf("[MQTT_CONFIG] JSON 解析失败: %s\n", cJSON_GetErrorPtr());
		return -1;
	}

	cJSON *data = cJSON_GetObjectItem(root, "data");
	cJSON *mqtt = data ? cJSON_GetObjectItem(data, "mqtt") : NULL;
	if (!mqtt) {
		printf("[MQTT_CONFIG] 未找到 data.mqtt\n");
		cJSON_Delete(root);
		return -1;
	}

	cJSON *item;
	item = cJSON_GetObjectItem(mqtt, "endpoint");
	if (cJSON_IsString(item)) {
		/* 解析 endpoint: mqtts://host:port → 拆分 host / port */
		const char *ep = item->valuestring;
		/* 跳过协议前缀(如 mqtts:// 或 mqtt://) */
		const char *host_start = strstr(ep, "://");
		host_start = host_start ? host_start + 3 : ep;
		/* 查找端口分隔符 ':' */
		const char *port_start = strrchr(host_start, ':');
		if (port_start) {
			size_t host_len = (size_t)(port_start - host_start);
			if (host_len >= sizeof(cfg->host)) host_len = sizeof(cfg->host) - 1;
			memcpy(cfg->host, host_start, host_len);
			cfg->host[host_len] = '\0';
			cfg->port = atoi(port_start + 1);
		} else {
			strncpy(cfg->host, host_start, sizeof(cfg->host) - 1);
			cfg->port = 8883; /* 默认 MQTTS 端口 */
		}
	}

	item = cJSON_GetObjectItem(mqtt, "client_id");
	if (cJSON_IsString(item)) strncpy(cfg->client_id, item->valuestring, sizeof(cfg->client_id) - 1);

	item = cJSON_GetObjectItem(mqtt, "username");
	if (cJSON_IsString(item)) strncpy(cfg->username, item->valuestring, sizeof(cfg->username) - 1);

	item = cJSON_GetObjectItem(mqtt, "password");
	if (cJSON_IsString(item)) strncpy(cfg->password, item->valuestring, sizeof(cfg->password) - 1);

	item = cJSON_GetObjectItem(mqtt, "keepalive_seconds");
	if (cJSON_IsNumber(item)) cfg->keepalive_seconds = item->valueint;

	item = cJSON_GetObjectItem(mqtt, "credential_expire_at");
	if (cJSON_IsNumber(item)) cfg->credential_expire_at = (int64_t)item->valuedouble;

	/* 从 data 中提取 bind_status 和 device_status */
	item = cJSON_GetObjectItem(data, "bind_status");
	if (cJSON_IsString(item))
		strncpy(cfg->bind_status, item->valuestring, sizeof(cfg->bind_status) - 1);

	item = cJSON_GetObjectItem(data, "device_status");
	if (cJSON_IsString(item))
		strncpy(cfg->device_status, item->valuestring, sizeof(cfg->device_status) - 1);

	/* 从 data.endpoint_config 中提取 api_endpoint 和完整 endpoint_config */
	{
		cJSON *ep_cfg = cJSON_GetObjectItem(data, "endpoint_config");
		if (ep_cfg) {
			item = cJSON_GetObjectItem(ep_cfg, "api_endpoint");
			if (cJSON_IsString(item))
				strncpy(cfg->api_endpoint, item->valuestring, sizeof(cfg->api_endpoint) - 1);

			item = cJSON_GetObjectItem(ep_cfg, "sales_country_code");
			if (cJSON_IsString(item))
				strncpy(cfg->endpoint_config.sales_country_code, item->valuestring, sizeof(cfg->endpoint_config.sales_country_code) - 1);

			item = cJSON_GetObjectItem(ep_cfg, "cloud_region");
			if (cJSON_IsString(item))
				strncpy(cfg->endpoint_config.cloud_region, item->valuestring, sizeof(cfg->endpoint_config.cloud_region) - 1);

			item = cJSON_GetObjectItem(ep_cfg, "mqtt_endpoint");
			if (cJSON_IsString(item))
				strncpy(cfg->endpoint_config.mqtt_endpoint, item->valuestring, sizeof(cfg->endpoint_config.mqtt_endpoint) - 1);

			item = cJSON_GetObjectItem(ep_cfg, "first_api_endpoint");
			if (cJSON_IsString(item))
				strncpy(cfg->endpoint_config.first_api_endpoint, item->valuestring, sizeof(cfg->endpoint_config.first_api_endpoint) - 1);

			item = cJSON_GetObjectItem(ep_cfg, "endpoint_version");
			if (cJSON_IsNumber(item)) cfg->endpoint_config.endpoint_version = item->valueint;

			item = cJSON_GetObjectItem(ep_cfg, "endpoint_ttl_seconds");
			if (cJSON_IsNumber(item)) cfg->endpoint_config.endpoint_ttl_seconds = item->valueint;
		}
	}

	cJSON_Delete(root);

	printf_info("MQTT config parsed: host=%s port=%d client_id=%s api=%s keepalive=%d expire_at=%lld bind=%s dev_status=%s\n",
		cfg->host, cfg->port, cfg->client_id, cfg->api_endpoint,
		cfg->keepalive_seconds, (long long)cfg->credential_expire_at,
		cfg->bind_status, cfg->device_status);
	return 0;
}

/*
 * 从 JSON 字符串解析 bootstrap 响应, 填充 BootstrapConfig 结构体。
 * 响应格式: { "code":0, "data": { device_sn, ble_id, api_endpoint, bind_status, ... } }
 * 返回 0 成功, -1 失败。
 */
int bootstrap_config_parse_from_json(const char *json_str, BootstrapConfig *cfg)
{
	if (!json_str || !cfg) return -1;
	memset(cfg, 0, sizeof(BootstrapConfig));

	cJSON *root = cJSON_Parse(json_str);
	if (!root) {
		printf("[BOOTSTRAP] JSON 解析失败\n");
		return -1;
	}

	cJSON *data = cJSON_GetObjectItem(root, "data");
	if (!data) {
		printf("[BOOTSTRAP] 未找到 data 节点\n");
		cJSON_Delete(root);
		return -1;
	}

	cJSON *item;

	/* device_sn — 服务端领取生成的系统身份 */
	item = cJSON_GetObjectItem(data, "device_sn");
	if (cJSON_IsString(item)) {
		strncpy(cfg->device_sn, item->valuestring, sizeof(cfg->device_sn) - 1);
	}

	/* ble_id — 6位短码, 屏幕二维码用 */
	item = cJSON_GetObjectItem(data, "ble_id");
	if (cJSON_IsString(item)) {
		strncpy(cfg->ble_id, item->valuestring, sizeof(cfg->ble_id) - 1);
	}

	/* ble_name — 蓝牙名称 */
	item = cJSON_GetObjectItem(data, "ble_name");
	if (cJSON_IsString(item)) {
		strncpy(cfg->ble_name, item->valuestring, sizeof(cfg->ble_name) - 1);
	}

	/* qr_url — 二维码 URL */
	item = cJSON_GetObjectItem(data, "qr_url");
	if (cJSON_IsString(item)) {
		strncpy(cfg->qr_url, item->valuestring, sizeof(cfg->qr_url) - 1);
	}

	/* mac — 确认服务端返回的 MAC */
	item = cJSON_GetObjectItem(data, "mac");
	if (cJSON_IsString(item)) {
		strncpy(cfg->mac, item->valuestring, sizeof(cfg->mac) - 1);
	}

	/* product_key */
	item = cJSON_GetObjectItem(data, "product_key");
	if (cJSON_IsString(item)) {
		strncpy(cfg->product_key, item->valuestring, sizeof(cfg->product_key) - 1);
	}

	/* sales_country_code */
	item = cJSON_GetObjectItem(data, "sales_country_code");
	if (cJSON_IsString(item)) {
		strncpy(cfg->sales_country_code, item->valuestring, sizeof(cfg->sales_country_code) - 1);
	}

	/* cloud_region */
	item = cJSON_GetObjectItem(data, "cloud_region");
	if (cJSON_IsString(item)) {
		strncpy(cfg->cloud_region, item->valuestring, sizeof(cfg->cloud_region) - 1);
	}

	/* api_endpoint — 权威 API 地址, 后续请求走这里 */
	item = cJSON_GetObjectItem(data, "api_endpoint");
	if (cJSON_IsString(item)) {
		strncpy(cfg->api_endpoint, item->valuestring, sizeof(cfg->api_endpoint) - 1);
	}

	/* mqtt_endpoint — 仅展示, 未绑定不可接入 */
	item = cJSON_GetObjectItem(data, "mqtt_endpoint");
	if (cJSON_IsString(item)) {
		strncpy(cfg->mqtt_endpoint, item->valuestring, sizeof(cfg->mqtt_endpoint) - 1);
	}

	/* first_api_endpoint — 兜底首次接触地址 */
	item = cJSON_GetObjectItem(data, "first_api_endpoint");
	if (cJSON_IsString(item)) {
		strncpy(cfg->first_api_endpoint, item->valuestring, sizeof(cfg->first_api_endpoint) - 1);
	}

	/* endpoint_version */
	item = cJSON_GetObjectItem(data, "endpoint_version");
	if (cJSON_IsNumber(item)) {
		cfg->endpoint_version = item->valueint;
	}

	/* bind_status — UNBOUND / BINDING / BOUND */
	item = cJSON_GetObjectItem(data, "bind_status");
	if (cJSON_IsString(item)) {
		strncpy(cfg->bind_status, item->valuestring, sizeof(cfg->bind_status) - 1);
	}

	/* bind_session_id — 仅 BINDING 时返回 */
	item = cJSON_GetObjectItem(data, "bind_session_id");
	if (cJSON_IsString(item)) {
		strncpy(cfg->bind_session_id, item->valuestring, sizeof(cfg->bind_session_id) - 1);
	}

	cJSON_Delete(root);

	printf("[BOOTSTRAP] 解析成功: device_sn=%s ble_id=%s api_endpoint=%s bind_status=%s\n",
		cfg->device_sn, cfg->ble_id, cfg->api_endpoint, cfg->bind_status);

	return 0;
}

/*
 * 从 MqttConfig 构建 state.device 节点的 JSON 字符串:
 *   cfg->device_status   → "bind_status"
 *   cfg->mqtt_connected  → "online_status" (1→"ONLINE", 0→"OFFLINE")
 * 其他字段使用默认值: work_status="IDLE", fault_summary="NORMAL", faults=[]
 * 调用者 free 返回值。
 */
char *mqtt_config_build_device_json(MqttConfig *cfg)
{
	if (!cfg) return NULL;

	cJSON *device = cJSON_CreateObject();

	/* bind_status 来自 cfg->device_status */
	cJSON_AddStringToObject(device, "bind_status",
		cfg->device_status[0] ? cfg->device_status : "BOUND");

	/* work_status 固定为 IDLE */
	cJSON_AddStringToObject(device, "work_status", "IDLE");

	/* online_status 来自 cfg->mqtt_connected */
	cJSON_AddStringToObject(device, "online_status",
		cfg->mqtt_connected ? "ONLINE" : "OFFLINE");

	/* fault_summary 固定为 NORMAL */
	cJSON_AddStringToObject(device, "fault_summary", "NORMAL");

	/* faults 空数组 */
	cJSON_AddArrayToObject(device, "faults");

	char *json_str = cJSON_PrintUnformatted(device);
	cJSON_Delete(device);
	return json_str;
}

/*
 * 从 MqttConfig 和本机网络信息构建 state.network 节点的 JSON 字符串:
 *   network_type  - 通过 getifaddrs 获取接口名(如 wlan0/eth0)
 *   rssi          - 读取 /proc/net/wireless 获取信号强度, 失败则填 0
 *   ip            - 通过 getifaddrs 获取本机 IPv4 地址
 *   mqtt_connected - 来自 cfg->mqtt_connected
 * 调用者 free 返回值。
 */
char *mqtt_config_build_network_json(MqttConfig *cfg)
{
	if (!cfg) return NULL;

	cJSON *network = cJSON_CreateObject();

	/* 获取本机 IP 和接口名 */
	char ip_str[64] = "0.0.0.0";
	char ifname[16] = "wlan0";
	struct ifaddrs *ifaddr, *ifa;
	if (getifaddrs(&ifaddr) == 0) {
		for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
			if (ifa->ifa_addr == NULL) continue;
			if (ifa->ifa_addr->sa_family == AF_INET) {
				struct sockaddr_in *sin = (struct sockaddr_in *)ifa->ifa_addr;
				/* 跳过回环地址 127.0.0.1 */
				if (sin->sin_addr.s_addr != htonl(INADDR_LOOPBACK)) {
					inet_ntop(AF_INET, &sin->sin_addr, ip_str, sizeof(ip_str));
					strncpy(ifname, ifa->ifa_name, sizeof(ifname) - 1);
					break;
				}
			}
		}
		freeifaddrs(ifaddr);
	}

	/* network_type: 用接口名, 如 wlan0 / eth0 */
	cJSON_AddStringToObject(network, "network_type", ifname);

	/* rssi: 尝试从 /proc/net/wireless 读取 (wireless-ext) */
	int rssi = 0;
	FILE *wl_fp = fopen("/proc/net/wireless", "r");
	if (wl_fp) {
		char line[256];
		if (fgets(line, sizeof(line), wl_fp) && fgets(line, sizeof(line), wl_fp)) {
			while (fgets(line, sizeof(line), wl_fp)) {
				char *p = strchr(line, ':');
				if (p) {
					int v1, v2, v3;
					if (sscanf(p + 1, " %d %d %d", &v1, &v2, &v3) >= 3) {
						rssi = v3;
					}
				}
			}
		}
		fclose(wl_fp);
	}

	/* 若 /proc/net/wireless 无效, 尝试 iw dev 命令 (nl80211) */
	if (rssi == 0) {
		char cmd[128];
		snprintf(cmd, sizeof(cmd), "iw dev %s link 2>/dev/null", ifname);
		FILE *iw_fp = popen(cmd, "r");
		if (iw_fp) {
			char line[256];
			while (fgets(line, sizeof(line), iw_fp)) {
				if (strstr(line, "signal:")) {
					int val = 0;
					if (sscanf(line, " signal: %d", &val) == 1) {
						rssi = val;
					}
				}
			}
			pclose(iw_fp);
		}
	}
	cJSON_AddNumberToObject(network, "rssi", rssi);

	/* ip */
	cJSON_AddStringToObject(network, "ip", ip_str);

	/* mqtt_connected */
	cJSON_AddBoolToObject(network, "mqtt_connected", cfg->mqtt_connected ? 1 : 0);

	char *json_str = cJSON_PrintUnformatted(network);
	cJSON_Delete(network);
	return json_str;
}

/* ============================================================================
 * 状态字段字典 — 枚举↔字符串转换 + DeviceFullState 序列化/反序列化
 * 依据: ShowDoc §54 状态字段字典 (54.1 ~ 54.8)
 * ============================================================================ */

#define MATCH_STR(s, literal) (s && strcmp(s, literal) == 0)

/* ---- 54.1 设备基础状态 ---- */

/* bind_status: UNBOUND / BINDING / BOUND */
const char *bind_status_to_str(BindStatus v) {
    switch (v) {
        case BIND_STATUS_BOUND:   return "BOUND";
        case BIND_STATUS_BINDING: return "BINDING";
        case BIND_STATUS_UNBOUND: return "UNBOUND";
        default: return "UNBOUND";
    }
}
BindStatus bind_status_from_str(const char *s) {
    if (MATCH_STR(s, "BOUND"))   return BIND_STATUS_BOUND;
    if (MATCH_STR(s, "BINDING")) return BIND_STATUS_BINDING;
    return BIND_STATUS_UNBOUND;
}

/* work_status: IDLE / WORKING / SLEEP / ERROR */
const char *work_status_to_str(WorkStatus v) {
    switch (v) {
        case WORK_STATUS_IDLE:    return "IDLE";
        case WORK_STATUS_WORKING: return "WORKING";
        case WORK_STATUS_SLEEP:   return "SLEEP";
        case WORK_STATUS_ERROR:   return "ERROR";
        default: return "IDLE";
    }
}
WorkStatus work_status_from_str(const char *s) {
    if (MATCH_STR(s, "WORKING")) return WORK_STATUS_WORKING;
    if (MATCH_STR(s, "SLEEP"))   return WORK_STATUS_SLEEP;
    if (MATCH_STR(s, "ERROR"))   return WORK_STATUS_ERROR;
    return WORK_STATUS_IDLE;
}

/* online_status: ONLINE / OFFLINE */
const char *online_status_to_str(OnlineStatus v) {
    switch (v) {
        case ONLINE_STATUS_ONLINE:  return "ONLINE";
        case ONLINE_STATUS_OFFLINE: return "OFFLINE";
        default: return "OFFLINE";
    }
}
OnlineStatus online_status_from_str(const char *s) {
    if (MATCH_STR(s, "ONLINE"))  return ONLINE_STATUS_ONLINE;
    return ONLINE_STATUS_OFFLINE;
}

/* fault_level: NORMAL / WARN / ERROR / CRITICAL */
const char *fault_level_to_str(FaultLevel v) {
    switch (v) {
        case FAULT_LEVEL_NORMAL:   return "NORMAL";
        case FAULT_LEVEL_WARN:     return "WARN";
        case FAULT_LEVEL_ERROR:    return "ERROR";
        case FAULT_LEVEL_CRITICAL: return "CRITICAL";
        default: return "NORMAL";
    }
}
FaultLevel fault_level_from_str(const char *s) {
    if (MATCH_STR(s, "CRITICAL")) return FAULT_LEVEL_CRITICAL;
    if (MATCH_STR(s, "ERROR"))    return FAULT_LEVEL_ERROR;
    if (MATCH_STR(s, "WARN"))     return FAULT_LEVEL_WARN;
    return FAULT_LEVEL_NORMAL;
}

/* lifecycle_status: ACTIVE / INACTIVE / REPLACED / SCRAPPED */
const char *lifecycle_status_to_str(LifecycleStatus v) {
    switch (v) {
        case LIFECYCLE_ACTIVE:   return "ACTIVE";
        case LIFECYCLE_INACTIVE: return "INACTIVE";
        case LIFECYCLE_REPLACED: return "REPLACED";
        case LIFECYCLE_SCRAPPED: return "SCRAPPED";
        default: return "ACTIVE";
    }
}
LifecycleStatus lifecycle_status_from_str(const char *s) {
    if (MATCH_STR(s, "INACTIVE")) return LIFECYCLE_INACTIVE;
    if (MATCH_STR(s, "REPLACED")) return LIFECYCLE_REPLACED;
    if (MATCH_STR(s, "SCRAPPED")) return LIFECYCLE_SCRAPPED;
    return LIFECYCLE_ACTIVE;
}

/* ---- 54.2 网络状态 ---- */

/* network_type: wifi / 4g / ethernet */
const char *network_type_to_str(NetworkType v) {
    switch (v) {
        case NETWORK_TYPE_WIFI:     return "wifi";
        case NETWORK_TYPE_4G:       return "4g";
        case NETWORK_TYPE_ETHERNET: return "ethernet";
        default: return "unknown";
    }
}
NetworkType network_type_from_str(const char *s) {
    if (!s) return NETWORK_TYPE_UNKNOWN;
    if (strncmp(s, "wifi", 4) == 0 || strncmp(s, "wlan", 4) == 0) return NETWORK_TYPE_WIFI;
    if (strncmp(s, "4g", 2) == 0 || strncmp(s, "usb", 3) == 0)   return NETWORK_TYPE_4G;
    if (strncmp(s, "eth", 3) == 0 || strncmp(s, "ether", 5) == 0) return NETWORK_TYPE_ETHERNET;
    return NETWORK_TYPE_UNKNOWN;
}

/* ---- 54.3 电源状态 ---- */

const char *power_mode_to_str(PowerMode v) {
    switch (v) {
        case POWER_MODE_AC:      return "AC";
        case POWER_MODE_BATTERY: return "BATTERY";
        default: return "AC";
    }
}
PowerMode power_mode_from_str(const char *s) {
    if (MATCH_STR(s, "BATTERY")) return POWER_MODE_BATTERY;
    return POWER_MODE_AC;
}

/* ---- 54.4 门锁状态 ---- */

const char *door_status_to_str(DoorStatus v) {
    switch (v) {
        case DOOR_STATUS_OPEN:    return "OPEN";
        case DOOR_STATUS_CLOSED:  return "CLOSED";
        case DOOR_STATUS_UNKNOWN: return "UNKNOWN";
        default: return "UNKNOWN";
    }
}
DoorStatus door_status_from_str(const char *s) {
    if (MATCH_STR(s, "OPEN"))   return DOOR_STATUS_OPEN;
    if (MATCH_STR(s, "CLOSED")) return DOOR_STATUS_CLOSED;
    return DOOR_STATUS_UNKNOWN;
}

const char *lock_status_to_str(LockStatus v) {
    switch (v) {
        case LOCK_STATUS_LOCKED:   return "LOCKED";
        case LOCK_STATUS_UNLOCKED: return "UNLOCKED";
        case LOCK_STATUS_JAMMED:   return "JAMMED";
        case LOCK_STATUS_UNKNOWN:  return "UNKNOWN";
        default: return "UNKNOWN";
    }
}
LockStatus lock_status_from_str(const char *s) {
    if (MATCH_STR(s, "LOCKED"))   return LOCK_STATUS_LOCKED;
    if (MATCH_STR(s, "UNLOCKED")) return LOCK_STATUS_UNLOCKED;
    if (MATCH_STR(s, "JAMMED"))   return LOCK_STATUS_JAMMED;
    return LOCK_STATUS_UNKNOWN;
}

/* ---- 54.6 执行器状态 ---- */

const char *climate_control_status_to_str(ClimateControlStatus v) {
    switch (v) {
        case CLIMATE_CTRL_OFF:     return "OFF";
        case CLIMATE_CTRL_HEATING: return "HEATING";
        case CLIMATE_CTRL_HOLDING: return "HOLDING";
        case CLIMATE_CTRL_COOLING: return "COOLING";
        default: return "UNKNOWN";
    }
}
ClimateControlStatus climate_control_status_from_str(const char *s) {
    if (MATCH_STR(s, "OFF"))     return CLIMATE_CTRL_OFF;
    if (MATCH_STR(s, "HEATING")) return CLIMATE_CTRL_HEATING;
    if (MATCH_STR(s, "HOLDING")) return CLIMATE_CTRL_HOLDING;
    if (MATCH_STR(s, "COOLING")) return CLIMATE_CTRL_COOLING;
    return CLIMATE_CTRL_UNKNOWN;
}

const char *liquid_status_to_str(LiquidStatus v) {
    switch (v) {
        case LIQUID_NORMAL:      return "NORMAL";
        case LIQUID_LOW_WARNING: return "LOW_WARNING";
        case LIQUID_EMPTY:       return "EMPTY";
        case LIQUID_DRY_BURN:    return "DRY_BURN";
        default: return "NORMAL";
    }
}
LiquidStatus liquid_status_from_str(const char *s) {
    if (MATCH_STR(s, "LOW_WARNING")) return LIQUID_LOW_WARNING;
    if (MATCH_STR(s, "EMPTY"))       return LIQUID_EMPTY;
    if (MATCH_STR(s, "DRY_BURN"))    return LIQUID_DRY_BURN;
    return LIQUID_NORMAL;
}

/* ---- 54.7 摄像头状态 ---- */

const char *camera_status_to_str(CameraStatus v) {
    switch (v) {
        case CAMERA_STATUS_OFFLINE:  return "OFFLINE";
        case CAMERA_STATUS_ONLINE:   return "ONLINE";
        case CAMERA_STATUS_ERROR:    return "ERROR";
        case CAMERA_STATUS_DISABLED: return "DISABLED";
        default: return "OFFLINE";
    }
}
CameraStatus camera_status_from_str(const char *s) {
    if (MATCH_STR(s, "ONLINE"))   return CAMERA_STATUS_ONLINE;
    if (MATCH_STR(s, "ERROR"))    return CAMERA_STATUS_ERROR;
    if (MATCH_STR(s, "DISABLED")) return CAMERA_STATUS_DISABLED;
    return CAMERA_STATUS_OFFLINE;
}

const char *stream_status_to_str(StreamStatus v) {
    switch (v) {
        case STREAM_STATUS_IDLE:      return "IDLE";
        case STREAM_STATUS_STARTING:  return "STARTING";
        case STREAM_STATUS_STREAMING: return "STREAMING";
        case STREAM_STATUS_STOPPING:  return "STOPPING";
        case STREAM_STATUS_ERROR:     return "ERROR";
        default: return "IDLE";
    }
}
StreamStatus stream_status_from_str(const char *s) {
    if (MATCH_STR(s, "STARTING"))  return STREAM_STATUS_STARTING;
    if (MATCH_STR(s, "STREAMING")) return STREAM_STATUS_STREAMING;
    if (MATCH_STR(s, "STOPPING"))  return STREAM_STATUS_STOPPING;
    if (MATCH_STR(s, "ERROR"))     return STREAM_STATUS_ERROR;
    return STREAM_STATUS_IDLE;
}

const char *video_codec_to_str(VideoCodec v) {
    switch (v) {
        case VIDEO_CODEC_H264: return "H264";
        case VIDEO_CODEC_H265: return "H265";
        default: return "H264";
    }
}
VideoCodec video_codec_from_str(const char *s) {
    if (MATCH_STR(s, "H265")) return VIDEO_CODEC_H265;
    return VIDEO_CODEC_H264;
}

const char *resolution_to_str(Resolution v) {
    switch (v) {
        case RESOLUTION_480P:  return "480p";
        case RESOLUTION_720P:  return "720p";
        case RESOLUTION_1080P: return "1080p";
        default: return "720p";
    }
}
Resolution resolution_from_str(const char *s) {
    if (MATCH_STR(s, "480p"))  return RESOLUTION_480P;
    if (MATCH_STR(s, "1080p")) return RESOLUTION_1080P;
    return RESOLUTION_720P;
}

/* ---- 54.8 设备本地设置 ---- */

const char *screen_mode_to_str(ScreenMode v) {
    switch (v) {
        case SCREEN_MODE_SCREEN_SAVER: return "SCREEN_SAVER";
        case SCREEN_MODE_STANDBY:      return "STANDBY";
        default: return "SCREEN_SAVER";
    }
}
ScreenMode screen_mode_from_str(const char *s) {
    if (MATCH_STR(s, "STANDBY")) return SCREEN_MODE_STANDBY;
    return SCREEN_MODE_SCREEN_SAVER;
}

/* ============================================================================
 * 从 MqttConfig 填充 DeviceFullState
 * ============================================================================ */
void device_full_state_from_mqtt_config(const MqttConfig *cfg, DeviceFullState *state)
{
    if (!cfg || !state) return;
    memset(state, 0, sizeof(DeviceFullState));

    /* device 节点 */
    state->device.bind_status     = bind_status_from_str(cfg->device_status);
    state->device.work_status     = WORK_STATUS_IDLE;
    state->device.online_status   = cfg->mqtt_connected ? ONLINE_STATUS_ONLINE : ONLINE_STATUS_OFFLINE;
    state->device.fault_summary   = FAULT_LEVEL_NORMAL;
    state->device.faults          = NULL;
    state->device.faults_count    = 0;
    state->device.lifecycle_status = LIFECYCLE_ACTIVE;

    /* network 节点 */
    state->network.network_type   = NETWORK_TYPE_UNKNOWN;
    state->network.ifname[0]      = '\0';
    state->network.rssi           = 0;
    state->network.ip[0]          = '\0';
    state->network.mqtt_connected = cfg->mqtt_connected;

    /* power 节点 */
    state->power.power_mode       = POWER_MODE_AC;
    state->power.battery_percent  = 100;
    state->power.charging         = 1;

    /* door 节点 */
    state->door.door_status = DOOR_STATUS_UNKNOWN;
    state->door.lock_status = LOCK_STATUS_UNKNOWN;
    state->door.last_open_time = 0;

    /* environment 节点 */
    state->environment.temperature = 0;
    state->environment.humidity    = 0;
    state->environment.pm25        = 0;
    state->environment.co2         = 0;

    /* actuators 节点 — 全零(memset 已做) */

    /* camera 节点 */
    state->camera.camera_status   = CAMERA_STATUS_OFFLINE;
    state->camera.stream_status   = STREAM_STATUS_IDLE;
    state->camera.provider[0]     = '\0';
    state->camera.video_codec     = VIDEO_CODEC_H264;
    state->camera.resolution      = RESOLUTION_720P;
    state->camera.fps             = 0;
    state->camera.audio_enabled   = 0;
    state->camera.privacy_mode    = 0;
    state->camera.last_session_id[0] = '\0';
    state->camera.last_error_code[0] = '\0';

    /* settings 节点 */
    state->settings.temperature_unit[0] = 'C';
    state->settings.temperature_unit[1] = '\0';
    state->settings.display.brightness_percent     = 80;
    state->settings.display.screen_timeout_minutes = 5;
    state->settings.display.screen_mode            = SCREEN_MODE_SCREEN_SAVER;
    state->settings.volume_percent                  = 50;
    state->settings.language[0]                     = '\0';
    state->settings.local_password_enabled          = 0;

    /* firmware 节点 */
    strncpy(state->firmware.firmware_version, FW_VERSION, sizeof(state->firmware.firmware_version) - 1);
    strncpy(state->firmware.hardware_version, HW_VERSION, sizeof(state->firmware.hardware_version) - 1);
    state->firmware.config_version = 0;

    /* storage 节点 */
    state->storage.storage_total_mb = 0;
    state->storage.storage_free_mb  = 0;
}

/* ============================================================================
 * 构建单个 actuator 子对象的 JSON 辅助宏
 * ============================================================================ */
#define ADD_BOOL_IF_NOT_ZERO(obj, key, val)  do { if (val) cJSON_AddTrueToObject(obj, key); else cJSON_AddFalseToObject(obj, key); } while(0)

/* 从 DeviceFullState 构建 data.state JSON 字符串 */
char *device_full_state_to_json(const DeviceFullState *state)
{
    if (!state) return NULL;

    cJSON *root  = cJSON_CreateObject();
    cJSON *jstate = cJSON_CreateObject();

    /* ---- 54.1 device 节点 ---- */
    {
        cJSON *jdev = cJSON_CreateObject();
        cJSON_AddStringToObject(jdev, "bind_status",
            bind_status_to_str(state->device.bind_status));
        cJSON_AddStringToObject(jdev, "work_status",
            work_status_to_str(state->device.work_status));
        cJSON_AddStringToObject(jdev, "online_status",
            online_status_to_str(state->device.online_status));
        cJSON_AddStringToObject(jdev, "fault_summary",
            fault_level_to_str(state->device.fault_summary));
        cJSON_AddStringToObject(jdev, "lifecycle_status",
            lifecycle_status_to_str(state->device.lifecycle_status));

        cJSON *jfaults = cJSON_CreateArray();
        for (int i = 0; i < state->device.faults_count; i++) {
            cJSON *jf = cJSON_CreateObject();
            cJSON_AddStringToObject(jf, "code", state->device.faults[i].code);
            cJSON_AddStringToObject(jf, "level",
                fault_level_to_str(state->device.faults[i].level));
            if (state->device.faults[i].module[0])
                cJSON_AddStringToObject(jf, "module", state->device.faults[i].module);
            if (state->device.faults[i].source[0])
                cJSON_AddStringToObject(jf, "source", state->device.faults[i].source);
            cJSON_AddNumberToObject(jf, "occurred_at",
                (double)state->device.faults[i].occurred_at);
            if (state->device.faults[i].cleared_at > 0)
                cJSON_AddNumberToObject(jf, "cleared_at",
                    (double)state->device.faults[i].cleared_at);
            cJSON_AddItemToArray(jfaults, jf);
        }
        cJSON_AddItemToObject(jdev, "faults", jfaults);
        cJSON_AddItemToObject(jstate, "device", jdev);
    }

    /* ---- 54.2 network 节点 ---- */
    {
        cJSON *jnet = cJSON_CreateObject();
        cJSON_AddStringToObject(jnet, "network_type",
            state->network.ifname[0] ? state->network.ifname
                                     : network_type_to_str(state->network.network_type));
        cJSON_AddNumberToObject(jnet, "rssi", state->network.rssi);
        cJSON_AddStringToObject(jnet, "ip",
            state->network.ip[0] ? state->network.ip : "0.0.0.0");
        cJSON_AddBoolToObject(jnet, "mqtt_connected",
            state->network.mqtt_connected ? 1 : 0);
        cJSON_AddItemToObject(jstate, "network", jnet);
    }

    /* ---- 54.3 power 节点 ---- */
    {
        cJSON *jpow = cJSON_CreateObject();
        cJSON_AddStringToObject(jpow, "power_mode",
            power_mode_to_str(state->power.power_mode));
        cJSON_AddNumberToObject(jpow, "battery_percent", state->power.battery_percent);
        cJSON_AddBoolToObject(jpow, "charging", state->power.charging ? 1 : 0);
        cJSON_AddItemToObject(jstate, "power", jpow);
    }

    /* ---- 54.4 door 节点 ---- */
    {
        cJSON *jdoor = cJSON_CreateObject();
        cJSON_AddStringToObject(jdoor, "door_status",
            door_status_to_str(state->door.door_status));
        cJSON_AddStringToObject(jdoor, "lock_status",
            lock_status_to_str(state->door.lock_status));
        cJSON_AddNumberToObject(jdoor, "last_open_time", state->door.last_open_time);
        cJSON_AddItemToObject(jstate, "door", jdoor);
    }

    /* ---- 54.5 environment 节点 ---- */
    {
        cJSON *jenv = cJSON_CreateObject();
        cJSON_AddNumberToObject(jenv, "temperature", state->environment.temperature);
        cJSON_AddNumberToObject(jenv, "humidity", state->environment.humidity);
        cJSON_AddNumberToObject(jenv, "pm25", state->environment.pm25);
        cJSON_AddNumberToObject(jenv, "co2", state->environment.co2);
        cJSON_AddItemToObject(jstate, "environment", jenv);
    }

    /* ---- 54.6 actuators 节点 ---- */
    {
        cJSON *jact = cJSON_CreateObject();

        /* exhaust */
        cJSON *jex = cJSON_CreateObject();
        ADD_BOOL_IF_NOT_ZERO(jex, "enabled", state->actuators.exhaust.enabled);
        cJSON_AddNumberToObject(jex, "speed_level", state->actuators.exhaust.speed_level);
        cJSON_AddNumberToObject(jex, "interval_hours", state->actuators.exhaust.interval_hours);
        ADD_BOOL_IF_NOT_ZERO(jex, "running", state->actuators.exhaust.running);
        cJSON_AddItemToObject(jact, "exhaust", jex);

        /* light */
        cJSON *jli = cJSON_CreateObject();
        ADD_BOOL_IF_NOT_ZERO(jli, "enabled", state->actuators.light.enabled);
        cJSON_AddNumberToObject(jli, "brightness_level", state->actuators.light.brightness_level);
        ADD_BOOL_IF_NOT_ZERO(jli, "running", state->actuators.light.running);
        cJSON_AddItemToObject(jact, "light", jli);

        /* uvb */
        cJSON *juv = cJSON_CreateObject();
        ADD_BOOL_IF_NOT_ZERO(juv, "enabled", state->actuators.uvb.enabled);
        cJSON_AddNumberToObject(juv, "level", state->actuators.uvb.level);
        cJSON_AddNumberToObject(juv, "daily_hours", state->actuators.uvb.daily_hours);
        ADD_BOOL_IF_NOT_ZERO(juv, "running", state->actuators.uvb.running);
        cJSON_AddItemToObject(jact, "uvb", juv);

        /* anion */
        cJSON *jan = cJSON_CreateObject();
        ADD_BOOL_IF_NOT_ZERO(jan, "enabled", state->actuators.anion.enabled);
        ADD_BOOL_IF_NOT_ZERO(jan, "running", state->actuators.anion.running);
        cJSON_AddItemToObject(jact, "anion", jan);

        /* plasma */
        cJSON *jpl = cJSON_CreateObject();
        ADD_BOOL_IF_NOT_ZERO(jpl, "enabled", state->actuators.plasma.enabled);
        ADD_BOOL_IF_NOT_ZERO(jpl, "running", state->actuators.plasma.running);
        cJSON_AddItemToObject(jact, "plasma", jpl);

        /* climate */
        cJSON *jcl = cJSON_CreateObject();
        ADD_BOOL_IF_NOT_ZERO(jcl, "enabled", state->actuators.climate.enabled);
        if (state->actuators.climate.target_celsius < 0)
            cJSON_AddNullToObject(jcl, "target_celsius");
        else
            cJSON_AddNumberToObject(jcl, "target_celsius", state->actuators.climate.target_celsius);
        cJSON_AddStringToObject(jcl, "control_status",
            climate_control_status_to_str(state->actuators.climate.control_status));
        ADD_BOOL_IF_NOT_ZERO(jcl, "running", state->actuators.climate.running);
        cJSON_AddItemToObject(jact, "climate", jcl);

        /* humidifier */
        cJSON *jhu = cJSON_CreateObject();
        ADD_BOOL_IF_NOT_ZERO(jhu, "enabled", state->actuators.humidifier.enabled);
        cJSON_AddNumberToObject(jhu, "interval_hours", state->actuators.humidifier.interval_hours);
        cJSON_AddNumberToObject(jhu, "running_hours", state->actuators.humidifier.running_hours);
        ADD_BOOL_IF_NOT_ZERO(jhu, "running", state->actuators.humidifier.running);
        cJSON_AddStringToObject(jhu, "liquid_status",
            liquid_status_to_str(state->actuators.humidifier.liquid_status));
        cJSON_AddItemToObject(jact, "humidifier", jhu);

        /* inlet_fan */
        cJSON *jif = cJSON_CreateObject();
        ADD_BOOL_IF_NOT_ZERO(jif, "enabled", state->actuators.inlet_fan.enabled);
        cJSON_AddNumberToObject(jif, "speed_level", state->actuators.inlet_fan.speed_level);
        ADD_BOOL_IF_NOT_ZERO(jif, "running", state->actuators.inlet_fan.running);
        cJSON_AddItemToObject(jact, "inlet_fan", jif);

        /* filter */
        cJSON *jfi = cJSON_CreateObject();
        cJSON_AddNumberToObject(jfi, "life_percent", state->actuators.filter.life_percent);
        ADD_BOOL_IF_NOT_ZERO(jfi, "need_replace", state->actuators.filter.need_replace);
        cJSON_AddItemToObject(jact, "filter", jfi);

        cJSON_AddItemToObject(jstate, "actuators", jact);
    }

    /* ---- 54.7 camera 节点 ---- */
    {
        cJSON *jcam = cJSON_CreateObject();
        cJSON_AddStringToObject(jcam, "camera_status",
            camera_status_to_str(state->camera.camera_status));
        cJSON_AddStringToObject(jcam, "stream_status",
            stream_status_to_str(state->camera.stream_status));
        if (state->camera.provider[0])
            cJSON_AddStringToObject(jcam, "provider", state->camera.provider);
        cJSON_AddStringToObject(jcam, "video_codec",
            video_codec_to_str(state->camera.video_codec));
        cJSON_AddStringToObject(jcam, "resolution",
            resolution_to_str(state->camera.resolution));
        cJSON_AddNumberToObject(jcam, "fps", state->camera.fps);
        cJSON_AddBoolToObject(jcam, "audio_enabled",
            state->camera.audio_enabled ? 1 : 0);
        cJSON_AddBoolToObject(jcam, "privacy_mode",
            state->camera.privacy_mode ? 1 : 0);
        if (state->camera.last_error_code[0])
            cJSON_AddStringToObject(jcam, "last_error_code",
                state->camera.last_error_code);
        else
            cJSON_AddNullToObject(jcam, "last_error_code");
        if (state->camera.last_session_id[0])
            cJSON_AddStringToObject(jcam, "last_session_id",
                state->camera.last_session_id);
        else
            cJSON_AddNullToObject(jcam, "last_session_id");
        cJSON_AddItemToObject(jstate, "camera", jcam);
    }

    /* ---- 54.8 settings 节点 ---- */
    {
        cJSON *jset = cJSON_CreateObject();
        if (state->settings.temperature_unit[0])
            cJSON_AddStringToObject(jset, "temperature_unit",
                state->settings.temperature_unit);

        cJSON *jdisp = cJSON_CreateObject();
        cJSON_AddNumberToObject(jdisp, "brightness_percent",
            state->settings.display.brightness_percent);
        cJSON_AddNumberToObject(jdisp, "screen_timeout_minutes",
            state->settings.display.screen_timeout_minutes);
        cJSON_AddStringToObject(jdisp, "screen_mode",
            screen_mode_to_str(state->settings.display.screen_mode));
        cJSON_AddItemToObject(jset, "display", jdisp);

        cJSON_AddNumberToObject(jset, "volume_percent", state->settings.volume_percent);
        if (state->settings.language[0])
            cJSON_AddStringToObject(jset, "language", state->settings.language);
        cJSON_AddBoolToObject(jset, "local_password_enabled",
            state->settings.local_password_enabled ? 1 : 0);
        cJSON_AddItemToObject(jstate, "settings", jset);
    }

    /* ---- firmware 节点(§51.4) ---- */
    {
        cJSON *jfw = cJSON_CreateObject();
        if (state->firmware.firmware_version[0])
            cJSON_AddStringToObject(jfw, "firmware_version",
                state->firmware.firmware_version);
        if (state->firmware.hardware_version[0])
            cJSON_AddStringToObject(jfw, "hardware_version",
                state->firmware.hardware_version);
        cJSON_AddNumberToObject(jfw, "config_version", state->firmware.config_version);
        cJSON_AddItemToObject(jstate, "firmware", jfw);
    }

    /* ---- storage 节点(§51.4) ---- */
    {
        cJSON *jstor = cJSON_CreateObject();
        cJSON_AddNumberToObject(jstor, "storage_total_mb", state->storage.storage_total_mb);
        cJSON_AddNumberToObject(jstor, "storage_free_mb", state->storage.storage_free_mb);
        cJSON_AddItemToObject(jstate, "storage", jstor);
    }

    cJSON_AddItemToObject(root, "state", jstate);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_str;
}

/* 从 JSON 字符串解析 data.state, 填充 DeviceFullState */
int device_full_state_parse_from_json(const char *json_str, DeviceFullState *state)
{
    if (!json_str || !state) return -1;
    memset(state, 0, sizeof(DeviceFullState));

    cJSON *root = cJSON_Parse(json_str);
    if (!root) return -1;

    /* 兼容外层有/无 "state" 包裹 */
    cJSON *jstate = cJSON_GetObjectItem(root, "state");
    if (!jstate) jstate = root;

    /* ---- device 节点 ---- */
    cJSON *jdev = cJSON_GetObjectItem(jstate, "device");
    if (jdev) {
        cJSON *item;
        item = cJSON_GetObjectItem(jdev, "bind_status");
        if (cJSON_IsString(item))
            state->device.bind_status = bind_status_from_str(item->valuestring);

        item = cJSON_GetObjectItem(jdev, "work_status");
        if (cJSON_IsString(item))
            state->device.work_status = work_status_from_str(item->valuestring);

        item = cJSON_GetObjectItem(jdev, "online_status");
        if (cJSON_IsString(item))
            state->device.online_status = online_status_from_str(item->valuestring);

        item = cJSON_GetObjectItem(jdev, "fault_summary");
        if (cJSON_IsString(item))
            state->device.fault_summary = fault_level_from_str(item->valuestring);

        item = cJSON_GetObjectItem(jdev, "lifecycle_status");
        if (cJSON_IsString(item))
            state->device.lifecycle_status = lifecycle_status_from_str(item->valuestring);

        item = cJSON_GetObjectItem(jdev, "faults");
        if (cJSON_IsArray(item)) {
            int n = cJSON_GetArraySize(item);
            if (n > 0 && n <= 50) {
                state->device.faults = (FaultEntry *)calloc(n, sizeof(FaultEntry));
                if (state->device.faults) {
                    for (int i = 0; i < n; i++) {
                        cJSON *jf = cJSON_GetArrayItem(item, i);
                        cJSON *fc = cJSON_GetObjectItem(jf, "code");
                        cJSON *fl = cJSON_GetObjectItem(jf, "level");
                        cJSON *fm = cJSON_GetObjectItem(jf, "module");
                        cJSON *fs = cJSON_GetObjectItem(jf, "source");
                        cJSON *oa = cJSON_GetObjectItem(jf, "occurred_at");
                        cJSON *ca = cJSON_GetObjectItem(jf, "cleared_at");
                        if (cJSON_IsString(fc))
                            strncpy(state->device.faults[i].code,
                                fc->valuestring, sizeof(state->device.faults[i].code) - 1);
                        if (cJSON_IsString(fl))
                            state->device.faults[i].level = fault_level_from_str(fl->valuestring);
                        if (cJSON_IsString(fm))
                            strncpy(state->device.faults[i].module,
                                fm->valuestring, sizeof(state->device.faults[i].module) - 1);
                        if (cJSON_IsString(fs))
                            strncpy(state->device.faults[i].source,
                                fs->valuestring, sizeof(state->device.faults[i].source) - 1);
                        if (cJSON_IsNumber(oa))
                            state->device.faults[i].occurred_at = (int64_t)oa->valuedouble;
                        if (cJSON_IsNumber(ca))
                            state->device.faults[i].cleared_at = (int64_t)ca->valuedouble;
                    }
                    state->device.faults_count = n;
                }
            }
        }
    }

    /* ---- network 节点 ---- */
    cJSON *jnet = cJSON_GetObjectItem(jstate, "network");
    if (jnet) {
        cJSON *item;
        item = cJSON_GetObjectItem(jnet, "network_type");
        if (cJSON_IsString(item)) {
            strncpy(state->network.ifname, item->valuestring, sizeof(state->network.ifname) - 1);
            state->network.network_type = network_type_from_str(item->valuestring);
        }
        item = cJSON_GetObjectItem(jnet, "rssi");
        if (cJSON_IsNumber(item)) state->network.rssi = (int)item->valuedouble;
        item = cJSON_GetObjectItem(jnet, "ip");
        if (cJSON_IsString(item))
            strncpy(state->network.ip, item->valuestring, sizeof(state->network.ip) - 1);
        item = cJSON_GetObjectItem(jnet, "mqtt_connected");
        if (cJSON_IsBool(item) || cJSON_IsNumber(item))
            state->network.mqtt_connected = (item->valueint != 0);
    }

    /* ---- power 节点 ---- */
    cJSON *jpow = cJSON_GetObjectItem(jstate, "power");
    if (jpow) {
        cJSON *item;
        item = cJSON_GetObjectItem(jpow, "power_mode");
        if (cJSON_IsString(item))
            state->power.power_mode = power_mode_from_str(item->valuestring);
        item = cJSON_GetObjectItem(jpow, "battery_percent");
        if (cJSON_IsNumber(item)) state->power.battery_percent = (int)item->valuedouble;
        item = cJSON_GetObjectItem(jpow, "charging");
        if (cJSON_IsBool(item) || cJSON_IsNumber(item))
            state->power.charging = (item->valueint != 0);
    }

    /* ---- door 节点 ---- */
    cJSON *jdoor = cJSON_GetObjectItem(jstate, "door");
    if (jdoor) {
        cJSON *item;
        item = cJSON_GetObjectItem(jdoor, "door_status");
        if (cJSON_IsString(item))
            state->door.door_status = door_status_from_str(item->valuestring);
        item = cJSON_GetObjectItem(jdoor, "lock_status");
        if (cJSON_IsString(item))
            state->door.lock_status = lock_status_from_str(item->valuestring);
        item = cJSON_GetObjectItem(jdoor, "last_open_time");
        if (cJSON_IsNumber(item)) state->door.last_open_time = item->valuedouble;
    }

    /* ---- environment 节点 ---- */
    cJSON *jenv = cJSON_GetObjectItem(jstate, "environment");
    if (jenv) {
        cJSON *item;
        item = cJSON_GetObjectItem(jenv, "temperature");
        if (cJSON_IsNumber(item)) state->environment.temperature = item->valuedouble;
        item = cJSON_GetObjectItem(jenv, "humidity");
        if (cJSON_IsNumber(item)) state->environment.humidity = item->valuedouble;
        item = cJSON_GetObjectItem(jenv, "pm25");
        if (cJSON_IsNumber(item)) state->environment.pm25 = item->valuedouble;
        item = cJSON_GetObjectItem(jenv, "co2");
        if (cJSON_IsNumber(item)) state->environment.co2 = item->valuedouble;
    }

    /* ---- actuators 节点 ---- */
    cJSON *jact = cJSON_GetObjectItem(jstate, "actuators");
    if (jact) {
        /* exhaust */
        cJSON *jex = cJSON_GetObjectItem(jact, "exhaust");
        if (jex) {
            cJSON *item;
            item = cJSON_GetObjectItem(jex, "enabled");
            if (cJSON_IsBool(item) || cJSON_IsNumber(item))
                state->actuators.exhaust.enabled = (item->valueint != 0);
            item = cJSON_GetObjectItem(jex, "speed_level");
            if (cJSON_IsNumber(item)) state->actuators.exhaust.speed_level = (int)item->valuedouble;
            item = cJSON_GetObjectItem(jex, "interval_hours");
            if (cJSON_IsNumber(item)) state->actuators.exhaust.interval_hours = item->valuedouble;
            item = cJSON_GetObjectItem(jex, "running");
            if (cJSON_IsBool(item) || cJSON_IsNumber(item))
                state->actuators.exhaust.running = (item->valueint != 0);
        }
        /* light */
        cJSON *jli = cJSON_GetObjectItem(jact, "light");
        if (jli) {
            cJSON *item;
            item = cJSON_GetObjectItem(jli, "enabled");
            if (cJSON_IsBool(item) || cJSON_IsNumber(item))
                state->actuators.light.enabled = (item->valueint != 0);
            item = cJSON_GetObjectItem(jli, "brightness_level");
            if (cJSON_IsNumber(item)) state->actuators.light.brightness_level = (int)item->valuedouble;
            item = cJSON_GetObjectItem(jli, "running");
            if (cJSON_IsBool(item) || cJSON_IsNumber(item))
                state->actuators.light.running = (item->valueint != 0);
        }
        /* uvb */
        cJSON *juv = cJSON_GetObjectItem(jact, "uvb");
        if (juv) {
            cJSON *item;
            item = cJSON_GetObjectItem(juv, "enabled");
            if (cJSON_IsBool(item) || cJSON_IsNumber(item))
                state->actuators.uvb.enabled = (item->valueint != 0);
            item = cJSON_GetObjectItem(juv, "level");
            if (cJSON_IsNumber(item)) state->actuators.uvb.level = (int)item->valuedouble;
            item = cJSON_GetObjectItem(juv, "daily_hours");
            if (cJSON_IsNumber(item)) state->actuators.uvb.daily_hours = (int)item->valuedouble;
            item = cJSON_GetObjectItem(juv, "running");
            if (cJSON_IsBool(item) || cJSON_IsNumber(item))
                state->actuators.uvb.running = (item->valueint != 0);
        }
        /* anion */
        cJSON *jan = cJSON_GetObjectItem(jact, "anion");
        if (jan) {
            cJSON *item;
            item = cJSON_GetObjectItem(jan, "enabled");
            if (cJSON_IsBool(item) || cJSON_IsNumber(item))
                state->actuators.anion.enabled = (item->valueint != 0);
            item = cJSON_GetObjectItem(jan, "running");
            if (cJSON_IsBool(item) || cJSON_IsNumber(item))
                state->actuators.anion.running = (item->valueint != 0);
        }
        /* plasma */
        cJSON *jpl = cJSON_GetObjectItem(jact, "plasma");
        if (jpl) {
            cJSON *item;
            item = cJSON_GetObjectItem(jpl, "enabled");
            if (cJSON_IsBool(item) || cJSON_IsNumber(item))
                state->actuators.plasma.enabled = (item->valueint != 0);
            item = cJSON_GetObjectItem(jpl, "running");
            if (cJSON_IsBool(item) || cJSON_IsNumber(item))
                state->actuators.plasma.running = (item->valueint != 0);
        }
        /* climate */
        cJSON *jcl = cJSON_GetObjectItem(jact, "climate");
        if (jcl) {
            cJSON *item;
            item = cJSON_GetObjectItem(jcl, "enabled");
            if (cJSON_IsBool(item) || cJSON_IsNumber(item))
                state->actuators.climate.enabled = (item->valueint != 0);
            item = cJSON_GetObjectItem(jcl, "target_celsius");
            if (cJSON_IsNumber(item))
                state->actuators.climate.target_celsius = item->valuedouble;
            else if (cJSON_IsNull(item))
                state->actuators.climate.target_celsius = -1;
            item = cJSON_GetObjectItem(jcl, "control_status");
            if (cJSON_IsString(item))
                state->actuators.climate.control_status = climate_control_status_from_str(item->valuestring);
            item = cJSON_GetObjectItem(jcl, "running");
            if (cJSON_IsBool(item) || cJSON_IsNumber(item))
                state->actuators.climate.running = (item->valueint != 0);
        }
        /* humidifier */
        cJSON *jhu = cJSON_GetObjectItem(jact, "humidifier");
        if (jhu) {
            cJSON *item;
            item = cJSON_GetObjectItem(jhu, "enabled");
            if (cJSON_IsBool(item) || cJSON_IsNumber(item))
                state->actuators.humidifier.enabled = (item->valueint != 0);
            item = cJSON_GetObjectItem(jhu, "interval_hours");
            if (cJSON_IsNumber(item)) state->actuators.humidifier.interval_hours = item->valuedouble;
            item = cJSON_GetObjectItem(jhu, "running_hours");
            if (cJSON_IsNumber(item)) state->actuators.humidifier.running_hours = item->valuedouble;
            item = cJSON_GetObjectItem(jhu, "running");
            if (cJSON_IsBool(item) || cJSON_IsNumber(item))
                state->actuators.humidifier.running = (item->valueint != 0);
            item = cJSON_GetObjectItem(jhu, "liquid_status");
            if (cJSON_IsString(item))
                state->actuators.humidifier.liquid_status = liquid_status_from_str(item->valuestring);
        }
        /* inlet_fan */
        cJSON *jif = cJSON_GetObjectItem(jact, "inlet_fan");
        if (jif) {
            cJSON *item;
            item = cJSON_GetObjectItem(jif, "enabled");
            if (cJSON_IsBool(item) || cJSON_IsNumber(item))
                state->actuators.inlet_fan.enabled = (item->valueint != 0);
            item = cJSON_GetObjectItem(jif, "speed_level");
            if (cJSON_IsNumber(item)) state->actuators.inlet_fan.speed_level = (int)item->valuedouble;
            item = cJSON_GetObjectItem(jif, "running");
            if (cJSON_IsBool(item) || cJSON_IsNumber(item))
                state->actuators.inlet_fan.running = (item->valueint != 0);
        }
        /* filter */
        cJSON *jfi = cJSON_GetObjectItem(jact, "filter");
        if (jfi) {
            cJSON *item;
            item = cJSON_GetObjectItem(jfi, "life_percent");
            if (cJSON_IsNumber(item)) state->actuators.filter.life_percent = (int)item->valuedouble;
            item = cJSON_GetObjectItem(jfi, "need_replace");
            if (cJSON_IsBool(item) || cJSON_IsNumber(item))
                state->actuators.filter.need_replace = (item->valueint != 0);
        }
    }

    /* ---- camera 节点 ---- */
    cJSON *jcam = cJSON_GetObjectItem(jstate, "camera");
    if (jcam) {
        cJSON *item;
        item = cJSON_GetObjectItem(jcam, "camera_status");
        if (cJSON_IsString(item))
            state->camera.camera_status = camera_status_from_str(item->valuestring);
        item = cJSON_GetObjectItem(jcam, "stream_status");
        if (cJSON_IsString(item))
            state->camera.stream_status = stream_status_from_str(item->valuestring);
        item = cJSON_GetObjectItem(jcam, "provider");
        if (cJSON_IsString(item))
            strncpy(state->camera.provider, item->valuestring, sizeof(state->camera.provider) - 1);
        item = cJSON_GetObjectItem(jcam, "video_codec");
        if (cJSON_IsString(item))
            state->camera.video_codec = video_codec_from_str(item->valuestring);
        item = cJSON_GetObjectItem(jcam, "resolution");
        if (cJSON_IsString(item))
            state->camera.resolution = resolution_from_str(item->valuestring);
        item = cJSON_GetObjectItem(jcam, "fps");
        if (cJSON_IsNumber(item)) state->camera.fps = item->valuedouble;
        item = cJSON_GetObjectItem(jcam, "audio_enabled");
        if (cJSON_IsBool(item) || cJSON_IsNumber(item))
            state->camera.audio_enabled = (item->valueint != 0);
        item = cJSON_GetObjectItem(jcam, "privacy_mode");
        if (cJSON_IsBool(item) || cJSON_IsNumber(item))
            state->camera.privacy_mode = (item->valueint != 0);
        item = cJSON_GetObjectItem(jcam, "last_error_code");
        if (cJSON_IsString(item))
            strncpy(state->camera.last_error_code, item->valuestring,
                sizeof(state->camera.last_error_code) - 1);
        else if (cJSON_IsNull(item))
            state->camera.last_error_code[0] = '\0';
        item = cJSON_GetObjectItem(jcam, "last_session_id");
        if (cJSON_IsString(item))
            strncpy(state->camera.last_session_id, item->valuestring,
                sizeof(state->camera.last_session_id) - 1);
        else if (cJSON_IsNull(item))
            state->camera.last_session_id[0] = '\0';
    }

    /* ---- settings 节点 ---- */
    cJSON *jset = cJSON_GetObjectItem(jstate, "settings");
    if (jset) {
        cJSON *item;
        item = cJSON_GetObjectItem(jset, "temperature_unit");
        if (cJSON_IsString(item))
            strncpy(state->settings.temperature_unit, item->valuestring,
                sizeof(state->settings.temperature_unit) - 1);

        cJSON *jdisp = cJSON_GetObjectItem(jset, "display");
        if (jdisp) {
            item = cJSON_GetObjectItem(jdisp, "brightness_percent");
            if (cJSON_IsNumber(item)) state->settings.display.brightness_percent = (int)item->valuedouble;
            item = cJSON_GetObjectItem(jdisp, "screen_timeout_minutes");
            if (cJSON_IsNumber(item)) state->settings.display.screen_timeout_minutes = (int)item->valuedouble;
            item = cJSON_GetObjectItem(jdisp, "screen_mode");
            if (cJSON_IsString(item))
                state->settings.display.screen_mode = screen_mode_from_str(item->valuestring);
        }

        item = cJSON_GetObjectItem(jset, "volume_percent");
        if (cJSON_IsNumber(item)) state->settings.volume_percent = (int)item->valuedouble;
        item = cJSON_GetObjectItem(jset, "language");
        if (cJSON_IsString(item))
            strncpy(state->settings.language, item->valuestring, sizeof(state->settings.language) - 1);
        item = cJSON_GetObjectItem(jset, "local_password_enabled");
        if (cJSON_IsBool(item) || cJSON_IsNumber(item))
            state->settings.local_password_enabled = (item->valueint != 0);
    }

    /* ---- firmware 节点(§51.4) ---- */
    cJSON *jfw = cJSON_GetObjectItem(jstate, "firmware");
    if (jfw) {
        cJSON *item;
        item = cJSON_GetObjectItem(jfw, "firmware_version");
        if (cJSON_IsString(item))
            strncpy(state->firmware.firmware_version, item->valuestring,
                sizeof(state->firmware.firmware_version) - 1);
        item = cJSON_GetObjectItem(jfw, "hardware_version");
        if (cJSON_IsString(item))
            strncpy(state->firmware.hardware_version, item->valuestring,
                sizeof(state->firmware.hardware_version) - 1);
        item = cJSON_GetObjectItem(jfw, "config_version");
        if (cJSON_IsNumber(item)) state->firmware.config_version = (int)item->valuedouble;
    }

    /* ---- storage 节点(§51.4) ---- */
    cJSON *jstor = cJSON_GetObjectItem(jstate, "storage");
    if (jstor) {
        cJSON *item;
        item = cJSON_GetObjectItem(jstor, "storage_total_mb");
        if (cJSON_IsNumber(item)) state->storage.storage_total_mb = (int)item->valuedouble;
        item = cJSON_GetObjectItem(jstor, "storage_free_mb");
        if (cJSON_IsNumber(item)) state->storage.storage_free_mb = (int)item->valuedouble;
    }

    cJSON_Delete(root);
    return 0;
}

/*
 * 从 JSON 字符串解析 server.command 消息, 填充 ServerCommand 结构体。
 * 返回 0 成功, -1 失败。
 */
int server_command_parse_from_json(const char *json_str, ServerCommand *cmd)
{
	if (!json_str || !cmd) return -1;
	memset(cmd, 0, sizeof(ServerCommand));

	cJSON *root = cJSON_Parse(json_str);
	if (!root) return -1;

	/* 顶层字段 */
	{
		cJSON *item = cJSON_GetObjectItem(root, "msg_id");
		if (cJSON_IsString(item)) strncpy(cmd->msg_id, item->valuestring, sizeof(cmd->msg_id) - 1);

		item = cJSON_GetObjectItem(root, "msg_type");
		if (cJSON_IsString(item)) strncpy(cmd->msg_type, item->valuestring, sizeof(cmd->msg_type) - 1);

		item = cJSON_GetObjectItem(root, "protocol_version");
		if (cJSON_IsString(item)) strncpy(cmd->protocol_version, item->valuestring, sizeof(cmd->protocol_version) - 1);

		item = cJSON_GetObjectItem(root, "device_sn");
		if (cJSON_IsString(item)) strncpy(cmd->device_sn, item->valuestring, sizeof(cmd->device_sn) - 1);

		item = cJSON_GetObjectItem(root, "product_key");
		if (cJSON_IsString(item)) strncpy(cmd->product_key, item->valuestring, sizeof(cmd->product_key) - 1);

		item = cJSON_GetObjectItem(root, "timestamp");
		if (cJSON_IsNumber(item)) cmd->timestamp = (int64_t)item->valuedouble;
	}

	/* data 字段 */
	cJSON *data = cJSON_GetObjectItem(root, "data");
	if (data) {
		cJSON *item = cJSON_GetObjectItem(data, "command_id");
		if (cJSON_IsString(item)) strncpy(cmd->command_id, item->valuestring, sizeof(cmd->command_id) - 1);

		item = cJSON_GetObjectItem(data, "cmd");
		if (cJSON_IsString(item)) strncpy(cmd->cmd, item->valuestring, sizeof(cmd->cmd) - 1);

		item = cJSON_GetObjectItem(data, "expire_at");
		if (cJSON_IsNumber(item)) cmd->data_expire_at = (int64_t)item->valuedouble;

		/* data.params 字段 */
		cJSON *params = cJSON_GetObjectItem(data, "params");
		if (params) {
			item = cJSON_GetObjectItem(params, "session_id");
			if (cJSON_IsString(item)) strncpy(cmd->session_id, item->valuestring, sizeof(cmd->session_id) - 1);

			item = cJSON_GetObjectItem(params, "provider");
			if (cJSON_IsString(item)) strncpy(cmd->provider, item->valuestring, sizeof(cmd->provider) - 1);

			item = cJSON_GetObjectItem(params, "expire_at");
			if (cJSON_IsNumber(item)) cmd->params_expire_at = (int64_t)item->valuedouble;

			item = cJSON_GetObjectItem(params, "stream_type");
			if (cJSON_IsString(item)) strncpy(cmd->stream_type, item->valuestring, sizeof(cmd->stream_type) - 1);

			item = cJSON_GetObjectItem(params, "video_profile");
			if (cJSON_IsString(item)) strncpy(cmd->video_profile, item->valuestring, sizeof(cmd->video_profile) - 1);

			item = cJSON_GetObjectItem(params, "audio_enabled");
			if (cJSON_IsBool(item)) cmd->audio_enabled = cJSON_IsTrue(item) ? 1 : 0;

			/* data.params.provider_params 字段 */
			cJSON *pp = cJSON_GetObjectItem(params, "provider_params");
			if (pp) {
				item = cJSON_GetObjectItem(pp, "app_id");
				if (cJSON_IsString(item)) strncpy(cmd->provider_app_id, item->valuestring, sizeof(cmd->provider_app_id) - 1);

				item = cJSON_GetObjectItem(pp, "device_id");
				if (cJSON_IsString(item)) strncpy(cmd->provider_device_id, item->valuestring, sizeof(cmd->provider_device_id) - 1);
			}

			/* 保存原始 params JSON 字符串, 供执行器指令解析 */
			char *params_str = cJSON_PrintUnformatted(params);
			if (params_str) {
				strncpy(cmd->params_json, params_str, sizeof(cmd->params_json) - 1);
				free(params_str);
			}
		}
	}

	cJSON_Delete(root);
	return 0;
}

/*
 * 从 JSON 字符串解析 device.command_ack 消息, 填充 CommandAck 结构体。
 * 返回 0 成功, -1 失败。
 */
int command_ack_parse_from_json(const char *json_str, CommandAck *ack)
{
	if (!json_str || !ack) return -1;
	memset(ack, 0, sizeof(CommandAck));

	cJSON *root = cJSON_Parse(json_str);
	if (!root) return -1;

	/* 顶层字段 */
	{
		cJSON *item = cJSON_GetObjectItem(root, "msg_id");
		if (cJSON_IsString(item)) strncpy(ack->msg_id, item->valuestring, sizeof(ack->msg_id) - 1);

		item = cJSON_GetObjectItem(root, "msg_type");
		if (cJSON_IsString(item)) strncpy(ack->msg_type, item->valuestring, sizeof(ack->msg_type) - 1);

		item = cJSON_GetObjectItem(root, "device_sn");
		if (cJSON_IsString(item)) strncpy(ack->device_sn, item->valuestring, sizeof(ack->device_sn) - 1);

		item = cJSON_GetObjectItem(root, "product_key");
		if (cJSON_IsString(item)) strncpy(ack->product_key, item->valuestring, sizeof(ack->product_key) - 1);

		item = cJSON_GetObjectItem(root, "timestamp");
		if (cJSON_IsNumber(item)) ack->timestamp = (int64_t)item->valuedouble;
	}

	/* data 字段 */
	cJSON *data = cJSON_GetObjectItem(root, "data");
	if (data) {
		cJSON *item = cJSON_GetObjectItem(data, "command_id");
		if (cJSON_IsString(item)) strncpy(ack->command_id, item->valuestring, sizeof(ack->command_id) - 1);

		item = cJSON_GetObjectItem(data, "server_msg_id");
		if (cJSON_IsString(item)) strncpy(ack->server_msg_id, item->valuestring, sizeof(ack->server_msg_id) - 1);

		item = cJSON_GetObjectItem(data, "status");
		if (cJSON_IsString(item)) strncpy(ack->status, item->valuestring, sizeof(ack->status) - 1);

		/* data.result 字段 */
		cJSON *result = cJSON_GetObjectItem(data, "result");
		if (result) {
			item = cJSON_GetObjectItem(result, "session_id");
			if (cJSON_IsString(item)) strncpy(ack->session_id, item->valuestring, sizeof(ack->session_id) - 1);

			item = cJSON_GetObjectItem(result, "provider");
			if (cJSON_IsString(item)) strncpy(ack->provider, item->valuestring, sizeof(ack->provider) - 1);

			item = cJSON_GetObjectItem(result, "camera_status");
			if (cJSON_IsString(item)) strncpy(ack->camera_status, item->valuestring, sizeof(ack->camera_status) - 1);
		}
	}

	cJSON_Delete(root);
	return 0;
}

/*
 * 对 video.start_session 命令构建 device.command_ack 应答并发布到 MQTT。
 * 参数:
 *   mosq          - MQTT 客户端实例
 *   cmd           - 收到的 server.command (提取 command_id / server_msg_id)
 *   status        - 执行结果: "SUCCESS" / "FAILED"
 *   session_id    - 视频会话 ID (成功时传入)
 *   provider      - 流媒体提供商 (成功时传入, 如 "TANGE")
 *   camera_status - 摄像头状态: "STREAMING" / "IDLE" / "ERROR"
 *   error_code    - 失败原因机器码, 仅 FAILED 时写入 data.error_code
 * 返回 mosquitto_publish 的返回值。
 */
int command_ack_publish(struct mosquitto *mosq, const ServerCommand *cmd,
                        const char *status, const char *session_id,
                        const char *provider, const char *camera_status,
                        const char *error_code)
{
	if (!mosq || !cmd) return -1;

	cJSON *root = cJSON_CreateObject();
	int64_t now = (int64_t)time(NULL);

	/* 顶层字段 */
	char msg_id[128];
	snprintf(msg_id, sizeof(msg_id), "ACK-%s-%lld", cmd->command_id, (long long)now);
	cJSON_AddStringToObject(root, "msg_id", msg_id);
	cJSON_AddStringToObject(root, "msg_type", "device.command_ack");
	cJSON_AddStringToObject(root, "protocol_version", PROTOCOL_VER);
	cJSON_AddStringToObject(root, "device_sn", cmd->device_sn[0] ? cmd->device_sn : DEVICE_SN);
	cJSON_AddStringToObject(root, "product_key", cmd->product_key[0] ? cmd->product_key : PRODUCT_KEY);
	cJSON_AddNumberToObject(root, "timestamp", now);

	/* data 字段 */
	cJSON *data = cJSON_CreateObject();
	cJSON_AddStringToObject(data, "command_id", cmd->command_id);
	cJSON_AddStringToObject(data, "server_msg_id",
		cmd->msg_id[0] ? cmd->msg_id : cmd->command_id);
	cJSON_AddStringToObject(data, "status", status ? status : "SUCCESS");
	if (status && error_code && error_code[0] &&
	    (strcmp(status, "FAILED") == 0 || strcmp(status, "UNSUPPORTED") == 0)) {
		cJSON_AddStringToObject(data, "error_code", error_code);
	}

	/* data.result 字段 (仅视频会话类指令成功时填入, 执行器指令跳过) */
	if (status && strcmp(status, "SUCCESS") == 0
	    && session_id && session_id[0]) {
		cJSON *result = cJSON_CreateObject();
		cJSON_AddStringToObject(result, "session_id", session_id);
		cJSON_AddStringToObject(result, "provider",
			provider && provider[0] ? provider : cmd->provider);
		cJSON_AddStringToObject(result, "camera_status",
			camera_status ? camera_status : "STREAMING");
		cJSON_AddItemToObject(data, "result", result);
	}

	cJSON_AddItemToObject(root, "data", data);

	char *json_str = cJSON_PrintUnformatted(root);
	cJSON_Delete(root);
	if (!json_str) return -1;

	/* 发布到上行 Topic */
	char topic[128];
	snprintf(topic, sizeof(topic), "/mcqx/device/%s/up",
		cmd->device_sn[0] ? cmd->device_sn : DEVICE_SN);

	printf_info("[command_ack_publish] topic=%s\n", topic);
	printf_info("[command_ack_publish] payload=%s\n", json_str);

	int rc = mosquitto_publish(mosq, NULL, topic,
		(int)strlen(json_str), json_str, 1, false);
	if (rc == MOSQ_ERR_SUCCESS) {
		printf_info("command_ack published: cmd=%s status=%s\n", cmd->command_id, status);
	} else {
		printf_info("command_ack publish failed: %s\n", mosquitto_strerror(rc));
	}

	free(json_str);
	return rc;
}

/*
 * 将 MqttConfig 以二进制形式保存到 /mnt/UDISK/mqtt_save_config.txt。
 * 返回 0 成功, -1 失败。
 */
int mqtt_config_save(MqttConfig *cfg)
{
	if (!cfg) return -1;

	/* 先读取已有数据, 相同则跳过写入, 减少 flash 擦写 */
	MqttConfig old;
	if (mqtt_config_load(&old) == 0) {
		if (memcmp(&old, cfg, sizeof(MqttConfig)) == 0) {
			printf_info("mqtt_config_save: data unchanged, skip write\n");
			return 0;
		}
	}

	FILE *fp = fopen(MQTT_CONFIG_SAVE_PATH, "wb");
	if (!fp) {
		printf_info("mqtt_config_save: open %s failed\n", MQTT_CONFIG_SAVE_PATH);
		return -1;
	}

	size_t written = fwrite(cfg, sizeof(MqttConfig), 1, fp);
	fclose(fp);

	if (written != 1) {
		printf_info("mqtt_config_save: write %s failed\n", MQTT_CONFIG_SAVE_PATH);
		return -1;
	}

	printf_info("mqtt_config_save: saved to %s (%zu bytes)\n", MQTT_CONFIG_SAVE_PATH, sizeof(MqttConfig));
	return 0;
}

/*
 * 从 /mnt/UDISK/mqtt_save_config.txt 读取二进制数据, 恢复 MqttConfig。
 * 返回 0 成功, -1 失败。
 */
int mqtt_config_load(MqttConfig *cfg)
{
	if (!cfg) return -1;

	FILE *fp = fopen(MQTT_CONFIG_SAVE_PATH, "rb");
	if (!fp) {
		printf_info("mqtt_config_load: open %s failed\n", MQTT_CONFIG_SAVE_PATH);
		return -1;
	}

	size_t read = fread(cfg, sizeof(MqttConfig), 1, fp);
	fclose(fp);

	if (read != 1) {
		printf_info("mqtt_config_load: read %s failed\n", MQTT_CONFIG_SAVE_PATH);
		memset(cfg, 0, sizeof(MqttConfig));
		return -1;
	}

	printf_info("mqtt_config_load: loaded from %s host=%s port=%d client_id=%s\n",
		MQTT_CONFIG_SAVE_PATH, cfg->host, cfg->port, cfg->client_id);
	return 0;
}

/*
 * 将 BootstrapConfig 以二进制格式保存到 /mnt/UDISK/bootstrap_config.txt。
 * 设备自举成功后持久化 device_sn / ble_id / endpoint / bind_status。
 * 返回 0 成功, -1 失败。
 */
int bootstrap_config_save(BootstrapConfig *cfg)
{
	if (!cfg) return -1;

	/* 先读取已有数据, 相同则跳过写入, 减少 flash 擦写 */
	BootstrapConfig old;
	if (bootstrap_config_load(&old) == 0) {
		if (memcmp(&old, cfg, sizeof(BootstrapConfig)) == 0) {
			printf_info("bootstrap_config_save: data unchanged, skip write\n");
			return 0;
		}
	}

	FILE *fp = fopen(BOOTSTRAP_CONFIG_SAVE_PATH, "wb");
	if (!fp) {
		printf_info("bootstrap_config_save: open %s failed\n", BOOTSTRAP_CONFIG_SAVE_PATH);
		return -1;
	}

	size_t written = fwrite(cfg, sizeof(BootstrapConfig), 1, fp);
	fclose(fp);

	if (written != 1) {
		printf_info("bootstrap_config_save: write %s failed\n", BOOTSTRAP_CONFIG_SAVE_PATH);
		return -1;
	}

	printf_info("bootstrap_config_save: saved to %s (%zu bytes) device_sn=%s bind_status=%s\n",
		BOOTSTRAP_CONFIG_SAVE_PATH, sizeof(BootstrapConfig),
		cfg->device_sn, cfg->bind_status);
	return 0;
}

/*
 * 从 /mnt/UDISK/bootstrap_config.txt 读取二进制数据, 恢复 BootstrapConfig。
 * 返回 0 成功, -1 失败。
 */
int bootstrap_config_load(BootstrapConfig *cfg)
{
	if (!cfg) return -1;

	FILE *fp = fopen(BOOTSTRAP_CONFIG_SAVE_PATH, "rb");
	if (!fp) {
		printf_info("bootstrap_config_load: open %s failed (not bootstrapped yet)\n",
			BOOTSTRAP_CONFIG_SAVE_PATH);
		return -1;
	}

	size_t read = fread(cfg, sizeof(BootstrapConfig), 1, fp);
	fclose(fp);

	if (read != 1) {
		printf_info("bootstrap_config_load: read %s failed\n", BOOTSTRAP_CONFIG_SAVE_PATH);
		memset(cfg, 0, sizeof(BootstrapConfig));
		return -1;
	}

	printf_info("bootstrap_config_load: loaded from %s device_sn=%s ble_id=%s bind_status=%s\n",
		BOOTSTRAP_CONFIG_SAVE_PATH, cfg->device_sn, cfg->ble_id, cfg->bind_status);
	return 0;
}

/*
 * 将 VideoLicenseInfo 以二进制格式保存到 /mnt/UDISK/video_license_config.txt。
 * 返回 0 成功, -1 失败。
 */
int video_license_save(VideoLicenseInfo *info)
{
	if (!info) return -1;

	/* 先读取已有数据, 相同则跳过写入, 减少 flash 擦写 */
	VideoLicenseInfo old;
	if (video_license_load(&old) == 0) {
		if (memcmp(&old, info, sizeof(VideoLicenseInfo)) == 0) {
			printf_info("video_license_save: data unchanged, skip write\n");
			return 0;
		}
	}

	FILE *fp = fopen(VIDEO_LICENSE_SAVE_PATH, "wb");
	if (!fp) {
		printf_info("video_license_save: open %s failed\n", VIDEO_LICENSE_SAVE_PATH);
		return -1;
	}

	size_t written = fwrite(info, sizeof(VideoLicenseInfo), 1, fp);
	fclose(fp);

	if (written != 1) {
		printf_info("video_license_save: write %s failed\n", VIDEO_LICENSE_SAVE_PATH);
		return -1;
	}

	printf_info("video_license_save: saved to %s tange_device_id=%s (%zu bytes)\n",
		VIDEO_LICENSE_SAVE_PATH, info->tange_device_id, sizeof(VideoLicenseInfo));
	return 0;
}

/*
 * 从 /mnt/UDISK/video_license_config.txt 读取二进制数据, 恢复 VideoLicenseInfo。
 * 返回 0 成功, -1 失败。
 */
int video_license_load(VideoLicenseInfo *info)
{
	if (!info) return -1;

	FILE *fp = fopen(VIDEO_LICENSE_SAVE_PATH, "rb");
	if (!fp) {
		printf_info("video_license_load: open %s failed (file not exist, first boot)\n",
			VIDEO_LICENSE_SAVE_PATH);
		return -1;
	}

	size_t read = fread(info, sizeof(VideoLicenseInfo), 1, fp);
	fclose(fp);

	if (read != 1) {
		printf_info("video_license_load: read %s failed\n", VIDEO_LICENSE_SAVE_PATH);
		memset(info, 0, sizeof(VideoLicenseInfo));
		return -1;
	}

	printf_info("video_license_load: loaded from %s tange_device_id=%s provider=%s\n",
		VIDEO_LICENSE_SAVE_PATH, info->tange_device_id, info->provider);
	return 0;
}

/* device_secret 持久化(出厂烧录后保留, 恢复出厂不清除) */
static char g_runtime_device_secret[44];
static int g_runtime_device_secret_loaded = 0;
static void device_secret_invalidate_cache(void);

int device_secret_save(const char *secret)
{
	if (!secret || strlen(secret) != 43) return -1;

	char old[44];
	if (device_secret_load(old, sizeof(old)) == 0 && strcmp(old, secret) == 0) {
		printf_info("device_secret_save: unchanged, skip write\n");
		return 0;
	}

	FILE *fp = fopen(DEVICE_SECRET_SAVE_PATH, "w");
	if (!fp) {
		printf_info("device_secret_save: open %s failed\n", DEVICE_SECRET_SAVE_PATH);
		return -1;
	}
	if (fprintf(fp, "%s", secret) < 0) {
		fclose(fp);
		printf_info("device_secret_save: write %s failed\n", DEVICE_SECRET_SAVE_PATH);
		return -1;
	}
	fclose(fp);
	printf_info("device_secret_save: saved to %s\n", DEVICE_SECRET_SAVE_PATH);
	device_secret_invalidate_cache();
	return 0;
}

int device_secret_load(char *out, size_t out_size)
{
	if (!out || out_size < 44) return -1;

	FILE *fp = fopen(DEVICE_SECRET_SAVE_PATH, "r");
	if (!fp) {
		printf_info("device_secret_load: open %s failed (not burned yet)\n",
			DEVICE_SECRET_SAVE_PATH);
		return -1;
	}

	if (!fgets(out, (int)out_size, fp)) {
		fclose(fp);
		return -1;
	}
	fclose(fp);

	size_t len = strlen(out);
	if (len > 0 && out[len - 1] == '\n') {
		out[len - 1] = '\0';
		len--;
	}
	if (len != 43) {
		printf_info("device_secret_load: invalid length %zu\n", len);
		out[0] = '\0';
		return -1;
	}

	printf_info("device_secret_load: loaded from %s\n", DEVICE_SECRET_SAVE_PATH);
	return 0;
}

static void device_secret_invalidate_cache(void)
{
	g_runtime_device_secret_loaded = 0;
	g_runtime_device_secret[0] = '\0';
}

static const char *get_device_secret(void)
{
	if (!g_runtime_device_secret_loaded) {
		if (device_secret_load(g_runtime_device_secret, sizeof(g_runtime_device_secret)) != 0) {
			strncpy(g_runtime_device_secret, DEVICE_SECRET, sizeof(g_runtime_device_secret) - 1);
		}
		g_runtime_device_secret_loaded = 1;
	}
	return g_runtime_device_secret;
}

/* 构建完整 device.snapshot JSON 并通过 MQTT 发布 */
int snapshot_publish(struct mosquitto *mosq, const SnapshotMessage *snap,
                     const DeviceFullState *state)
{
    if (!mosq || !snap || !state) return -1;

    /* 构建完整 JSON */
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "msg_id", snap->msg_id);
    cJSON_AddStringToObject(root, "msg_type", snap->msg_type);
    cJSON_AddStringToObject(root, "protocol_version", snap->protocol_version);
    cJSON_AddStringToObject(root, "device_sn", snap->device_sn);
    cJSON_AddStringToObject(root, "ble_id", snap->ble_id);
    cJSON_AddStringToObject(root, "product_key", snap->product_key);
    cJSON_AddNumberToObject(root, "timestamp", snap->timestamp);
    cJSON_AddNumberToObject(root, "seq", snap->seq);

    /* 使用 DeviceFullState 构建 data */
    char *state_json = device_full_state_to_json(state);
    if (state_json) {
        cJSON *data_node = cJSON_Parse(state_json);
        free(state_json);

        if (data_node) {
            /* 用 MqttConfig 替换 state.device 节点(动态值) */
            cJSON *state_node = cJSON_GetObjectItem(data_node, "state");
            cJSON *old_device = state_node ? cJSON_GetObjectItem(state_node, "device") : NULL;
            char *device_json = mqtt_config_build_device_json(&g_mqtt_config);
            if (device_json) {
                cJSON *new_device = cJSON_Parse(device_json);
                if (new_device) {
                    if (old_device) {
                        cJSON_ReplaceItemInObject(state_node, "device", new_device);
                    } else {
                        cJSON_AddItemToObject(state_node, "device", new_device);
                    }
                }
                free(device_json);
            }

            /* 用本机网络信息替换 state.network 节点(动态值) */
            cJSON *old_network = state_node ? cJSON_GetObjectItem(state_node, "network") : NULL;
            char *network_json = mqtt_config_build_network_json(&g_mqtt_config);
            if (network_json) {
                cJSON *new_network = cJSON_Parse(network_json);
                if (new_network) {
                    if (old_network) {
                        cJSON_ReplaceItemInObject(state_node, "network", new_network);
                    } else {
                        cJSON_AddItemToObject(state_node, "network", new_network);
                    }
                }
                free(network_json);
            }

            /* 添加 reported_at 时间戳 */
            cJSON_AddNumberToObject(data_node, "reported_at", (double)snap->reported_at);

            cJSON_AddItemToObject(root, "data", data_node);
        } else {
            cJSON_AddObjectToObject(root, "data");
        }
    } else {
        cJSON_AddObjectToObject(root, "data");
    }

    char *full_json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!full_json) return -1;

    /* 发布到 MQTT */
    char topic[128];
    snprintf(topic, sizeof(topic), "/mcqx/device/%s/up", DEVICE_SN);

    int rc = mosquitto_publish(mosq, NULL, topic,
                               (int)strlen(full_json), full_json, 1, false);
    if (rc == MOSQ_ERR_SUCCESS) {
        printf_info("snapshot published: seq=%d len=%zu\n", snap->seq, strlen(full_json));
    } else {
        printf_info("snapshot publish failed: %s\n", mosquitto_strerror(rc));
    }

    free(full_json);
    return rc;
}

/* ============================================================================
 * Bootstrap 结果 TLV 下发串口 — 将自举结果发送到 D5/LCD
 * ============================================================================ */

int bootstrap_result_send_uart(const BootstrapConfig *cfg)
{
	if (!cfg) return -1;

	uint8_t tlv_buf[512];
	int tlv_len;

	/* 填充 BootstrapResultTlv 结构体 */
	BootstrapResultTlv tlv;
	memset(&tlv, 0, sizeof(tlv));
	tlv.struct_type = TLV_STRUCT_BOOTSTRAP_RESULT;   /* 0x6C */
	strncpy(tlv.device_sn, cfg->device_sn, sizeof(tlv.device_sn) - 1);
	strncpy(tlv.ble_id, cfg->ble_id, sizeof(tlv.ble_id) - 1);
	strncpy(tlv.api_endpoint, cfg->api_endpoint, sizeof(tlv.api_endpoint) - 1);
	strncpy(tlv.bind_status, cfg->bind_status, sizeof(tlv.bind_status) - 1);
	strncpy(tlv.qr_url, cfg->qr_url, sizeof(tlv.qr_url) - 1);

	/* TLV 打包 */
	tlv_len = bootstrap_result_tlv_pack(&tlv, tlv_buf, sizeof(tlv_buf));
	if (tlv_len < 0) {
		printf("[BOOTSTRAP_UART] TLV pack failed\n");
		return -1;
	}

	/* 组装完整帧: AA 55 | lenH lenL | cmd | struct_type | seg_lenH seg_lenL | tlv_payload */
	int seg_total = 3 + tlv_len;       /* struct_type(1) + seg_len(2) + payload */
	int total = 5 + seg_total;          /* AA 55 + len(2) + cmd(1) + seg */
	char *msg = (char *)malloc(total);
	if (!msg) {
		printf("[BOOTSTRAP_UART] malloc failed\n");
		return -1;
	}

	msg[0] = 0xAA;
	msg[1] = 0x55;
	msg[2] = (char)((seg_total >> 8) & 0xFF);       /* lenH */
	msg[3] = (char)(seg_total & 0xFF);               /* lenL */
	msg[4] = TLV_CMD_BOOTSTRAP_RESULT;                /* 0x37 */
	msg[5] = (char)TLV_STRUCT_BOOTSTRAP_RESULT;       /* 0x6C */
	msg[6] = (char)((tlv_len >> 8) & 0xFF);           /* seg_len H */
	msg[7] = (char)(tlv_len & 0xFF);                  /* seg_len L */
	memcpy(msg + 8, tlv_buf, tlv_len);

	/* 推入串口队列 */
	if (tiny_queue_push(queue_2_uart, msg) == 0) {
		printf("[BOOTSTRAP_UART] TLV sent: device_sn=%s ble_id=%s bind_status=%s (%d bytes)\n",
			cfg->device_sn, cfg->ble_id, cfg->bind_status, total);
	} else {
		printf("[BOOTSTRAP_UART] tiny_queue_push failed\n");
		free(msg);
		return -1;
	}

	return 0;
}

/* ============================================================================
 * Bootstrap 请求 — /api/device/v1/bootstrap
 * 设备自举: 凭 MAC + device_secret HMAC-SHA256 签名取回身份
 * 保存 device_sn / ble_id / endpoint / bind_status 四个核心状态
 * ============================================================================ */

int http_post_bootstrap(void)
{
    printf("============================================================\n");
    printf("  设备 Bootstrap 自举 — /api/device/v1/bootstrap\n");
    printf("============================================================\n\n");

    /* ---- 1. 获取 MAC 地址 ---- */
    char mac[16] = {0};
    if (get_wifi_mac(mac, sizeof(mac)) != 0) {
        printf("[BOOTSTRAP] 获取 WiFi MAC 失败, 无法自举\n");
        return -1;
    }
    printf("[BOOTSTRAP] MAC: %s\n", mac);

    /* ---- 2. 获取 device_secret ---- */
    const char *device_secret = get_device_secret();
    if (!device_secret || device_secret[0] == '\0') {
        printf("[BOOTSTRAP] device_secret 为空, 无法签名\n");
        return -1;
    }
    printf("[BOOTSTRAP] device_secret: %s\n", device_secret);

    /* ---- 3. 构建 Canonical JSON body(不含 auth) ---- */
    int64_t timestamp;
    char *body_canonical = build_bootstrap_body_canonical(mac, &timestamp);
    if (!body_canonical) {
        printf("[BOOTSTRAP] 构建 canonical body 失败\n");
        return -1;
    }

    printf("--- Step 1: Canonical JSON body ---\n");
    printf("%s\n\n", body_canonical);

    /* ---- 4. 计算 body_hash = SHA-256(Canonical JSON) ---- */
    char body_hash[65];
    sha256_hex(body_canonical, strlen(body_canonical), body_hash);

    printf("--- Step 2: body_hash(SHA-256) ---\n");
    printf("%s\n\n", body_hash);

    /* ---- 5. 生成 nonce ---- */
    char nonce[65];
    generate_nonce(nonce);

    printf("--- Step 3: nonce ---\n");
    printf("%s\n\n", nonce);

    /* ---- 6. 计算 sign_string 和 signature ---- */
    /* ★ 与 hello 的唯一差异: sign_string 第一行用 mac(而非 device_sn) */
    char sign_string[512];
    int sign_len = snprintf(sign_string, sizeof(sign_string),
        "%s\n%lld\n%s\n%s",
        mac, (long long)timestamp, nonce, body_hash);

    printf("--- Step 4: sign_string ---\n");
    printf("%s\n\n", sign_string);

    char signature[65];
    hmac_sha256_hex(
        device_secret, strlen(device_secret),
        sign_string, (size_t)sign_len,
        signature);

    printf("--- Step 5: signature(HMAC-SHA256) ---\n");
    printf("%s\n\n", signature);

    /* ---- 7. 构建完整请求 JSON(含 auth) ---- */
    char *request_json = build_signed_request(body_canonical, nonce, signature);
    free(body_canonical);

    printf("--- Step 6: 完整请求 JSON ---\n");
    printf("%s\n\n", request_json);

    /* ---- 8. 发送 HTTP POST 请求 ---- */
    /* 首次 bootstrap 使用固件内置域名, 后续可使用 first_api_endpoint */
    char bootstrap_url[512];
    snprintf(bootstrap_url, sizeof(bootstrap_url), "%s/api/device/v1/bootstrap", get_api_base());

    printf("--- Step 7: 发送 HTTP POST ---\n");
    printf("URL: %s\n", bootstrap_url);
    printf("----\n");

    char *response = NULL;
    size_t response_len = 0;
    int http_code = http_post_json(bootstrap_url, request_json, &response, &response_len);

    int ret = -1;

    if (http_code > 0) {
        printf("\n--- 响应(%d) ---\n", http_code);
        if (response) {
            printf("%s\n", response);
        }

        if (http_code == 200 && response) {
            printf("\n✓ Bootstrap 成功! 设备已获取身份.\n");

            /* 解析响应 JSON, 填充全局 BootstrapConfig */
            if (bootstrap_config_parse_from_json(response, &g_bootstrap_config) == 0) {
                g_bootstrap_config.bootstrap_done = 1;

                /* 持久化保存 device_sn / ble_id / endpoint / bind_status */
                bootstrap_config_save(&g_bootstrap_config);

                printf("\n--- 保存的核心状态 ---\n");
                printf("  device_sn:   %s\n", g_bootstrap_config.device_sn);
                printf("  ble_id:      %s\n", g_bootstrap_config.ble_id);
                printf("  endpoint:    %s\n", g_bootstrap_config.api_endpoint);
                printf("  bind_status: %s\n", g_bootstrap_config.bind_status);
                printf("  qr_url:      %s\n", g_bootstrap_config.qr_url);

                if (strcmp(g_bootstrap_config.bind_status, "UNBOUND") == 0) {
                    printf("\n  → 设备未绑定, 屏幕显示二维码等待扫码.\n");
                    printf("  → 设备保持每 5~10s 轮询 bootstrap.\n");
                } else if (strcmp(g_bootstrap_config.bind_status, "BINDING") == 0) {
                    printf("\n  → 检测到绑定会话(BINDING), bind_session_id=%s\n",
                        g_bootstrap_config.bind_session_id);
                    printf("  → 设备应立即触发 hello 完成绑定.\n");
                } else if (strcmp(g_bootstrap_config.bind_status, "BOUND") == 0) {
                    printf("\n  → 设备已绑定, 可直接走 hello 获取 MQTT 凭证.\n");
                }

                ret = 0;
            } else {
                printf("\n✗ Bootstrap 响应解析失败.\n");
            }
        } else if (http_code == 401 && response) {
            printf("\n✗ 签名失败: %s\n", response);
            printf("  -> SIGNATURE_INVALID = device_secret 烧错.\n");
            printf("  -> DEVICE_CREDENTIAL_NOT_FOUND = MAC 未在服务端建档.\n");
        } else {
            printf("\n✗ Bootstrap 失败, HTTP %d.\n", http_code);
        }
    } else {
        printf("\n✗ 网络请求失败, 检查 URL 和网络连通性.\n");
    }

    free(request_json);
    free(response);

    printf("\n============================================================\n");
    printf("  Bootstrap 结束\n");
    printf("============================================================\n");
    return ret;
}

/* ============================================================================
 * main — 完整 Hello 流程演示
 * ============================================================================ */

int http_post_hello(void)
{
    printf("============================================================\n");
    printf("  设备 Hello — C 参考实现\n");
    printf("  设备: %s\n", DEVICE_SN);
    printf("============================================================\n\n");

    /* ---- 1. 构建 Canonical JSON body(不含 auth) ---- */
    int64_t timestamp;
    char *body_canonical = build_hello_body_canonical(&timestamp);

    printf("--- Step 1: Canonical JSON body(不含 auth) ---\n");
    printf("%s\n\n", body_canonical);

    /* ---- 2. 计算 body_hash = SHA-256(Canonical JSON) ---- */
    char body_hash[65];
    sha256_hex(body_canonical, strlen(body_canonical), body_hash);

    printf("--- Step 2: body_hash(SHA-256) ---\n");
    printf("%s\n\n", body_hash);

    /* ---- 3. 生成 nonce ---- */
    char nonce[65];
    generate_nonce(nonce);

    printf("--- Step 3: nonce(32 hex chars) ---\n");
    printf("%s\n\n", nonce);

    /* ---- 4. 计算 sign_string 和 signature ---- */
    /*
     * sign_string = device_sn + "\n" + timestamp + "\n" + nonce + "\n" + body_hash
     */
    char sign_string[512];
    int sign_len = snprintf(sign_string, sizeof(sign_string),
        "%s\n%lld\n%s\n%s",
        DEVICE_SN, (long long)timestamp, nonce, body_hash);

    printf("--- Step 4: sign_string ---\n");
    printf("%s\n\n", sign_string);

    char signature[65];
    hmac_sha256_hex(
        DEVICE_SECRET, strlen(DEVICE_SECRET),
        sign_string, (size_t)sign_len,
        signature);

    printf("--- Step 5: signature(HMAC-SHA256) ---\n");
    printf("%s\n\n", signature);

    /* ---- 5. 构建完整请求 JSON(含 auth) ---- */
    char *request_json = build_signed_request(body_canonical, nonce, signature);
    free(body_canonical);

    printf("--- Step 6: 完整请求 JSON ---\n");
    printf("%s\n\n", request_json);

    /* ---- 6. 发送 HTTP POST 请求 ---- */
    char hello_url[512];
    snprintf(hello_url, sizeof(hello_url), "%s/api/device/v1/hello", get_api_base());

    printf("--- Step 7: 发送 HTTP POST ---\n");
    printf("URL: %s\n", hello_url);
    printf("----\n");

    char *response = NULL;
    size_t response_len = 0;
    int http_code = http_post_json(hello_url, request_json, &response, &response_len);

    if (http_code > 0) {
        printf("\n--- 响应(%d) ---\n", http_code);
        if (response) {
            printf("%s\n", response);
        }

        if (http_code == 200 && response) {
            printf("\n✓ Hello 成功! 设备可用响应中的 mqtt 信息连接 MQTT.\n");

            /* 解析响应 JSON, 填充全局 MQTT 配置 */
            mqtt_config_parse_from_json(response, &g_mqtt_config);
            g_mqtt_config.hello_getnew_flag = 1;
            device_full_state_from_mqtt_config(&g_mqtt_config, &g_device_state);

         
        } else if (http_code == 409 && response) {
            printf("\n✗ 业务拒绝: %s\n", response);
            printf("  -> 签名校验已通过,非 SIGNATURE_INVALID 即说明 HMAC 正确.\n");
            printf("  -> 若为 BIND_SESSION_NOT_FOUND,需先用 App 创建绑定会话.\n");
            printf("  -> 若为 DEVICE_NOT_BOUND,需携带有效 bind_session_id 重新 hello.\n");
        } else {
            printf("\n✗ Hello 失败,检查 device_sn/secret 是否匹配,以及绑定会话是否有效.\n");
        }
    } else {
        printf("\n✗ 网络请求失败,检查 URL 和网络连通性.\n");
    }

    free(request_json);
    free(response);

    printf("\n============================================================\n");
    printf("  结束\n");
    printf("============================================================\n");
    return 0;
}

/* ============================================================================
 * 视频 License 请求 — /api/device/v1/video/tange_tirtc/license
 * ============================================================================ */

/**
 * 构造 Video License 请求(不含 auth), 返回 Canonical JSON 字符串和 timestamp。
 * 调用者负责 free()。
 */
static char *build_video_license_body_canonical(const char *session_id, const char *provider, int64_t *out_timestamp)
{
    int64_t ts = (int64_t)time(NULL);
    *out_timestamp = ts;

    char session_id_fragment[128];
    if (!session_id || session_id[0] == '\0') {
        snprintf(session_id_fragment, sizeof(session_id_fragment), "\"session_id\":null");
    } else {
        snprintf(session_id_fragment, sizeof(session_id_fragment), "\"session_id\":\"%s\"", session_id);
    }

    const char *prov = (provider && provider[0]) ? provider : "TANGE";

    /*
     * Canonical JSON key 升序规则:
     *   ble_id < data < device_sn < msg_id < msg_type < product_key < protocol_version < seq < timestamp
     * 内部 data 节点: provider < session_id ('p' < 's')
     */
    const char *fmt =
        "{"
          "\"ble_id\":\"%s\","
          "\"data\":{"
            "\"provider\":\"%s\","
            "%s"
          "},"
          "\"device_sn\":\"%s\","
          "\"msg_id\":\"%s\","
          "\"msg_type\":\"device.video_license\","
          "\"product_key\":\"%s\","
          "\"protocol_version\":\"%s\","
          "\"seq\":1,"
          "\"timestamp\":%lld"
        "}";

    char msg_id[128];
    snprintf(msg_id, sizeof(msg_id), "LIC-%s-%lld", DEVICE_SN, (long long)ts);

    int len = snprintf(NULL, 0, fmt,
        BLE_ID,
        prov,
        session_id_fragment,
        DEVICE_SN,
        msg_id,
        PRODUCT_KEY,
        PROTOCOL_VER,
        (long long)ts
    );

    if (len < 0) return NULL;

    char *json = (char *)malloc((size_t)len + 1);
    if (!json) return NULL;
    snprintf(json, (size_t)len + 1, fmt,
        BLE_ID,
        prov,
        session_id_fragment,
        DEVICE_SN,
        msg_id,
        PRODUCT_KEY,
        PROTOCOL_VER,
        (long long)ts
    );

    return json;
}

/**
 * 发送 /api/device/v1/video/tange_tirtc/license 请求并解析响应中的 license 信息
 */
int http_post_video_license(const char *session_id, const char *provider, VideoLicenseInfo *out_info)
{
    printf("============================================================\n");
    printf("  设备 Video License 请求 — /api/device/v1/video/tange_tirtc/license\n");
    printf("  设备: %s\n", DEVICE_SN);
    printf("============================================================\n\n");

    int64_t timestamp;
    char *body_canonical = build_video_license_body_canonical(session_id, provider, &timestamp);
    if (!body_canonical) {
        printf("[VIDEO_LICENSE] Build canonical body failed\n");
        return -1;
    }

    /* SHA-256 body_hash */
    char body_hash[65];
    sha256_hex(body_canonical, strlen(body_canonical), body_hash);

    /* 随机 nonce */
    char nonce[65];
    generate_nonce(nonce);

    /* 拼接 sign_string = device_sn \n timestamp \n nonce \n body_hash */
    char sign_string[512];
    int sign_len = snprintf(sign_string, sizeof(sign_string),
        "%s\n%lld\n%s\n%s",
        DEVICE_SN, (long long)timestamp, nonce, body_hash);

    /* HMAC-SHA256 签名 */
    char signature[65];
    hmac_sha256_hex(
        DEVICE_SECRET, strlen(DEVICE_SECRET),
        sign_string, (size_t)sign_len,
        signature);

    /* 组装包含 auth 的完整请求 JSON */
    char *request_json = build_signed_request(body_canonical, nonce, signature);
    free(body_canonical);
    if (!request_json) {
        printf("[VIDEO_LICENSE] Build signed request failed\n");
        return -1;
    }

    char url[512];
    snprintf(url, sizeof(url), "%s/api/device/v1/video/tange_tirtc/license", get_api_base());

    printf("[VIDEO_LICENSE] Request URL: %s\n", url);
    printf("[VIDEO_LICENSE] Request Payload:\n%s\n\n", request_json);

    char *response = NULL;
    size_t response_len = 0;
    int http_code = http_post_json(url, request_json, &response, &response_len);
    free(request_json);

    int ret = -1;
    if (http_code == 200 && response) {
        printf("[VIDEO_LICENSE] Response (200 OK):\n%s\n\n", response);

        cJSON *root = cJSON_Parse(response);
        if (root) {
            cJSON *code = cJSON_GetObjectItem(root, "code");
            if (code && code->valueint == 0) {
                cJSON *data = cJSON_GetObjectItem(root, "data");
                if (data) {
                    if (out_info) {
                        memset(out_info, 0, sizeof(VideoLicenseInfo));

                        cJSON *item = cJSON_GetObjectItem(data, "provider");
                        if (cJSON_IsString(item)) strncpy(out_info->provider, item->valuestring, sizeof(out_info->provider) - 1);

                        item = cJSON_GetObjectItem(data, "license");
                        if (cJSON_IsString(item)) strncpy(out_info->license, item->valuestring, sizeof(out_info->license) - 1);

                        item = cJSON_GetObjectItem(data, "tange_device_id");
                        if (!item) item = cJSON_GetObjectItem(data, "device_id");
                        if (cJSON_IsString(item)) strncpy(out_info->tange_device_id, item->valuestring, sizeof(out_info->tange_device_id) - 1);

                        item = cJSON_GetObjectItem(data, "tange_device_key");
                        if (!item) item = cJSON_GetObjectItem(data, "device_secret_key");
                        if (cJSON_IsString(item)) strncpy(out_info->tange_device_key, item->valuestring, sizeof(out_info->tange_device_key) - 1);

                        item = cJSON_GetObjectItem(data, "expire_at");
                        if (cJSON_IsNumber(item)) out_info->expire_at = (int64_t)item->valuedouble;
                    }
                    printf("[VIDEO_LICENSE] Successfully parsed video license info\n");
                    out_info->fetched = 1;
                    ret = 0;
                }
            } else {
                cJSON *msg = cJSON_GetObjectItem(root, "message");
                printf("[VIDEO_LICENSE] API returned code %d: %s\n",
                    code ? code->valueint : -1,
                    msg && cJSON_IsString(msg) ? msg->valuestring : "Unknown error");
            }
            cJSON_Delete(root);
        }
    } else {
        printf("[VIDEO_LICENSE] HTTP POST failed with code %d\n", http_code);
    }

    free(response);
    return ret;
}

/* ============================================================================
 * 工厂设备上报 — /api/device/v1/factory-devices
 * 认证: Bearer FACTORY_API_KEY(非设备 HMAC 签名)
 * ============================================================================ */

static char *build_factory_devices_body(const FactoryDeviceSubmissionRequest *req)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) return NULL;

    cJSON_AddStringToObject(root, "submission_batch_id", req->submission_batch_id);

    cJSON *arr = cJSON_CreateArray();
    if (!arr) {
        cJSON_Delete(root);
        return NULL;
    }

    for (int i = 0; i < req->device_count; i++) {
        cJSON *item = cJSON_CreateObject();
        if (!item) continue;

        cJSON_AddStringToObject(item, "mac", req->devices[i].mac);
        cJSON_AddStringToObject(item, "sales_country_code", req->devices[i].sales_country_code);
        cJSON_AddStringToObject(item, "device_secret", req->devices[i].device_secret);
        cJSON_AddStringToObject(item, "product_key", req->devices[i].product_key);
        cJSON_AddStringToObject(item, "model", req->devices[i].model);
        cJSON_AddStringToObject(item, "hardware_version", req->devices[i].hardware_version);
        if (req->devices[i].firmware_version[0] != '\0') {
            cJSON_AddStringToObject(item, "firmware_version", req->devices[i].firmware_version);
        }
        if (req->devices[i].camera_vendor[0] != '\0') {
            cJSON_AddStringToObject(item, "camera_vendor", req->devices[i].camera_vendor);
        }
        cJSON_AddItemToArray(arr, item);
    }

    cJSON_AddItemToObject(root, "devices", arr);
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

static int parse_factory_devices_response(const char *response,
                                          FactoryDeviceSubmissionResult *out_result)
{
    if (!response || !out_result) return -1;

    memset(out_result, 0, sizeof(*out_result));

    cJSON *root = cJSON_Parse(response);
    if (!root) return -1;

    cJSON *code = cJSON_GetObjectItem(root, "code");
    if (!code || code->valueint != 0) {
        cJSON *msg = cJSON_GetObjectItem(root, "message");
        printf("[FACTORY_DEVICES] API returned code %d: %s\n",
            code ? code->valueint : -1,
            msg && cJSON_IsString(msg) ? msg->valuestring : "Unknown error");
        cJSON_Delete(root);
        return -1;
    }

    cJSON *data = cJSON_GetObjectItem(root, "data");
    if (!data) {
        cJSON_Delete(root);
        return -1;
    }

    cJSON *item = cJSON_GetObjectItem(data, "submission_batch_id");
    if (cJSON_IsString(item)) {
        strncpy(out_result->submission_batch_id, item->valuestring,
            sizeof(out_result->submission_batch_id) - 1);
    }

    item = cJSON_GetObjectItem(data, "total");
    if (cJSON_IsNumber(item)) out_result->total = item->valueint;

    item = cJSON_GetObjectItem(data, "accepted");
    if (cJSON_IsNumber(item)) out_result->accepted = item->valueint;

    item = cJSON_GetObjectItem(data, "rejected");
    if (cJSON_IsNumber(item)) out_result->rejected = item->valueint;

    cJSON *rejections = cJSON_GetObjectItem(data, "rejections");
    if (cJSON_IsArray(rejections)) {
        int n = cJSON_GetArraySize(rejections);
        for (int i = 0; i < n && i < FACTORY_DEVICE_MAX_REJECTIONS; i++) {
            cJSON *rej = cJSON_GetArrayItem(rejections, i);
            if (!rej) continue;

            cJSON *mac = cJSON_GetObjectItem(rej, "mac");
            cJSON *reason = cJSON_GetObjectItem(rej, "reason");
            if (cJSON_IsString(mac)) {
                strncpy(out_result->rejections[i].mac, mac->valuestring,
                    sizeof(out_result->rejections[i].mac) - 1);
            }
            if (cJSON_IsString(reason)) {
                strncpy(out_result->rejections[i].reason, reason->valuestring,
                    sizeof(out_result->rejections[i].reason) - 1);
            }
            out_result->rejection_count++;
        }
    }

    cJSON_Delete(root);
    return 0;
}

int http_post_factory_devices(const char *factory_api_key,
                              const FactoryDeviceSubmissionRequest *req,
                              FactoryDeviceSubmissionResult *out_result)
{
    if (!factory_api_key || factory_api_key[0] == '\0') {
        printf("[FACTORY_DEVICES] factory_api_key is empty\n");
        return -1;
    }
    if (!req || req->submission_batch_id[0] == '\0') {
        printf("[FACTORY_DEVICES] submission_batch_id is empty\n");
        return -1;
    }
    if (!req->devices || req->device_count != 1) {
        printf("[FACTORY_DEVICES] device_count must be 1, got %d\n", req->device_count);
        return -1;
    }

    printf("============================================================\n");
    printf("  工厂设备上报 — /api/device/v1/factory-devices\n");
    printf("  批次: %s, 设备数: %d\n", req->submission_batch_id, req->device_count);
    printf("============================================================\n\n");

    char *request_json = build_factory_devices_body(req);
    if (!request_json) {
        printf("[FACTORY_DEVICES] Build request body failed\n");
        return -1;
    }

    char url[512];
    snprintf(url, sizeof(url), "%s/api/device/v1/factory-devices", get_api_base());

    printf("[FACTORY_DEVICES] Request URL: %s\n", url);
    printf("[FACTORY_DEVICES] Request Payload:\n%s\n\n", request_json);

    char *response = NULL;
    size_t response_len = 0;
    int http_code = http_post_json_bearer(url, request_json, factory_api_key, &response, &response_len);
    free(request_json);

    int ret = -1;
    if (http_code == 200 && response) {
        printf("[FACTORY_DEVICES] Response (200 OK):\n%s\n\n", response);
        FactoryDeviceSubmissionResult local_result;
        FactoryDeviceSubmissionResult *result = out_result ? out_result : &local_result;
        if (parse_factory_devices_response(response, result) == 0) {
            printf("[FACTORY_DEVICES] accepted=%d rejected=%d\n",
                result->accepted, result->rejected);
            if (result->rejection_count > 0) {
                for (int i = 0; i < result->rejection_count; i++) {
                    printf("[FACTORY_DEVICES] rejection mac=%s reason=%s\n",
                        result->rejections[i].mac,
                        result->rejections[i].reason);
                }
            }
            ret = 0;
        }
    } else {
        printf("[FACTORY_DEVICES] HTTP POST failed with code %d\n", http_code);
        if (response) printf("[FACTORY_DEVICES] Response: %s\n", response);
    }

    free(response);
    return ret;
}

int http_post_factory_device_self(const char *factory_api_key,
                                  const char *mac,
                                  const char *sales_country_code,
                                  FactoryDeviceSubmissionResult *out_result)
{
    if (!mac || mac[0] == '\0') {
        printf("[FACTORY_DEVICES] mac is empty\n");
        return -1;
    }

    const char *api_key = factory_api_key;
    if (!api_key || api_key[0] == '\0') {
        api_key = FACTORY_API_KEY;
    }
    if (!api_key || api_key[0] == '\0') {
        printf("[FACTORY_DEVICES] factory_api_key is empty, set FACTORY_API_KEY macro\n");
        return -1;
    }

    const char *country = (sales_country_code && sales_country_code[0])
        ? sales_country_code : "CN";

    FactoryDeviceItem item;
    memset(&item, 0, sizeof(item));
    strncpy(item.mac, mac, sizeof(item.mac) - 1);
    strncpy(item.sales_country_code, country, sizeof(item.sales_country_code) - 1);
    strncpy(item.device_secret, DEVICE_SECRET, sizeof(item.device_secret) - 1);
    strncpy(item.product_key, PRODUCT_KEY, sizeof(item.product_key) - 1);
    strncpy(item.model, MODEL, sizeof(item.model) - 1);
    strncpy(item.hardware_version, HW_VERSION, sizeof(item.hardware_version) - 1);
    strncpy(item.firmware_version, FW_VERSION, sizeof(item.firmware_version) - 1);
    strncpy(item.camera_vendor, "tange_tirtc", sizeof(item.camera_vendor) - 1);

    char batch_id[72];
    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    snprintf(batch_id, sizeof(batch_id), "FACTORY-%04d%02d%02d-001",
        tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday);

    FactoryDeviceSubmissionRequest req;
    memset(&req, 0, sizeof(req));
    strncpy(req.submission_batch_id, batch_id, sizeof(req.submission_batch_id) - 1);
    req.devices = &item;
    req.device_count = 1;

    return http_post_factory_devices(api_key, &req, out_result);
}

/* ============================================================================
 * 工厂设备 TLV 处理 — 从 TLV 数据接收设备信息并上报云端
 * mac 由本机 WiFi MAC 地址填充(12 位大写 hex), 其余字段从 TLV 数据获取
 * 由 tlv_worker 线程通过 TlvProcessor.handler 回调调用
 * ============================================================================ */

/* 将工厂设备上报被拒结果打包为 TLV (0x36 命令字) 发送到串口
 * 消息格式: AA 55 | lenH lenL | 0x36 | struct_type(0x6B) seg_len | tlv_payload
 * 无被拒记录时不发送, 返回 0 */
int factory_device_result_send_uart(const FactoryDeviceSubmissionResult *result)
{
    uint8_t tlv_buf[384];
    int tlv_len;

    if (!result) return -1;
    if (result->rejection_count <= 0) return 0;

    for (int i = 0; i < result->rejection_count; i++) {
        FactoryDeviceRejectionTlv tlv;
        memset(&tlv, 0, sizeof(tlv));
        tlv.struct_type = TLV_STRUCT_FACTORY_REJECTION;
        strncpy(tlv.mac, result->rejections[i].mac, sizeof(tlv.mac) - 1);
        strncpy(tlv.reason, result->rejections[i].reason, sizeof(tlv.reason) - 1);

        tlv_len = factory_device_rejection_tlv_pack(&tlv, tlv_buf, sizeof(tlv_buf));
        if (tlv_len < 0) {
            printf("[FACTORY_DEVICES] factory_device_rejection_tlv_pack failed\n");
            continue;
        }

        /* 结构体段 = 1B type + 2B seg_len + tlv_data */
        int seg_total = 3 + tlv_len;
        /* 总长度 = 5B header + 结构体段 */
        int total = 5 + seg_total;
        char *msg = (char *)malloc(total);
        if (!msg) continue;

        msg[0] = 0xAA;
        msg[1] = 0x55;
        msg[2] = (char)((seg_total >> 8) & 0xFF);
        msg[3] = (char)(seg_total & 0xFF);
        msg[4] = TLV_CMD_FACTORY_DEVICE;          /* 命令字 0x36 */
        msg[5] = (char)TLV_STRUCT_FACTORY_REJECTION; /* struct_type 0x6B */
        msg[6] = (char)((tlv_len >> 8) & 0xFF);
        msg[7] = (char)(tlv_len & 0xFF);
        memcpy(msg + 8, tlv_buf, tlv_len);

        printf("[FACTORY_DEVICES] send rejection to uart: mac=%s reason=%s tlv_len=%d total=%d\n",
               result->rejections[i].mac, result->rejections[i].reason, tlv_len, total);

        if (tiny_queue_push(queue_2_uart, msg) == 0) {
            printf_info("[FACTORY_DEVICES] queue_2_uart push ok\n");
        } else {
            printf_info("[FACTORY_DEVICES] queue_2_uart push fail\n");
            free(msg);
        }
    }

    return 0;
}

/* 获取本机 WiFi MAC 地址 (12 位大写 hex, 如 "28AD3E13D940")
 * 读取 /sys/class/net/wlan0/address, 去除冒号并转大写
 * 返回 0 成功, -1 失败 */
static int get_wifi_mac(char *out, size_t out_size)
{
    FILE *fp;
    char raw[32] = {0};
    int i, j;

    if (!out || out_size < 13) return -1;

    fp = fopen("/sys/class/net/wlan0/address", "r");
    if (!fp) {
        printf("[FACTORY_DEVICES] cannot open /sys/class/net/wlan0/address\n");
        return -1;
    }

    if (!fgets(raw, sizeof(raw), fp)) {
        fclose(fp);
        return -1;
    }
    fclose(fp);

    /* 去除冒号、换行符, 转大写: "28:ad:3e:13:d9:40\n" → "28AD3E13D940" */
    j = 0;
    memset(out, 0, out_size);
    for (i = 0; raw[i] && j < (int)out_size - 1; i++) {
        if (raw[i] == ':' || raw[i] == '\n' || raw[i] == '\r') continue;
        if (raw[i] >= 'a' && raw[i] <= 'f')
            out[j++] = (char)(raw[i] - 'a' + 'A');
        else
            out[j++] = raw[i];
    }

    if (j != 12) {
        printf("[FACTORY_DEVICES] invalid MAC length %d: %s\n", j, out);
        return -1;
    }

    printf("[FACTORY_DEVICES] WiFi MAC: %s\n", out);
    return 0;
}

int handle_factory_device_tlv(const FactoryDeviceTlv *tlv)
{
    if (!tlv) {
        printf("[FACTORY_DEVICES] handle_factory_device_tlv: tlv is NULL\n");
        return -1;
    }

    printf("============================================================\n");
    printf("  工厂设备 TLV 处理 — 上报云端\n");
    printf("============================================================\n\n");

    FactoryDeviceItem item;
    memset(&item, 0, sizeof(item));

    /* mac 从本机 WiFi 获取 (12 位大写 hex) */
    if (get_wifi_mac(item.mac, sizeof(item.mac)) != 0) {
        printf("[FACTORY_DEVICES] get_wifi_mac failed\n");
        return -1;
    }
    strncpy(item.sales_country_code, tlv->sales_country_code, sizeof(item.sales_country_code) - 1);

    /* device_secret 由内部生成, 不从 TLV 接收 */
    if (generate_device_secret(item.device_secret, sizeof(item.device_secret)) != 0) {
        printf("[FACTORY_DEVICES] generate_device_secret failed\n");
        return -1;
    }

    strncpy(item.product_key, tlv->product_key, sizeof(item.product_key) - 1);
    strncpy(item.model, tlv->model, sizeof(item.model) - 1);
    strncpy(item.hardware_version, tlv->hardware_version, sizeof(item.hardware_version) - 1);
    strncpy(item.firmware_version, tlv->firmware_version, sizeof(item.firmware_version) - 1);
    strncpy(item.camera_vendor, "tange_tirtc", sizeof(item.camera_vendor) - 1);

    char batch_id[72];
    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    snprintf(batch_id, sizeof(batch_id), "FACTORY-%04d%02d%02d-001",
        tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday);

    FactoryDeviceSubmissionRequest req;
    memset(&req, 0, sizeof(req));
    strncpy(req.submission_batch_id, batch_id, sizeof(req.submission_batch_id) - 1);
    req.devices = &item;
    req.device_count = 1;

    FactoryDeviceSubmissionResult result;
    int ret = http_post_factory_devices(FACTORY_API_KEY, &req, &result);

    /* 被拒结果通过 TLV (0x36) 下发到串口, 通知 D5/LCD */
    factory_device_result_send_uart(&result);

    return ret;
}


/**
 * generate_device_secret.c
 *
 * 设备认证密钥生成 —— 与服务端完全一致。
 * 32 字节密码学随机数 → Base64URL 编码（无填充）→ 43 字符 ASCII。
 *
 * ── 集成方式 ──
 *
 * 将本文件加入工程编译，直接调用唯一公共函数：
 *
 *   // 声明（或写在配套 .h 中）
 *   int generate_device_secret(char *out, size_t out_size);
 *
 *   char secret[44];
 *   if (generate_device_secret(secret, sizeof(secret)) != 0) {
 *       // 随机数生成失败，中止烧录
 *   }
 *   // secret = "k8H2mPxVqR7wY3nB5dL9sA1cF4gJ6tN0oQ2uW5yZ8xI"  (43 字符)
 *   // 写入 NVS / 安全存储
 *
 * ── 平台适配 ──
 *
 *   ESP32：      自动使用 esp_fill_random（硬件 TRNG），无需额外配置。
 *                ⚠️ 调用前确保 RF/PHY 已初始化（WiFi 启动后），否则退化为伪随机。
 *   Linux：      自动使用 /dev/urandom。
 *   裸机 / 其它：需由调用方实现 platform_random_bytes()，签名见 fill_random()。
 *
 * ── HMAC 签名使用 ──
 *
 *   sign_string = device_sn + "\n" + timestamp + "\n" + nonce + "\n" + body_hash
 *   mbedtls_md_hmac(md_sha256, key = secret, key_len = 43, sign_string) → 64 位 hex
 *
 *   ⚠️ 密钥是 43 字符 ASCII（strlen = 43），不是 32 字节原始随机数。
 *   服务端使用 hash_hmac('sha256', sign_string, secret) 验签，格式一致。
 *
 * ── 注意事项 ──
 *
 *   1. 密钥写入 NVS 后，栈上缓冲区不建议打印到日志。
 *   2. 本文件所有内部函数均为 static，不污染全局符号表。
 *   3. 恢复出厂时保留 device_secret，清除 Wi-Fi / MQTT 凭证即可。
 */

#include <stdint.h>

#if defined(ESP_PLATFORM)
  #include "esp_random.h"
#elif defined(__linux__)
  #include <stdio.h>
#endif

/* ── 平台适配：密码学安全随机数 ──────────────────────────────────── */

/*
 * 裸机 / 无 OS 平台：调用方必须自行实现 platform_random_bytes（返回 0 成功，非 0 失败）。
 * ESP32 / Linux 分支由下方 fill_random() 内部处理，无需调用方提供。
 */
#if !defined(ESP_PLATFORM) && !defined(__linux__)
  extern int platform_random_bytes(uint8_t *buf, size_t len);
#endif

/**
 * 用平台密码学随机源填充 buf。
 *
 * 平台映射：
 *   - ESP32 (ESP_PLATFORM) : esp_fill_random（硬件 TRNG，内部已做健康检查）
 *   - Linux  (__linux__)   : /dev/urandom
 *   - 裸机 / 其它           : platform_random_bytes（调用方实现）
 *
 * @return 0 成功，-1 失败（调用方必须检查，失败时 buf 内容不可用）
 */
static int fill_random(uint8_t *buf, size_t len)
{
    if (buf == NULL || len == 0) {
        return -1;
    }

#if defined(ESP_PLATFORM)
    esp_fill_random(buf, len);
    return 0;
#elif defined(__linux__)
    FILE *f = fopen("/dev/urandom", "rb");
    if (f == NULL) {
        return -1;
    }
    size_t got = fread(buf, 1, len, f);
    fclose(f);                 /* 先关闭句柄，再判断读取结果，避免 FILE* 泄漏 */
    return (got == len) ? 0 : -1;
#else
    return platform_random_bytes(buf, len);
#endif
}

/**
 * 安全清零内存，防止编译器把 memset 优化掉。
 * 用于抹除栈上的密钥原始材料。
 */
static void secure_zero(void *ptr, size_t len)
{
    if (ptr == NULL) {
        return;
    }
    volatile uint8_t *p = (volatile uint8_t *) ptr;
    while (len--) {
        *p++ = 0;
    }
}

/* ── Base64URL 编码表 ────────────────────────────────────────────── */

/*
 * 标准 Base64:  A-Z a-z 0-9 + /
 * Base64URL:    A-Z a-z 0-9 - _
 *
 * 区别仅在最后两个字符：'+' → '-' , '/' → '_'
 */
static const char BASE64URL_ALPHABET[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789"
    "-_";  /* 注意：最后两个不是 '+/' */

/* ── 前置声明 ────────────────────────────────────────────────────── */

static size_t base64url_encode(
    const uint8_t *src,
    size_t src_len,
    char *dst,
    size_t dst_size);

/* ── 公共接口 ────────────────────────────────────────────────────── */

/**
 * 生成设备认证密钥，与服务端逻辑完全一致。
 *
 * @param out         输出缓冲区，至少 44 字节（43 字符 + '\0'）
 * @param out_size    输出缓冲区大小
 * @return            0 成功，-1 失败
 *
 * 生成的密钥特征：
 *   - 字符集：[A-Za-z0-9-_]
 *   - 长度：  43 字符（不含 '\0'）
 *   - 熵源：  32 字节（256 bit）平台密码学随机数
 *   - 编码：  Base64URL，无尾部填充 '='
 */
int generate_device_secret(char *out, size_t out_size)
{
    uint8_t raw[32];
    size_t encoded_len;

    if (out == NULL || out_size < 44) {
        return -1;
    }

    /* 步骤 1：生成 32 字节密码学安全随机数（失败必须中止，绝不能用未初始化内存生成密钥） */
    if (fill_random(raw, sizeof(raw)) != 0) {
        return -1;
    }

    /* 步骤 2：Base64URL 编码（无填充） */
    encoded_len = base64url_encode(raw, sizeof(raw), out, out_size);
    secure_zero(raw, sizeof(raw));          /* 抹除原始随机材料，缩小秘密暴露面 */
    if (encoded_len != 43) {
        return -1;
    }

    out[encoded_len] = '\0';
    return 0;
}

/* ── Base64URL 编码实现（无外部依赖） ────────────────────────────── */

/**
 * 将二进制数据编码为 Base64URL（无填充）。
 *
 * 与标准 Base64 的区别：
 *   1. 字母表最后两个字符：'+' → '-' , '/' → '_'
 *   2. 不输出末尾 '=' 填充
 *
 * @param src         原始二进制数据
 * @param src_len     原始数据长度
 * @param dst         输出缓冲区
 * @param dst_size    输出缓冲区大小
 * @return            编码后的字符数（不含 '\0'），失败返回 0
 */
static size_t base64url_encode(
    const uint8_t *src,
    size_t src_len,
    char *dst,
    size_t dst_size)
{
    size_t i, j;
    uint32_t triple;
    size_t out_len;

    if (src == NULL || dst == NULL) {
        return 0;
    }

    /* 计算输出长度：ceil(src_len * 8 / 6)，不含填充 */
    out_len = ((src_len * 8) + 5) / 6;
    if (out_len + 1 > dst_size) {
        return 0;
    }

    /* 按 3 字节一组处理 */
    for (i = 0, j = 0; i + 2 < src_len; i += 3, j += 4) {
        triple = ((uint32_t)src[i]     << 16)
               | ((uint32_t)src[i + 1] << 8)
               |  (uint32_t)src[i + 2];

        dst[j]     = BASE64URL_ALPHABET[(triple >> 18) & 0x3F];
        dst[j + 1] = BASE64URL_ALPHABET[(triple >> 12) & 0x3F];
        dst[j + 2] = BASE64URL_ALPHABET[(triple >> 6)  & 0x3F];
        dst[j + 3] = BASE64URL_ALPHABET[ triple        & 0x3F];
    }

    /* 处理末尾不足 3 字节的情况 */
    size_t remaining = src_len - i;
    if (remaining == 1) {
        triple = (uint32_t)src[i] << 16;
        dst[j]     = BASE64URL_ALPHABET[(triple >> 18) & 0x3F];
        dst[j + 1] = BASE64URL_ALPHABET[(triple >> 12) & 0x3F];
        /* 不输出 '=' 填充 */
        j += 2;
    } else if (remaining == 2) {
        triple = ((uint32_t)src[i]     << 16)
               | ((uint32_t)src[i + 1] << 8);
        dst[j]     = BASE64URL_ALPHABET[(triple >> 18) & 0x3F];
        dst[j + 1] = BASE64URL_ALPHABET[(triple >> 12) & 0x3F];
        dst[j + 2] = BASE64URL_ALPHABET[(triple >> 6)  & 0x3F];
        /* 不输出 '=' 填充 */
        j += 3;
    }

    return j;
}

/* ============================================================================
 * 设备上测试工厂设备上报 API
 *
 * 在设备(V851s)上直接调用此函数, 验证 /api/device/v1/factory-devices 接口:
 *   1. 获取本机 WiFi MAC 地址
 *   2. 生成或加载 device_secret
 *   3. 构造请求体并发送 HTTP POST
 *   4. 打印详细结果
 *
 * 调用方式(在 dwin_nettplayer.c 或 main 中合适位置):
 *   test_factory_device_self(NULL);           // 使用宏 FACTORY_API_KEY
 *   test_factory_device_self("my-api-key");  // 显式传入
 * ============================================================================ */
int test_factory_device_self(const char *factory_api_key)
{
    printf("\n");
    printf("============================================================\n");
    printf("  设备测试 — 工厂设备上报 API\n");
    printf("  端点: POST /api/device/v1/factory-devices\n");
    printf("============================================================\n\n");

    /* ---- Step 1: 获取 WiFi MAC 地址 ---- */
    printf("--- Step 1: 获取本机 WiFi MAC 地址 ---\n");
    char mac[16] = {0};
    if (get_wifi_mac(mac, sizeof(mac)) != 0) {
        printf("  [失败] 无法获取 WiFi MAC 地址\n");
        printf("  [提示] 检查 /sys/class/net/wlan0/address 是否存在\n");
        return -1;
    }
    printf("  MAC: %s\n\n", mac);

    /* ---- Step 2: 加载或生成 device_secret ---- */
    printf("--- Step 2: 加载/生成 device_secret ---\n");
    char secret[64] = {0};

    /* 优先加载已落盘的 device_secret(产测已烧录时复用) */
    if (device_secret_load(secret, sizeof(secret)) == 0 && secret[0] != '\0') {
        printf("  device_secret: %s (从 %s 加载)\n", secret, DEVICE_SECRET_SAVE_PATH);
    } else {
        /* 未烧录, 生成新的 device_secret */
        printf("  [信息] 未找到已保存的 device_secret, 生成新的...\n");
        if (generate_device_secret(secret, sizeof(secret)) != 0) {
            printf("  [失败] 生成 device_secret 失败\n");
            return -1;
        }
        printf("  device_secret: %s (新生成)\n", secret);

        /* 落盘保存, 供后续重启复用 */
        if (device_secret_save(secret) == 0) {
            printf("  [信息] 已保存到 %s\n", DEVICE_SECRET_SAVE_PATH);
        }
    }
    printf("\n");

    /* ---- Step 3: 确定 FACTORY_API_KEY ---- */
    printf("--- Step 3: 确定 API Key ---\n");
    const char *api_key = factory_api_key;
    if (!api_key || api_key[0] == '\0') {
        api_key = FACTORY_API_KEY;
    }
    if (!api_key || api_key[0] == '\0') {
        printf("  [失败] FACTORY_API_KEY 为空\n");
        printf("  [提示] 在 mqttssl_worker.c 中设置 FACTORY_API_KEY 宏, 或传入参数\n");
        return -1;
    }
    printf("  API Key: %s... (已设置)\n\n", api_key);

    /* ---- Step 4: 构造设备条目 ---- */
    printf("--- Step 4: 构造设备条目 ---\n");
    FactoryDeviceItem item;
    memset(&item, 0, sizeof(item));
    strncpy(item.mac, mac, sizeof(item.mac) - 1);
    strncpy(item.sales_country_code, "CN", sizeof(item.sales_country_code) - 1);
    strncpy(item.device_secret, secret, sizeof(item.device_secret) - 1);
    strncpy(item.product_key, PRODUCT_KEY, sizeof(item.product_key) - 1);
    strncpy(item.model, MODEL, sizeof(item.model) - 1);
    strncpy(item.hardware_version, HW_VERSION, sizeof(item.hardware_version) - 1);
    strncpy(item.firmware_version, FW_VERSION, sizeof(item.firmware_version) - 1);
    strncpy(item.camera_vendor, "tange_tirtc", sizeof(item.camera_vendor) - 1);

    printf("  mac:               %s\n", item.mac);
    printf("  sales_country_code: %s\n", item.sales_country_code);
    printf("  device_secret:     %s\n", item.device_secret);
    printf("  product_key:       %s\n", item.product_key);
    printf("  model:             %s\n", item.model);
    printf("  hardware_version:  %s\n", item.hardware_version);
    printf("  firmware_version:  %s\n", item.firmware_version);
    printf("  camera_vendor:     %s\n", item.camera_vendor);
    printf("\n");

    /* ---- Step 5: 构造批次号和请求体 ---- */
    printf("--- Step 5: 构造请求体 ---\n");
    char batch_id[72];
    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    snprintf(batch_id, sizeof(batch_id), "FACTORY-%04d%02d%02d-%03d",
        tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday,
        (int)(now % 1000));

    FactoryDeviceSubmissionRequest req;
    memset(&req, 0, sizeof(req));
    strncpy(req.submission_batch_id, batch_id, sizeof(req.submission_batch_id) - 1);
    req.devices = &item;
    req.device_count = 1;

    printf("  submission_batch_id: %s\n", req.submission_batch_id);
    printf("  device_count:        %d\n", req.device_count);
    printf("\n");

    /* ---- Step 6: 发送 HTTP POST 请求 ---- */
    printf("--- Step 6: 发送 HTTP POST ---\n");
    printf("  URL: %s/api/device/v1/factory-devices\n\n", get_api_base());
    printf("  等待响应...\n\n");

    FactoryDeviceSubmissionResult result;
    memset(&result, 0, sizeof(result));
    int ret = http_post_factory_devices(api_key, &req, &result);

    /* ---- Step 7: 打印结果 ---- */
    printf("\n============================================================\n");
    printf("  测试结果\n");
    printf("============================================================\n");

    if (ret == 0) {
        printf("  ✓ 请求成功\n");
        printf("  submission_batch_id: %s\n", result.submission_batch_id);
        printf("  total:    %d\n", result.total);
        printf("  accepted: %d\n", result.accepted);
        printf("  rejected: %d\n", result.rejected);

        if (result.rejection_count > 0) {
            printf("\n  被拒条目:\n");
            for (int i = 0; i < result.rejection_count; i++) {
                printf("    [%d] mac: %s, reason: %s\n",
                    i, result.rejections[i].mac, result.rejections[i].reason);
            }
        }
    } else {
        printf("  ✗ 请求失败\n");
        printf("  [提示] 检查:\n");
        printf("    1. FACTORY_API_KEY 是否正确\n");
        printf("    2. 网络是否连通\n");
        printf("    3. MAC 地址是否已被注册\n");
    }

    printf("\n============================================================\n");
    printf("  测试结束\n");
    printf("============================================================\n");
    return ret;
}


















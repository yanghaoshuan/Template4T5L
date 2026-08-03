/**
 * @file    tlv_worker.h
 * @brief   TLV 增量属性处理线程 (v2: 查表分发 + 多段消息)
 *
 * 从 queue_tlv_property 队列消费 TLV 数据，解析并处理。
 *
 * ============================================================================
 * 消息格式: AA 55 | lenH lenL | 0x35 | 结构体段序列
 * ============================================================================
 * 每个结构体段: [struct_type:1B][seg_len:2B BE][tlv_data:seg_len]
 * lenH/lenL = 所有结构体段的总字节数 (big-endian)
 *
 * ---- 单段示例: PowerState (power_mode=1, battery=85%, charging=0) ----
 * AA 55  00 0F  35  03  00 0C  01 00 01 01 02 00 01 55 03 00 01 00
 * │      │      │  │    │    └──────────────────────────────────┘
 * │      │      │  │    │              TLV 数据 (12 字节)
 * │      │      │  │    └─ seg_len = 0x000C = 12
 * │      │      │  └─ struct_type = 0x03 = PowerState
 * │      │      └─ 命令字 0x35
 * │      └─ lenH|lenL = 0x000F = 15 (3+12)
 * └─ 帧头 AA 55
 *
 *   TLV 字段解析: 01 00 01 01 → power_mode=1
 *                 02 00 01 55 → battery_percent=85 (0x55)
 *                 03 00 01 00 → charging=0
 *
 * ---- 多段示例: PowerState(12B) + DoorState(19B) ----
 * AA 55  00 25  35  03  00 0C  01 00 01 01 02 00 01 55 03 00 01 00  04  00 13  01 00 01 01 02 00 01 00 03 00 08 00 00 00 00 00 00 00 00
 * │      │      │  │    │    └──────────────────────────────────┘ │  │    └────────────────────────────────────────────────────────┘
 * │      │      │  │    │              PowerState (12B)           │  │                   DoorState (19B)
 * │      │      │  │    └─ seg_len = 0x000C = 12                  │  └─ seg_len = 0x0013 = 19
 * │      │      │  └─ struct_type = 0x03 = PowerState             └─ struct_type = 0x04 = DoorState
 * │      │      └─ 命令字 0x35
 * │      └─ lenH|lenL = 0x0025 = 37 (3+12+3+19)
 * └─ 帧头 AA 55
 *
 * ============================================================================
 * struct_type 定义 (见 mqttssl_worker.h 中 TLV_STRUCT_* 宏):
 *   0x01 = DeviceState        0x02 = NetworkState
 *   0x03 = PowerState        0x04 = DoorState
 *   0x05 = EnvironmentState  0x06 = ActuatorState
 *   0x07 = CameraState       0x08 = SettingsState
 *   0x09 = FirmwareState     0x0A = StorageState
 *   0x61 = ActuatorExhaust   0x62 = ActuatorLight
 *   0x63 = ActuatorUvb       0x64 = ActuatorAnion
 *   0x65 = ActuatorPlasma    0x66 = ActuatorClimate
 *   0x67 = ActuatorHumidifier 0x68 = ActuatorInletFan
 *   0x69 = ActuatorFilter     0x6A = FactoryDeviceTlv
 */
#ifndef TLV_WORKER_H
#define TLV_WORKER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * TLV 属性处理线程入口
 * 从 queue_tlv_property 循环消费数据，解析 TLV 并打印，后续可扩展 MQTT 上报
 */
int process_tlv_worker_task(void);

/**
 * 向 queue_tlv_property 队列发送一条 TLV 属性消息 (单段)
 * @param struct_type 结构体类型 (见顶部注释)
 * @param tlv_data    TLV 序列化后的数据
 * @param data_len    TLV 数据长度
 * @return 0 成功, -1 失败
 */
int tlv_property_send(uint8_t struct_type, const uint8_t *tlv_data, int data_len);

/**
 * 向 queue_tlv_property 队列发送多段 TLV 属性消息
 * 可将多个结构体合并为一条消息发送，减少队列操作次数
 *
 * @param struct_types 结构体类型数组 (长度 = count)
 * @param tlv_data     TLV 序列化后的数据数组 (长度 = count)
 * @param data_lens    各段数据长度数组 (长度 = count)
 * @param count        结构体段数量 (1..N)
 * @return 0 成功, -1 失败
 */
int tlv_property_send_multi(const uint8_t *struct_types,
                            const uint8_t *const *tlv_data,
                            const int *data_lens, int count);

#ifdef __cplusplus
}
#endif

#endif /* TLV_WORKER_H */
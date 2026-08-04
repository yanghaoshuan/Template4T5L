/**
 * @file    tlv_codec.h
 * @brief   设备状态结构体 TLV (Type-Length-Value) 编解码器
 *
 * 覆盖 54.1 ~ 54.10 全部属性结构体。
 * TLV 格式: [Type:1B][Length:2B big-endian][Value:Length bytes]
 *
 * v2 优化:
 *   - 引入 TlvFieldType + TlvFieldDesc 描述符表驱动编解码
 *   - 新增 tlv_generic_pack / tlv_generic_unpack 通用接口
 *   - 保留原有具名函数签名以保证向后兼容
 *
 * ============================================================================
 * TLV 编码示例 (各结构体典型值)
 * 消息格式: AA 55 | lenH lenL | 0x35 | struct_type[1B] seg_len[2B] | tlv_payload
 * ============================================================================
 *
 * ---- PowerState {power_mode=1, battery=85%, charging=0} ----
 * ---- 逐字段 TLV 编码 ----
 * power_mode = 1 (U8):
 *   01 00 01 01
 *   │  │     └─ val = 0x01 = 1
 *   │  └─ len = 0x0001
 *   └─ tag = 0x01 (TLV_TAG_POWER_MODE)
 *
 * battery_percent = 85 (U8):
 *   02 00 01 55
 *   │  │     └─ val = 0x55 = 85
 *   │  └─ len = 0x0001
 *   └─ tag = 0x02 (TLV_TAG_BATTERY_PERCENT)
 *
 * charging = 0 (U8):
 *   03 00 01 00
 *   │  │     └─ val = 0x00 = 0
 *   │  └─ len = 0x0001
 *   └─ tag = 0x03 (TLV_TAG_CHARGING)
 *
 * ---- 完整 TLV 段 (struct_type 0x03) ----
 * seg_len = 0x000C = 12 字节
 *
 * 03 00 0C
 * │  └───── seg_len = 12
 * └─ struct_type = 0x03 (PowerState)
 *
 * 01 00 01 01 02 00 01 55 03 00 01 00
 *
 * ---- 完整消息 (含帧头) ----
 * AA 55  00 0F  35  03  00 0C
 * │      │      │  │   └───── seg_len = 12
 * │      │      │  └─ struct_type = 0x03
 * │      │      └─ 命令字 0x35
 * │      └─ lenH|lenL = 0x000F = 15 (3 + 12)
 * └─ 帧头
 *
 * + 12 字节 TLV payload → 共 20 字节
 * 一行 hex: AA 55 00 0F 35 03 00 0C 01 00 01 01 02 00 01 55 03 00 01 00
 *
 * ---- DoorState {door=1, lock=0, last_open=0} ----
 * ---- 逐字段 TLV 编码 ----
 * door_status = 1 (U8):
 *   01 00 01 01
 *   │  │     └─ val = 0x01 = 1
 *   │  └─ len = 0x0001
 *   └─ tag = 0x01 (TLV_TAG_DOOR_STATUS)
 *
 * lock_status = 0 (U8):
 *   02 00 01 00
 *   │  │     └─ val = 0x00 = 0
 *   │  └─ len = 0x0001
 *   └─ tag = 0x02 (TLV_TAG_LOCK_STATUS)
 *
 * last_open_time = 0.0 (IEEE754 double):
 *   03 00 08 00 00 00 00 00 00 00 00
 *   │  │     └─ 0x0000000000000000 = 0.0
 *   │  └─ len = 0x0008 = 8
 *   └─ tag = 0x03 (TLV_TAG_LAST_OPEN_TIME)
 *
 * ---- 完整 TLV 段 (struct_type 0x04) ----
 * seg_len = 0x0013 = 19 字节
 *
 * 04 00 13
 * │  └───── seg_len = 19
 * └─ struct_type = 0x04 (DoorState)
 *
 * 01 00 01 01 02 00 01 00 03 00 08 00 00 00 00 00 00 00 00
 *
 * ---- 完整消息 (含帧头) ----
 * AA 55  00 16  35  04  00 13
 * │      │      │  │   └───── seg_len = 19
 * │      │      │  └─ struct_type = 0x04
 * │      │      └─ 命令字 0x35
 * │      └─ lenH|lenL = 0x0016 = 22 (3 + 19)
 * └─ 帧头
 *
 * + 19 字节 TLV payload → 共 27 字节
 * 一行 hex: AA 55 00 16 35 04 00 13 01 00 01 01 02 00 01 00 03 00 08 00 00 00 00 00 00 00 00
 *
 * ---- ActuatorLight {enabled=1, brightness=80, running=1} ----
 * ---- 逐字段 TLV 编码 ----
 * enabled = 1 (U8):
 *   01 00 01 01
 *   │  │     └─ val = 0x01 = 1
 *   │  └─ len = 0x0001
 *   └─ tag = 0x01 (TLV_TAG_LIGHT_ENABLED)
 *
 * brightness_level = 80 (U8):
 *   02 00 01 50
 *   │  │     └─ val = 0x50 = 80
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
 * 01 00 01 01 02 00 01 50 03 00 01 01
 *
 * ---- 完整消息 (含帧头) ----
 * AA 55  00 0F  35  62  00 0C
 * │      │      │  │   └───── seg_len = 12
 * │      │      │  └─ struct_type = 0x62
 * │      │      └─ 命令字 0x35
 * │      └─ lenH|lenL = 0x000F = 15 (3 + 12)
 * └─ 帧头
 *
 * + 12 字节 TLV payload → 共 20 字节
 * 一行 hex: AA 55 00 0F 35 62 00 0C 01 00 01 01 02 00 01 50 03 00 01 01
 *
 * ---- ActuatorClimate {enabled=1, target=30.0C, ctrl_status=1, running=1} ----
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
 * ctrl_status = 1 (U8):
 *   03 00 01 01
 *   │  │     └─ val = 0x01 = 1
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
 * │      │      │  │   └───── seg_len = 24
 * │      │      │  └─ struct_type = 0x66
 * │      │      └─ 命令字 0x35
 * │      └─ lenH|lenL = 0x001B = 27 (3 + 24)
 * └─ 帧头
 *
 * + 24 字节 TLV payload → 共 32 字节
 * 一行 hex: AA 55 00 1B 35 66 00 18 01 00 01 01 02 00 08 40 3E 00 00 00 00 00 00 03 00 01 01 04 00 01 01
 *
 * ---- ActuatorFilter {life=80%, need_replace=0} ----
 * ---- 逐字段 TLV 编码 ----
 * life_percent = 80 (U8):
 *   01 00 01 50
 *   │  │     └─ val = 0x50 = 80
 *   │  └─ len = 0x0001
 *   └─ tag = 0x01 (TLV_TAG_FILTER_LIFE_PERCENT)
 *
 * need_replace = 0 (U8):
 *   02 00 01 00
 *   │  │     └─ val = 0x00 = 0
 *   │  └─ len = 0x0001
 *   └─ tag = 0x02 (TLV_TAG_FILTER_NEED_REPLACE)
 *
 * ---- 完整 TLV 段 (struct_type 0x69) ----
 * seg_len = 0x0008 = 8 字节
 *
 * 69 00 08
 * │  └───── seg_len = 8
 * └─ struct_type = 0x69 (ActuatorFilter)
 *
 * 01 00 01 50 02 00 01 00
 *
 * ---- 完整消息 (含帧头) ----
 * AA 55  00 0B  35  69  00 08
 * │      │      │  │   └───── seg_len = 8
 * │      │      │  └─ struct_type = 0x69
 * │      │      └─ 命令字 0x35
 * │      └─ lenH|lenL = 0x000B = 11 (3 + 8)
 * └─ 帧头
 *
 * + 8 字节 TLV payload → 共 16 字节
 * 一行 hex: AA 55 00 0B 35 69 00 08 01 00 01 50 02 00 01 00
 *
 * ---- FirmwareState {fw="2.1.0", hw="R1", config_ver=5} ----
 * ---- 逐字段 TLV 编码 ----
 * firmware_version = "2.1.0" (STR, 5B):
 *   01 00 05 32 2E 31 2E 30
 *   │  │     └─ "2.1.0" = {0x32, 0x2E, 0x31, 0x2E, 0x30}
 *   │  └─ len = 0x0005 = 5
 *   └─ tag = 0x01 (TLV_TAG_FW_VERSION)
 *
 * hardware_version = "R1" (STR, 2B):
 *   02 00 02 52 31
 *   │  │     └─ "R1" = {0x52, 0x31}
 *   │  └─ len = 0x0002 = 2
 *   └─ tag = 0x02 (TLV_TAG_HW_VERSION)
 *
 * config_version = 5 (U32, big-endian):
 *   03 00 04 00 00 00 05
 *   │  │     └─ 0x00000005 = 5
 *   │  └─ len = 0x0004 = 4
 *   └─ tag = 0x03 (TLV_TAG_CONFIG_VERSION)
 *
 * ---- 完整 TLV 段 (struct_type 0x09) ----
 * seg_len = 0x0017 = 23 字节
 *
 * 09 00 17
 * │  └───── seg_len = 23
 * └─ struct_type = 0x09 (FirmwareState)
 *
 * 01 00 05 32 2E 31 2E 30 02 00 02 52 31 03 00 04 00 00 00 05
 *
 * ---- 完整消息 (含帧头) ----
 * AA 55  00 1A  35  09  00 17
 * │      │      │  │   └───── seg_len = 23
 * │      │      │  └─ struct_type = 0x09
 * │      │      └─ 命令字 0x35
 * │      └─ lenH|lenL = 0x001A = 26 (3 + 23)
 * └─ 帧头
 *
 * + 23 字节 TLV payload → 共 31 字节
 * 一行 hex: AA 55 00 1A 35 09 00 17 01 00 05 32 2E 31 2E 30 02 00 02 52 31 03 00 04 00 00 00 05
 *
 * ---- StorageState {total=8192MB, free=4096MB} ----
 * ---- 逐字段 TLV 编码 ----
 * storage_total_mb = 8192 (U32, big-endian):
 *   01 00 04 00 00 20 00
 *   │  │     └─ 0x00002000 = 8192
 *   │  └─ len = 0x0004 = 4
 *   └─ tag = 0x01 (TLV_TAG_STORAGE_TOTAL)
 *
 * storage_free_mb = 4096 (U32, big-endian):
 *   02 00 04 00 00 10 00
 *   │  │     └─ 0x00001000 = 4096
 *   │  └─ len = 0x0004 = 4
 *   └─ tag = 0x02 (TLV_TAG_STORAGE_FREE)
 *
 * ---- 完整 TLV 段 (struct_type 0x0A) ----
 * seg_len = 0x000E = 14 字节
 *
 * 0A 00 0E
 * │  └───── seg_len = 14
 * └─ struct_type = 0x0A (StorageState)
 *
 * 01 00 04 00 00 20 00 02 00 04 00 00 10 00
 *
 * ---- 完整消息 (含帧头) ----
 * AA 55  00 11  35  0A  00 0E
 * │      │      │  │   └───── seg_len = 14
 * │      │      │  └─ struct_type = 0x0A
 * │      │      └─ 命令字 0x35
 * │      └─ lenH|lenL = 0x0011 = 17 (3 + 14)
 * └─ 帧头
 *
 * + 14 字节 TLV payload → 共 22 字节
 * 一行 hex: AA 55 00 11 35 0A 00 0E 01 00 04 00 00 20 00 02 00 04 00 00 10 00
 *
 * ============================================================================
 * FactoryDeviceTlv 编码示例 (出厂烧录上报, mac 由本机 WiFi 获取, secret 内部生成)
 * 消息格式: AA 55 | lenH lenL | TLV_CMD_FACTORY_DEVICE (0x36) | struct_type[1B] seg_len[2B] | tlv_payload
 * ============================================================================
 * 输入: sales_country="JP", product_key="MCQX_PET_CABIN", model="MCQX-PET-CABIN-V1",
 *       hardware_version="HW-V2.0", firmware_version="FW-V1.0.0"
 *
 * ---- 逐字段 TLV 编码 ----
 * sales_country_code = "JP" (2B):
 *   01 00 02 4A 50
 *   │  │     └─ "JP" = {0x4A, 0x50}
 *   │  └─ len = 0x0002
 *   └─ tag = 0x01
 *
 * product_key = "MCQX_PET_CABIN" (14B):
 *   03 00 0E 4D 43 51 58 5F 50 45 54 5F 43 41 42 49 4E
 *   │  │     └─ "MCQX_PET_CABIN"
 *   │  └─ len = 0x000E = 14
 *   └─ tag = 0x03
 *
 * model = "MCQX-PET-CABIN-V1" (17B):
 *   04 00 11 4D 43 51 58 2D 50 45 54 2D 43 41 42 49 4E 2D 56 31
 *   │  │     └─ "MCQX-PET-CABIN-V1"
 *   │  └─ len = 0x0011 = 17
 *   └─ tag = 0x04
 *
 * hardware_version = "HW-V2.0" (7B):
 *   05 00 07 48 57 2D 56 32 2E 30
 *   │  │     └─ "HW-V2.0"
 *   │  └─ len = 0x0007 = 7
 *   └─ tag = 0x05
 *
 * firmware_version = "FW-V1.0.0" (9B):
 *   06 00 09 46 57 2D 56 31 2E 30 2E 30
 *   │  │     └─ "FW-V1.0.0"
 *   │  └─ len = 0x0009 = 9
 *   └─ tag = 0x06
 *
 * ---- 完整 TLV 段 (struct_type 0x6A) ----
 * seg_len = 0x0040 = 64 字节
 *
 * 6A 00 40
 * │  └───── seg_len = 64
 * └─ struct_type = 0x6A (FactoryDeviceTlv)
 *
 * 01 00 02 4A 50
 * 03 00 0E 4D 43 51 58 5F 50 45 54 5F 43 41 42 49 4E
 * 04 00 11 4D 43 51 58 2D 50 45 54 2D 43 41 42 49 4E 2D 56 31
 * 05 00 07 48 57 2D 56 32 2E 30
 * 06 00 09 46 57 2D 56 31 2E 30 2E 30
 *
 * ---- 完整消息 (含帧头) ----
 * AA 55  00 43  36  6A  00 40
 * │      │      │  │   └───── seg_len = 0x0040 = 64
 * │      │      │  └─ struct_type = 0x6A
 * │      │      └─ 命令字 TLV_CMD_FACTORY_DEVICE (0x36)
 * │      └─ lenH|lenL = 0x0043 = 67 (3 + 64)
 * └─ 帧头 AA 55
 *
 * + 64 字节 TLV payload (同上) → 共 72 字节
 *
 * 一行 hex:
 * AA 55 00 43 36 6A 00 40 01 00 02 4A 50 03 00 0E 4D 43 51 58 5F 50 45 54 5F 43 41 42 49 4E 04 00 11 4D 43 51 58 2D 50 45 54 2D 43 41 42 49 4E 2D 56 31 05 00 07 48 57 2D 56 32 2E 30 06 00 09 46 57 2D 56 31 2E 30 2E 30
 *
 * ============================================================================
 * FactoryDeviceTlv 解码与打印示例
 * ============================================================================
 * 收到上述 TLV 消息 (命令字 0x36) 后, tlv_dispatch(0x6A, payload, 64) 调用:
 *   factory_device_tlv_unpack(&u, payload, 64)
 *   print_factory_device_tlv(&u)
 *   handle_factory_device_tlv(&u)                     ← 业务回调
 *
 * ---- 逐字段 TLV 解码 ----
 * 01 00 02 4A 50
 *  │  │     └─ "JP" → sales_country_code = "JP"
 *  │  └─ len = 2
 *  └─ tag = 0x01 → TLV_TAG_FACTORY_SALES_COUNTRY
 *
 * 03 00 0E 4D 43 51 58 5F 50 45 54 5F 43 41 42 49 4E
 *  │  │     └─ "MCQX_PET_CABIN" → product_key = "MCQX_PET_CABIN"
 *  │  └─ len = 14
 *  └─ tag = 0x03 → TLV_TAG_FACTORY_PRODUCT_KEY
 *
 * 04 00 11 4D 43 51 58 2D 50 45 54 2D 43 41 42 49 4E 2D 56 31
 *  │  │     └─ "MCQX-PET-CABIN-V1" → model = "MCQX-PET-CABIN-V1"
 *  │  └─ len = 17
 *  └─ tag = 0x04 → TLV_TAG_FACTORY_MODEL
 *
 * 05 00 07 48 57 2D 56 32 2E 30
 *  │  │     └─ "HW-V2.0" → hardware_version = "HW-V2.0"
 *  │  └─ len = 7
 *  └─ tag = 0x05 → TLV_TAG_FACTORY_HW_VERSION
 *
 * 06 00 09 46 57 2D 56 31 2E 30 2E 30
 *  │  │     └─ "FW-V1.0.0" → firmware_version = "FW-V1.0.0"
 *  │  └─ len = 9
 *  └─ tag = 0x06 → TLV_TAG_FACTORY_FW_VERSION
 *
 * ---- 诊断打印 (print_factory_device_tlv) ----
 * [TLV] FactoryDevice: country=JP product_key=MCQX_PET_CABIN model=MCQX-PET-CABIN-V1 hw=HW-V2.0 fw=FW-V1.0.0
 *
 * ---- 业务处理 (handle_factory_device_tlv) ----
 * mac         = get_wifi_mac() (本机 WiFi MAC, 12 位大写 hex, 例: "28AD3E13D940")
 * device_secret = generate_device_secret() (内部生成 43 字符 Base64URL)
 *   → 组装 FactoryDeviceSubmissionRequest
 *   → http_post_factory_devices(FACTORY_API_KEY, &req, NULL)
 *
 * ============================================================================
 * FactoryDeviceRejectionTlv 编码示例 (被拒结果下发, V851 → D5)
 * 消息格式: AA 55 | lenH lenL | TLV_CMD_FACTORY_DEVICE (0x36) | struct_type(0x6B) seg_len | tlv_payload
 * ============================================================================
 * 输入: mac="A0B1C2D3E4F5", reason="DUPLICATE_MAC"
 *
 * ---- 逐字段 TLV 编码 ----
 * mac = "A0B1C2D3E4F5" (12B):
 *   01 00 0C 41 30 42 31 43 32 44 33 45 34 46 35
 *   │  │     └─ "A0B1C2D3E4F5" = {0x41,0x30,0x42,0x31,0x43,0x32,0x44,0x33,0x45,0x34,0x46,0x35}
 *   │  └─ len = 0x000C = 12
 *   └─ tag = 0x01 (TLV_TAG_FACTORY_REJECT_MAC)
 *
 * reason = "DUPLICATE_MAC" (13B):
 *   02 00 0D 44 55 50 4C 49 43 41 54 45 5F 4D 41 43
 *   │  │     └─ "DUPLICATE_MAC"
 *   │  └─ len = 0x000D = 13
 *   └─ tag = 0x02 (TLV_TAG_FACTORY_REJECT_REASON)
 *
 * ---- 完整 TLV 段 (struct_type 0x6B) ----
 * seg_len = 0x0022 = 34 字节
 *
 * 6B 00 22
 * │  └───── seg_len = 34
 * └─ struct_type = 0x6B (FactoryDeviceRejectionTlv)
 *
 * 01 00 0C 41 30 42 31 43 32 44 33 45 34 46 35
 * 02 00 0D 44 55 50 4C 49 43 41 54 45 5F 4D 41 43
 *
 * ---- 完整消息 (含帧头) ----
 * AA 55  00 25  36  6B  00 22
 * │      │      │  │   └───── seg_len = 0x0022 = 34
 * │      │      │  └─ struct_type = 0x6B
 * │      │      └─ 命令字 TLV_CMD_FACTORY_DEVICE (0x36)
 * │      └─ lenH|lenL = 0x0025 = 37 (3 + 34)
 * └─ 帧头
 *
 * + 34 字节 TLV payload → 共 42 字节
 * 一行 hex:
 * AA 55 00 25 36 6B 00 22 01 00 0C 41 30 42 31 43 32 44 33 45 34 46 35 02 00 0D 44 55 50 4C 49 43 41 54 45 5F 4D 41 43
 *
 * ---- 业务处理 (factory_device_result_send_uart) ----
 * 遍历 result->rejections[] → 逐条 pack 为 TLV 帧 → tiny_queue_push(queue_2_uart)
 * D5/LCD 收到后解析 struct_type=0x6B, 显示被拒 mac 和原因
 */
#ifndef TLV_CODEC_H
#define TLV_CODEC_H

#include "mqttssl_worker.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * TLV 字段描述符 — 表驱动编解码的核心数据结构
 * ============================================================================ */

/** 字段值类型 */
typedef enum {
    TLV_FIELD_U8,       /**< int/enum 字段, 序列化为 u8 TLV     例: 01 00 01 01 → tag=0x01, len=1, val=1 */
    TLV_FIELD_U32,      /**< int 字段, 序列化为 u32 TLV          例: 03 00 04 00 00 00 05 → tag=0x03, len=4, val=5 BE */
    TLV_FIELD_DOUBLE,   /**< double 字段, 序列化为 double TLV    例: 02 00 08 40 3E 00 00 00 00 00 00 → tag=0x02, len=8, val=30.0 */
    TLV_FIELD_STR,      /**< char[] 字段, 序列化为 string TLV    例: 01 00 05 32 2E 31 2E 30 → tag=0x01, len=5, "2.1.0" */
} TlvFieldType;

/** 单个字段的描述符 */
typedef struct _tlv_field_desc {
    uint8_t       tag;       /**< TLV 标签 (Type)            */
    TlvFieldType  type;      /**< 字段值类型                  */
    uint16_t      offset;    /**< 在结构体中的偏移 (offsetof) */
    uint16_t      extra;     /**< STR: 最大长度; 其他: 0     */
} TlvFieldDesc;

/* ============================================================================
 * 通用 TLV 编解码 — 基于描述符表
 * ============================================================================ */

/**
 * 通用 pack: 按描述符表将结构体序列化为 TLV 字节流
 * @param src        指向结构体的指针
 * @param fields     字段描述符表
 * @param field_count 字段数量
 * @param buf        输出缓冲区
 * @param buf_size   缓冲区大小
 * @return 写入的字节数, <0 表示错误
 */
int tlv_generic_pack(const void *src, const TlvFieldDesc *fields,
                     int field_count, uint8_t *buf, int buf_size);

/**
 * 通用 unpack: 按描述符表从 TLV 字节流反序列化到结构体
 * @param dst        指向结构体的指针 (调用前需 memset 为 0)
 * @param fields     字段描述符表
 * @param field_count 字段数量
 * @param buf        输入字节流
 * @param buf_size   字节流长度
 * @return 消耗的字节数, <0 表示错误
 */
int tlv_generic_unpack(void *dst, const TlvFieldDesc *fields,
                       int field_count, const uint8_t *buf, int buf_size);

/* ============================================================================
 * TLV 标签常量 — 按结构体分组
 * ============================================================================ */

/* ---- 54.3 PowerState ---- */
#define TLV_TAG_POWER_MODE              0x01
#define TLV_TAG_BATTERY_PERCENT         0x02
#define TLV_TAG_CHARGING                0x03

/* ---- 54.4 DoorState ---- */
#define TLV_TAG_DOOR_STATUS             0x01
#define TLV_TAG_LOCK_STATUS             0x02
#define TLV_TAG_LAST_OPEN_TIME          0x03

/* ---- 54.5 EnvironmentState ---- */
#define TLV_TAG_TEMPERATURE             0x01
#define TLV_TAG_HUMIDITY                0x02
#define TLV_TAG_PM25                    0x03
#define TLV_TAG_CO2                     0x04

/* ---- 54.6.1 ActuatorExhaust ---- */
#define TLV_TAG_EXH_ENABLED             0x01
#define TLV_TAG_EXH_SPEED_LEVEL         0x02
#define TLV_TAG_EXH_INTERVAL_HOURS      0x03
#define TLV_TAG_EXH_RUNNING             0x04

/* ---- 54.6.2 ActuatorLight ---- */
#define TLV_TAG_LIGHT_ENABLED           0x01
#define TLV_TAG_LIGHT_BRIGHTNESS        0x02
#define TLV_TAG_LIGHT_RUNNING           0x03

/* ---- 54.6.3 ActuatorUvb ---- */
#define TLV_TAG_UVB_ENABLED             0x01
#define TLV_TAG_UVB_LEVEL               0x02
#define TLV_TAG_UVB_DAILY_HOURS         0x03
#define TLV_TAG_UVB_RUNNING             0x04

/* ---- 54.6.4 ActuatorAnion ---- */
#define TLV_TAG_ANION_ENABLED           0x01
#define TLV_TAG_ANION_RUNNING           0x02

/* ---- 54.6.5 ActuatorPlasma ---- */
#define TLV_TAG_PLASMA_ENABLED          0x01
#define TLV_TAG_PLASMA_RUNNING          0x02

/* ---- 54.6.6 ActuatorClimate ---- */
#define TLV_TAG_CLIMATE_ENABLED         0x01
#define TLV_TAG_CLIMATE_TARGET          0x02
#define TLV_TAG_CLIMATE_CTRL_STATUS     0x03
#define TLV_TAG_CLIMATE_RUNNING         0x04

/* ---- 54.6.7 ActuatorHumidifier ---- */
#define TLV_TAG_HUMI_ENABLED            0x01
#define TLV_TAG_HUMI_INTERVAL_HOURS     0x02
#define TLV_TAG_HUMI_RUNNING_HOURS      0x03
#define TLV_TAG_HUMI_RUNNING            0x04
#define TLV_TAG_HUMI_LIQUID_STATUS      0x05

/* ---- 54.6.8 ActuatorInletFan ---- */
#define TLV_TAG_INLET_ENABLED           0x01
#define TLV_TAG_INLET_SPEED_LEVEL       0x02
#define TLV_TAG_INLET_RUNNING           0x03

/* ---- 54.6.9 ActuatorFilter ---- */
#define TLV_TAG_FILTER_LIFE_PERCENT     0x01
#define TLV_TAG_FILTER_NEED_REPLACE     0x02

/* ---- 54.7 CameraState ---- */
#define TLV_TAG_CAMERA_STATUS           0x01
#define TLV_TAG_STREAM_STATUS           0x02
#define TLV_TAG_PROVIDER                0x03
#define TLV_TAG_VIDEO_CODEC             0x04
#define TLV_TAG_RESOLUTION              0x05
#define TLV_TAG_FPS                     0x06
#define TLV_TAG_AUDIO_ENABLED           0x07
#define TLV_TAG_PRIVACY_MODE            0x08
#define TLV_TAG_LAST_SESSION_ID         0x09
#define TLV_TAG_LAST_ERROR_CODE         0x0A

/* ---- 54.8.1 DisplaySettings ---- */
#define TLV_TAG_DISP_BRIGHTNESS         0x01
#define TLV_TAG_DISP_SCREEN_TIMEOUT     0x02
#define TLV_TAG_DISP_SCREEN_MODE        0x03

/* ---- 54.8 SettingsState ---- */
#define TLV_TAG_TEMP_UNIT               0x01
#define TLV_TAG_DISPLAY                 0x02   /* 嵌套 TLV (DisplaySettings) */
#define TLV_TAG_VOLUME_PERCENT          0x03
#define TLV_TAG_LANGUAGE                0x04
#define TLV_TAG_LOCAL_PASSWORD          0x05

/* ---- 54.9 FirmwareState ---- */
#define TLV_TAG_FW_VERSION              0x01
#define TLV_TAG_HW_VERSION              0x02
#define TLV_TAG_CONFIG_VERSION          0x03

/* ---- 54.10 StorageState ---- */
#define TLV_TAG_STORAGE_TOTAL           0x01
#define TLV_TAG_STORAGE_FREE            0x02

/* ---- FactoryDeviceTlv (出厂烧录上报, 不含 device_secret) ---- */
#define TLV_TAG_FACTORY_SALES_COUNTRY   0x01
#define TLV_TAG_FACTORY_PRODUCT_KEY     0x03
#define TLV_TAG_FACTORY_MODEL           0x04
#define TLV_TAG_FACTORY_HW_VERSION      0x05
#define TLV_TAG_FACTORY_FW_VERSION      0x06

/* ---- FactoryDeviceRejectionTlv (被拒结果下发, V851 → D5) ---- */
#define TLV_TAG_FACTORY_REJECT_MAC      0x01
#define TLV_TAG_FACTORY_REJECT_REASON   0x02

/* ---- BootstrapResultTlv (自举结果下发, V851 → D5/LCD) ---- */
#define TLV_TAG_BOOT_DEVICE_SN          0x01
#define TLV_TAG_BOOT_BLE_ID             0x02
#define TLV_TAG_BOOT_API_ENDPOINT       0x03
#define TLV_TAG_BOOT_BIND_STATUS        0x04
#define TLV_TAG_BOOT_QR_URL             0x05

/* ============================================================================
 * TLV pack/unpack — 各结构体 (向后兼容的函数签名)
 *
 * 返回值: pack 返回写入的字节数; unpack 返回消耗的字节数。
 *         < 0 表示错误。
 * ============================================================================ */

/* ---- 54.1 DeviceState ---- */
int device_state_tlv_pack(const DeviceState *s, uint8_t *buf, int buf_size);
int device_state_tlv_unpack(DeviceState *s, const uint8_t *buf, int buf_size);

/* ---- 54.2 NetworkState ---- */
int network_state_tlv_pack(const NetworkState *s, uint8_t *buf, int buf_size);
int network_state_tlv_unpack(NetworkState *s, const uint8_t *buf, int buf_size);

/* ---- 54.3 PowerState ---- */
int power_state_tlv_pack(const PowerState *s, uint8_t *buf, int buf_size);
int power_state_tlv_unpack(PowerState *s, const uint8_t *buf, int buf_size);

/* ---- 54.4 DoorState ---- */
int door_state_tlv_pack(const DoorState *s, uint8_t *buf, int buf_size);
int door_state_tlv_unpack(DoorState *s, const uint8_t *buf, int buf_size);

/* ---- 54.5 EnvironmentState ---- */
int environment_state_tlv_pack(const EnvironmentState *s, uint8_t *buf, int buf_size);
int environment_state_tlv_unpack(EnvironmentState *s, const uint8_t *buf, int buf_size);

/* ---- 54.6.1 ActuatorExhaust ---- */
int actuator_exhaust_tlv_pack(const ActuatorExhaust *s, uint8_t *buf, int buf_size);
int actuator_exhaust_tlv_unpack(ActuatorExhaust *s, const uint8_t *buf, int buf_size);

/* ---- 54.6.2 ActuatorLight ---- */
int actuator_light_tlv_pack(const ActuatorLight *s, uint8_t *buf, int buf_size);
int actuator_light_tlv_unpack(ActuatorLight *s, const uint8_t *buf, int buf_size);

/* ---- 54.6.3 ActuatorUvb ---- */
int actuator_uvb_tlv_pack(const ActuatorUvb *s, uint8_t *buf, int buf_size);
int actuator_uvb_tlv_unpack(ActuatorUvb *s, const uint8_t *buf, int buf_size);

/* ---- 54.6.4 ActuatorAnion ---- */
int actuator_anion_tlv_pack(const ActuatorAnion *s, uint8_t *buf, int buf_size);
int actuator_anion_tlv_unpack(ActuatorAnion *s, const uint8_t *buf, int buf_size);

/* ---- 54.6.5 ActuatorPlasma ---- */
int actuator_plasma_tlv_pack(const ActuatorPlasma *s, uint8_t *buf, int buf_size);
int actuator_plasma_tlv_unpack(ActuatorPlasma *s, const uint8_t *buf, int buf_size);

/* ---- 54.6.6 ActuatorClimate ---- */
int actuator_climate_tlv_pack(const ActuatorClimate *s, uint8_t *buf, int buf_size);
int actuator_climate_tlv_unpack(ActuatorClimate *s, const uint8_t *buf, int buf_size);

/* ---- 54.6.7 ActuatorHumidifier ---- */
int actuator_humidifier_tlv_pack(const ActuatorHumidifier *s, uint8_t *buf, int buf_size);
int actuator_humidifier_tlv_unpack(ActuatorHumidifier *s, const uint8_t *buf, int buf_size);

/* ---- 54.6.8 ActuatorInletFan ---- */
int actuator_inlet_fan_tlv_pack(const ActuatorInletFan *s, uint8_t *buf, int buf_size);
int actuator_inlet_fan_tlv_unpack(ActuatorInletFan *s, const uint8_t *buf, int buf_size);

/* ---- 54.6.9 ActuatorFilter ---- */
int actuator_filter_tlv_pack(const ActuatorFilter *s, uint8_t *buf, int buf_size);
int actuator_filter_tlv_unpack(ActuatorFilter *s, const uint8_t *buf, int buf_size);

/* ---- 54.6 ActuatorState (聚合) ---- */
int actuator_state_tlv_pack(const ActuatorState *s, uint8_t *buf, int buf_size);
int actuator_state_tlv_unpack(ActuatorState *s, const uint8_t *buf, int buf_size);

/* ---- 54.7 CameraState ---- */
int camera_state_tlv_pack(const CameraState *s, uint8_t *buf, int buf_size);
int camera_state_tlv_unpack(CameraState *s, const uint8_t *buf, int buf_size);

/* ---- 54.8.1 DisplaySettings ---- */
int display_settings_tlv_pack(const DisplaySettings *s, uint8_t *buf, int buf_size);
int display_settings_tlv_unpack(DisplaySettings *s, const uint8_t *buf, int buf_size);

/* ---- 54.8 SettingsState (含嵌套 DisplaySettings) ---- */
int settings_state_tlv_pack(const SettingsState *s, uint8_t *buf, int buf_size);
int settings_state_tlv_unpack(SettingsState *s, const uint8_t *buf, int buf_size);

/* ---- 54.9 FirmwareState ---- */
int firmware_state_tlv_pack(const FirmwareState *s, uint8_t *buf, int buf_size);
int firmware_state_tlv_unpack(FirmwareState *s, const uint8_t *buf, int buf_size);

/* ---- 54.10 StorageState ---- */
int storage_state_tlv_pack(const StorageState *s, uint8_t *buf, int buf_size);
int storage_state_tlv_unpack(StorageState *s, const uint8_t *buf, int buf_size);

/* ---- FactoryDeviceTlv (出厂烧录上报) ---- */
int factory_device_tlv_pack(const FactoryDeviceTlv *s, uint8_t *buf, int buf_size);
int factory_device_tlv_unpack(FactoryDeviceTlv *s, const uint8_t *buf, int buf_size);

/* ---- FactoryDeviceRejectionTlv (被拒结果下发) ---- */
int factory_device_rejection_tlv_pack(const FactoryDeviceRejectionTlv *s, uint8_t *buf, int buf_size);

/* ---- BootstrapResultTlv (自举结果下发) ---- */
int bootstrap_result_tlv_pack(const BootstrapResultTlv *s, uint8_t *buf, int buf_size);
int bootstrap_result_tlv_unpack(BootstrapResultTlv *s, const uint8_t *buf, int buf_size);

#ifdef __cplusplus
}
#endif

#endif /* TLV_CODEC_H */
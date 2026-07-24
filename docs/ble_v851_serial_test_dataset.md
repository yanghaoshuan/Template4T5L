# T5L UART2 蓝牙与 UART4 V851 串口测试数据集

> 本文件由 `tools/t5l_serial_simulator.py render-md` 根据 `tests/serial_bridge_vectors.json` 自动生成，请勿手工修改 HEX。

## 测试连接

| 模拟对象 | T5L端口 | 参数 | 电脑端 |
|---|---|---|---|
| PB-03F 蓝牙 | UART2 | 115200, 8N1, 无流控 | 3.3V TTL USB串口，端口名由 `--ble-port` 指定 |
| V851 | UART4 | 921600, 8N1, 无流控 | 3.3V TTL USB串口，端口名由 `--v851-port` 指定 |

UART2接线：T5L P0.4(TX) → USB串口RX，T5L P0.5(RX) ← USB串口TX。两路串口均须共地。禁止直接连接 RS-232 电平，也不要用USB串口的5V电源脚给T5L供电。

## 运行方法

```powershell
python -m pip install "pyserial>=3.5"
python tools/t5l_serial_simulator.py ports --probe
python tools/t5l_serial_simulator.py monitor --port COM3 --baud 115200 --seconds 10
python tools/t5l_serial_simulator.py list
python tools/t5l_serial_simulator.py run --mode ble --ble-port COM3
python tools/t5l_serial_simulator.py run --mode v851 --v851-port COM4
python tools/t5l_serial_simulator.py run --mode both --ble-port COM3 --v851-port COM4
python tools/t5l_serial_simulator.py render-md --check
```

`run`在打开目标串口前会先列出系统当前识别到的端口。若端口显示存在但打开时报“拒绝访问”，请关闭串口调试助手、Keil串口窗口或其他占用该COM口的程序，再使用 `ports --probe` 确认可访问性。

- `--mode ble`：只打开UART2蓝牙串口，执行 11 条蓝牙单端用例。
- `--mode v851`：只打开UART4 V851串口，执行 30 条V851单端用例。
- `--mode both`：打开两路串口，执行完整桥接、双向转发及全部异常用例。

电脑端会自动响应 PB-03F AT 初始化。固定模拟 MAC 为 `A1B2C3D4E5F6`；多分片发送间隔为 20 ms，以适配 UART2 的 256 字节接收环形缓冲区。

## 协议摘要

- BLE帧：`4D 51 | 版本 | 标志 | msg_id(BE) | 分片序号(BE) | 分片总数(BE) | 载荷长度(BE) | JSON | CRC16(BE)`。
- BLE单分片JSON上限为 223 字节，完整JSON上限为 2000 字节。
- V851帧：`AA 55 | body_len(BE) | A1 | JSON | CRC16(BE)`；CRC覆盖 `A1 + JSON`。
- CRC算法为 Modbus CRC16（初值 `FFFF`、多项式 `A001`），但两个协议在线路上均按大端发送CRC。

## PB-03F 自动初始化预期

T5L应依次发出：`AT`、`AT+BLEMODE=9`、服务UUID、TX UUID、RX UUID、`AT+BLEMTU=240`、`AT+BLEMAC?`、`AT+BLEAUTH=...`、`AT+BLENAME=...`、`AT+BLEADVDATA=...`、`AT+BLEMODE=0`、`AT+TRANSENTER`。模拟器对MAC查询返回 `+BLEMAC:A1B2C3D4E5F6\r\nOK\r\n`，其余命令返回 `OK\r\n`。

## 用例

### INIT-001 — PB-03F完整AT初始化

- 分类：`init`
- 前置条件：T5L重新上电或复位，电脑端模拟器已打开两路串口
- 说明：验证T5L在UART2上完成12步PB-03F配置并进入透传。

#### 发送步骤

无需主动发送，观察T5L输出。

#### 预期结果

- UART2依次出现12类AT命令，MAC查询、认证、名称和广播命令均存在。
- PB-03F模拟器至少完成1次透传切换。

### V851-ID-001 — 缓存V851设备身份并重配蓝牙名称

- 分类：`bridge`
- 前置条件：INIT-001通过，PB-03F已进入透传
- 说明：V851上报device.hello后，T5L缓存身份；新的ble_id触发退出透传并重新配置PB-03F。

#### 发送步骤

1. 发送 V851 JSON

   JSON：

   ```json
   {
     "msg_id": "HELLO-001",
     "msg_type": "device.hello",
     "protocol_version": "1.0",
     "device_sn": "SN-TEST-0001",
     "ble_id": "ABC234",
     "product_key": "PK-TEST",
     "data": {
       "device_identity": {
         "device_sn": "SN-TEST-0001",
         "ble_id": "ABC234",
         "ble_name": "MQXQ-ABC2",
         "product_key": "PK-TEST",
         "model": "T5L-TEST",
         "hardware_version": "HW-1.0",
         "firmware_version": "FW-1.0",
         "sales_country_code": "CN"
       }
     }
   }
   ```

   实际JSON长度：`364` 字节。

   完整帧：`371` 字节，帧尾CRC字段 `DC2B`。

   ```text
   AA 55 01 6F A1 7B 22 6D 73 67 5F 69 64 22 3A 22 48 45 4C 4C 4F 2D 30 30 31 22 2C 22 6D 73 67 5F
   74 79 70 65 22 3A 22 64 65 76 69 63 65 2E 68 65 6C 6C 6F 22 2C 22 70 72 6F 74 6F 63 6F 6C 5F 76
   65 72 73 69 6F 6E 22 3A 22 31 2E 30 22 2C 22 64 65 76 69 63 65 5F 73 6E 22 3A 22 53 4E 2D 54 45
   53 54 2D 30 30 30 31 22 2C 22 62 6C 65 5F 69 64 22 3A 22 41 42 43 32 33 34 22 2C 22 70 72 6F 64
   75 63 74 5F 6B 65 79 22 3A 22 50 4B 2D 54 45 53 54 22 2C 22 64 61 74 61 22 3A 7B 22 64 65 76 69
   63 65 5F 69 64 65 6E 74 69 74 79 22 3A 7B 22 64 65 76 69 63 65 5F 73 6E 22 3A 22 53 4E 2D 54 45
   53 54 2D 30 30 30 31 22 2C 22 62 6C 65 5F 69 64 22 3A 22 41 42 43 32 33 34 22 2C 22 62 6C 65 5F
   6E 61 6D 65 22 3A 22 4D 51 58 51 2D 41 42 43 32 22 2C 22 70 72 6F 64 75 63 74 5F 6B 65 79 22 3A
   22 50 4B 2D 54 45 53 54 22 2C 22 6D 6F 64 65 6C 22 3A 22 54 35 4C 2D 54 45 53 54 22 2C 22 68 61
   72 64 77 61 72 65 5F 76 65 72 73 69 6F 6E 22 3A 22 48 57 2D 31 2E 30 22 2C 22 66 69 72 6D 77 61
   72 65 5F 76 65 72 73 69 6F 6E 22 3A 22 46 57 2D 31 2E 30 22 2C 22 73 61 6C 65 73 5F 63 6F 75 6E
   74 72 79 5F 63 6F 64 65 22 3A 22 43 4E 22 7D 7D 7D DC 2B
   ```

2. 等待 PB-03F 再次进入透传

#### 预期结果

- device.hello本身不产生UART4应答。
- UART2先收到+++，随后完成第二轮AT配置并再次进入透传。

#### Keil调试器检查

- V851ProtocolGetDeviceInfo()中的device_sn、ble_id、product_key、model及版本字段与发送值一致。

### V851-TIME-001 — 缓存服务器时间与绑定状态

- 分类：`bridge`
- 前置条件：V851-ID-001通过
- 说明：server.hello_ack只更新本地时间基准、国家码和绑定状态，不产生串口应答。

#### 发送步骤

1. 发送 V851 JSON

   JSON：

   ```json
   {
     "msg_id": "HELLO-ACK-001",
     "msg_type": "server.hello_ack",
     "timestamp": 2000000000,
     "data": {
       "server_time": 2000000000,
       "bind_status": "BOUND",
       "sales_country_code": "CN"
     }
   }
   ```

   实际JSON长度：`161` 字节。

   完整帧：`168` 字节，帧尾CRC字段 `DAF6`。

   ```text
   AA 55 00 A4 A1 7B 22 6D 73 67 5F 69 64 22 3A 22 48 45 4C 4C 4F 2D 41 43 4B 2D 30 30 31 22 2C 22
   6D 73 67 5F 74 79 70 65 22 3A 22 73 65 72 76 65 72 2E 68 65 6C 6C 6F 5F 61 63 6B 22 2C 22 74 69
   6D 65 73 74 61 6D 70 22 3A 32 30 30 30 30 30 30 30 30 30 2C 22 64 61 74 61 22 3A 7B 22 73 65 72
   76 65 72 5F 74 69 6D 65 22 3A 32 30 30 30 30 30 30 30 30 30 2C 22 62 69 6E 64 5F 73 74 61 74 75
   73 22 3A 22 42 4F 55 4E 44 22 2C 22 73 61 6C 65 73 5F 63 6F 75 6E 74 72 79 5F 63 6F 64 65 22 3A
   22 43 4E 22 7D 7D DA F6
   ```

#### 预期结果

- server.hello_ack不产生UART4应答。

#### Keil调试器检查

- V851ProtocolGetTimestamp()不小于2000000000，bind_status为BOUND。

### BLE-INFO-001 — 蓝牙查询设备信息

- 分类：`bridge`
- 前置条件：身份、绑定状态和PB-03F MAC均已缓存
- 说明：手机侧get_device_info由T5L本地处理，不转发给V851。

#### 发送步骤

1. 发送 BLE JSON

   JSON：

   ```json
   {
     "cmd": "get_device_info",
     "request_id": "REQ-INFO-001",
     "data": {}
   }
   ```

   实际JSON长度：`63` 字节。

   完整帧：`77` 字节，帧尾CRC字段 `59E6`。

   ```text
   4D 51 01 01 01 01 00 00 00 01 00 3F 7B 22 63 6D 64 22 3A 22 67 65 74 5F 64 65 76 69 63 65 5F 69
   6E 66 6F 22 2C 22 72 65 71 75 65 73 74 5F 69 64 22 3A 22 52 45 51 2D 49 4E 46 4F 2D 30 30 31 22
   2C 22 64 61 74 61 22 3A 7B 7D 7D 59 E6
   ```

#### 预期结果

- UART2返回get_device_info_ack，身份、MAC、名称和绑定状态正确。

  ```json
  {
    "cmd": "get_device_info_ack",
    "request_id": "REQ-INFO-001",
    "result": "OK",
    "error_code": null,
    "data": {
      "mac": "A1B2C3D4E5F6",
      "device_sn": "SN-TEST-0001",
      "ble_id": "ABC234",
      "ble_name": "MQXQ-ABC2",
      "product_key": "PK-TEST",
      "model": "T5L-TEST",
      "hardware_version": "HW-1.0",
      "firmware_version": "FW-1.0",
      "sales_country_code": "CN",
      "bind_status": "BOUND"
    }
  }
  ```
- UART4无转发数据。

### BLE-NET-001 — 蓝牙配网请求转发到V851

- 分类：`bridge`
- 前置条件：PB-03F处于透传状态
- 说明：合法configure_network应保持JSON内容并封装成AA55帧发往UART4。

#### 发送步骤

1. 发送 BLE JSON

   JSON：

   ```json
   {
     "cmd": "configure_network",
     "request_id": "REQ-NET-001",
     "data": {
       "ssid": "TestWiFi",
       "password": "12345678"
     }
   }
   ```

   实际JSON长度：`103` 字节。

   完整帧：`117` 字节，帧尾CRC字段 `59F5`。

   ```text
   4D 51 01 01 01 02 00 00 00 01 00 67 7B 22 63 6D 64 22 3A 22 63 6F 6E 66 69 67 75 72 65 5F 6E 65
   74 77 6F 72 6B 22 2C 22 72 65 71 75 65 73 74 5F 69 64 22 3A 22 52 45 51 2D 4E 45 54 2D 30 30 31
   22 2C 22 64 61 74 61 22 3A 7B 22 73 73 69 64 22 3A 22 54 65 73 74 57 69 46 69 22 2C 22 70 61 73
   73 77 6F 72 64 22 3A 22 31 32 33 34 35 36 37 38 22 7D 7D 59 F5
   ```

#### 预期结果

- UART4收到与蓝牙请求等价的configure_network JSON。

  ```json
  {
    "cmd": "configure_network",
    "request_id": "REQ-NET-001",
    "data": {
      "ssid": "TestWiFi",
      "password": "12345678"
    }
  }
  ```

### V851-NET-ACK-001 — V851配网应答转回蓝牙

- 分类：`bridge`
- 前置条件：BLE-NET-001已完成
- 说明：V851的configure_network_ack属于允许转回蓝牙的应答。

#### 发送步骤

1. 发送 V851 JSON

   JSON：

   ```json
   {
     "cmd": "configure_network_ack",
     "request_id": "REQ-NET-001",
     "result": "OK",
     "error_code": null,
     "message": null,
     "data": {
       "ip": "192.168.1.100"
     }
   }
   ```

   实际JSON长度：`135` 字节。

   完整帧：`142` 字节，帧尾CRC字段 `E5E2`。

   ```text
   AA 55 00 8A A1 7B 22 63 6D 64 22 3A 22 63 6F 6E 66 69 67 75 72 65 5F 6E 65 74 77 6F 72 6B 5F 61
   63 6B 22 2C 22 72 65 71 75 65 73 74 5F 69 64 22 3A 22 52 45 51 2D 4E 45 54 2D 30 30 31 22 2C 22
   72 65 73 75 6C 74 22 3A 22 4F 4B 22 2C 22 65 72 72 6F 72 5F 63 6F 64 65 22 3A 6E 75 6C 6C 2C 22
   6D 65 73 73 61 67 65 22 3A 6E 75 6C 6C 2C 22 64 61 74 61 22 3A 7B 22 69 70 22 3A 22 31 39 32 2E
   31 36 38 2E 31 2E 31 30 30 22 7D 7D E5 E2
   ```

#### 预期结果

- UART2收到分片封装后的configure_network_ack。

  ```json
  {
    "cmd": "configure_network_ack",
    "request_id": "REQ-NET-001",
    "result": "OK",
    "data": {
      "ip": "192.168.1.100"
    }
  }
  ```

### V851-NET-STATUS-001 — V851网络状态转回蓝牙

- 分类：`bridge`
- 前置条件：PB-03F处于透传状态
- 说明：network_status属于允许转回蓝牙的异步消息。

#### 发送步骤

1. 发送 V851 JSON

   JSON：

   ```json
   {
     "cmd": "network_status",
     "request_id": "REQ-NET-001",
     "result": "OK",
     "data": {
       "connected": true,
       "rssi": -45
     }
   }
   ```

   实际JSON长度：`102` 字节。

   完整帧：`109` 字节，帧尾CRC字段 `B4F4`。

   ```text
   AA 55 00 69 A1 7B 22 63 6D 64 22 3A 22 6E 65 74 77 6F 72 6B 5F 73 74 61 74 75 73 22 2C 22 72 65
   71 75 65 73 74 5F 69 64 22 3A 22 52 45 51 2D 4E 45 54 2D 30 30 31 22 2C 22 72 65 73 75 6C 74 22
   3A 22 4F 4B 22 2C 22 64 61 74 61 22 3A 7B 22 63 6F 6E 6E 65 63 74 65 64 22 3A 74 72 75 65 2C 22
   72 73 73 69 22 3A 2D 34 35 7D 7D B4 F4
   ```

#### 预期结果

- UART2收到network_status。

  ```json
  {
    "cmd": "network_status",
    "data": {
      "connected": true,
      "rssi": -45
    }
  }
  ```

### BLE-UNSUPPORTED-001 — 蓝牙不支持的业务命令

- 分类：`ble_negative`
- 前置条件：PB-03F处于透传状态
- 说明：除get_device_info和configure_network外的命令由T5L直接拒绝。

#### 发送步骤

1. 发送 BLE JSON

   JSON：

   ```json
   {
     "cmd": "reboot",
     "request_id": "REQ-BAD-001",
     "data": {}
   }
   ```

   实际JSON长度：`53` 字节。

   完整帧：`67` 字节，帧尾CRC字段 `4A04`。

   ```text
   4D 51 01 01 01 03 00 00 00 01 00 35 7B 22 63 6D 64 22 3A 22 72 65 62 6F 6F 74 22 2C 22 72 65 71
   75 65 73 74 5F 69 64 22 3A 22 52 45 51 2D 42 41 44 2D 30 30 31 22 2C 22 64 61 74 61 22 3A 7B 7D
   7D 4A 04
   ```

#### 预期结果

- UART2返回UNSUPPORTED_CMD。

  ```json
  {
    "cmd": "reboot_ack",
    "request_id": "REQ-BAD-001",
    "result": "FAIL",
    "error_code": "UNSUPPORTED_CMD",
    "message": "command is not supported by T5L"
  }
  ```

### BLE-FIELDS-001 — 蓝牙JSON缺少必要字段

- 分类：`ble_negative`
- 前置条件：PB-03F处于透传状态
- 说明：缺少cmd或request_id时返回INVALID_PAYLOAD。

#### 发送步骤

1. 发送 BLE JSON

   JSON：

   ```json
   {
     "request_id": "REQ-BAD-002",
     "data": {}
   }
   ```

   实际JSON长度：`38` 字节。

   完整帧：`52` 字节，帧尾CRC字段 `92F8`。

   ```text
   4D 51 01 01 01 04 00 00 00 01 00 26 7B 22 72 65 71 75 65 73 74 5F 69 64 22 3A 22 52 45 51 2D 42
   41 44 2D 30 30 32 22 2C 22 64 61 74 61 22 3A 7B 7D 7D 92 F8
   ```

#### 预期结果

- UART2返回unknown_ack和INVALID_PAYLOAD。

  ```json
  {
    "cmd": "unknown_ack",
    "request_id": null,
    "result": "FAIL",
    "error_code": "INVALID_PAYLOAD",
    "message": "cmd or request_id is missing"
  }
  ```

### V851-CTL-EXHAUST — 排风控制命令

- 分类：`v851_control`
- 前置条件：V851-TIME-001通过
- 说明：验证exhaust.set参数写入0x3000并返回成功应答。

#### 发送步骤

1. 发送 V851 JSON

   JSON：

   ```json
   {
     "msg_id": "MSG-EXHAUST",
     "msg_type": "server.command",
     "data": {
       "command_id": "CMD-EXHAUST",
       "cmd": "exhaust.set",
       "expire_at": 0,
       "params": {
         "enabled": true,
         "level": 3
       }
     }
   }
   ```

   实际JSON长度：`158` 字节。

   完整帧：`165` 字节，帧尾CRC字段 `1312`。

   ```text
   AA 55 00 A1 A1 7B 22 6D 73 67 5F 69 64 22 3A 22 4D 53 47 2D 45 58 48 41 55 53 54 22 2C 22 6D 73
   67 5F 74 79 70 65 22 3A 22 73 65 72 76 65 72 2E 63 6F 6D 6D 61 6E 64 22 2C 22 64 61 74 61 22 3A
   7B 22 63 6F 6D 6D 61 6E 64 5F 69 64 22 3A 22 43 4D 44 2D 45 58 48 41 55 53 54 22 2C 22 63 6D 64
   22 3A 22 65 78 68 61 75 73 74 2E 73 65 74 22 2C 22 65 78 70 69 72 65 5F 61 74 22 3A 30 2C 22 70
   61 72 61 6D 73 22 3A 7B 22 65 6E 61 62 6C 65 64 22 3A 74 72 75 65 2C 22 6C 65 76 65 6C 22 3A 33
   7D 7D 7D 13 12
   ```

#### 预期结果

- UART4返回SUCCESS及实际应用参数。

  ```json
  {
    "msg_type": "device.command_ack",
    "device_sn": "SN-TEST-0001",
    "ble_id": "ABC234",
    "data": {
      "command_id": "CMD-EXHAUST",
      "server_msg_id": "MSG-EXHAUST",
      "status": "SUCCESS",
      "result": {
        "cmd": "exhaust.set",
        "executed": true,
        "applied": {
          "enabled": true,
          "level": 3
        }
      }
    }
  }
  ```

#### Keil调试器检查

- 0x3000为1、0x3001为3；V851ControlInfoGet(V851_CONTROL_EXHAUST)有效且updated=1。

### V851-CTL-PLASMA — 等离子控制命令

- 分类：`v851_control`
- 前置条件：身份与时间已缓存
- 说明：验证plasma.set控制类型。

#### 发送步骤

1. 发送 V851 JSON

   JSON：

   ```json
   {
     "msg_id": "MSG-PLASMA",
     "msg_type": "server.command",
     "data": {
       "command_id": "CMD-PLASMA",
       "cmd": "plasma.set",
       "expire_at": 0,
       "params": {
         "enabled": true
       }
     }
   }
   ```

   实际JSON长度：`145` 字节。

   完整帧：`152` 字节，帧尾CRC字段 `ACBD`。

   ```text
   AA 55 00 94 A1 7B 22 6D 73 67 5F 69 64 22 3A 22 4D 53 47 2D 50 4C 41 53 4D 41 22 2C 22 6D 73 67
   5F 74 79 70 65 22 3A 22 73 65 72 76 65 72 2E 63 6F 6D 6D 61 6E 64 22 2C 22 64 61 74 61 22 3A 7B
   22 63 6F 6D 6D 61 6E 64 5F 69 64 22 3A 22 43 4D 44 2D 50 4C 41 53 4D 41 22 2C 22 63 6D 64 22 3A
   22 70 6C 61 73 6D 61 2E 73 65 74 22 2C 22 65 78 70 69 72 65 5F 61 74 22 3A 30 2C 22 70 61 72 61
   6D 73 22 3A 7B 22 65 6E 61 62 6C 65 64 22 3A 74 72 75 65 7D 7D 7D AC BD
   ```

#### 预期结果

- UART4返回UNSUPPORTED_CMD。

  ```json
  {
    "msg_type": "device.command_ack",
    "data": {
      "command_id": "CMD-PLASMA",
      "status": "UNSUPPORTED",
      "error_code": "UNSUPPORTED_CMD"
    }
  }
  ```

#### Keil调试器检查

- V851_CONTROL_PLASMA快照valid=1、updated=1。

### V851-CTL-ANION — 负离子控制命令

- 分类：`v851_control`
- 前置条件：身份与时间已缓存
- 说明：验证anion.set控制类型。

#### 发送步骤

1. 发送 V851 JSON

   JSON：

   ```json
   {
     "msg_id": "MSG-ANION",
     "msg_type": "server.command",
     "data": {
       "command_id": "CMD-ANION",
       "cmd": "anion.set",
       "expire_at": 0,
       "params": {
         "enabled": true
       }
     }
   }
   ```

   实际JSON长度：`142` 字节。

   完整帧：`149` 字节，帧尾CRC字段 `C6E8`。

   ```text
   AA 55 00 91 A1 7B 22 6D 73 67 5F 69 64 22 3A 22 4D 53 47 2D 41 4E 49 4F 4E 22 2C 22 6D 73 67 5F
   74 79 70 65 22 3A 22 73 65 72 76 65 72 2E 63 6F 6D 6D 61 6E 64 22 2C 22 64 61 74 61 22 3A 7B 22
   63 6F 6D 6D 61 6E 64 5F 69 64 22 3A 22 43 4D 44 2D 41 4E 49 4F 4E 22 2C 22 63 6D 64 22 3A 22 61
   6E 69 6F 6E 2E 73 65 74 22 2C 22 65 78 70 69 72 65 5F 61 74 22 3A 30 2C 22 70 61 72 61 6D 73 22
   3A 7B 22 65 6E 61 62 6C 65 64 22 3A 74 72 75 65 7D 7D 7D C6 E8
   ```

#### 预期结果

- UART4返回UNSUPPORTED_CMD。

  ```json
  {
    "msg_type": "device.command_ack",
    "data": {
      "command_id": "CMD-ANION",
      "status": "UNSUPPORTED",
      "error_code": "UNSUPPORTED_CMD"
    }
  }
  ```

#### Keil调试器检查

- V851_CONTROL_ANION快照valid=1、updated=1。

### V851-CTL-CLIMATE — 温控命令

- 分类：`v851_control`
- 前置条件：身份与时间已缓存
- 说明：验证climate.set参数写入0x3010并返回成功应答。

#### 发送步骤

1. 发送 V851 JSON

   JSON：

   ```json
   {
     "msg_id": "MSG-CLIMATE",
     "msg_type": "server.command",
     "data": {
       "command_id": "CMD-CLIMATE",
       "cmd": "climate.set",
       "expire_at": 0,
       "params": {
         "enabled": true,
         "mode": "AUTO",
         "target_temperature": 24
       }
     }
   }
   ```

   实际JSON长度：`186` 字节。

   完整帧：`193` 字节，帧尾CRC字段 `6E95`。

   ```text
   AA 55 00 BD A1 7B 22 6D 73 67 5F 69 64 22 3A 22 4D 53 47 2D 43 4C 49 4D 41 54 45 22 2C 22 6D 73
   67 5F 74 79 70 65 22 3A 22 73 65 72 76 65 72 2E 63 6F 6D 6D 61 6E 64 22 2C 22 64 61 74 61 22 3A
   7B 22 63 6F 6D 6D 61 6E 64 5F 69 64 22 3A 22 43 4D 44 2D 43 4C 49 4D 41 54 45 22 2C 22 63 6D 64
   22 3A 22 63 6C 69 6D 61 74 65 2E 73 65 74 22 2C 22 65 78 70 69 72 65 5F 61 74 22 3A 30 2C 22 70
   61 72 61 6D 73 22 3A 7B 22 65 6E 61 62 6C 65 64 22 3A 74 72 75 65 2C 22 6D 6F 64 65 22 3A 22 41
   55 54 4F 22 2C 22 74 61 72 67 65 74 5F 74 65 6D 70 65 72 61 74 75 72 65 22 3A 32 34 7D 7D 7D 6E
   95
   ```

#### 预期结果

- UART4返回SUCCESS及实际应用参数。

  ```json
  {
    "msg_type": "device.command_ack",
    "data": {
      "command_id": "CMD-CLIMATE",
      "status": "SUCCESS",
      "result": {
        "cmd": "climate.set",
        "executed": true,
        "applied": {
          "enabled": true,
          "mode": "AUTO",
          "target_temperature": 24
        }
      }
    }
  }
  ```

#### Keil调试器检查

- 0x3010为1、0x3011为24、0x3012开始为AUTO；控制快照updated=1。

### V851-CTL-INLET — 进风风机控制命令

- 分类：`v851_control`
- 前置条件：身份与时间已缓存
- 说明：验证inlet_fan.set控制类型。

#### 发送步骤

1. 发送 V851 JSON

   JSON：

   ```json
   {
     "msg_id": "MSG-INLET",
     "msg_type": "server.command",
     "data": {
       "command_id": "CMD-INLET",
       "cmd": "inlet_fan.set",
       "expire_at": 0,
       "params": {
         "enabled": true,
         "level": 2
       }
     }
   }
   ```

   实际JSON长度：`156` 字节。

   完整帧：`163` 字节，帧尾CRC字段 `BBA2`。

   ```text
   AA 55 00 9F A1 7B 22 6D 73 67 5F 69 64 22 3A 22 4D 53 47 2D 49 4E 4C 45 54 22 2C 22 6D 73 67 5F
   74 79 70 65 22 3A 22 73 65 72 76 65 72 2E 63 6F 6D 6D 61 6E 64 22 2C 22 64 61 74 61 22 3A 7B 22
   63 6F 6D 6D 61 6E 64 5F 69 64 22 3A 22 43 4D 44 2D 49 4E 4C 45 54 22 2C 22 63 6D 64 22 3A 22 69
   6E 6C 65 74 5F 66 61 6E 2E 73 65 74 22 2C 22 65 78 70 69 72 65 5F 61 74 22 3A 30 2C 22 70 61 72
   61 6D 73 22 3A 7B 22 65 6E 61 62 6C 65 64 22 3A 74 72 75 65 2C 22 6C 65 76 65 6C 22 3A 32 7D 7D
   7D BB A2
   ```

#### 预期结果

- UART4返回UNSUPPORTED_CMD。

  ```json
  {
    "msg_type": "device.command_ack",
    "data": {
      "command_id": "CMD-INLET",
      "status": "UNSUPPORTED",
      "error_code": "UNSUPPORTED_CMD"
    }
  }
  ```

#### Keil调试器检查

- V851_CONTROL_INLET_FAN快照valid=1、updated=1。

### V851-CTL-HUMIDIFIER — 加湿控制命令

- 分类：`v851_control`
- 前置条件：身份与时间已缓存
- 说明：验证humidifier.set参数写入0x3040并返回成功应答。

#### 发送步骤

1. 发送 V851 JSON

   JSON：

   ```json
   {
     "msg_id": "MSG-HUMIDIFIER",
     "msg_type": "server.command",
     "data": {
       "command_id": "CMD-HUMIDIFIER",
       "cmd": "humidifier.set",
       "expire_at": 0,
       "params": {
         "enabled": true,
         "target_humidity": 55
       }
     }
   }
   ```

   实际JSON长度：`178` 字节。

   完整帧：`185` 字节，帧尾CRC字段 `130A`。

   ```text
   AA 55 00 B5 A1 7B 22 6D 73 67 5F 69 64 22 3A 22 4D 53 47 2D 48 55 4D 49 44 49 46 49 45 52 22 2C
   22 6D 73 67 5F 74 79 70 65 22 3A 22 73 65 72 76 65 72 2E 63 6F 6D 6D 61 6E 64 22 2C 22 64 61 74
   61 22 3A 7B 22 63 6F 6D 6D 61 6E 64 5F 69 64 22 3A 22 43 4D 44 2D 48 55 4D 49 44 49 46 49 45 52
   22 2C 22 63 6D 64 22 3A 22 68 75 6D 69 64 69 66 69 65 72 2E 73 65 74 22 2C 22 65 78 70 69 72 65
   5F 61 74 22 3A 30 2C 22 70 61 72 61 6D 73 22 3A 7B 22 65 6E 61 62 6C 65 64 22 3A 74 72 75 65 2C
   22 74 61 72 67 65 74 5F 68 75 6D 69 64 69 74 79 22 3A 35 35 7D 7D 7D 13 0A
   ```

#### 预期结果

- UART4返回SUCCESS及实际应用参数。

  ```json
  {
    "msg_type": "device.command_ack",
    "data": {
      "command_id": "CMD-HUMIDIFIER",
      "server_msg_id": "MSG-HUMIDIFIER",
      "status": "SUCCESS",
      "result": {
        "cmd": "humidifier.set",
        "executed": true,
        "applied": {
          "enabled": true,
          "target_humidity": 55
        }
      }
    }
  }
  ```

#### Keil调试器检查

- 0x3040为1、0x3041为55；V851_CONTROL_HUMIDIFIER快照valid=1、updated=1。

### V851-CTL-UVB — UVB控制命令

- 分类：`v851_control`
- 前置条件：身份与时间已缓存
- 说明：验证uvb.set控制类型。

#### 发送步骤

1. 发送 V851 JSON

   JSON：

   ```json
   {
     "msg_id": "MSG-UVB",
     "msg_type": "server.command",
     "data": {
       "command_id": "CMD-UVB",
       "cmd": "uvb.set",
       "expire_at": 0,
       "params": {
         "enabled": true,
         "duration_minutes": 15
       }
     }
   }
   ```

   实际JSON长度：`158` 字节。

   完整帧：`165` 字节，帧尾CRC字段 `8FEF`。

   ```text
   AA 55 00 A1 A1 7B 22 6D 73 67 5F 69 64 22 3A 22 4D 53 47 2D 55 56 42 22 2C 22 6D 73 67 5F 74 79
   70 65 22 3A 22 73 65 72 76 65 72 2E 63 6F 6D 6D 61 6E 64 22 2C 22 64 61 74 61 22 3A 7B 22 63 6F
   6D 6D 61 6E 64 5F 69 64 22 3A 22 43 4D 44 2D 55 56 42 22 2C 22 63 6D 64 22 3A 22 75 76 62 2E 73
   65 74 22 2C 22 65 78 70 69 72 65 5F 61 74 22 3A 30 2C 22 70 61 72 61 6D 73 22 3A 7B 22 65 6E 61
   62 6C 65 64 22 3A 74 72 75 65 2C 22 64 75 72 61 74 69 6F 6E 5F 6D 69 6E 75 74 65 73 22 3A 31 35
   7D 7D 7D 8F EF
   ```

#### 预期结果

- UART4返回UNSUPPORTED_CMD。

  ```json
  {
    "msg_type": "device.command_ack",
    "data": {
      "command_id": "CMD-UVB",
      "status": "UNSUPPORTED",
      "error_code": "UNSUPPORTED_CMD"
    }
  }
  ```

#### Keil调试器检查

- V851_CONTROL_UVB快照valid=1、updated=1。

### V851-CTL-LIGHT — 灯光控制命令

- 分类：`v851_control`
- 前置条件：身份与时间已缓存
- 说明：验证light.set参数写入0x3020并返回成功应答。

#### 发送步骤

1. 发送 V851 JSON

   JSON：

   ```json
   {
     "msg_id": "MSG-LIGHT",
     "msg_type": "server.command",
     "data": {
       "command_id": "CMD-LIGHT",
       "cmd": "light.set",
       "expire_at": 0,
       "params": {
         "enabled": true,
         "brightness": 80,
         "color_temperature": 4500
       }
     }
   }
   ```

   实际JSON长度：`183` 字节。

   完整帧：`190` 字节，帧尾CRC字段 `C5A2`。

   ```text
   AA 55 00 BA A1 7B 22 6D 73 67 5F 69 64 22 3A 22 4D 53 47 2D 4C 49 47 48 54 22 2C 22 6D 73 67 5F
   74 79 70 65 22 3A 22 73 65 72 76 65 72 2E 63 6F 6D 6D 61 6E 64 22 2C 22 64 61 74 61 22 3A 7B 22
   63 6F 6D 6D 61 6E 64 5F 69 64 22 3A 22 43 4D 44 2D 4C 49 47 48 54 22 2C 22 63 6D 64 22 3A 22 6C
   69 67 68 74 2E 73 65 74 22 2C 22 65 78 70 69 72 65 5F 61 74 22 3A 30 2C 22 70 61 72 61 6D 73 22
   3A 7B 22 65 6E 61 62 6C 65 64 22 3A 74 72 75 65 2C 22 62 72 69 67 68 74 6E 65 73 73 22 3A 38 30
   2C 22 63 6F 6C 6F 72 5F 74 65 6D 70 65 72 61 74 75 72 65 22 3A 34 35 30 30 7D 7D 7D C5 A2
   ```

#### 预期结果

- UART4返回SUCCESS及实际应用参数。

  ```json
  {
    "msg_type": "device.command_ack",
    "data": {
      "command_id": "CMD-LIGHT",
      "status": "SUCCESS",
      "result": {
        "cmd": "light.set",
        "executed": true,
        "applied": {
          "enabled": true,
          "brightness": 80,
          "color_temperature": 4500
        }
      }
    }
  }
  ```

#### Keil调试器检查

- 0x3020为1、0x3021为80、0x3022为4500；控制快照updated=1。

### V851-CTL-SETTINGS — 设备设置命令

- 分类：`v851_control`
- 前置条件：身份与时间已缓存
- 说明：验证device_settings.set参数写入0x3030并返回成功应答。

#### 发送步骤

1. 发送 V851 JSON

   JSON：

   ```json
   {
     "msg_id": "MSG-SETTINGS",
     "msg_type": "server.command",
     "data": {
       "command_id": "CMD-SETTINGS",
       "cmd": "device_settings.set",
       "expire_at": 0,
       "params": {
         "language": "zh-CN",
         "volume": 60,
         "screen_brightness": 70
       }
     }
   }
   ```

   实际JSON长度：`197` 字节。

   完整帧：`204` 字节，帧尾CRC字段 `7B15`。

   ```text
   AA 55 00 C8 A1 7B 22 6D 73 67 5F 69 64 22 3A 22 4D 53 47 2D 53 45 54 54 49 4E 47 53 22 2C 22 6D
   73 67 5F 74 79 70 65 22 3A 22 73 65 72 76 65 72 2E 63 6F 6D 6D 61 6E 64 22 2C 22 64 61 74 61 22
   3A 7B 22 63 6F 6D 6D 61 6E 64 5F 69 64 22 3A 22 43 4D 44 2D 53 45 54 54 49 4E 47 53 22 2C 22 63
   6D 64 22 3A 22 64 65 76 69 63 65 5F 73 65 74 74 69 6E 67 73 2E 73 65 74 22 2C 22 65 78 70 69 72
   65 5F 61 74 22 3A 30 2C 22 70 61 72 61 6D 73 22 3A 7B 22 6C 61 6E 67 75 61 67 65 22 3A 22 7A 68
   2D 43 4E 22 2C 22 76 6F 6C 75 6D 65 22 3A 36 30 2C 22 73 63 72 65 65 6E 5F 62 72 69 67 68 74 6E
   65 73 73 22 3A 37 30 7D 7D 7D 7B 15
   ```

#### 预期结果

- UART4返回SUCCESS及实际应用参数。

  ```json
  {
    "msg_type": "device.command_ack",
    "data": {
      "command_id": "CMD-SETTINGS",
      "status": "SUCCESS",
      "result": {
        "cmd": "device_settings.set",
        "executed": true,
        "applied": {
          "language": "zh-CN",
          "volume": 60,
          "screen_brightness": 70
        }
      }
    }
  }
  ```

#### Keil调试器检查

- 0x3030为60、0x3031为70、0x3032开始为zh-CN；控制快照updated=1。

### V851-CTL-INVALID-MISSING — 控制参数缺少必填字段

- 分类：`v851_control`
- 前置条件：身份与时间已缓存
- 说明：排风命令缺少level时拒绝执行且不改写0x3000。

#### 发送步骤

1. 发送 V851 JSON

   JSON：

   ```json
   {
     "msg_id": "MSG-INVALID-MISSING",
     "msg_type": "server.command",
     "data": {
       "command_id": "CMD-INVALID-MISSING",
       "cmd": "exhaust.set",
       "expire_at": 0,
       "params": {
         "enabled": true
       }
     }
   }
   ```

   实际JSON长度：`164` 字节。

   完整帧：`171` 字节，帧尾CRC字段 `386A`。

   ```text
   AA 55 00 A7 A1 7B 22 6D 73 67 5F 69 64 22 3A 22 4D 53 47 2D 49 4E 56 41 4C 49 44 2D 4D 49 53 53
   49 4E 47 22 2C 22 6D 73 67 5F 74 79 70 65 22 3A 22 73 65 72 76 65 72 2E 63 6F 6D 6D 61 6E 64 22
   2C 22 64 61 74 61 22 3A 7B 22 63 6F 6D 6D 61 6E 64 5F 69 64 22 3A 22 43 4D 44 2D 49 4E 56 41 4C
   49 44 2D 4D 49 53 53 49 4E 47 22 2C 22 63 6D 64 22 3A 22 65 78 68 61 75 73 74 2E 73 65 74 22 2C
   22 65 78 70 69 72 65 5F 61 74 22 3A 30 2C 22 70 61 72 61 6D 73 22 3A 7B 22 65 6E 61 62 6C 65 64
   22 3A 74 72 75 65 7D 7D 7D 38 6A
   ```

#### 预期结果

- UART4返回FAILED/INVALID_PARAMS。

  ```json
  {
    "msg_type": "device.command_ack",
    "data": {
      "command_id": "CMD-INVALID-MISSING",
      "status": "FAILED",
      "error_code": "INVALID_PARAMS"
    }
  }
  ```

#### Keil调试器检查

- 0x3000控制槽保持上一条有效排风控制值，不被缺字段命令清空。

### V851-CTL-INVALID-BOOL — 控制布尔字段类型错误

- 分类：`v851_control`
- 前置条件：身份与时间已缓存
- 说明：加湿enabled为字符串时拒绝执行且不改写0x3040。

#### 发送步骤

1. 发送 V851 JSON

   JSON：

   ```json
   {
     "msg_id": "MSG-INVALID-BOOL",
     "msg_type": "server.command",
     "data": {
       "command_id": "CMD-INVALID-BOOL",
       "cmd": "humidifier.set",
       "expire_at": 0,
       "params": {
         "enabled": "true",
         "target_humidity": 55
       }
     }
   }
   ```

   实际JSON长度：`184` 字节。

   完整帧：`191` 字节，帧尾CRC字段 `4D97`。

   ```text
   AA 55 00 BB A1 7B 22 6D 73 67 5F 69 64 22 3A 22 4D 53 47 2D 49 4E 56 41 4C 49 44 2D 42 4F 4F 4C
   22 2C 22 6D 73 67 5F 74 79 70 65 22 3A 22 73 65 72 76 65 72 2E 63 6F 6D 6D 61 6E 64 22 2C 22 64
   61 74 61 22 3A 7B 22 63 6F 6D 6D 61 6E 64 5F 69 64 22 3A 22 43 4D 44 2D 49 4E 56 41 4C 49 44 2D
   42 4F 4F 4C 22 2C 22 63 6D 64 22 3A 22 68 75 6D 69 64 69 66 69 65 72 2E 73 65 74 22 2C 22 65 78
   70 69 72 65 5F 61 74 22 3A 30 2C 22 70 61 72 61 6D 73 22 3A 7B 22 65 6E 61 62 6C 65 64 22 3A 22
   74 72 75 65 22 2C 22 74 61 72 67 65 74 5F 68 75 6D 69 64 69 74 79 22 3A 35 35 7D 7D 7D 4D 97
   ```

#### 预期结果

- UART4返回FAILED/INVALID_PARAMS。

  ```json
  {
    "msg_type": "device.command_ack",
    "data": {
      "command_id": "CMD-INVALID-BOOL",
      "status": "FAILED",
      "error_code": "INVALID_PARAMS"
    }
  }
  ```

#### Keil调试器检查

- 0x3040控制槽保持上一条有效加湿控制值。

### V851-CTL-INVALID-TEXT — 控制文本字段过长

- 分类：`v851_control`
- 前置条件：身份与时间已缓存
- 说明：设备语言超过8字节时拒绝执行且不改写0x3030。

#### 发送步骤

1. 发送 V851 JSON

   JSON：

   ```json
   {
     "msg_id": "MSG-INVALID-TEXT",
     "msg_type": "server.command",
     "data": {
       "command_id": "CMD-INVALID-TEXT",
       "cmd": "device_settings.set",
       "expire_at": 0,
       "params": {
         "language": "zh-CN-EXT",
         "volume": 60,
         "screen_brightness": 70
       }
     }
   }
   ```

   实际JSON长度：`209` 字节。

   完整帧：`216` 字节，帧尾CRC字段 `3125`。

   ```text
   AA 55 00 D4 A1 7B 22 6D 73 67 5F 69 64 22 3A 22 4D 53 47 2D 49 4E 56 41 4C 49 44 2D 54 45 58 54
   22 2C 22 6D 73 67 5F 74 79 70 65 22 3A 22 73 65 72 76 65 72 2E 63 6F 6D 6D 61 6E 64 22 2C 22 64
   61 74 61 22 3A 7B 22 63 6F 6D 6D 61 6E 64 5F 69 64 22 3A 22 43 4D 44 2D 49 4E 56 41 4C 49 44 2D
   54 45 58 54 22 2C 22 63 6D 64 22 3A 22 64 65 76 69 63 65 5F 73 65 74 74 69 6E 67 73 2E 73 65 74
   22 2C 22 65 78 70 69 72 65 5F 61 74 22 3A 30 2C 22 70 61 72 61 6D 73 22 3A 7B 22 6C 61 6E 67 75
   61 67 65 22 3A 22 7A 68 2D 43 4E 2D 45 58 54 22 2C 22 76 6F 6C 75 6D 65 22 3A 36 30 2C 22 73 63
   72 65 65 6E 5F 62 72 69 67 68 74 6E 65 73 73 22 3A 37 30 7D 7D 7D 31 25
   ```

#### 预期结果

- UART4返回FAILED/INVALID_PARAMS。

  ```json
  {
    "msg_type": "device.command_ack",
    "data": {
      "command_id": "CMD-INVALID-TEXT",
      "status": "FAILED",
      "error_code": "INVALID_PARAMS"
    }
  }
  ```

#### Keil调试器检查

- 0x3030控制槽保持上一条有效设备设置值。

### V851-CTL-INVALID-RANGE — 控制数值超出VP范围

- 分类：`v851_control`
- 前置条件：身份与时间已缓存
- 说明：灯光brightness超过65535时拒绝执行且不改写0x3020。

#### 发送步骤

1. 发送 V851 JSON

   JSON：

   ```json
   {
     "msg_id": "MSG-INVALID-RANGE",
     "msg_type": "server.command",
     "data": {
       "command_id": "CMD-INVALID-RANGE",
       "cmd": "light.set",
       "expire_at": 0,
       "params": {
         "enabled": true,
         "brightness": 65536,
         "color_temperature": 4500
       }
     }
   }
   ```

   实际JSON长度：`202` 字节。

   完整帧：`209` 字节，帧尾CRC字段 `02BC`。

   ```text
   AA 55 00 CD A1 7B 22 6D 73 67 5F 69 64 22 3A 22 4D 53 47 2D 49 4E 56 41 4C 49 44 2D 52 41 4E 47
   45 22 2C 22 6D 73 67 5F 74 79 70 65 22 3A 22 73 65 72 76 65 72 2E 63 6F 6D 6D 61 6E 64 22 2C 22
   64 61 74 61 22 3A 7B 22 63 6F 6D 6D 61 6E 64 5F 69 64 22 3A 22 43 4D 44 2D 49 4E 56 41 4C 49 44
   2D 52 41 4E 47 45 22 2C 22 63 6D 64 22 3A 22 6C 69 67 68 74 2E 73 65 74 22 2C 22 65 78 70 69 72
   65 5F 61 74 22 3A 30 2C 22 70 61 72 61 6D 73 22 3A 7B 22 65 6E 61 62 6C 65 64 22 3A 74 72 75 65
   2C 22 62 72 69 67 68 74 6E 65 73 73 22 3A 36 35 35 33 36 2C 22 63 6F 6C 6F 72 5F 74 65 6D 70 65
   72 61 74 75 72 65 22 3A 34 35 30 30 7D 7D 7D 02 BC
   ```

#### 预期结果

- UART4返回FAILED/INVALID_PARAMS。

  ```json
  {
    "msg_type": "device.command_ack",
    "data": {
      "command_id": "CMD-INVALID-RANGE",
      "status": "FAILED",
      "error_code": "INVALID_PARAMS"
    }
  }
  ```

#### Keil调试器检查

- 0x3020控制槽保持上一条有效灯光控制值。

### V851-CTL-PASSWORD — 设备密码命令脱敏

- 分类：`v851_control`
- 前置条件：身份与时间已缓存
- 说明：验证device_password.set被识别但快照不保存密码明文。

#### 发送步骤

1. 发送 V851 JSON

   JSON：

   ```json
   {
     "msg_id": "MSG-PASSWORD",
     "msg_type": "server.command",
     "data": {
       "command_id": "CMD-PASSWORD",
       "cmd": "device_password.set",
       "expire_at": 0,
       "params": {
         "password": "654321"
       }
     }
   }
   ```

   实际JSON长度：`163` 字节。

   完整帧：`170` 字节，帧尾CRC字段 `7BF7`。

   ```text
   AA 55 00 A6 A1 7B 22 6D 73 67 5F 69 64 22 3A 22 4D 53 47 2D 50 41 53 53 57 4F 52 44 22 2C 22 6D
   73 67 5F 74 79 70 65 22 3A 22 73 65 72 76 65 72 2E 63 6F 6D 6D 61 6E 64 22 2C 22 64 61 74 61 22
   3A 7B 22 63 6F 6D 6D 61 6E 64 5F 69 64 22 3A 22 43 4D 44 2D 50 41 53 53 57 4F 52 44 22 2C 22 63
   6D 64 22 3A 22 64 65 76 69 63 65 5F 70 61 73 73 77 6F 72 64 2E 73 65 74 22 2C 22 65 78 70 69 72
   65 5F 61 74 22 3A 30 2C 22 70 61 72 61 6D 73 22 3A 7B 22 70 61 73 73 77 6F 72 64 22 3A 22 36 35
   34 33 32 31 22 7D 7D 7D 7B F7
   ```

#### 预期结果

- UART4返回UNSUPPORTED_CMD。

  ```json
  {
    "msg_type": "device.command_ack",
    "data": {
      "command_id": "CMD-PASSWORD",
      "status": "UNSUPPORTED",
      "error_code": "UNSUPPORTED_CMD"
    }
  }
  ```

#### Keil调试器检查

- V851_CONTROL_DEVICE_PASSWORD快照params_redacted=1、stored_len=0，内存中不出现654321。

### V851-CTL-OTA — 应用层OTA关闭时的合法升级命令

- 分类：`v851_control`
- 前置条件：otaOTA_ENABLED为0
- 说明：合法ota.upgrade参数通过校验后，因为应用层OTA关闭而返回UNSUPPORTED。

#### 发送步骤

1. 发送 V851 JSON

   JSON：

   ```json
   {
     "msg_id": "MSG-OTA",
     "msg_type": "server.command",
     "data": {
       "command_id": "CMD-OTA",
       "cmd": "ota.upgrade",
       "expire_at": 0,
       "params": {
         "firmware_id": 1001,
         "firmware_version": "2.0.0",
         "firmware_url": "https://example.invalid/fw.bin",
         "size_bytes": 4096,
         "sha256": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
       }
     }
   }
   ```

   实际JSON长度：`313` 字节。

   完整帧：`320` 字节，帧尾CRC字段 `E4ED`。

   ```text
   AA 55 01 3C A1 7B 22 6D 73 67 5F 69 64 22 3A 22 4D 53 47 2D 4F 54 41 22 2C 22 6D 73 67 5F 74 79
   70 65 22 3A 22 73 65 72 76 65 72 2E 63 6F 6D 6D 61 6E 64 22 2C 22 64 61 74 61 22 3A 7B 22 63 6F
   6D 6D 61 6E 64 5F 69 64 22 3A 22 43 4D 44 2D 4F 54 41 22 2C 22 63 6D 64 22 3A 22 6F 74 61 2E 75
   70 67 72 61 64 65 22 2C 22 65 78 70 69 72 65 5F 61 74 22 3A 30 2C 22 70 61 72 61 6D 73 22 3A 7B
   22 66 69 72 6D 77 61 72 65 5F 69 64 22 3A 31 30 30 31 2C 22 66 69 72 6D 77 61 72 65 5F 76 65 72
   73 69 6F 6E 22 3A 22 32 2E 30 2E 30 22 2C 22 66 69 72 6D 77 61 72 65 5F 75 72 6C 22 3A 22 68 74
   74 70 73 3A 2F 2F 65 78 61 6D 70 6C 65 2E 69 6E 76 61 6C 69 64 2F 66 77 2E 62 69 6E 22 2C 22 73
   69 7A 65 5F 62 79 74 65 73 22 3A 34 30 39 36 2C 22 73 68 61 32 35 36 22 3A 22 30 31 32 33 34 35
   36 37 38 39 61 62 63 64 65 66 30 31 32 33 34 35 36 37 38 39 61 62 63 64 65 66 30 31 32 33 34 35
   36 37 38 39 61 62 63 64 65 66 30 31 32 33 34 35 36 37 38 39 61 62 63 64 65 66 22 7D 7D 7D E4 ED
   ```

#### 预期结果

- UART4明确返回T5L OTA is disabled，不进入AB CD固件数据流。

  ```json
  {
    "msg_type": "device.command_ack",
    "data": {
      "command_id": "CMD-OTA",
      "status": "UNSUPPORTED",
      "error_code": "UNSUPPORTED_CMD",
      "error_message": "T5L OTA is disabled"
    }
  }
  ```

#### Keil调试器检查

- V851_CONTROL_OTA快照valid=1、updated=1，应用OTA状态未启动。

### V851-UNKNOWN-001 — 未知V851控制命令

- 分类：`v851_negative`
- 前置条件：身份与时间已缓存
- 说明：未知cmd不映射到任何控制类型，也不创建控制快照。

#### 发送步骤

1. 发送 V851 JSON

   JSON：

   ```json
   {
     "msg_id": "MSG-UNKNOWN",
     "msg_type": "server.command",
     "data": {
       "command_id": "CMD-UNKNOWN",
       "cmd": "unknown.set",
       "expire_at": 0,
       "params": {
         "value": 1
       }
     }
   }
   ```

   实际JSON长度：`143` 字节。

   完整帧：`150` 字节，帧尾CRC字段 `9833`。

   ```text
   AA 55 00 92 A1 7B 22 6D 73 67 5F 69 64 22 3A 22 4D 53 47 2D 55 4E 4B 4E 4F 57 4E 22 2C 22 6D 73
   67 5F 74 79 70 65 22 3A 22 73 65 72 76 65 72 2E 63 6F 6D 6D 61 6E 64 22 2C 22 64 61 74 61 22 3A
   7B 22 63 6F 6D 6D 61 6E 64 5F 69 64 22 3A 22 43 4D 44 2D 55 4E 4B 4E 4F 57 4E 22 2C 22 63 6D 64
   22 3A 22 75 6E 6B 6E 6F 77 6E 2E 73 65 74 22 2C 22 65 78 70 69 72 65 5F 61 74 22 3A 30 2C 22 70
   61 72 61 6D 73 22 3A 7B 22 76 61 6C 75 65 22 3A 31 7D 7D 7D 98 33
   ```

#### 预期结果

- UART4返回UNSUPPORTED_CMD。

  ```json
  {
    "msg_type": "device.command_ack",
    "data": {
      "command_id": "CMD-UNKNOWN",
      "status": "UNSUPPORTED",
      "error_code": "UNSUPPORTED_CMD"
    }
  }
  ```

#### Keil调试器检查

- 没有任何V851ControlDetails条目的revision因CMD-UNKNOWN增加。

### V851-DEDUP-001 — 重复command_id去重

- 分类：`v851_negative`
- 前置条件：身份与时间已缓存
- 说明：连续发送相同command_id，第二次直接复用去重记录，不再次更新控制快照。

#### 发送步骤

1. 发送 V851 JSON

   JSON：

   ```json
   {
     "msg_id": "MSG-DEDUP-1",
     "msg_type": "server.command",
     "data": {
       "command_id": "CMD-DEDUP",
       "cmd": "light.set",
       "expire_at": 0,
       "params": {
         "enabled": true,
         "brightness": 20,
         "color_temperature": 4500
       }
     }
   }
   ```

   实际JSON长度：`185` 字节。

   完整帧：`192` 字节，帧尾CRC字段 `F737`。

   ```text
   AA 55 00 BC A1 7B 22 6D 73 67 5F 69 64 22 3A 22 4D 53 47 2D 44 45 44 55 50 2D 31 22 2C 22 6D 73
   67 5F 74 79 70 65 22 3A 22 73 65 72 76 65 72 2E 63 6F 6D 6D 61 6E 64 22 2C 22 64 61 74 61 22 3A
   7B 22 63 6F 6D 6D 61 6E 64 5F 69 64 22 3A 22 43 4D 44 2D 44 45 44 55 50 22 2C 22 63 6D 64 22 3A
   22 6C 69 67 68 74 2E 73 65 74 22 2C 22 65 78 70 69 72 65 5F 61 74 22 3A 30 2C 22 70 61 72 61 6D
   73 22 3A 7B 22 65 6E 61 62 6C 65 64 22 3A 74 72 75 65 2C 22 62 72 69 67 68 74 6E 65 73 73 22 3A
   32 30 2C 22 63 6F 6C 6F 72 5F 74 65 6D 70 65 72 61 74 75 72 65 22 3A 34 35 30 30 7D 7D 7D F7 37
   ```

2. 等待 100 ms

3. 发送 V851 JSON

   JSON：

   ```json
   {
     "msg_id": "MSG-DEDUP-2",
     "msg_type": "server.command",
     "data": {
       "command_id": "CMD-DEDUP",
       "cmd": "light.set",
       "expire_at": 0,
       "params": {
         "enabled": true,
         "brightness": 99,
         "color_temperature": 5000
       }
     }
   }
   ```

   实际JSON长度：`185` 字节。

   完整帧：`192` 字节，帧尾CRC字段 `853E`。

   ```text
   AA 55 00 BC A1 7B 22 6D 73 67 5F 69 64 22 3A 22 4D 53 47 2D 44 45 44 55 50 2D 32 22 2C 22 6D 73
   67 5F 74 79 70 65 22 3A 22 73 65 72 76 65 72 2E 63 6F 6D 6D 61 6E 64 22 2C 22 64 61 74 61 22 3A
   7B 22 63 6F 6D 6D 61 6E 64 5F 69 64 22 3A 22 43 4D 44 2D 44 45 44 55 50 22 2C 22 63 6D 64 22 3A
   22 6C 69 67 68 74 2E 73 65 74 22 2C 22 65 78 70 69 72 65 5F 61 74 22 3A 30 2C 22 70 61 72 61 6D
   73 22 3A 7B 22 65 6E 61 62 6C 65 64 22 3A 74 72 75 65 2C 22 62 72 69 67 68 74 6E 65 73 73 22 3A
   39 39 2C 22 63 6F 6C 6F 72 5F 74 65 6D 70 65 72 61 74 75 72 65 22 3A 35 30 30 30 7D 7D 7D 85 3E
   ```

#### 预期结果

- 第一次请求执行并返回SUCCESS。

  ```json
  {
    "msg_type": "device.command_ack",
    "data": {
      "command_id": "CMD-DEDUP",
      "status": "SUCCESS"
    }
  }
  ```
- 第二次请求返回相同状态，不重新执行。

  ```json
  {
    "msg_type": "device.command_ack",
    "data": {
      "command_id": "CMD-DEDUP",
      "status": "SUCCESS"
    }
  }
  ```

#### Keil调试器检查

- V851_CONTROL_LIGHT的revision只增加1，0x3021保持20且0x3022保持4500。

### V851-EXPIRED-001 — 已过期控制命令

- 分类：`v851_negative`
- 前置条件：V851-TIME-001已把服务器时间设置为2000000000
- 说明：expire_at早于当前服务器时间时拒绝命令且不更新快照。

#### 发送步骤

1. 发送 V851 JSON

   JSON：

   ```json
   {
     "msg_id": "MSG-EXPIRED",
     "msg_type": "server.command",
     "data": {
       "command_id": "CMD-EXPIRED",
       "cmd": "exhaust.set",
       "expire_at": 1999999999,
       "params": {
         "enabled": false
       }
     }
   }
   ```

   实际JSON长度：`158` 字节。

   完整帧：`165` 字节，帧尾CRC字段 `EA1B`。

   ```text
   AA 55 00 A1 A1 7B 22 6D 73 67 5F 69 64 22 3A 22 4D 53 47 2D 45 58 50 49 52 45 44 22 2C 22 6D 73
   67 5F 74 79 70 65 22 3A 22 73 65 72 76 65 72 2E 63 6F 6D 6D 61 6E 64 22 2C 22 64 61 74 61 22 3A
   7B 22 63 6F 6D 6D 61 6E 64 5F 69 64 22 3A 22 43 4D 44 2D 45 58 50 49 52 45 44 22 2C 22 63 6D 64
   22 3A 22 65 78 68 61 75 73 74 2E 73 65 74 22 2C 22 65 78 70 69 72 65 5F 61 74 22 3A 31 39 39 39
   39 39 39 39 39 39 2C 22 70 61 72 61 6D 73 22 3A 7B 22 65 6E 61 62 6C 65 64 22 3A 66 61 6C 73 65
   7D 7D 7D EA 1B
   ```

#### 预期结果

- UART4返回FAILED/INVALID_PARAMS。

  ```json
  {
    "msg_type": "device.command_ack",
    "data": {
      "command_id": "CMD-EXPIRED",
      "status": "FAILED",
      "error_code": "INVALID_PARAMS",
      "error_message": "command expired"
    }
  }
  ```

#### Keil调试器检查

- V851_CONTROL_EXHAUST的revision不因CMD-EXPIRED增加。

### V851-HEARTBEAT-001 — V851心跳静默处理

- 分类：`bridge`
- 前置条件：V851协议已初始化
- 说明：device.heartbeat属于V851云链路，T5L明确忽略。

#### 发送步骤

1. 发送 V851 JSON

   JSON：

   ```json
   {
     "msg_id": "HEARTBEAT-001",
     "msg_type": "device.heartbeat",
     "timestamp": 2000000001,
     "data": {}
   }
   ```

   实际JSON长度：`89` 字节。

   完整帧：`96` 字节，帧尾CRC字段 `0567`。

   ```text
   AA 55 00 5C A1 7B 22 6D 73 67 5F 69 64 22 3A 22 48 45 41 52 54 42 45 41 54 2D 30 30 31 22 2C 22
   6D 73 67 5F 74 79 70 65 22 3A 22 64 65 76 69 63 65 2E 68 65 61 72 74 62 65 61 74 22 2C 22 74 69
   6D 65 73 74 61 6D 70 22 3A 32 30 30 30 30 30 30 30 30 31 2C 22 64 61 74 61 22 3A 7B 7D 7D 05 67
   ```

#### 预期结果

- UART4无应答。
- UART2无转发。

### BLE-CRC-001 — BLE帧CRC错误

- 分类：`ble_negative`
- 前置条件：PB-03F处于透传状态
- 说明：破坏帧尾CRC，T5L返回BLE_PACKET_INVALID。

#### 发送步骤

1. 发送 BLE JSON

   JSON：

   ```json
   {
     "cmd": "get_device_info",
     "request_id": "REQ-CRC",
     "data": {}
   }
   ```

   实际JSON长度：`58` 字节。

   完整帧：`72` 字节，帧尾CRC字段 `19A7`。

   ```text
   4D 51 01 01 02 01 00 00 00 01 00 3A 7B 22 63 6D 64 22 3A 22 67 65 74 5F 64 65 76 69 63 65 5F 69
   6E 66 6F 22 2C 22 72 65 71 75 65 73 74 5F 69 64 22 3A 22 52 45 51 2D 43 52 43 22 2C 22 64 61 74
   61 22 3A 7B 7D 7D 19 A7
   ```

#### 预期结果

- UART2返回packet_ack，message为crc error。

  ```json
  {
    "cmd": "packet_ack",
    "result": "FAIL",
    "error_code": "BLE_PACKET_INVALID",
    "message": "crc error",
    "data": {
      "msg_id": 513
    }
  }
  ```

### BLE-JSON-001 — BLE载荷不是合法JSON

- 分类：`ble_negative`
- 前置条件：PB-03F处于透传状态
- 说明：帧结构和CRC正确，但载荷JSON语法错误。

#### 发送步骤

1. 发送 BLE 分片 1/1

   实际JSON长度：`8` 字节。

   完整帧：`22` 字节，帧尾CRC字段 `AC17`。

   ```text
   4D 51 01 01 02 02 00 00 00 01 00 08 7B 22 62 61 64 22 3A 7D AC 17
   ```

#### 预期结果

- UART2返回invalid json。

  ```json
  {
    "cmd": "packet_ack",
    "result": "FAIL",
    "error_code": "BLE_PACKET_INVALID",
    "message": "invalid json",
    "data": {
      "msg_id": 514
    }
  }
  ```

### BLE-VERSION-001 — BLE帧版本错误

- 分类：`ble_negative`
- 前置条件：PB-03F处于透传状态
- 说明：把协议版本从01改为02，验证帧头检查。

#### 发送步骤

1. 发送 BLE JSON

   JSON：

   ```json
   {
     "cmd": "get_device_info",
     "request_id": "REQ-VERSION",
     "data": {}
   }
   ```

   实际JSON长度：`62` 字节。

   完整帧：`76` 字节，帧尾CRC字段 `7A05`。

   ```text
   4D 51 02 01 02 03 00 00 00 01 00 3E 7B 22 63 6D 64 22 3A 22 67 65 74 5F 64 65 76 69 63 65 5F 69
   6E 66 6F 22 2C 22 72 65 71 75 65 73 74 5F 69 64 22 3A 22 52 45 51 2D 56 45 52 53 49 4F 4E 22 2C
   22 64 61 74 61 22 3A 7B 7D 7D 7A 05
   ```

#### 预期结果

- UART2返回invalid frame header。

  ```json
  {
    "cmd": "packet_ack",
    "result": "FAIL",
    "error_code": "BLE_PACKET_INVALID",
    "message": "invalid frame header",
    "data": {
      "msg_id": 515
    }
  }
  ```

### BLE-ORDER-001 — BLE缺少首分片

- 分类：`ble_negative`
- 前置条件：当前无进行中的BLE重组
- 说明：直接发送第2个分片，验证missing first chunk。

#### 发送步骤

1. 发送 BLE 分片 2/2

   实际JSON长度：`2` 字节。

   完整帧：`16` 字节，帧尾CRC字段 `B964`。

   ```text
   4D 51 01 01 02 04 00 01 00 02 00 02 7B 7D B9 64
   ```

#### 预期结果

- UART2返回missing first chunk。

  ```json
  {
    "cmd": "packet_ack",
    "result": "FAIL",
    "error_code": "BLE_PACKET_INVALID",
    "message": "missing first chunk",
    "data": {
      "msg_id": 516
    }
  }
  ```

### BLE-DUP-001 — BLE重复分片

- 分类：`ble_negative`
- 前置条件：当前无进行中的BLE重组
- 说明：同一msg_id的首分片发送两次，验证重复检测。

#### 发送步骤

1. 发送 BLE 分片 1/2

   实际JSON长度：`7` 字节。

   完整帧：`21` 字节，帧尾CRC字段 `5EA5`。

   ```text
   4D 51 01 00 02 05 00 00 00 02 00 07 7B 22 63 6D 64 22 3A 5E A5
   ```

2. 等待 30 ms

3. 发送 BLE 分片 1/2

   实际JSON长度：`7` 字节。

   完整帧：`21` 字节，帧尾CRC字段 `5EA5`。

   ```text
   4D 51 01 00 02 05 00 00 00 02 00 07 7B 22 63 6D 64 22 3A 5E A5
   ```

#### 预期结果

- UART2返回missing or duplicate chunk。

  ```json
  {
    "cmd": "packet_ack",
    "result": "FAIL",
    "error_code": "BLE_PACKET_INVALID",
    "message": "missing or duplicate chunk",
    "data": {
      "msg_id": 517
    }
  }
  ```

### BLE-TIMEOUT-001 — BLE重组超时

- 分类：`ble_negative`
- 前置条件：当前无进行中的BLE重组
- 说明：只发送两分片消息的首分片，等待超过5000ms。

#### 发送步骤

1. 发送 BLE 分片 1/2

   实际JSON长度：`7` 字节。

   完整帧：`21` 字节，帧尾CRC字段 `5DA6`。

   ```text
   4D 51 01 00 02 06 00 00 00 02 00 07 7B 22 63 6D 64 22 3A 5D A6
   ```

2. 等待 5200 ms

#### 预期结果

- UART2在重组超时后返回missing chunk。

  ```json
  {
    "cmd": "packet_ack",
    "result": "FAIL",
    "error_code": "BLE_PACKET_INVALID",
    "message": "missing chunk",
    "data": {
      "msg_id": 518
    }
  }
  ```

### BLE-INCOMPLETE-001 — BLE半帧接收超时

- 分类：`ble_negative`
- 前置条件：当前无进行中的BLE帧
- 说明：发送声明2字节载荷但仅带1字节载荷的半帧。

#### 发送步骤

1. 发送原始 HEX

   完整帧：`13` 字节，帧尾CRC字段 `027B`。

   ```text
   4D 51 01 01 00 12 00 00 00 01 00 02 7B
   ```

2. 等待 5200 ms

#### 预期结果

- UART2在流超时后返回incomplete frame。

  ```json
  {
    "cmd": "packet_ack",
    "result": "FAIL",
    "error_code": "BLE_PACKET_INVALID",
    "message": "incomplete frame",
    "data": {
      "msg_id": 18
    }
  }
  ```

### V851-CRC-001 — V851帧CRC错误

- 分类：`v851_negative`
- 前置条件：V851接收状态空闲
- 说明：破坏AA55帧CRC，T5L应静默丢弃。

#### 发送步骤

1. 发送 V851 JSON

   JSON：

   ```json
   {
     "msg_id": "BAD-CRC",
     "msg_type": "device.heartbeat",
     "data": {}
   }
   ```

   实际JSON长度：`60` 字节。

   完整帧：`67` 字节，帧尾CRC字段 `6A1E`。

   ```text
   AA 55 00 3F A1 7B 22 6D 73 67 5F 69 64 22 3A 22 42 41 44 2D 43 52 43 22 2C 22 6D 73 67 5F 74 79
   70 65 22 3A 22 64 65 76 69 63 65 2E 68 65 61 72 74 62 65 61 74 22 2C 22 64 61 74 61 22 3A 7B 7D
   7D 6A 1E
   ```

#### 预期结果

- UART4无应答，错误帧被丢弃。

### V851-JSON-001 — V851载荷不是合法JSON

- 分类：`v851_negative`
- 前置条件：V851接收状态空闲
- 说明：AA55结构和CRC正确，但JSON语法错误。

#### 发送步骤

1. 发送原始 HEX

   完整帧：`15` 字节，帧尾CRC字段 `C363`。

   ```text
   AA 55 00 0B A1 7B 22 62 61 64 22 3A 7D C3 63
   ```

#### 预期结果

- UART4无应答，非法JSON被丢弃。

### V851-COMMAND-001 — V851命令字错误

- 分类：`v851_negative`
- 前置条件：V851接收状态空闲
- 说明：将JSON命令字A1改为A2，验证命令字过滤。

#### 发送步骤

1. 发送 V851 JSON

   JSON：

   ```json
   {
     "msg_id": "BAD-COMMAND",
     "msg_type": "device.heartbeat",
     "data": {}
   }
   ```

   实际JSON长度：`64` 字节。

   完整帧：`71` 字节，帧尾CRC字段 `AF10`。

   ```text
   AA 55 00 43 A2 7B 22 6D 73 67 5F 69 64 22 3A 22 42 41 44 2D 43 4F 4D 4D 41 4E 44 22 2C 22 6D 73
   67 5F 74 79 70 65 22 3A 22 64 65 76 69 63 65 2E 68 65 61 72 74 62 65 61 74 22 2C 22 64 61 74 61
   22 3A 7B 7D 7D AF 10
   ```

#### 预期结果

- UART4无应答。

### V851-LENGTH-001 — V851声明长度大于实收长度

- 分类：`v851_negative`
- 前置条件：V851接收状态空闲
- 说明：body_len增加1，使当前UART读取批次不足一帧，验证本批数据被静默丢弃。

#### 发送步骤

1. 发送 V851 JSON

   JSON：

   ```json
   {
     "msg_id": "BAD-LENGTH",
     "msg_type": "device.heartbeat",
     "data": {}
   }
   ```

   实际JSON长度：`63` 字节。

   完整帧：`70` 字节，帧尾CRC字段 `E1FC`。

   ```text
   AA 55 00 43 A1 7B 22 6D 73 67 5F 69 64 22 3A 22 42 41 44 2D 4C 45 4E 47 54 48 22 2C 22 6D 73 67
   5F 74 79 70 65 22 3A 22 64 65 76 69 63 65 2E 68 65 61 72 74 62 65 61 74 22 2C 22 64 61 74 61 22
   3A 7B 7D 7D E1 FC
   ```

#### 预期结果

- UART4无应答，不完整帧随当前读取批次结束而丢弃。

### V851-OVERSIZE-HEADER — V851长度字段超过协议上限

- 分类：`v851_negative`
- 前置条件：V851接收状态空闲
- 说明：发送body_len=2004，超过允许的2003，扫帧循环立即跳过非法帧头。

#### 发送步骤

1. 发送原始 HEX

   完整帧：`4` 字节，帧尾CRC字段 `07D4`。

   ```text
   AA 55 07 D4
   ```

#### 预期结果

- UART4无应答。

### V851-INCOMPLETE-001 — V851半帧批次丢弃

- 分类：`v851_negative`
- 前置条件：V851接收状态空闲
- 说明：发送AA55头、合法长度和部分载荷，验证当前UART读取批次结束后静默丢弃。

#### 发送步骤

1. 发送原始 HEX

   完整帧：`6` 字节，帧尾CRC字段 `A17B`。

   ```text
   AA 55 00 0A A1 7B
   ```

#### 预期结果

- UART4无应答，半帧不保留到下一读取批次。

### V851-RECOVERY-001 — V851噪声与错误帧后恢复

- 分类：`v851_stream`
- 前置条件：V851-TIME-001通过且接收状态空闲
- 说明：同一批数据包含前导噪声、CRC错误心跳帧和合法控制帧，验证解析器静默丢弃错误帧并继续处理下一帧。

#### 发送步骤

1. 发送原始 HEX

   完整帧：`241` 字节，帧尾CRC字段 `C14C`。

   ```text
   00 FF 12 AA 55 00 45 A1 7B 22 6D 73 67 5F 69 64 22 3A 22 42 41 44 2D 54 48 45 4E 2D 47 4F 4F 44
   22 2C 22 6D 73 67 5F 74 79 70 65 22 3A 22 64 65 76 69 63 65 2E 68 65 61 72 74 62 65 61 74 22 2C
   22 64 61 74 61 22 3A 7B 7D 7D 38 DB AA 55 00 A1 A1 7B 22 6D 73 67 5F 69 64 22 3A 22 4D 53 47 2D
   52 45 43 4F 56 45 52 22 2C 22 6D 73 67 5F 74 79 70 65 22 3A 22 73 65 72 76 65 72 2E 63 6F 6D 6D
   61 6E 64 22 2C 22 64 61 74 61 22 3A 7B 22 63 6F 6D 6D 61 6E 64 5F 69 64 22 3A 22 43 4D 44 2D 52
   45 43 4F 56 45 52 22 2C 22 63 6D 64 22 3A 22 65 78 68 61 75 73 74 2E 73 65 74 22 2C 22 65 78 70
   69 72 65 5F 61 74 22 3A 30 2C 22 70 61 72 61 6D 73 22 3A 7B 22 65 6E 61 62 6C 65 64 22 3A 74 72
   75 65 2C 22 6C 65 76 65 6C 22 3A 31 7D 7D 7D C1 4C
   ```

#### 预期结果

- UART4只响应后续合法控制帧并返回SUCCESS。

  ```json
  {
    "msg_type": "device.command_ack",
    "device_sn": "SN-TEST-0001",
    "ble_id": "ABC234",
    "data": {
      "command_id": "CMD-RECOVER",
      "server_msg_id": "MSG-RECOVER",
      "status": "SUCCESS"
    }
  }
  ```

### BLE-BOUNDARY-2000 — BLE最大2000字节JSON与多分片

- 分类：`boundary`
- 前置条件：PB-03F处于透传状态且无待转发请求
- 说明：构造恰好2000字节的configure_network，验证9个BLE分片和UART4转发。

#### 发送步骤

1. 发送 BLE JSON，补齐为 2000 字节

   JSON：

   ```json
   {
     "cmd": "configure_network",
     "request_id": "REQ-BOUNDARY-2000",
     "data": {
       "ssid": "BoundaryWiFi",
       "password": "12345678"
     }
   }
   ```

   实际JSON长度：`2000` 字节。

   分片 1/9：`237` 字节，帧尾CRC字段 `6109`。

   ```text
   4D 51 01 00 03 01 00 00 00 09 00 DF 7B 22 63 6D 64 22 3A 22 63 6F 6E 66 69 67 75 72 65 5F 6E 65
   74 77 6F 72 6B 22 2C 22 72 65 71 75 65 73 74 5F 69 64 22 3A 22 52 45 51 2D 42 4F 55 4E 44 41 52
   59 2D 32 30 30 30 22 2C 22 64 61 74 61 22 3A 7B 22 73 73 69 64 22 3A 22 42 6F 75 6E 64 61 72 79
   57 69 46 69 22 2C 22 70 61 73 73 77 6F 72 64 22 3A 22 31 32 33 34 35 36 37 38 22 2C 22 70 61 64
   64 69 6E 67 22 3A 22 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 61 09
   ```

   分片 2/9：`237` 字节，帧尾CRC字段 `66B1`。

   ```text
   4D 51 01 00 03 01 00 01 00 09 00 DF 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 66 B1
   ```

   分片 3/9：`237` 字节，帧尾CRC字段 `3F9C`。

   ```text
   4D 51 01 00 03 01 00 02 00 09 00 DF 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 3F 9C
   ```

   分片 4/9：`237` 字节，帧尾CRC字段 `0887`。

   ```text
   4D 51 01 00 03 01 00 03 00 09 00 DF 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 08 87
   ```

   分片 5/9：`237` 字节，帧尾CRC字段 `8DC6`。

   ```text
   4D 51 01 00 03 01 00 04 00 09 00 DF 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 8D C6
   ```

   分片 6/9：`237` 字节，帧尾CRC字段 `BADD`。

   ```text
   4D 51 01 00 03 01 00 05 00 09 00 DF 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 BA DD
   ```

   分片 7/9：`237` 字节，帧尾CRC字段 `E3F0`。

   ```text
   4D 51 01 00 03 01 00 06 00 09 00 DF 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 E3 F0
   ```

   分片 8/9：`237` 字节，帧尾CRC字段 `D4EB`。

   ```text
   4D 51 01 00 03 01 00 07 00 09 00 DF 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 D4 EB
   ```

   分片 9/9：`230` 字节，帧尾CRC字段 `F56A`。

   ```text
   4D 51 01 01 03 01 00 08 00 09 00 D8 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 22 7D 7D F5 6A
   ```

#### 预期结果

- UART4收到完整2000字节JSON，字段未被截断。

  ```json
  {
    "cmd": "configure_network",
    "request_id": "REQ-BOUNDARY-2000",
    "data": {
      "ssid": "BoundaryWiFi",
      "password": "12345678"
    }
  }
  ```

### V851-BOUNDARY-2000 — V851最大2000字节JSON

- 分类：`boundary`
- 前置条件：V851接收状态空闲
- 说明：构造恰好2000字节的排风命令，验证最大AA55帧和控制快照截断标志。

#### 发送步骤

1. 发送 V851 JSON，补齐为 2000 字节

   JSON：

   ```json
   {
     "msg_id": "MSG-BOUNDARY-2000",
     "msg_type": "server.command",
     "data": {
       "command_id": "CMD-BOUNDARY-2000",
       "cmd": "exhaust.set",
       "expire_at": 0,
       "params": {
         "enabled": true,
         "level": 3
       }
     }
   }
   ```

   实际JSON长度：`2000` 字节。

   完整帧：`2007` 字节，帧尾CRC字段 `4DFE`。

   ```text
   AA 55 07 D3 A1 7B 22 6D 73 67 5F 69 64 22 3A 22 4D 53 47 2D 42 4F 55 4E 44 41 52 59 2D 32 30 30
   30 22 2C 22 6D 73 67 5F 74 79 70 65 22 3A 22 73 65 72 76 65 72 2E 63 6F 6D 6D 61 6E 64 22 2C 22
   64 61 74 61 22 3A 7B 22 63 6F 6D 6D 61 6E 64 5F 69 64 22 3A 22 43 4D 44 2D 42 4F 55 4E 44 41 52
   59 2D 32 30 30 30 22 2C 22 63 6D 64 22 3A 22 65 78 68 61 75 73 74 2E 73 65 74 22 2C 22 65 78 70
   69 72 65 5F 61 74 22 3A 30 2C 22 70 61 72 61 6D 73 22 3A 7B 22 65 6E 61 62 6C 65 64 22 3A 74 72
   75 65 2C 22 6C 65 76 65 6C 22 3A 33 2C 22 70 61 64 64 69 6E 67 22 3A 22 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58
   58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 58 22 7D 7D 7D 4D FE
   ```

#### 预期结果

- UART4完成解析、写入排风参数并返回SUCCESS，而不是因帧长度丢弃。

  ```json
  {
    "msg_type": "device.command_ack",
    "data": {
      "command_id": "CMD-BOUNDARY-2000",
      "status": "SUCCESS"
    }
  }
  ```

#### Keil调试器检查

- V851_CONTROL_EXHAUST的params_len大于256、stored_len=256、params_truncated=1。

### BLE-OVERSIZE-2001 — BLE拒绝2001字节JSON

- 分类：`boundary`
- 前置条件：仅离线编码检查
- 说明：超过BRIDGE_JSON_MAX的数据不得组帧或写入串口。

#### 发送步骤

1. 离线验证 ble 拒绝 2001 字节 JSON

   该步骤不向串口发送数据；编码器必须在组帧前拒绝超限载荷。

#### 预期结果

- 编码器抛出长度错误，串口无数据发送。

### V851-OVERSIZE-2001 — V851拒绝2001字节JSON

- 分类：`boundary`
- 前置条件：仅离线编码检查
- 说明：超过BRIDGE_JSON_MAX的数据不得组帧或写入串口。

#### 发送步骤

1. 离线验证 v851 拒绝 2001 字节 JSON

   该步骤不向串口发送数据；编码器必须在组帧前拒绝超限载荷。

#### 预期结果

- 编码器抛出长度错误，串口无数据发送。

## 总体验收标准

- 自动用例无协议、CRC、字段或超时断言失败。
- UART2只出现PB-03F AT命令和 `4D 51` 数据，不出现DGUS、Modbus或页面调试文本。
- UART4保持 `AA 55` V851协议；普通控制命令因未注册处理器返回 `UNSUPPORTED_CMD`，但已识别命令仍更新 `V851ControlInfo`。
- 将 `blePB03F_UART_ID` 改回 `5` 后，重新编译即可恢复生产UART5蓝牙映射。

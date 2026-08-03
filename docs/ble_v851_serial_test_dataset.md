# V851 UART4 TLV / Wi-Fi / OTA 串口测试集

当前固件不再使用 PB-03F 或 JSON。UART4 固定为 921600 波特，接收以下三类帧：

- TLV：`AA 55 | segment_len BE | 35/36/37 | segments`，总长 `5 + segment_len`，最大 2048 字节。
- Wi-Fi：`AA 55 | len BE | C0/C1/C5 | payload`，总长 `4 + len`。
- OTA：`AB CD | len BE | cmd | payload`，总长 `4 + len`，最大 4124 字节。

TLV 十六进制向量位于 `tests/serial_bridge_vectors.json`，包括 Power、Door、Light、Climate、Filter、Firmware、Storage、FactoryDevice 和 FactoryRejection。`tlv_codec.h` 中 Climate、Firmware 和 FactoryRejection 注释行的长度字段与实际字段字节数不一致；测试向量按正式格式重新计算了这三组长度。

常用模拟器命令：

```powershell
python tools/t5l_serial_simulator.py decode "AA 55 00 0F 35 03 00 0C 01 00 01 01 02 00 01 55 03 00 01 00"
python tools/t5l_serial_simulator.py wifi-scan 0
python tools/t5l_serial_simulator.py wifi-connect TestNet 1234
python tools/t5l_serial_simulator.py monitor --port COM8 --baud 921600
```

边界和恢复测试由 `python -m unittest discover -s tests -v` 执行，覆盖 2048 字节 TLV、4124 字节 OTA、粘包、噪声、截断、畸形长度以及坏帧后的重新同步。

# V851 UART4 TLV / Wi-Fi / OTA 串口测试集

当前固件不再使用 PB-03F 或 JSON。UART4 固定为 115200 波特，接收以下三类帧：

- TLV：`AA 55 | segment_len BE | 35/36/37/38 | segments`，总长 `5 + segment_len`，最大 2048 字节。`0x37` 按方向复用：T5L → V851 为环境状态加九类执行器的全量 Snapshot，V851 → T5L 为 `0x6C` BootstrapResult。
- Wi-Fi：`AA 55 | len BE | C0/C1/C5 | payload`，总长 `4 + len`。
- OTA：`AB CD | len BE | cmd | payload`，总长 `4 + len`，最大 4124 字节。

TLV 十六进制向量位于 `tests/serial_bridge_vectors.json`，包括 Power、Door、Light、Climate、Filter、Firmware、Storage、FactoryDevice 和 FactoryRejection。`tlv_codec.h` 中 Climate、Firmware 和 FactoryRejection 注释行的长度字段与实际字段字节数不一致；测试向量按正式格式重新计算了这三组长度。

常用模拟器命令：

```powershell
python tools/t5l_serial_simulator.py decode "AA 55 00 0F 35 03 00 0C 01 00 01 01 02 00 01 55 03 00 01 00"
python tools/t5l_serial_simulator.py wifi-scan 0
python tools/t5l_serial_simulator.py wifi-connect TestNet 1234
python tools/t5l_serial_simulator.py monitor --port COM8 --baud 115200
```

边界和恢复测试由 `python -m unittest discover -s tests -v` 执行，覆盖 2048 字节 TLV、4124 字节 OTA、粘包、噪声、截断、畸形长度以及坏帧后的重新同步。

控制与状态 VP 分离：V851 下发的执行器控制写入 `0x5100–0x5124` 命令槽；T5L 的 `0x35` 增量和 `0x37` Snapshot 从环境状态 `0x3010` 及执行器实际状态 `0x301A–0x3043` 读取。环境段 `0x05` 上报实时温度和湿度，执行器段为 `0x61–0x69`。启动后的第一次 500ms 扫描只建立状态基线，不触发增量；服务器重复下发但实际状态未变化时也不触发增量。

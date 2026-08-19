# MCQX 固件精简验收记录

日期：2026-08-19

## 应用固件

Keil C51 重构建结果：0 Error、0 Warning。

| 指标 | 基线 | 目标上限 | 最终值 | 相对基线下降 | 目标余量 |
|---|---:|---:|---:|---:|---:|
| code | 50,917 | 46,500 | 44,116 | 6,801（13.36%） | 2,384 |
| xdata | 23,975 | 14,500 | 13,982 | 9,993（41.68%） | 518 |

- Build log：`docs/build_logs/app_keil_rebuild_2026-08-19.log`
- Keil map：`project/Listings/T5L51.map`
- 应用分支：保持当前 `MCQX_CABIN`，未新建分支。

## BOOT 固件

Keil C51 重构建结果：0 Error、0 Warning；`data=9.0`、`xdata=13,017`、`code=18,868`。

- Build log：`C:/Users/Young/Desktop/bootloader4T5L/docs/build_logs/boot_keil_rebuild_2026-08-19.log`
- Keil map：`C:/Users/Young/Desktop/bootloader4T5L/project/Listings/T5L51.m51`
- `BootLoadApp` map 地址：`0xFF70`。
- BOOT 本地分支：`codex/mcqx-uart4-ota`，未推送远端。

## 自动化测试

- 应用本次影响范围：59 项通过。
- BOOT 协议：6 项通过。
- 应用全量：89 项中仅保留既有 `test_t5l_stc_optimistic.py` 29 个无关失败；本次涉及测试全部通过。

硬件联调项目（正常启动、04 后复位、F3、完整升级、坏包/整文件 CRC、30 秒回退、断电恢复和新程序加载）需要在目标板与 V851 环境中执行。

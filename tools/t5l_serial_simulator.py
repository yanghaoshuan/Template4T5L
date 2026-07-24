#!/usr/bin/env python3
"""T5L UART2 PB-03F / UART4 V851 serial simulator and dataset renderer."""

from __future__ import annotations

import argparse
import copy
import dataclasses
import json
import queue
import sys
import threading
import time
from pathlib import Path
from typing import Any, Iterable


REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_VECTORS = REPO_ROOT / "tests" / "serial_bridge_vectors.json"
DEFAULT_DOCUMENT = REPO_ROOT / "docs" / "ble_v851_serial_test_dataset.md"

BLE_MAGIC = b"\x4d\x51"
BLE_VERSION = 0x01
BLE_LAST_FLAG = 0x01
BLE_HEADER_SIZE = 12
BLE_CRC_SIZE = 2
BLE_MTU = 240
BLE_ATT_PAYLOAD_MAX = BLE_MTU - 3
BLE_CHUNK_PAYLOAD_MAX = BLE_ATT_PAYLOAD_MAX - BLE_HEADER_SIZE - BLE_CRC_SIZE
BLE_JSON_MAX = 2000

V851_MAGIC = b"\xaa\x55"
V851_JSON_COMMAND = 0xA1
V851_JSON_MAX = 2000

DEFAULT_BLE_BAUD = 115200
DEFAULT_V851_BAUD = 921600
DEFAULT_FRAGMENT_GAP_MS = 20
DEFAULT_EXPECT_TIMEOUT_MS = 1500
DEFAULT_V851_STARTUP_DELAY_MS = 1500
PB03F_TEST_MAC = "A1B2C3D4E5F6"


class ProtocolError(ValueError):
    """Raised when a wire frame is malformed."""


@dataclasses.dataclass
class DecodedFrame:
    raw: bytes
    payload: bytes
    json_value: Any | None
    metadata: dict[str, Any]


def crc16_modbus(data: bytes) -> int:
    """Return the CRC used by the T5L firmware (poly 0xA001, init 0xFFFF)."""
    crc = 0xFFFF
    for value in data:
        crc ^= value
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if (crc & 1) else (crc >> 1)
    return crc & 0xFFFF


def compact_json(value: Any) -> bytes:
    return json.dumps(
        value, ensure_ascii=False, separators=(",", ":")
    ).encode("utf-8")


def _set_path(root: dict[str, Any], dotted_path: str, value: Any) -> None:
    parts = dotted_path.split(".")
    current: dict[str, Any] = root
    for part in parts[:-1]:
        child = current.get(part)
        if not isinstance(child, dict):
            child = {}
            current[part] = child
        current = child
    current[parts[-1]] = value


def json_payload(
    value: Any,
    target_length: int | None = None,
    padding_path: str = "padding",
) -> bytes:
    """Serialize compact JSON and optionally pad one string field to an exact size."""
    if target_length is None:
        return compact_json(value)
    if not isinstance(value, dict):
        raise ValueError("target_length requires a JSON object")

    padded = copy.deepcopy(value)
    _set_path(padded, padding_path, "")
    base = compact_json(padded)
    padding_size = target_length - len(base)
    if padding_size < 0:
        raise ValueError(
            f"JSON base length {len(base)} exceeds target {target_length}"
        )
    _set_path(padded, padding_path, "X" * padding_size)
    result = compact_json(padded)
    if len(result) != target_length:
        raise AssertionError("failed to create exact-length JSON")
    return result


def encode_ble_frame(
    payload: bytes,
    *,
    msg_id: int,
    chunk_index: int,
    chunk_total: int,
    last: bool,
    version: int = BLE_VERSION,
) -> bytes:
    if not payload or len(payload) > BLE_CHUNK_PAYLOAD_MAX:
        raise ValueError(
            f"BLE chunk payload must be 1..{BLE_CHUNK_PAYLOAD_MAX} bytes"
        )
    if not (0 <= msg_id <= 0xFFFF):
        raise ValueError("BLE msg_id must fit uint16")
    if not (0 <= chunk_index <= 0xFFFF and 1 <= chunk_total <= 0xFFFF):
        raise ValueError("BLE chunk indexes must fit uint16")

    flags = BLE_LAST_FLAG if last else 0
    header = (
        BLE_MAGIC
        + bytes((version, flags))
        + msg_id.to_bytes(2, "big")
        + chunk_index.to_bytes(2, "big")
        + chunk_total.to_bytes(2, "big")
        + len(payload).to_bytes(2, "big")
    )
    crc = crc16_modbus(header + payload)
    return header + payload + crc.to_bytes(2, "big")


def encode_ble_chunks(payload: bytes, *, msg_id: int) -> list[bytes]:
    if not payload or len(payload) > BLE_JSON_MAX:
        raise ValueError(f"BLE JSON must be 1..{BLE_JSON_MAX} bytes")
    parts = [
        payload[offset : offset + BLE_CHUNK_PAYLOAD_MAX]
        for offset in range(0, len(payload), BLE_CHUNK_PAYLOAD_MAX)
    ]
    return [
        encode_ble_frame(
            part,
            msg_id=msg_id,
            chunk_index=index,
            chunk_total=len(parts),
            last=index == len(parts) - 1,
        )
        for index, part in enumerate(parts)
    ]


def decode_ble_frame(frame: bytes) -> DecodedFrame:
    if len(frame) < BLE_HEADER_SIZE + BLE_CRC_SIZE:
        raise ProtocolError("BLE frame is too short")
    if frame[:2] != BLE_MAGIC:
        raise ProtocolError("BLE magic mismatch")
    payload_len = int.from_bytes(frame[10:12], "big")
    expected_len = BLE_HEADER_SIZE + payload_len + BLE_CRC_SIZE
    if len(frame) != expected_len:
        raise ProtocolError(
            f"BLE length mismatch: expected {expected_len}, got {len(frame)}"
        )
    expected_crc = crc16_modbus(frame[:-2])
    received_crc = int.from_bytes(frame[-2:], "big")
    if expected_crc != received_crc:
        raise ProtocolError(
            f"BLE CRC mismatch: expected {expected_crc:04X}, got {received_crc:04X}"
        )
    payload = frame[BLE_HEADER_SIZE:-BLE_CRC_SIZE]
    try:
        json_value = json.loads(payload.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError):
        json_value = None
    return DecodedFrame(
        raw=frame,
        payload=payload,
        json_value=json_value,
        metadata={
            "version": frame[2],
            "flags": frame[3],
            "msg_id": int.from_bytes(frame[4:6], "big"),
            "chunk_index": int.from_bytes(frame[6:8], "big"),
            "chunk_total": int.from_bytes(frame[8:10], "big"),
            "payload_len": payload_len,
        },
    )


def encode_v851(payload: bytes, *, command: int = V851_JSON_COMMAND) -> bytes:
    if not payload or len(payload) > V851_JSON_MAX:
        raise ValueError(f"V851 JSON must be 1..{V851_JSON_MAX} bytes")
    body_len = len(payload) + 3
    body = bytes((command,)) + payload
    crc = crc16_modbus(body)
    return V851_MAGIC + body_len.to_bytes(2, "big") + body + crc.to_bytes(2, "big")


def decode_v851_frame(frame: bytes) -> DecodedFrame:
    if len(frame) < 7:
        raise ProtocolError("V851 frame is too short")
    if frame[:2] != V851_MAGIC:
        raise ProtocolError("V851 magic mismatch")
    body_len = int.from_bytes(frame[2:4], "big")
    if len(frame) != body_len + 4:
        raise ProtocolError(
            f"V851 length mismatch: expected {body_len + 4}, got {len(frame)}"
        )
    if frame[4] != V851_JSON_COMMAND:
        raise ProtocolError(f"unexpected V851 command 0x{frame[4]:02X}")
    expected_crc = crc16_modbus(frame[4:-2])
    received_crc = int.from_bytes(frame[-2:], "big")
    if expected_crc != received_crc:
        raise ProtocolError(
            f"V851 CRC mismatch: expected {expected_crc:04X}, got {received_crc:04X}"
        )
    payload = frame[5:-2]
    try:
        json_value = json.loads(payload.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError):
        json_value = None
    return DecodedFrame(
        raw=frame,
        payload=payload,
        json_value=json_value,
        metadata={"body_len": body_len, "command": frame[4]},
    )


def _apply_mutations(frames: list[bytes], mutations: Iterable[str]) -> list[bytes]:
    result = [bytearray(frame) for frame in frames]
    for mutation in mutations:
        if not result:
            raise ValueError("cannot mutate an empty frame list")
        frame = result[0]
        if mutation == "bad_crc":
            frame[-1] ^= 0xFF
        elif mutation == "bad_magic":
            frame[0] ^= 0xFF
        elif mutation == "bad_version":
            if frame[:2] != BLE_MAGIC:
                raise ValueError("bad_version only applies to BLE")
            frame[2] = 0x02
        elif mutation == "bad_length_plus_one":
            if frame[:2] != V851_MAGIC:
                raise ValueError("bad_length_plus_one only applies to V851")
            body_len = int.from_bytes(frame[2:4], "big") + 1
            frame[2:4] = body_len.to_bytes(2, "big")
        elif mutation == "command_a2":
            if frame[:2] != V851_MAGIC:
                raise ValueError("command_a2 only applies to V851")
            frame[4] = 0xA2
        else:
            raise ValueError(f"unknown mutation: {mutation}")
    return [bytes(frame) for frame in result]


def materialize_action(action: dict[str, Any]) -> tuple[list[bytes], bytes | None]:
    """Return wire frames and the logical JSON payload for one dataset action."""
    kind = action["kind"]
    if kind in {"wait", "wait_ble_transparent", "offline_oversize"}:
        return [], None
    if kind == "raw_hex":
        return [bytes.fromhex(action["hex"])], None
    if kind == "ble_chunk":
        payload = action.get("payload_text", "").encode("utf-8")
        frame = encode_ble_frame(
            payload,
            msg_id=int(action["msg_id"]),
            chunk_index=int(action["chunk_index"]),
            chunk_total=int(action["chunk_total"]),
            last=bool(action["last"]),
            version=int(action.get("version", BLE_VERSION)),
        )
        return _apply_mutations([frame], action.get("mutations", [])), payload
    if kind != "json":
        raise ValueError(f"unsupported action kind: {kind}")

    payload = json_payload(
        action["json"],
        action.get("target_length"),
        action.get("padding_path", "padding"),
    )
    port = action["port"]
    if port == "ble":
        frames = encode_ble_chunks(payload, msg_id=int(action.get("msg_id", 1)))
    elif port == "v851":
        frames = [encode_v851(payload)]
    else:
        raise ValueError(f"unsupported action port: {port}")
    return _apply_mutations(frames, action.get("mutations", [])), payload


def load_vectors(path: Path = DEFAULT_VECTORS) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    if (
        data.get("schema_version") != 1
        or not isinstance(data.get("cases"), list)
        or not isinstance(data.get("standalone_cases"), dict)
    ):
        raise ValueError("unsupported or invalid serial test vector file")
    case_ids = {case.get("id") for case in data["cases"]}
    for mode in ("ble", "v851"):
        configured = data["standalone_cases"].get(mode)
        if not isinstance(configured, list) or not set(configured).issubset(case_ids):
            raise ValueError(f"invalid standalone_cases.{mode}")
    return data


def _hex_lines(data: bytes, width: int = 32) -> str:
    tokens = [f"{value:02X}" for value in data]
    return "\n".join(
        " ".join(tokens[offset : offset + width])
        for offset in range(0, len(tokens), width)
    )


def _json_block(value: Any) -> str:
    return json.dumps(value, ensure_ascii=False, indent=2)


def _action_summary(action: dict[str, Any]) -> str:
    kind = action["kind"]
    if kind == "wait":
        return f"等待 {action['ms']} ms"
    if kind == "wait_ble_transparent":
        return "等待 PB-03F 再次进入透传"
    if kind == "offline_oversize":
        return f"离线验证 {action['port']} 拒绝 {action['target_length']} 字节 JSON"
    if kind == "raw_hex":
        return "发送原始 HEX"
    if kind == "ble_chunk":
        return (
            f"发送 BLE 分片 {action['chunk_index'] + 1}/"
            f"{action['chunk_total']}"
        )
    target = action.get("target_length")
    suffix = f"，补齐为 {target} 字节" if target else ""
    return f"发送 {action['port'].upper()} JSON{suffix}"


def render_markdown(vectors: dict[str, Any]) -> str:
    lines: list[str] = [
        "# T5L UART2 蓝牙与 UART4 V851 串口测试数据集",
        "",
        "> 本文件由 `tools/t5l_serial_simulator.py render-md` 根据"
        " `tests/serial_bridge_vectors.json` 自动生成，请勿手工修改 HEX。",
        "",
        "## 测试连接",
        "",
        "| 模拟对象 | T5L端口 | 参数 | 电脑端 |",
        "|---|---|---|---|",
        "| PB-03F 蓝牙 | UART2 | 115200, 8N1, 无流控 | 3.3V TTL USB串口，端口名由 `--ble-port` 指定 |",
        "| V851 | UART4 | 921600, 8N1, 无流控 | 3.3V TTL USB串口，端口名由 `--v851-port` 指定 |",
        "",
        "UART2接线：T5L P0.4(TX) → USB串口RX，T5L P0.5(RX) ← USB串口TX。"
        "两路串口均须共地。禁止直接连接 RS-232 电平，也不要用USB串口的5V电源脚给T5L供电。",
        "",
        "## 运行方法",
        "",
        "```powershell",
        "python -m pip install \"pyserial>=3.5\"",
        "python tools/t5l_serial_simulator.py ports --probe",
        "python tools/t5l_serial_simulator.py monitor --port COM3 --baud 115200 --seconds 10",
        "python tools/t5l_serial_simulator.py list",
        "python tools/t5l_serial_simulator.py run --mode ble --ble-port COM3",
        "python tools/t5l_serial_simulator.py run --mode v851 --v851-port COM4",
        "python tools/t5l_serial_simulator.py run --mode both --ble-port COM3 --v851-port COM4",
        "python tools/t5l_serial_simulator.py render-md --check",
        "```",
        "",
        "`run`在打开目标串口前会先列出系统当前识别到的端口。若端口显示存在但"
        "打开时报“拒绝访问”，请关闭串口调试助手、Keil串口窗口或其他占用该COM口"
        "的程序，再使用 `ports --probe` 确认可访问性。",
        "",
        f"- `--mode ble`：只打开UART2蓝牙串口，执行"
        f" {len(vectors['standalone_cases']['ble'])} 条蓝牙单端用例。",
        f"- `--mode v851`：只打开UART4 V851串口，执行"
        f" {len(vectors['standalone_cases']['v851'])} 条V851单端用例。",
        "- `--mode both`：打开两路串口，执行完整桥接、双向转发及全部异常用例。",
        "",
        "电脑端会自动响应 PB-03F AT 初始化。固定模拟 MAC 为"
        f" `{PB03F_TEST_MAC}`；多分片发送间隔为"
        f" {DEFAULT_FRAGMENT_GAP_MS} ms，以适配 UART2 的 256 字节接收环形缓冲区。",
        "",
        "## 协议摘要",
        "",
        "- BLE帧：`4D 51 | 版本 | 标志 | msg_id(BE) | 分片序号(BE) | 分片总数(BE) | 载荷长度(BE) | JSON | CRC16(BE)`。",
        f"- BLE单分片JSON上限为 {BLE_CHUNK_PAYLOAD_MAX} 字节，完整JSON上限为 {BLE_JSON_MAX} 字节。",
        "- V851帧：`AA 55 | body_len(BE) | A1 | JSON | CRC16(BE)`；CRC覆盖 `A1 + JSON`。",
        "- CRC算法为 Modbus CRC16（初值 `FFFF`、多项式 `A001`），但两个协议在线路上均按大端发送CRC。",
        "",
        "## PB-03F 自动初始化预期",
        "",
        "T5L应依次发出：`AT`、`AT+BLEMODE=9`、服务UUID、TX UUID、RX UUID、"
        "`AT+BLEMTU=240`、`AT+BLEMAC?`、`AT+BLEAUTH=...`、"
        "`AT+BLENAME=...`、`AT+BLEADVDATA=...`、`AT+BLEMODE=0`、"
        "`AT+TRANSENTER`。模拟器对MAC查询返回"
        f" `+BLEMAC:{PB03F_TEST_MAC}\\r\\nOK\\r\\n`，其余命令返回 `OK\\r\\n`。",
        "",
        "## 用例",
        "",
    ]

    for case in vectors["cases"]:
        lines.extend(
            [
                f"### {case['id']} — {case['title']}",
                "",
                f"- 分类：`{case['suite']}`",
                f"- 前置条件：{case.get('precondition', '无')}",
                f"- 说明：{case['description']}",
                "",
                "#### 发送步骤",
                "",
            ]
        )
        if not case.get("actions"):
            lines.extend(["无需主动发送，观察T5L输出。", ""])
        for index, action in enumerate(case.get("actions", []), start=1):
            lines.extend([f"{index}. {_action_summary(action)}", ""])
            if action["kind"] == "json":
                lines.extend(
                    [
                        "   JSON：",
                        "",
                        "   ```json",
                        *[f"   {line}" for line in _json_block(action["json"]).splitlines()],
                        "   ```",
                        "",
                    ]
                )
            frames, payload = materialize_action(action)
            if payload is not None:
                lines.append(f"   实际JSON长度：`{len(payload)}` 字节。")
                lines.append("")
            for frame_index, frame in enumerate(frames, start=1):
                crc_text = (
                    f"`{int.from_bytes(frame[-2:], 'big'):04X}`"
                    if len(frame) >= 2
                    else "不适用"
                )
                label = (
                    f"分片 {frame_index}/{len(frames)}"
                    if len(frames) > 1
                    else "完整帧"
                )
                lines.extend(
                    [
                        f"   {label}：`{len(frame)}` 字节，帧尾CRC字段 {crc_text}。",
                        "",
                        "   ```text",
                        *[f"   {line}" for line in _hex_lines(frame).splitlines()],
                        "   ```",
                        "",
                    ]
                )
            if action["kind"] == "offline_oversize":
                lines.extend(
                    [
                        "   该步骤不向串口发送数据；编码器必须在组帧前拒绝超限载荷。",
                        "",
                    ]
                )

        lines.extend(["#### 预期结果", ""])
        for expected in case.get("expected", []):
            lines.append(f"- {expected['description']}")
            if "match" in expected:
                lines.extend(
                    [
                        "",
                        "  ```json",
                        *[
                            f"  {line}"
                            for line in _json_block(expected["match"]).splitlines()
                        ],
                        "  ```",
                    ]
                )
        lines.append("")
        manual = case.get("debugger")
        if manual:
            lines.extend(["#### Keil调试器检查", "", f"- {manual}", ""])

    lines.extend(
        [
            "## 总体验收标准",
            "",
            "- 自动用例无协议、CRC、字段或超时断言失败。",
            "- UART2只出现PB-03F AT命令和 `4D 51` 数据，不出现DGUS、Modbus或页面调试文本。",
            "- UART4保持 `AA 55` V851协议；普通控制命令因未注册处理器返回"
            " `UNSUPPORTED_CMD`，但已识别命令仍更新 `V851ControlInfo`。",
            "- 将 `blePB03F_UART_ID` 改回 `5` 后，重新编译即可恢复生产UART5蓝牙映射。",
            "",
        ]
    )
    return "\n".join(lines)


def _parse_json(payload: bytes) -> Any | None:
    try:
        return json.loads(payload.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError):
        return None


def _find_magic(buffer: bytearray, magic: bytes) -> bool:
    index = buffer.find(magic)
    if index < 0:
        if len(buffer) > len(magic):
            del buffer[:-len(magic) + 1]
        return False
    if index:
        del buffer[:index]
    return True


class BlePeer:
    """Computer-side PB-03F simulator with automatic AT handling."""

    def __init__(self, serial_port: Any):
        self.serial = serial_port
        self.buffer = bytearray()
        self.messages: queue.Queue[DecodedFrame] = queue.Queue()
        self.at_history: list[str] = []
        self.at_lock = threading.Lock()
        self.transparent = threading.Event()
        self.stop_event = threading.Event()
        self.thread = threading.Thread(target=self._run, name="ble-peer", daemon=True)
        self.assemblies: dict[int, list[bytes]] = {}
        self.transition_count = 0
        self.rx_total = 0
        self.rx_recent = bytearray()
        self.rx_lock = threading.Lock()

    def start(self) -> None:
        self.thread.start()

    def close(self) -> None:
        self.stop_event.set()
        self.thread.join(timeout=1.0)

    def _write(self, data: bytes) -> None:
        self.serial.write(data)
        self.serial.flush()

    def _handle_at_line(self, raw_line: bytes) -> None:
        command = raw_line.decode("ascii", errors="replace").strip()
        if not command:
            return
        with self.at_lock:
            self.at_history.append(command)
        print(f"[UART2 T5L→PC AT] {command}")
        if command == "AT+BLEMAC?":
            self._write(f"+BLEMAC:{PB03F_TEST_MAC}\r\nOK\r\n".encode("ascii"))
        else:
            self._write(b"OK\r\n")
        if command == "AT+TRANSENTER":
            self.transition_count += 1
            self.transparent.set()

    def _handle_ble_frame(self, raw: bytes) -> None:
        try:
            frame = decode_ble_frame(raw)
        except ProtocolError as exc:
            print(f"[UART2 T5L→PC] 无法解码BLE帧: {exc}")
            return
        meta = frame.metadata
        msg_id = int(meta["msg_id"])
        chunk_index = int(meta["chunk_index"])
        chunk_total = int(meta["chunk_total"])
        if chunk_index == 0:
            self.assemblies[msg_id] = []
        parts = self.assemblies.get(msg_id)
        if parts is None or len(parts) != chunk_index:
            print(f"[UART2 T5L→PC] BLE响应分片顺序错误 msg_id={msg_id}")
            self.assemblies.pop(msg_id, None)
            return
        parts.append(frame.payload)
        if chunk_index + 1 == chunk_total:
            payload = b"".join(parts)
            self.assemblies.pop(msg_id, None)
            complete = DecodedFrame(
                raw=raw,
                payload=payload,
                json_value=_parse_json(payload),
                metadata={"msg_id": msg_id, "chunk_total": chunk_total},
            )
            self.messages.put(complete)
            print(
                f"[UART2 T5L→PC BLE] {_display_payload(payload)}"
            )

    def _consume_at(self) -> bool:
        newline = self.buffer.find(b"\n")
        if newline < 0:
            return False
        line = bytes(self.buffer[: newline + 1])
        del self.buffer[: newline + 1]
        self._handle_at_line(line)
        return True

    def _consume_transparent(self) -> bool:
        if self.buffer.startswith(b"+++"):
            del self.buffer[:3]
            print("[UART2 T5L→PC AT] +++")
            self.transparent.clear()
            self._write(b"OK\r\n")
            return True
        if not _find_magic(self.buffer, BLE_MAGIC):
            return False
        if len(self.buffer) < BLE_HEADER_SIZE:
            return False
        payload_len = int.from_bytes(self.buffer[10:12], "big")
        frame_len = BLE_HEADER_SIZE + payload_len + BLE_CRC_SIZE
        if len(self.buffer) < frame_len:
            return False
        raw = bytes(self.buffer[:frame_len])
        del self.buffer[:frame_len]
        self._handle_ble_frame(raw)
        return True

    def _run(self) -> None:
        while not self.stop_event.is_set():
            data = self.serial.read(self.serial.in_waiting or 1)
            if data:
                with self.rx_lock:
                    self.rx_total += len(data)
                    self.rx_recent.extend(data)
                    if len(self.rx_recent) > 256:
                        del self.rx_recent[:-256]
                self.buffer.extend(data)
            progressed = True
            while progressed:
                progressed = (
                    self._consume_transparent()
                    if self.transparent.is_set()
                    else self._consume_at()
                )

    def wait_transparent(self, timeout: float = 20.0) -> bool:
        return self.transparent.wait(timeout)

    def wait_transition_count(self, count: int, timeout: float) -> bool:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if self.transition_count >= count:
                return True
            time.sleep(0.02)
        return False

    def receive_diagnostic(self) -> str:
        with self.rx_lock:
            total = self.rx_total
            recent = bytes(self.rx_recent)
        if total == 0:
            return (
                "UART2未收到任何字节；请确认固件已重新编译下载、T5L已复位、"
                "P0.4(TX)→USB串口RX、P0.5(RX)←USB串口TX并共地"
            )
        return (
            f"UART2共收到{total}字节，但未解析出完整AT命令；"
            f"最近HEX: {_hex_lines(recent, width=24).replace(chr(10), ' | ')}；"
            "请检查115200波特率、TX/RX方向和乱码"
        )


class V851Peer:
    def __init__(self, serial_port: Any):
        self.serial = serial_port
        self.buffer = bytearray()
        self.messages: queue.Queue[DecodedFrame] = queue.Queue()
        self.stop_event = threading.Event()
        self.thread = threading.Thread(target=self._run, name="v851-peer", daemon=True)

    def start(self) -> None:
        self.thread.start()

    def close(self) -> None:
        self.stop_event.set()
        self.thread.join(timeout=1.0)

    def _run(self) -> None:
        while not self.stop_event.is_set():
            data = self.serial.read(self.serial.in_waiting or 1)
            if data:
                self.buffer.extend(data)
            while _find_magic(self.buffer, V851_MAGIC):
                if len(self.buffer) < 4:
                    break
                body_len = int.from_bytes(self.buffer[2:4], "big")
                frame_len = body_len + 4
                if len(self.buffer) < frame_len:
                    break
                raw = bytes(self.buffer[:frame_len])
                del self.buffer[:frame_len]
                try:
                    frame = decode_v851_frame(raw)
                except ProtocolError as exc:
                    print(f"[UART4 T5L→PC] 无法解码V851帧: {exc}")
                    continue
                self.messages.put(frame)
                print(f"[UART4 T5L→PC V851] {_display_payload(frame.payload)}")


def _display_payload(payload: bytes) -> str:
    try:
        return payload.decode("utf-8")
    except UnicodeDecodeError:
        return _hex_lines(payload, width=24).replace("\n", " | ")


def _drain(messages: queue.Queue[DecodedFrame]) -> None:
    while True:
        try:
            messages.get_nowait()
        except queue.Empty:
            return


def _match_subset(actual: Any, expected: Any, path: str = "$") -> list[str]:
    errors: list[str] = []
    if isinstance(expected, dict):
        if not isinstance(actual, dict):
            return [f"{path}: expected object, got {type(actual).__name__}"]
        for key, value in expected.items():
            if key not in actual:
                errors.append(f"{path}.{key}: missing")
            else:
                errors.extend(_match_subset(actual[key], value, f"{path}.{key}"))
    elif isinstance(expected, list):
        if not isinstance(actual, list):
            return [f"{path}: expected array, got {type(actual).__name__}"]
        if len(actual) != len(expected):
            return [
                f"{path}: expected {len(expected)} items, got {len(actual)}"
            ]
        for index, value in enumerate(expected):
            errors.extend(
                _match_subset(actual[index], value, f"{path}[{index}]")
            )
    elif expected == "$INT":
        if not isinstance(actual, int):
            errors.append(f"{path}: expected integer, got {actual!r}")
    elif expected == "$NONEMPTY":
        if not isinstance(actual, str) or not actual:
            errors.append(f"{path}: expected non-empty string, got {actual!r}")
    elif actual != expected:
        errors.append(f"{path}: expected {expected!r}, got {actual!r}")
    return errors


class HardwareRunner:
    def __init__(
        self,
        ble_serial: Any | None,
        v851_serial: Any | None,
        *,
        fragment_gap_ms: int = DEFAULT_FRAGMENT_GAP_MS,
        timeout_ms: int = DEFAULT_EXPECT_TIMEOUT_MS,
        startup_delay_ms: int = DEFAULT_V851_STARTUP_DELAY_MS,
    ):
        self.ble_serial = ble_serial
        self.v851_serial = v851_serial
        self.ble = BlePeer(ble_serial) if ble_serial is not None else None
        self.v851 = V851Peer(v851_serial) if v851_serial is not None else None
        self.fragment_gap = fragment_gap_ms / 1000.0
        self.timeout = timeout_ms / 1000.0
        self.startup_delay = startup_delay_ms / 1000.0
        self.manual_checks = 0

    def start(self) -> None:
        if self.ble is not None:
            self.ble.start()
        if self.v851 is not None:
            self.v851.start()
        if self.ble is not None:
            if not self.ble.wait_transparent(20.0):
                raise TimeoutError(
                    "20秒内未完成PB-03F AT初始化；"
                    + self.ble.receive_diagnostic()
                )
        elif self.startup_delay > 0:
            print(f"仅V851模式：等待T5L启动 {self.startup_delay:.2f}s")
            time.sleep(self.startup_delay)

    def close(self) -> None:
        if self.ble is not None:
            self.ble.close()
        if self.v851 is not None:
            self.v851.close()

    def _send_action(self, action: dict[str, Any]) -> None:
        kind = action["kind"]
        if kind == "wait":
            time.sleep(int(action["ms"]) / 1000.0)
            return
        if kind == "wait_ble_transparent":
            if self.ble is None:
                return
            count = int(action.get("transition_count", 1))
            timeout = int(action.get("timeout_ms", 15000)) / 1000.0
            if not self.ble.wait_transition_count(count, timeout):
                raise TimeoutError(f"PB-03F未达到第{count}次透传")
            if not self.ble.wait_transparent(timeout):
                raise TimeoutError("PB-03F未进入透传")
            return
        if kind == "offline_oversize":
            payload = json_payload(
                action["json"],
                int(action["target_length"]),
                action.get("padding_path", "padding"),
            )
            try:
                if action["port"] == "ble":
                    encode_ble_chunks(payload, msg_id=1)
                else:
                    encode_v851(payload)
            except ValueError:
                return
            raise AssertionError("超限JSON未被编码器拒绝")

        frames, payload = materialize_action(action)
        port = action.get("port")
        serial_port = self.ble_serial if port == "ble" else self.v851_serial
        if serial_port is None:
            raise AssertionError(f"当前模式未打开{port}串口")
        for index, frame in enumerate(frames):
            print(
                f"[PC→{('UART2 BLE' if port == 'ble' else 'UART4 V851')}] "
                f"{len(frame)} bytes: {_hex_lines(frame, width=24).replace(chr(10), ' | ')}"
            )
            serial_port.write(frame)
            serial_port.flush()
            if index + 1 < len(frames):
                time.sleep(self.fragment_gap)
        if payload is not None:
            print(f"[PC逻辑JSON] {_display_payload(payload)}")

    def _check_expected(self, expected: dict[str, Any]) -> list[str]:
        kind = expected["kind"]
        timeout = int(expected.get("timeout_ms", self.timeout * 1000)) / 1000.0
        if kind == "manual":
            self.manual_checks += 1
            return []
        if kind == "at_contains":
            if self.ble is None:
                return []
            with self.ble.at_lock:
                history = list(self.ble.at_history)
            missing = [
                prefix
                for prefix in expected["prefixes"]
                if not any(command.startswith(prefix) for command in history)
            ]
            return [f"缺少AT命令前缀: {', '.join(missing)}"] if missing else []
        if kind == "at_transition_count":
            if self.ble is None:
                return []
            count = int(expected["count"])
            return (
                []
                if self.ble.wait_transition_count(count, timeout)
                else [f"PB-03F透传次数小于{count}"]
            )

        peer = self.ble if expected["port"] == "ble" else self.v851
        if peer is None:
            return []
        if kind == "no_frame":
            try:
                unexpected = peer.messages.get(timeout=timeout)
            except queue.Empty:
                return []
            return [f"收到非预期数据: {_display_payload(unexpected.payload)}"]
        if kind == "json":
            try:
                received = peer.messages.get(timeout=timeout)
            except queue.Empty:
                return [f"{expected['port']}在{timeout:.2f}s内无JSON响应"]
            if received.json_value is None:
                return [f"收到的载荷不是合法JSON: {_display_payload(received.payload)}"]
            return _match_subset(received.json_value, expected["match"])
        return [f"未知预期类型: {kind}"]

    def run_case(self, case: dict[str, Any]) -> tuple[bool, list[str]]:
        if self.ble is not None:
            _drain(self.ble.messages)
        if self.v851 is not None:
            _drain(self.v851.messages)
        failures: list[str] = []
        try:
            for action in case.get("actions", []):
                self._send_action(action)
            for expected in case.get("expected", []):
                failures.extend(self._check_expected(expected))
        except (AssertionError, TimeoutError, ValueError) as exc:
            failures.append(str(exc))
        return not failures, failures


def _select_cases(
    vectors: dict[str, Any],
    case_id: str | None,
    suite: str | None,
    mode: str = "both",
) -> list[dict[str, Any]]:
    cases = vectors["cases"]
    if case_id:
        selected = [case for case in cases if case["id"] == case_id]
        if not selected:
            raise ValueError(f"unknown case: {case_id}")
    elif suite and suite != "all":
        selected = [case for case in cases if case["suite"] == suite]
        if not selected:
            raise ValueError(f"unknown or empty suite: {suite}")
    else:
        selected = list(cases)

    if mode != "both":
        allowed = set(vectors["standalone_cases"][mode])
        incompatible = [case["id"] for case in selected if case["id"] not in allowed]
        selected = [case for case in selected if case["id"] in allowed]
        if case_id and incompatible:
            raise ValueError(
                f"{case_id}依赖另一侧串口，不能在{mode}单端模式运行"
            )
        if not selected:
            raise ValueError(f"所选范围没有可在{mode}单端模式运行的用例")
        if incompatible:
            print(
                f"{mode}单端模式自动跳过 {len(incompatible)} 条依赖另一侧串口的用例。"
            )
    return selected


def _add_prerequisite_cases(
    vectors: dict[str, Any],
    selected: list[dict[str, Any]],
    mode: str,
) -> list[dict[str, Any]]:
    if mode == "ble":
        return selected
    selected_ids = {case["id"] for case in selected}
    setup_ids = {"V851-ID-001", "V851-TIME-001"}
    if selected_ids.issubset({"INIT-001"} | setup_ids):
        return selected

    by_id = {case["id"]: case for case in vectors["cases"]}
    prefix = [by_id[case_id] for case_id in ("V851-ID-001", "V851-TIME-001")
              if case_id not in selected_ids]
    if prefix:
        print(
            "自动加入V851身份/时间前置用例: "
            + ", ".join(case["id"] for case in prefix)
        )
    return prefix + selected


def command_list(args: argparse.Namespace) -> int:
    vectors = load_vectors(Path(args.vectors))
    cases = _select_cases(vectors, None, "all", args.mode)
    for case in cases:
        print(f"{case['id']:<22} {case['suite']:<12} {case['title']}")
    print(f"共 {len(cases)} 条，模式: {args.mode}")
    return 0


def command_render(args: argparse.Namespace) -> int:
    vectors = load_vectors(Path(args.vectors))
    content = render_markdown(vectors)
    output = Path(args.output)
    if args.check:
        if not output.exists() or output.read_text(encoding="utf-8") != content:
            print(f"文档不是最新生成结果: {output}", file=sys.stderr)
            return 1
        print(f"文档与测试向量一致: {output}")
        return 0
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(content, encoding="utf-8", newline="\n")
    print(f"已生成: {output}")
    return 0


def _require_pyserial() -> Any:
    try:
        import serial  # type: ignore
    except ImportError as exc:
        raise RuntimeError(
            '缺少pyserial，请执行: python -m pip install "pyserial>=3.5"'
        ) from exc
    return serial


def discover_serial_ports(probe: bool = False) -> list[dict[str, str]]:
    """Return serial ports currently reported by the operating system."""
    serial = _require_pyserial()
    try:
        from serial.tools import list_ports  # type: ignore
    except ImportError as exc:
        raise RuntimeError("当前pyserial不包含serial.tools.list_ports") from exc

    rows: list[dict[str, str]] = []
    for info in sorted(list_ports.comports(), key=lambda item: item.device.casefold()):
        status = "未探测"
        if probe:
            try:
                handle = serial.Serial(
                    port=info.device,
                    baudrate=DEFAULT_BLE_BAUD,
                    timeout=0,
                    write_timeout=0,
                    xonxoff=False,
                    rtscts=False,
                    dsrdtr=False,
                )
                handle.close()
                status = "可访问"
            except (PermissionError, OSError, serial.SerialException) as exc:
                status = f"不可访问/可能被占用: {exc}"
        rows.append(
            {
                "device": info.device,
                "description": info.description or "",
                "hwid": info.hwid or "",
                "status": status,
            }
        )
    return rows


def print_serial_ports(rows: list[dict[str, str]]) -> None:
    print("当前系统识别到的串口：")
    if not rows:
        print("  （未发现串口）")
        return
    for row in rows:
        print(
            f"  {row['device']:<8} {row['description'] or '-'}"
            f" | {row['status']} | {row['hwid'] or '-'}"
        )


def _resolve_port(name: str, rows: list[dict[str, str]]) -> str:
    for row in rows:
        if row["device"].casefold() == name.casefold():
            return row["device"]
    available = ", ".join(row["device"] for row in rows) or "无"
    raise RuntimeError(f"未发现串口{name}；当前端口: {available}")


def command_ports(args: argparse.Namespace) -> int:
    rows = discover_serial_ports(probe=args.probe)
    print_serial_ports(rows)
    return 0 if rows else 1


def command_monitor(args: argparse.Namespace) -> int:
    rows = discover_serial_ports(probe=False)
    print_serial_ports(rows)
    name = _resolve_port(args.port, rows)
    handle = _open_serial(name, args.baud)
    received = 0
    start = time.monotonic()
    deadline = start + args.seconds
    print(
        f"监听{name}，{args.baud} 8N1，共{args.seconds:g}秒。"
        "请现在复位或重新上电T5L；按Ctrl+C可提前结束。"
    )
    try:
        while time.monotonic() < deadline:
            data = handle.read(handle.in_waiting or 1)
            if not data:
                continue
            received += len(data)
            ascii_text = "".join(
                chr(value) if 32 <= value <= 126 else "."
                for value in data
            )
            print(
                f"[+{time.monotonic() - start:6.2f}s] "
                f"HEX: {_hex_lines(data, width=24).replace(chr(10), ' | ')}"
                f" | ASCII: {ascii_text}"
            )
    except KeyboardInterrupt:
        print("监听已由用户停止。")
    finally:
        handle.close()
    if received == 0:
        print(
            "未收到任何字节。请检查固件是否已重新下载，以及"
            "P0.4(TX)→USB串口RX、P0.5(RX)←USB串口TX、GND共地。"
        )
        return 1
    print(f"共收到 {received} 字节。")
    return 0


def _open_serial(name: str, baudrate: int) -> Any:
    serial = _require_pyserial()
    try:
        return serial.Serial(
            port=name,
            baudrate=baudrate,
            bytesize=8,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=0.05,
            write_timeout=2.0,
            xonxoff=False,
            rtscts=False,
            dsrdtr=False,
        )
    except (PermissionError, OSError, serial.SerialException) as exc:
        raise RuntimeError(
            f"无法打开{name}（可能正被串口助手、Keil或其他程序占用）：{exc}"
        ) from exc


def command_run(args: argparse.Namespace) -> int:
    vectors = load_vectors(Path(args.vectors))
    cases = _select_cases(vectors, args.case, args.suite, args.mode)
    cases = _add_prerequisite_cases(vectors, cases, args.mode)
    needs_ble = args.mode in {"ble", "both"}
    needs_v851 = args.mode in {"v851", "both"}
    if needs_ble and not args.ble_port:
        raise RuntimeError(f"{args.mode}模式必须提供--ble-port")
    if needs_v851 and not args.v851_port:
        raise RuntimeError(f"{args.mode}模式必须提供--v851-port")

    rows = discover_serial_ports(probe=False)
    print_serial_ports(rows)
    ble_name = _resolve_port(args.ble_port, rows) if needs_ble else None
    v851_name = _resolve_port(args.v851_port, rows) if needs_v851 else None
    if ble_name is not None and v851_name is not None:
        if ble_name.casefold() == v851_name.casefold():
            raise RuntimeError("蓝牙和V851不能使用同一个COM口")

    ble_serial = None
    v851_serial = None
    runner = None
    failures = 0
    try:
        if ble_name is not None:
            ble_serial = _open_serial(ble_name, args.ble_baud)
        if v851_name is not None:
            v851_serial = _open_serial(v851_name, args.v851_baud)
        runner = HardwareRunner(
            ble_serial,
            v851_serial,
            fragment_gap_ms=args.fragment_gap_ms,
            timeout_ms=args.timeout_ms,
            startup_delay_ms=args.startup_delay_ms,
        )
        runner.start()
        for case in cases:
            ok, reasons = runner.run_case(case)
            print(f"[{'PASS' if ok else 'FAIL'}] {case['id']} {case['title']}")
            for reason in reasons:
                print(f"       {reason}")
            failures += 0 if ok else 1
    finally:
        if runner is not None:
            runner.close()
        if ble_serial is not None:
            ble_serial.close()
        if v851_serial is not None:
            v851_serial.close()

    print(
        f"完成 {len(cases)} 条：通过 {len(cases) - failures}，"
        f"失败 {failures}，另有 "
        f"{runner.manual_checks if runner is not None else 0} 项调试器人工检查。"
    )
    return 1 if failures else 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    list_parser = subparsers.add_parser("list", help="列出测试用例")
    list_parser.add_argument("--vectors", default=str(DEFAULT_VECTORS))
    list_parser.add_argument(
        "--mode", choices=("ble", "v851", "both"), default="both"
    )
    list_parser.set_defaults(func=command_list)

    ports_parser = subparsers.add_parser("ports", help="列出当前串口")
    ports_parser.add_argument(
        "--probe", action="store_true", help="逐个尝试打开以检查是否被占用"
    )
    ports_parser.set_defaults(func=command_ports)

    monitor_parser = subparsers.add_parser("monitor", help="只监听一个串口的原始字节")
    monitor_parser.add_argument("--port", required=True)
    monitor_parser.add_argument("--baud", type=int, default=DEFAULT_BLE_BAUD)
    monitor_parser.add_argument("--seconds", type=float, default=10.0)
    monitor_parser.set_defaults(func=command_monitor)

    render_parser = subparsers.add_parser("render-md", help="生成或检查Markdown")
    render_parser.add_argument("--vectors", default=str(DEFAULT_VECTORS))
    render_parser.add_argument("--output", default=str(DEFAULT_DOCUMENT))
    render_parser.add_argument("--check", action="store_true")
    render_parser.set_defaults(func=command_render)

    run_parser = subparsers.add_parser("run", help="连接两路串口执行测试")
    run_parser.add_argument("--vectors", default=str(DEFAULT_VECTORS))
    run_parser.add_argument(
        "--mode", choices=("ble", "v851", "both"), default="both"
    )
    run_parser.add_argument("--ble-port")
    run_parser.add_argument("--v851-port")
    run_parser.add_argument("--ble-baud", type=int, default=DEFAULT_BLE_BAUD)
    run_parser.add_argument("--v851-baud", type=int, default=DEFAULT_V851_BAUD)
    run_parser.add_argument(
        "--fragment-gap-ms", type=int, default=DEFAULT_FRAGMENT_GAP_MS
    )
    run_parser.add_argument("--timeout-ms", type=int, default=DEFAULT_EXPECT_TIMEOUT_MS)
    run_parser.add_argument(
        "--startup-delay-ms", type=int, default=DEFAULT_V851_STARTUP_DELAY_MS
    )
    selection = run_parser.add_mutually_exclusive_group()
    selection.add_argument("--case")
    selection.add_argument("--suite", default="all")
    run_parser.set_defaults(func=command_run)
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        return int(args.func(args))
    except (OSError, RuntimeError, ValueError, TimeoutError) as exc:
        print(f"错误: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())

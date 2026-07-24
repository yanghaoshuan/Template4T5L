from __future__ import annotations

import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
UART_SOURCE = REPO_ROOT / "source" / "uart.c"
V851_SOURCE = REPO_ROOT / "modules" / "v851_protocol.c"
V851_HEADER = REPO_ROOT / "modules" / "v851_protocol.h"
V851_MOCK_SOURCE = REPO_ROOT / "modules" / "v851_control_mock.c"
PB03F_SOURCE = REPO_ROOT / "modules" / "pb03f_ble.c"
PB03F_HEADER = REPO_ROOT / "modules" / "pb03f_ble.h"


def function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[brace : index + 1]
    raise AssertionError(f"unterminated function: {signature}")


class UartBridgeRoutingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.source = UART_SOURCE.read_text(encoding="utf-8")
        cls.v851_source = V851_SOURCE.read_text(encoding="utf-8")
        cls.v851_header = V851_HEADER.read_text(encoding="utf-8")
        cls.v851_mock_source = V851_MOCK_SOURCE.read_text(encoding="utf-8")
        cls.pb03f_source = PB03F_SOURCE.read_text(encoding="utf-8")
        cls.pb03f_header = PB03F_HEADER.read_text(encoding="utf-8")

    def test_ble_and_v851_use_the_common_frame_scanner(self) -> None:
        body = function_body(self.source, "void UartReadFrame(UART_TYPE *uart)")
        task = function_body(
            self.source,
            "void UartProtocalHandleTask(void)",
        )
        scanner = body.index("while(i > 0)")
        self.assertGreater(
            body.index("PB03F_BLE_FRAME_MAGIC_HIGH"),
            scanner,
        )
        self.assertGreater(
            body.index("frame[frame_offset] == 0xaa"),
            scanner,
        )
        self.assertIn("uart == &Uart4", body)
        self.assertIn("uart == &PB03F_BLE_UART", body)
        self.assertIn("Pb03fBleIsTransparent() != 0U", body)
        self.assertNotIn("UartBridgeDwin8283Route", self.source)
        self.assertNotIn("UartBridgeReceive", self.source)
        self.assertNotIn("V851ProtocolReceive(", body)
        self.assertIn("UartReadFrame(&Uart4);", task)

    def test_common_scanner_keeps_standard_dwin_routing(self) -> None:
        body = function_body(self.source, "void UartReadFrame(UART_TYPE *uart)")
        protocol = function_body(
            self.source,
            "static void UartStandardDwin8283Protocal(",
        )
        self.assertIn("frame[frame_offset] == 0x5a", body)
        self.assertIn("UartStandardDwin8283Protocal", body)
        self.assertIn("frame[3] == 0x82", protocol)
        self.assertIn("frame[3] == 0x83", protocol)

    def test_pb03f_4d51_branch_checks_header_length_and_crc(self) -> None:
        body = function_body(self.source, "void UartReadFrame(UART_TYPE *uart)")
        self.assertIn("PB03F_BLE_FRAME_MAGIC_HIGH", body)
        self.assertIn("PB03F_BLE_FRAME_MAGIC_LOW", body)
        self.assertIn("PB03F_BLE_FRAME_VERSION", body)
        self.assertIn("payload_len > PB03F_BLE_CHUNK_PAYLOAD_MAX", body)
        self.assertIn("PB03F_BLE_FRAME_HEADER_SIZE + payload_len", body)
        self.assertIn("crc_16(&frame[frame_offset],", body)
        self.assertIn('Pb03fBleFrameError(msg_id, "crc error")', body)
        self.assertIn(
            "Pb03fBleReceive(&frame[frame_offset], one_frame_len)",
            body,
        )

    def test_uart4_aa55_branch_checks_length_command_and_crc(self) -> None:
        body = function_body(self.source, "void UartReadFrame(UART_TYPE *uart)")
        self.assertIn(
            "frame[frame_offset] == 0xaa",
            body,
        )
        self.assertIn(
            "frame[frame_offset + 1] == 0x55",
            body,
        )
        self.assertIn(
            "body_len = ((uint16_t)frame[frame_offset + 2U] << 8)",
            body,
        )
        self.assertIn("body_len < V851_JSON_BODY_OVERHEAD", body)
        self.assertIn("body_len > (BRIDGE_JSON_MAX +", body)
        self.assertIn("one_frame_len = body_len + 4U", body)
        self.assertIn(
            "frame[frame_offset + 4U] == V851_JSON_COMMAND",
            body,
        )
        self.assertIn(
            "crc_16(&frame[frame_offset + 4U],",
            body,
        )
        self.assertIn("body_len - 2U", body)
        self.assertIn("crc_calc == crc_recv", body)
        self.assertIn(
            "&frame[frame_offset + 5U], json_len",
            body,
        )
        self.assertIn("i -= one_frame_len", body)

    def test_uart4_has_no_partial_frame_state_or_ota_receive_path(self) -> None:
        self.assertNotIn("uart4_bridge_", self.source)
        self.assertNotIn("V851_RX_STREAM_TIMEOUT_MS", self.source)
        self.assertNotIn("V851_OTA_MAGIC_HIGH", self.source)
        body = function_body(self.source, "void UartReadFrame(UART_TYPE *uart)")
        self.assertIn("UartStandardDwin8283Protocal", body)
        self.assertIn("if(uart == &Uart_R11)", body)

    def test_pb03f_has_no_stream_frame_parser_and_uses_new_receive_api(self) -> None:
        self.assertNotIn("pb_frame_expected_len", self.pb03f_source)
        self.assertNotIn("pb_frame_tick", self.pb03f_source)
        self.assertNotIn("PbHandleTransparentBytes", self.pb03f_source)
        self.assertNotIn("PbSendAtCommand", self.pb03f_source)
        self.assertNotIn("PbForwardTask", self.pb03f_source)
        self.assertNotIn("PbSendTask", self.pb03f_source)
        self.assertIn(
            "void Pb03fBleReceive(const uint8_t *_data, uint16_t len);",
            self.pb03f_header,
        )
        self.assertIn(
            "void Pb03fBleFrameError(uint16_t msg_id, const char *message);",
            self.pb03f_header,
        )
        self.assertIn(
            "uint8_t Pb03fBleIsTransparent(void);",
            self.pb03f_header,
        )

    def test_tasks_assemble_queued_frames_and_use_uart_send_data(self) -> None:
        pb_task = function_body(self.pb03f_source, "void Pb03fBleTask(void)")
        v851_task = function_body(
            self.v851_source,
            "void V851ProtocolTask(void)",
        )
        self.assertIn("PB03F_BLE_FRAME_MAGIC_HIGH", pb_task)
        self.assertIn("crc_16(pb_tx_frame", pb_task)
        self.assertIn("UartSendData(&PB03F_BLE_UART", pb_task)
        self.assertIn("V851_FRAME_MAGIC_HIGH", v851_task)
        self.assertIn("crc_16(&v851_tx_buffer[slot][4]", v851_task)
        self.assertIn("UartSendData(&Uart4", v851_task)
        self.assertNotIn("V851UartIdle", self.v851_source)

    def test_single_reply_is_immediate_and_busy_reply_stays_as_json(self) -> None:
        pb_commit = function_body(
            self.pb03f_source,
            "static uint8_t PbCommitJson(",
        )
        v851_send = function_body(
            self.v851_source,
            "uint8_t V851ProtocolSendJson(",
        )
        self.assertIn("len <= PB03F_BLE_CHUNK_PAYLOAD_MAX", pb_commit)
        self.assertIn("pb_tx_count == 0U", pb_commit)
        self.assertIn("PB03F_BLE_UART.TxBusy == 0U", pb_commit)
        self.assertIn("UartSendData(&PB03F_BLE_UART", pb_commit)
        self.assertIn("memcpy(&v851_tx_buffer[slot][5], _data, len)", v851_send)
        self.assertIn("v851_tx_count == 0U", v851_send)
        self.assertIn("Uart4.TxBusy == 0U", v851_send)
        self.assertIn("v851_tx_len[slot] = len", v851_send)

    def test_protocol_exposes_json_only_receive_entry(self) -> None:
        self.assertIn(
            "void V851ProtocolReceiveJson(const uint8_t *_data, uint16_t len);",
            self.v851_header,
        )
        self.assertNotIn(
            "void V851ProtocolReceive(UART_TYPE *uart",
            self.v851_header,
        )
        receive_json = function_body(
            self.v851_source,
            "void V851ProtocolReceiveJson(",
        )
        self.assertIn("len > BRIDGE_JSON_MAX", receive_json)
        self.assertIn("JSON_Validate", receive_json)
        self.assertIn("V851HandleServerCommand", receive_json)
        self.assertIn(
            "V851ProtocolReceiveJson(v851_control_mock_json, json_len);",
            self.v851_mock_source,
        )


if __name__ == "__main__":
    unittest.main()

from __future__ import annotations

import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
UART_SOURCE = REPO_ROOT / "source" / "uart.c"
V851_SOURCE = REPO_ROOT / "modules" / "v851_protocol.c"
V851_HEADER = REPO_ROOT / "modules" / "v851_protocol.h"
V851_MOCK_SOURCE = REPO_ROOT / "modules" / "v851_control_mock.c"


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

    def test_uart4_uses_common_frame_scanner_and_pb03f_keeps_router(self) -> None:
        body = function_body(self.source, "void UartReadFrame(UART_TYPE *uart)")
        task = function_body(
            self.source,
            "void UartProtocalHandleTask(void)",
        )
        self.assertIn("while(i > 0)", body)
        self.assertIn(
            "if(uart == &Uart4)",
            body,
        )
        self.assertIn(
            "if(uart == &PB03F_BLE_UART)",
            body,
        )
        self.assertIn(
            "UartBridgeDwin8283Route(uart, frame, total_frame_len);",
            body,
        )
        self.assertNotIn(
            "V851ProtocolReceive(",
            body,
        )
        self.assertNotIn("Uart4Bridge", self.source)
        self.assertIn("UartReadFrame(&Uart4);", task)

    def test_router_handles_both_standard_dwin_commands(self) -> None:
        body = function_body(
            self.source,
            "static void UartBridgeDwin8283Route(",
        )
        self.assertIn("command == 0x82U", body)
        self.assertIn("command == 0x83U", body)
        self.assertIn("UartStandardDwin8283Protocal", body)
        self.assertIn("UartBridgeReceive", body)

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

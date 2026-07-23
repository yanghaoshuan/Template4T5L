from __future__ import annotations

import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
UART_SOURCE = REPO_ROOT / "source" / "uart.c"


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

    def test_bridge_ports_use_shared_dwin_router(self) -> None:
        body = function_body(self.source, "void UartReadFrame(UART_TYPE *uart)")
        self.assertIn(
            "if((uart == &Uart4) || (uart == &PB03F_BLE_UART))",
            body,
        )
        self.assertIn(
            "UartBridgeDwin8283Route(uart, frame, total_frame_len);",
            body,
        )
        self.assertNotIn(
            "V851ProtocolReceive(uart, frame, total_frame_len);",
            body,
        )
        self.assertNotIn(
            "Pb03fBleReceive(uart, frame, total_frame_len);",
            body,
        )

    def test_router_handles_both_standard_dwin_commands(self) -> None:
        body = function_body(
            self.source,
            "static void UartBridgeDwin8283Route(",
        )
        self.assertIn("command == 0x82U", body)
        self.assertIn("command == 0x83U", body)
        self.assertIn("UartStandardDwin8283Protocal", body)
        self.assertIn("UartBridgeReceive", body)


if __name__ == "__main__":
    unittest.main()

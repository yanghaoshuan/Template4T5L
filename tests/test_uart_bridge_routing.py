from __future__ import annotations

import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]


class UartRoutingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.config = (REPO_ROOT / "include/T5L/T5LOSConfig.h").read_text(encoding="utf-8")
        cls.config_t5f = (REPO_ROOT / "include/T5F/T5FOSConfig.h").read_text(encoding="utf-8")
        cls.uart = (REPO_ROOT / "source/uart.c").read_text(encoding="utf-8")
        cls.uart_h = (REPO_ROOT / "source/uart.h").read_text(encoding="utf-8")
        cls.main = (REPO_ROOT / "user/main.c").read_text(encoding="utf-8")
        cls.protocol = (REPO_ROOT / "modules/v851_protocol.c").read_text(encoding="utf-8")
        cls.protocol_h = (REPO_ROOT / "modules/v851_protocol.h").read_text(encoding="utf-8")
        cls.project = (REPO_ROOT / "project/T5L51.uvproj").read_text(encoding="utf-8")

    def test_feature_switches_and_uart_sizes(self) -> None:
        for expected in (
            "#define v851PROTOCOL_ENABLED             1",
            "#define pb03fBLE_ENABLED                 0",
            "#define otaOTA_ENABLED                 1",
            "#define uartUART2_ENABLED               1",
            "#define uartUART5_ENABLED               (sysBEAUTY_MODE_ENABLED ||",
            "#define uartUART_COMMON_FRAME_SIZE     4160",
            "#define uartUART4_RXBUF_SIZE         4160",
            "#define uartUART4_TXBUF_SIZE         2112",
            "#define uartUART4_BAUDRATE           115200",
        ):
            with self.subTest(expected=expected):
                self.assertIn(expected, self.config)
        self.assertIn("#define uartUART4_BAUDRATE           115200", self.config_t5f)

    def test_main_registers_only_v851_wifi_protocol_and_ota_paths(self) -> None:
        self.assertIn("V851WifiInit();", self.main)
        self.assertIn("V851ProtocolInit();", self.main)
        self.assertIn("V851_WIFI_TASK_INTERVAL, V851WifiTask", self.main)
        self.assertIn("V851_PROTOCOL_TASK_INTERVAL, V851ProtocolTask", self.main)
        self.assertIn("otaTASK_INTERVAL, OtaTask", self.main)
        self.assertNotIn("V851ControlMockTask", self.main)
        self.assertNotIn("core_json.h", self.main)

    def test_uart4_dispatches_all_three_length_families(self) -> None:
        self.assertIn("body_len +\n                                                  V851_TLV_FRAME_FIXED_SIZE", self.uart)
        self.assertIn("one_frame_len = (uint16_t)(body_len + 4U)", self.uart)
        self.assertIn("V851ProtocolReceiveFrame(&frame[frame_offset]", self.uart)
        self.assertIn("V851WifiReceiveFrame(&frame[frame_offset]", self.uart)
        self.assertIn("OtaReceive(&frame[frame_offset], one_frame_len)", self.uart)
        for obsolete in ("V851_JSON", "ReceiveJson", "BRIDGE_JSON_MAX"):
            self.assertNotIn(obsolete, self.uart)
        for command in (
            "V851_TLV_CMD_PROPERTY",
            "V851_TLV_CMD_FACTORY",
            "V851_TLV_CMD_BOOTSTRAP_RESULT",
            "V851_TLV_CMD_OTA_STATUS",
        ):
            self.assertIn(command, self.uart)
        self.assertIn("V851_TLV_CMD_SNAPSHOT", self.protocol_h)

    def test_uart4_overflow_discards_batch(self) -> None:
        self.assertIn("uint8_t RxOverflow:1", self.uart_h)
        self.assertIn("if(next_head == Uart4.RxTail)", self.uart)
        self.assertIn("Uart4.RxOverflow = 1U", self.uart)
        self.assertIn("uart->RxTail = uart->RxHead", self.uart)
        self.assertIn("uart->RxOverflow = 0U", self.uart)

    def test_v851_active_protocol_has_no_json_or_pb03f(self) -> None:
        for obsolete in (
            "core_json",
            "bridge_json",
            "pb03f",
            "ReceiveJson",
            "SendJson",
            "hello",
            "dedup",
            "expire_at",
        ):
            with self.subTest(obsolete=obsolete):
                self.assertNotIn(obsolete, self.protocol.lower())

    def test_new_modules_are_in_keil_project(self) -> None:
        for filename in ("v851_tlv_app.c", "v851_wifi.c", "v851_wifi.h"):
            self.assertIn(filename, self.project)


if __name__ == "__main__":
    unittest.main()

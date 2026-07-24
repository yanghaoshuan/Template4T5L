from __future__ import annotations

import re
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
V851_SOURCE = REPO_ROOT / "modules" / "v851_protocol.c"


def function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    index = brace
    quote: str | None = None
    escaped = False
    while index < len(source):
        char = source[index]
        next_char = source[index + 1] if index + 1 < len(source) else ""
        if quote is not None:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == quote:
                quote = None
        elif char in {'"', "'"}:
            quote = char
        elif char == "/" and next_char == "*":
            end = source.index("*/", index + 2)
            index = end + 1
        elif char == "/" and next_char == "/":
            end = source.find("\n", index + 2)
            index = len(source) if end < 0 else end
        elif char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[brace : index + 1]
        index += 1
    raise AssertionError(f"unterminated function: {signature}")


class V851StateReportingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.source = V851_SOURCE.read_text(encoding="utf-8")

    def test_hello_and_hello_ack_schedule_power_on_snapshot(self) -> None:
        hello = function_body(
            self.source,
            "static void V851CacheDeviceHello(",
        )
        hello_ack = function_body(
            self.source,
            "static void V851CacheHelloAck(",
        )
        self.assertIn("V851StateRequestSnapshot();", hello)
        self.assertIn("V851StateRequestSnapshot();", hello_ack)
        self.assertIn("v851_time_valid = 1U;", hello_ack)

    def test_snapshot_uses_showdoc_envelope_and_state_shape(self) -> None:
        snapshot = function_body(
            self.source,
            "static uint16_t V851StateBuildSnapshot(void)",
        )
        header = function_body(
            self.source,
            "static void V851StateWriterReportHeader(",
        )
        self.assertIn('"device.snapshot"', snapshot)
        self.assertIn('\\"data\\":{\\"state\\":{\\"device\\":{', snapshot)
        self.assertIn('\\"bind_status\\":', snapshot)
        self.assertIn('\\"actuators\\":{', snapshot)
        self.assertIn('\\"reported_at\\":', snapshot)
        self.assertIn('\\"protocol_version\\":\\"1.0\\",', header)
        self.assertIn("V851WriterIdentity(writer);", header)

    def test_v851_owned_state_groups_are_omitted(self) -> None:
        snapshot = function_body(
            self.source,
            "static uint16_t V851StateBuildSnapshot(void)",
        )
        for group in (
            '\\"firmware\\"',
            '\\"network\\"',
            '\\"power\\"',
            '\\"environment\\"',
            '\\"door\\"',
            '\\"camera\\"',
            '\\"storage\\"',
        ):
            with self.subTest(group=group):
                self.assertNotIn(group, snapshot)

    def test_incremental_paths_stay_inside_showdoc_whitelist(self) -> None:
        control = function_body(
            self.source,
            "static uint8_t V851StateReadControl(",
        )
        groups = set(
            re.findall(r'\*group = "([a-z_]+)"', control)
        )
        number_fields = set(
            re.findall(r'\*value_name = "([a-z_]+)"', control)
        )
        self.assertEqual(
            groups,
            {
                "exhaust",
                "light",
                "uvb",
                "anion",
                "plasma",
                "climate",
                "humidifier",
                "inlet_fan",
            },
        )
        self.assertEqual(
            number_fields,
            {"speed_level", "brightness_level", "target_celsius"},
        )
        property_writer = function_body(
            self.source,
            "static void V851StateWriterProperty(",
        )
        self.assertIn('{\\"path\\":\\"actuators.', property_writer)
        actuator_writer = function_body(
            self.source,
            "static uint8_t V851StateWriterActuator(",
        )
        self.assertIn('"enabled"', actuator_writer)
        self.assertIn('\\"value_type\\":\\"boolean\\"', self.source)
        self.assertIn('\\"value_type\\":\\"number\\"', self.source)

    def test_control_slots_are_read_for_state_reporting(self) -> None:
        control = function_body(
            self.source,
            "static uint8_t V851StateReadControl(",
        )
        mark = function_body(
            self.source,
            "static void V851StateMarkControlChanged(",
        )
        for name in (
            "EXHAUST",
            "INLET_FAN",
            "UVB",
            "HUMIDIFIER",
            "LIGHT",
            "CLIMATE",
            "PLASMA",
            "ANION",
        ):
            with self.subTest(name=name):
                self.assertIn(f"V851_CONTROL_DGUS_{name}_ADDR", control)
        self.assertIn("read_dgus_vp(address, record", control)
        self.assertIn("v851_state_pending_mask |=", mark)
        command = function_body(
            self.source,
            "static void V851HandleServerCommand(",
        )
        self.assertNotIn("V851StateMarkControlChanged(command.type);", command)

    def test_device_side_control_changes_are_detected_periodically(self) -> None:
        scan = function_body(
            self.source,
            "static void V851StateScanControls(void)",
        )
        service = function_body(
            self.source,
            "static void V851StateServiceReports(void)",
        )
        actuator_writer = function_body(
            self.source,
            "static uint8_t V851StateWriterActuator(",
        )
        self.assertIn(
            "#define V851_STATE_CONTROL_SCAN_INTERVAL_MS      500UL",
            self.source,
        )
        self.assertIn("v851_state_snapshot_sent == 0U", scan)
        self.assertIn("V851_STATE_CONTROL_SCAN_INTERVAL_MS", scan)
        self.assertIn("type < V851_CONTROL_DEVICE_SETTINGS", scan)
        self.assertIn("V851StateReadControl(type, record", scan)
        self.assertIn("memcmp(v851_state_control_shadow[type]", scan)
        self.assertIn("memcpy(v851_state_control_shadow[type]", scan)
        self.assertIn("V851StateMarkControlChanged(type);", scan)
        self.assertIn("V851StateScanControls();", service)
        self.assertIn(
            "memcpy(v851_state_control_shadow[type]",
            actuator_writer,
        )

    def test_report_task_sends_snapshot_before_incremental(self) -> None:
        service = function_body(
            self.source,
            "static void V851StateServiceReports(void)",
        )
        snapshot = service.index("v851_state_snapshot_pending")
        incremental = service.index("V851StateBuildPropertyReport")
        self.assertLess(snapshot, incremental)
        self.assertIn("v851_state_snapshot_sent = 1U;", service)
        self.assertIn("V851ProtocolSendJson(v851_state_report_json", service)

    def test_incremental_reports_apply_five_second_coalescing_limit(self) -> None:
        service = function_body(
            self.source,
            "static void V851StateServiceReports(void)",
        )
        self.assertIn(
            "#define V851_STATE_MIN_REPORT_INTERVAL_MS       5000UL",
            self.source,
        )
        self.assertIn("v851_state_last_report_tick", service)
        self.assertIn("V851_STATE_MIN_REPORT_INTERVAL_MS", service)
        self.assertIn("v851_state_report_started = 1U;", service)


if __name__ == "__main__":
    unittest.main()

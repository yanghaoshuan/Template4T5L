from __future__ import annotations

import json
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]


class V851ControlDgusTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.header = (
            REPO_ROOT / "modules" / "v851_control_info.h"
        ).read_text(encoding="utf-8")
        cls.source = (
            REPO_ROOT / "modules" / "v851_control_info.c"
        ).read_text(encoding="utf-8")
        cls.main = (REPO_ROOT / "user" / "main.c").read_text(encoding="utf-8")
        cls.mock_source = (
            REPO_ROOT / "modules" / "v851_control_mock.c"
        ).read_text(encoding="utf-8")
        cls.mock_header = (
            REPO_ROOT / "modules" / "v851_control_mock.h"
        ).read_text(encoding="utf-8")
        cls.pb_source = (
            REPO_ROOT / "modules" / "pb03f_ble.c"
        ).read_text(encoding="utf-8")
        cls.config = (
            REPO_ROOT / "include" / "T5L" / "T5LOSConfig.h"
        ).read_text(encoding="utf-8")
        cls.vectors = json.loads(
            (REPO_ROOT / "tests" / "serial_bridge_vectors.json").read_text(
                encoding="utf-8"
            )
        )

    def test_dgus_address_layout_matches_vp_plan(self) -> None:
        expected = {
            "EXHAUST": "0x5100UL",
            "INLET_FAN": "0x5104UL",
            "UVB": "0x510CUL",
            "HUMIDIFIER": "0x5110UL",
            "LIGHT": "0x5114UL",
            "CLIMATE": "0x5118UL",
            "PLASMA": "0x5120UL",
            "ANION": "0x5124UL",
        }
        self.assertIn(
            "#define V851_CONTROL_DGUS_SLOT_WORDS             0x0004U",
            self.header,
        )
        for name, address in expected.items():
            with self.subTest(name=name):
                self.assertIn(
                    f"#define V851_CONTROL_DGUS_{name}_ADDR",
                    self.header,
                )
                self.assertIn(address, self.header)

    def test_control_info_includes_timer_for_get_sys_tick(self) -> None:
        self.assertIn('#include "timer.h"', self.source)
        self.assertIn("GetSysTick()", self.source)

    def test_eight_handlers_are_registered_and_write_actual_fields(self) -> None:
        for control_type in (
            "V851_CONTROL_EXHAUST",
            "V851_CONTROL_INLET_FAN",
            "V851_CONTROL_UVB",
            "V851_CONTROL_HUMIDIFIER",
            "V851_CONTROL_CLIMATE",
            "V851_CONTROL_LIGHT",
            "V851_CONTROL_PLASMA",
            "V851_CONTROL_ANION",
        ):
            with self.subTest(control_type=control_type):
                self.assertIn(
                    f"{control_type}, V851ControlInfoDgusHandler",
                    self.source,
                )
        self.assertIn('"target_humidity"', self.source)
        self.assertIn('"duration_minutes"', self.source)
        self.assertIn("V851_CONTROL_STATUS_SUCCESS", self.source)
        self.assertIn('"INVALID_PARAMS"', self.source)
        self.assertIn("write_dgus_vp(address, record", self.source)
        self.assertNotIn(
            "V851_CONTROL_DEVICE_SETTINGS, V851ControlInfoDgusHandler",
            self.source,
        )

    def test_mock_is_enabled_but_injected_after_scheduler_starts(self) -> None:
        register_index = self.main.index("(void)V851ControlInfoDgusInit();")
        mock_task_index = self.main.index(
            "V851_CONTROL_MOCK_TASK_INTERVAL, V851ControlMockTask"
        )
        self.assertLess(register_index, mock_task_index)
        self.assertIn(
            "#define v851CONTROL_MOCK_ENABLED         1",
            self.config,
        )
        self.assertNotIn("V851ControlMockInjectAll();", self.main)
        self.assertIn("V851_CONTROL_MOCK_START_DELAY_MS", self.mock_source)
        self.assertIn(
            "V851ControlMockInject(v851_control_mock_task_case)",
            self.mock_source,
        )
        self.assertIn("v851_control_mock_task_case++", self.mock_source)
        for mock_case in (
            "V851_CONTROL_MOCK_EXHAUST",
            "V851_CONTROL_MOCK_INLET_FAN",
            "V851_CONTROL_MOCK_UVB",
            "V851_CONTROL_MOCK_HUMIDIFIER",
            "V851_CONTROL_MOCK_LIGHT",
            "V851_CONTROL_MOCK_CLIMATE",
            "V851_CONTROL_MOCK_PLASMA",
            "V851_CONTROL_MOCK_ANION",
        ):
            with self.subTest(mock_case=mock_case):
                self.assertIn(mock_case, self.mock_header)
                self.assertIn(mock_case, self.mock_source)
        self.assertNotIn("V851_CONTROL_MOCK_DEVICE_SETTINGS", self.mock_header)
        self.assertNotIn('"device_settings.set"', self.mock_source)
        self.assertNotIn("V851ControlInfoDgusTask", self.main)

    def test_pb_binding_qr_uses_planned_address(self) -> None:
        self.assertIn("#define PB_QR_VP_ADDR", self.pb_source)
        self.assertIn("0x5500U", self.pb_source)
        self.assertNotIn("#define PB_QR_VP_ADDR                         0x05ADU", self.pb_source)

    def test_dgus_handler_has_a_c51_overlay_safe_direct_call(self) -> None:
        protocol = (
            REPO_ROOT / "modules" / "v851_protocol.c"
        ).read_text(encoding="utf-8")
        self.assertIn(
            "v851_handlers[command.type] == V851ControlInfoDgusHandler",
            protocol,
        )
        self.assertIn(
            "V851ControlInfoDgusHandler(&handler_context);",
            protocol,
        )

    def test_selected_control_vectors_expect_success(self) -> None:
        selected = {
            "V851-CTL-EXHAUST",
            "V851-CTL-PLASMA",
            "V851-CTL-ANION",
            "V851-CTL-CLIMATE",
            "V851-CTL-INLET",
            "V851-CTL-HUMIDIFIER",
            "V851-CTL-UVB",
            "V851-CTL-LIGHT",
        }
        by_id = {case["id"]: case for case in self.vectors["cases"]}
        for case_id in selected:
            with self.subTest(case_id=case_id):
                match = by_id[case_id]["expected"][0]["match"]["data"]
                self.assertEqual(match["status"], "SUCCESS")
                self.assertNotIn("error_code", match)

    def test_device_settings_now_expects_unsupported(self) -> None:
        by_id = {case["id"]: case for case in self.vectors["cases"]}
        match = by_id["V851-CTL-SETTINGS"]["expected"][0]["match"]["data"]
        self.assertEqual(match["status"], "UNSUPPORTED")
        self.assertEqual(match["error_code"], "UNSUPPORTED_CMD")

    def test_invalid_control_vectors_expect_invalid_params(self) -> None:
        by_id = {case["id"]: case for case in self.vectors["cases"]}
        for case_id in (
            "V851-CTL-INVALID-MISSING",
            "V851-CTL-INVALID-BOOL",
            "V851-CTL-INVALID-TEXT",
            "V851-CTL-INVALID-RANGE",
        ):
            with self.subTest(case_id=case_id):
                match = by_id[case_id]["expected"][0]["match"]["data"]
                self.assertEqual(match["status"], "FAILED")
                self.assertEqual(match["error_code"], "INVALID_PARAMS")


if __name__ == "__main__":
    unittest.main()

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
        cls.vectors = json.loads(
            (REPO_ROOT / "tests" / "serial_bridge_vectors.json").read_text(
                encoding="utf-8"
            )
        )

    def test_dgus_address_layout_is_contiguous(self) -> None:
        expected = {
            "EXHAUST": "0x3000UL",
            "CLIMATE": "0x3010UL",
            "LIGHT": "0x3020UL",
            "SETTINGS": "0x3030UL",
            "HUMIDIFIER": "0x3040UL",
        }
        for name, address in expected.items():
            with self.subTest(name=name):
                self.assertIn(
                    f"#define V851_CONTROL_DGUS_{name}_ADDR",
                    self.header,
                )
                self.assertIn(address, self.header)

    def test_five_handlers_are_registered_and_write_actual_fields(self) -> None:
        for control_type in (
            "V851_CONTROL_EXHAUST",
            "V851_CONTROL_CLIMATE",
            "V851_CONTROL_LIGHT",
            "V851_CONTROL_DEVICE_SETTINGS",
            "V851_CONTROL_HUMIDIFIER",
        ):
            with self.subTest(control_type=control_type):
                self.assertIn(
                    f"{control_type}, V851ControlInfoDgusHandler",
                    self.source,
                )
        self.assertIn('"target_humidity"', self.source)
        self.assertIn("V851_CONTROL_STATUS_SUCCESS", self.source)
        self.assertIn('"INVALID_PARAMS"', self.source)
        self.assertIn("write_dgus_vp(address, record", self.source)

    def test_handlers_are_registered_before_mock_injection(self) -> None:
        register_index = self.main.index("(void)V851ControlInfoDgusInit();")
        mock_index = self.main.index("(void)V851ControlMockInjectAll();")
        self.assertLess(register_index, mock_index)
        self.assertNotIn("V851ControlInfoDgusTask", self.main)

    def test_selected_control_vectors_expect_success(self) -> None:
        selected = {
            "V851-CTL-EXHAUST",
            "V851-CTL-CLIMATE",
            "V851-CTL-LIGHT",
            "V851-CTL-SETTINGS",
            "V851-CTL-HUMIDIFIER",
        }
        by_id = {case["id"]: case for case in self.vectors["cases"]}
        for case_id in selected:
            with self.subTest(case_id=case_id):
                match = by_id[case_id]["expected"][0]["match"]["data"]
                self.assertEqual(match["status"], "SUCCESS")
                self.assertNotIn("error_code", match)

    def test_unregistered_control_still_expects_unsupported(self) -> None:
        by_id = {case["id"]: case for case in self.vectors["cases"]}
        match = by_id["V851-CTL-PLASMA"]["expected"][0]["match"]["data"]
        self.assertEqual(match["status"], "UNSUPPORTED")

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

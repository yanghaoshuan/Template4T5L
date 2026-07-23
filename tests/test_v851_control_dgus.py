from __future__ import annotations

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

    def test_dgus_address_layout_uses_requested_base_and_stride(self) -> None:
        self.assertIn(
            "#define V851_CONTROL_DGUS_BASE_ADDR              0x3000UL",
            self.header,
        )
        self.assertIn(
            "#define V851_CONTROL_DGUS_ADDR_STRIDE            0x0010UL",
            self.header,
        )
        for slot, control_type in enumerate(
            (
                "V851_CONTROL_EXHAUST",
                "V851_CONTROL_CLIMATE",
                "V851_CONTROL_LIGHT",
                "V851_CONTROL_DEVICE_SETTINGS",
            )
        ):
            with self.subTest(slot=slot):
                case = f"case {slot}U:"
                case_index = self.source.index(case)
                return_index = self.source.index(
                    f"return {control_type};",
                    case_index,
                )
                self.assertGreater(return_index, case_index)

    def test_control_api_is_called_and_updates_are_written(self) -> None:
        self.assertIn("V851ControlInfoIsUpdated(type)", self.source)
        self.assertIn("V851ControlInfoGet(type)", self.source)
        self.assertIn("write_dgus_vp(address, record", self.source)
        self.assertIn("V851ControlInfoClearUpdated(type)", self.source)

    def test_mock_controls_are_injected_and_sync_task_is_registered(self) -> None:
        self.assertIn("(void)V851ControlMockInjectAll();", self.main)
        self.assertIn("V851ControlInfoDgusTask();", self.main)
        self.assertIn(
            "V851_CONTROL_DGUS_TASK_INTERVAL, V851ControlInfoDgusTask",
            self.main,
        )


if __name__ == "__main__":
    unittest.main()

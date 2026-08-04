from __future__ import annotations

import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]


class V851ControlDgusTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.header = (REPO_ROOT / "modules/v851_control_info.h").read_text(encoding="utf-8")
        cls.source = (REPO_ROOT / "modules/v851_control_info.c").read_text(encoding="utf-8")
        cls.protocol = (REPO_ROOT / "modules/v851_protocol.c").read_text(encoding="utf-8")
        cls.protocol_h = (REPO_ROOT / "modules/v851_protocol.h").read_text(encoding="utf-8")
        cls.tlv_app = (REPO_ROOT / "modules/v851_tlv_app.c").read_text(encoding="utf-8")

    def test_dgus_address_layout(self) -> None:
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
        for name, address in expected.items():
            self.assertIn(f"V851_CONTROL_DGUS_{name}_ADDR", self.header)
            self.assertIn(address, self.header)

    def test_only_semantically_compatible_fields_are_mapped(self) -> None:
        self.assertIn("V851ControlInfoIsEnabledTag", self.source)
        self.assertIn("V851ControlInfoIsLevelTag", self.source)
        self.assertIn("V851_TLV_STRUCT_CLIMATE", self.source)
        self.assertIn("V851TlvReadBinary64Uint16", self.source)
        for obsolete in (
            "target_humidity",
            "duration_minutes",
            "color_temperature",
            "core_json",
        ):
            self.assertNotIn(obsolete, self.source)

    def test_whole_frame_validation_precedes_any_apply(self) -> None:
        first_pass = self.protocol.index("First pass: reject the complete frame")
        second_pass = self.protocol.index("Second pass: the complete frame")
        apply = self.protocol.index("V851ControlInfoApplySegment", second_pass)
        self.assertLess(first_pass, second_pass)
        self.assertLess(second_pass, apply)
        self.assertNotIn("V851ControlInfoApplySegment", self.protocol[first_pass:second_pass])

    def test_remote_write_updates_shadow_without_ack(self) -> None:
        self.assertIn("memcpy(v851_control_shadow[index], record", self.source)
        self.assertNotIn("ACK", self.protocol)
        self.assertNotIn("ack", self.protocol)

    def test_binary64_uses_words_not_native_double(self) -> None:
        self.assertIn("uint32_t high_word", self.protocol)
        self.assertIn("uint32_t low_word", self.protocol)
        self.assertIn("exponent_bits", self.protocol)
        self.assertNotIn("double", self.protocol)

    def test_all_documented_types_and_tags_are_public(self) -> None:
        for value in range(0x01, 0x0B):
            self.assertIn(f"0x{value:02X}U", self.protocol_h)
        for value in range(0x61, 0x6E):
            self.assertIn(f"0x{value:02X}U", self.protocol_h)
        for name in (
            "POWER_MODE", "LAST_OPEN_TIME", "CO2", "EXH_INTERVAL_HOURS",
            "LIGHT_RUNNING", "UVB_DAILY_HOURS", "ANION_RUNNING",
            "PLASMA_RUNNING", "CLIMATE_CTRL_STATUS", "HUMI_LIQUID_STATUS",
            "INLET_RUNNING", "FILTER_NEED_REPLACE", "LAST_ERROR_CODE",
            "DISP_SCREEN_MODE", "LOCAL_PASSWORD", "CONFIG_VERSION",
            "STORAGE_FREE", "FACTORY_FW_VERSION", "FACTORY_REJECT_REASON",
            "BOOT_DEVICE_SN", "BOOT_BLE_ID", "BOOT_API_ENDPOINT",
            "BOOT_BIND_STATUS", "BOOT_QR_URL",
        ):
            self.assertIn(f"V851_TLV_TAG_{name}", self.protocol_h)

    def test_single_direct_project_hook(self) -> None:
        self.assertEqual(self.protocol.count("V851TlvApplicationSegment(command, &segment)"), 1)
        self.assertNotIn("V851ControlHandler", self.protocol)
        self.assertEqual(self.tlv_app.count("void V851TlvApplicationSegment("), 1)

    def test_bootstrap_cache_revalidates_before_writing(self) -> None:
        validate = self.tlv_app.index("Validate again locally")
        clear = self.tlv_app.index("memset(&v851_bootstrap_result")
        valid = self.tlv_app.index("v851_bootstrap_result_valid = 1U")
        self.assertLess(validate, clear)
        self.assertLess(clear, valid)


if __name__ == "__main__":
    unittest.main()

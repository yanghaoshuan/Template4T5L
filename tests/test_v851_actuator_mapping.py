from __future__ import annotations

import math
import unittest
from pathlib import Path

from tools import t5l_serial_simulator as sim


REPO_ROOT = Path(__file__).resolve().parents[1]


class V851ActuatorWireMappingTests(unittest.TestCase):
    def test_nine_segment_snapshot_matches_wire_vector(self) -> None:
        segments = [
            (0x61, [sim.field_u8(1, 1), sim.field_u8(2, 3),
                    sim.field_binary64(3, 2.0), sim.field_u8(4, 1)]),
            (0x62, [sim.field_u8(1, 1), sim.field_u8(2, 2),
                    sim.field_u8(3, 1)]),
            (0x63, [sim.field_u8(1, 1), sim.field_u8(2, 2),
                    sim.field_u8(3, 4), sim.field_u8(4, 1)]),
            (0x64, [sim.field_u8(1, 1), sim.field_u8(2, 1)]),
            (0x65, [sim.field_u8(1, 1), sim.field_u8(2, 1)]),
            (0x66, [sim.field_u8(1, 1), sim.field_binary64(2, 30.0),
                    sim.field_u8(3, 1), sim.field_u8(4, 1)]),
            (0x67, [sim.field_u8(1, 1), sim.field_binary64(2, 4.0),
                    sim.field_binary64(3, 1.0), sim.field_u8(4, 1),
                    sim.field_u8(5, 0)]),
            (0x68, [sim.field_u8(1, 1), sim.field_u8(2, 2),
                    sim.field_u8(3, 1)]),
            (0x69, [sim.field_u8(1, 80), sim.field_u8(2, 0)]),
        ]
        frame = sim.encode_snapshot(segments)
        expected = bytes.fromhex(
            "AA 55 00 AC 37 "
            "61 00 17 01 00 01 01 02 00 01 03 03 00 08 "
            "40 00 00 00 00 00 00 00 04 00 01 01 "
            "62 00 0C 01 00 01 01 02 00 01 02 03 00 01 01 "
            "63 00 10 01 00 01 01 02 00 01 02 03 00 01 04 04 00 01 01 "
            "64 00 08 01 00 01 01 02 00 01 01 "
            "65 00 08 01 00 01 01 02 00 01 01 "
            "66 00 17 01 00 01 01 02 00 08 40 3E 00 00 00 00 00 00 "
            "03 00 01 01 04 00 01 01 "
            "67 00 22 01 00 01 01 02 00 08 40 10 00 00 00 00 00 00 "
            "03 00 08 3F F0 00 00 00 00 00 00 04 00 01 01 05 00 01 00 "
            "68 00 0C 01 00 01 01 02 00 01 02 03 00 01 01 "
            "69 00 08 01 00 01 50 02 00 01 00"
        )
        self.assertEqual(frame, expected)
        decoded = sim.decode_tlv_frame(frame)
        self.assertEqual([segment.struct_type for segment in decoded.segments],
                         list(range(0x61, 0x6A)))

    def test_all_vp_hour_tables_round_trip_exactly(self) -> None:
        for mapping, table in sim.VP_HOUR_TABLES.items():
            for code, protocol_value in table.items():
                with self.subTest(mapping=mapping, code=code):
                    self.assertEqual(
                        sim.vp_code_to_protocol_value(mapping, code), protocol_value
                    )
                    self.assertEqual(
                        sim.protocol_value_to_vp_code(mapping, protocol_value), code
                    )
        self.assertEqual(sim.field_binary64(1, 0.5).value.hex(),
                         "3fe0000000000000")
        self.assertEqual(sim.field_binary64(1, 8.0).value.hex(),
                         "4020000000000000")
        self.assertEqual(sim.field_binary64(1, 12.0).value.hex(),
                         "4028000000000000")

    def test_unrepresentable_hours_and_climate_values_are_rejected(self) -> None:
        for value in (6.0, 12.0, 0.0, 1.5):
            with self.subTest(exhaust=value):
                with self.assertRaises(ValueError):
                    sim.protocol_value_to_vp_code("exhaust_interval", value)
        for value in (19.0, 20.5, 36.0, math.inf, math.nan):
            with self.subTest(climate=value):
                with self.assertRaises(ValueError):
                    sim.climate_target_to_vp(value)
        self.assertEqual(sim.climate_target_to_vp(20.0), 20)
        self.assertEqual(sim.climate_target_to_vp(35.0), 35)


class V851ActuatorFirmwareSourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.control_h = (REPO_ROOT / "modules/v851_control_info.h").read_text(
            encoding="utf-8"
        )
        cls.control = (REPO_ROOT / "modules/v851_control_info.c").read_text(
            encoding="utf-8"
        )
        cls.protocol = (REPO_ROOT / "modules/v851_protocol.c").read_text(
            encoding="utf-8"
        )
        cls.stc_h = (REPO_ROOT / "modules/t5l_stc.h").read_text(encoding="utf-8")
        cls.stc = (REPO_ROOT / "modules/t5l_stc.c").read_text(encoding="utf-8")

    def test_capacity_mask_and_per_type_word_counts_cover_all_fields(self) -> None:
        for declaration in (
            "V851_CONTROL_COUNT                       9U",
            "V851_CONTROL_FULL_MASK                   0x01FFU",
            "V851_CONTROL_REPORT_MAX_WORDS            5U",
            "V851_CONTROL_FIELD_MAX_BYTES             34U",
        ):
            self.assertIn(declaration, self.control_h)
        for words in ("return 5U", "return 4U", "return 3U", "return 2U"):
            self.assertIn(words, self.control)
        self.assertIn("V851_TLV_STRUCT_FILTER", self.control)

    def test_remote_commands_write_only_the_documented_word_count(self) -> None:
        self.assertIn("V851ControlInfoCommandWords(struct_type)", self.control)
        self.assertIn("read_dgus_vp(address, record, command_words)", self.control)
        self.assertIn("write_dgus_vp(address, record, command_words)", self.control)
        self.assertNotIn("V851_CONTROL_COMMAND_SLOT_WORDS", self.control)
        self.assertIn("default:\n            return 0UL", self.control)

    def test_status_fields_are_encoded_but_not_applied_as_controls(self) -> None:
        apply_start = self.control.index("uint8_t V851ControlInfoApplySegment")
        build_start = self.control.index("uint16_t V851ControlInfoBuildFields")
        apply = self.control[apply_start:build_start]
        for status_tag in (
            "V851_TLV_TAG_EXH_RUNNING",
            "V851_TLV_TAG_CLIMATE_CTRL_STATUS",
            "V851_TLV_TAG_HUMI_LIQUID_STATUS",
            "V851_TLV_TAG_FILTER_LIFE_PERCENT",
        ):
            self.assertNotIn(status_tag, apply)
            self.assertIn(status_tag, self.control[build_start:])

    def test_report_sources_and_timer_width_are_corrected(self) -> None:
        self.assertIn("databuf[22] = G_Device_Ctrl.UVB.running_time_h", self.stc)
        self.assertIn("databuf[44] = G_Device_Ctrl.Humidifier.liquid_status", self.stc)
        self.assertIn("databuf[47] = G_Device_Ctrl.Inlet_Fan.running", self.stc)
        self.assertIn("databuf[37] = 2", self.stc)
        self.assertIn("uint32_t Get_Interval_Run_Sec", self.stc)
        self.assertIn("uint32_t interval_time_s", self.stc_h)
        self.assertIn("MIST_INTERVAL_TIMER_12H (0x4)", self.stc_h)
        self.assertIn("OutWind_TIMER_8H (0x5)", self.stc_h)
        self.assertNotIn("OutWind_TIMER_6H", self.stc_h)
        self.assertNotIn("else if (th == 6)", self.stc)

    def test_incremental_can_send_valid_subset_but_snapshot_is_atomic(self) -> None:
        queue_start = self.protocol.index("static uint16_t V851ProtocolQueueStateMask")
        queue_end = self.protocol.index("void V851ProtocolRequestFactoryReport", queue_start)
        queue = self.protocol[queue_start:queue_end]
        self.assertIn("included_mask != requested_mask", queue)
        self.assertIn("command == V851_TLV_CMD_SNAPSHOT", queue)
        self.assertIn("return included_mask", queue)


if __name__ == "__main__":
    unittest.main()

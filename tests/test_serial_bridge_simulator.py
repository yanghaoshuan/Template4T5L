from __future__ import annotations

import json
import struct
import unittest
from pathlib import Path

from tools import t5l_serial_simulator as sim


REPO_ROOT = Path(__file__).resolve().parents[1]


class TlvCodecTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.vectors = json.loads(
            (REPO_ROOT / "tests" / "serial_bridge_vectors.json").read_text(
                encoding="utf-8"
            )
        )

    def test_document_examples_decode_and_reencode_byte_for_byte(self) -> None:
        for vector in self.vectors["tlv_examples"]:
            with self.subTest(vector=vector["id"]):
                raw = bytes.fromhex(vector["hex"])
                decoded = sim.decode_tlv_frame(raw)
                self.assertEqual(decoded.command, int(vector["command"], 16))
                self.assertEqual(
                    decoded.segments[0].struct_type,
                    int(vector["struct_type"], 16),
                )
                rebuilt = sim.encode_tlv_frame(
                    decoded.command,
                    [
                        (segment.struct_type, segment.fields)
                        for segment in decoded.segments
                    ],
                )
                self.assertEqual(rebuilt, raw)

    def test_default_factory_report_matches_document_vector(self) -> None:
        expected = bytes.fromhex(
            "AA 55 00 44 36 6A 00 40 "
            "01 00 02 4A 50 "
            "03 00 0E 4D 43 51 58 5F 50 45 54 5F 43 41 42 49 4E "
            "04 00 11 4D 43 51 58 2D 50 45 54 2D 43 41 42 49 4E 2D 56 31 "
            "05 00 07 48 57 2D 56 32 2E 30 "
            "06 00 09 46 57 2D 56 31 2E 30 2E 30"
        )
        frame = sim.encode_factory_report()
        self.assertEqual(frame, expected)
        self.assertEqual(len(frame), 72)
        self.assertEqual(len(frame), 4 + int.from_bytes(frame[2:4], "big"))

    def test_all_documented_alarm_vectors(self) -> None:
        vectors = {
            ("TEMP_HIGH", "HIGH"): "AA 55 00 1F 38 6D 00 1B 01 00 01 01 02 00 09 54 45 4D 50 5F 48 49 47 48 03 00 04 48 49 47 48 04 00 01 00",
            ("TEMP_LOW", "HIGH"): "AA 55 00 1E 38 6D 00 1A 01 00 01 01 02 00 08 54 45 4D 50 5F 4C 4F 57 03 00 04 48 49 47 48 04 00 01 00",
            ("TEMP_SENSOR_FAULT", "HIGH"): "AA 55 00 27 38 6D 00 23 01 00 01 01 02 00 11 54 45 4D 50 5F 53 45 4E 53 4F 52 5F 46 41 55 4C 54 03 00 04 48 49 47 48 04 00 01 00",
            ("HEATER_FAULT", "HIGH"): "AA 55 00 22 38 6D 00 1E 01 00 01 01 02 00 0C 48 45 41 54 45 52 5F 46 41 55 4C 54 03 00 04 48 49 47 48 04 00 01 00",
            ("OVERHEAT_PROTECTION", "CRITICAL"): "AA 55 00 2D 38 6D 00 29 01 00 01 01 02 00 13 4F 56 45 52 48 45 41 54 5F 50 52 4F 54 45 43 54 49 4F 4E 03 00 08 43 52 49 54 49 43 41 4C 04 00 01 00",
            ("LIQUID_LOW_WARNING", "MEDIUM"): "AA 55 00 2A 38 6D 00 26 01 00 01 01 02 00 12 4C 49 51 55 49 44 5F 4C 4F 57 5F 57 41 52 4E 49 4E 47 03 00 06 4D 45 44 49 55 4D 04 00 01 00",
            ("LOW_LIQUID", "MEDIUM"): "AA 55 00 22 38 6D 00 1E 01 00 01 01 02 00 0A 4C 4F 57 5F 4C 49 51 55 49 44 03 00 06 4D 45 44 49 55 4D 04 00 01 00",
            ("NEBULIZER_DRY_BURN", "HIGH"): "AA 55 00 28 38 6D 00 24 01 00 01 01 02 00 12 4E 45 42 55 4C 49 5A 45 52 5F 44 52 59 5F 42 55 52 4E 03 00 04 48 49 47 48 04 00 01 00",
            ("WATER_LEVEL_SENSOR_FAULT", "HIGH"): "AA 55 00 2E 38 6D 00 2A 01 00 01 01 02 00 18 57 41 54 45 52 5F 4C 45 56 45 4C 5F 53 45 4E 53 4F 52 5F 46 41 55 4C 54 03 00 04 48 49 47 48 04 00 01 00",
            ("HUMIDITY_SENSOR_FAULT", "HIGH"): "AA 55 00 2B 38 6D 00 27 01 00 01 01 02 00 15 48 55 4D 49 44 49 54 59 5F 53 45 4E 53 4F 52 5F 46 41 55 4C 54 03 00 04 48 49 47 48 04 00 01 00",
            ("EXHAUST_FAN_FAULT", "HIGH"): "AA 55 00 27 38 6D 00 23 01 00 01 01 02 00 11 45 58 48 41 55 53 54 5F 46 41 4E 5F 46 41 55 4C 54 03 00 04 48 49 47 48 04 00 01 00",
            ("INLET_FAN_FAULT", "HIGH"): "AA 55 00 25 38 6D 00 21 01 00 01 01 02 00 0F 49 4E 4C 45 54 5F 46 41 4E 5F 46 41 55 4C 54 03 00 04 48 49 47 48 04 00 01 00",
            ("FILTER_LIFE_EXHAUSTED", "MEDIUM"): "AA 55 00 2D 38 6D 00 29 01 00 01 01 02 00 15 46 49 4C 54 45 52 5F 4C 49 46 45 5F 45 58 48 41 55 53 54 45 44 03 00 06 4D 45 44 49 55 4D 04 00 01 00",
        }
        for (code, level), expected_hex in vectors.items():
            with self.subTest(code=code):
                self.assertEqual(
                    sim.encode_event_alarm(code, level, False),
                    bytes.fromhex(expected_hex),
                )

        recovery = sim.encode_event_alarm("TEMP_HIGH", "HIGH", True)
        self.assertEqual(recovery[-1], 1)

    def test_alarm_scan_trigger_recovery_change_and_busy_retry(self) -> None:
        reporter = sim.AlarmReporterModel()
        reporter.scan([0] * 10)
        self.assertIsNone(reporter.next_report())

        active = [0] * 10
        active[1] = 1
        reporter.scan(active)
        self.assertIsNone(reporter.next_report(queue_available=False))
        self.assertEqual(reporter.reported[1], 0)
        first = reporter.next_report()
        self.assertEqual(first[:3], (1, 1, False))
        self.assertIsNone(reporter.next_report())

        active[1] = 2
        reporter.scan(active)
        recovered = reporter.next_report()
        replacement = reporter.next_report()
        self.assertEqual(recovered[:3], (1, 1, True))
        self.assertEqual(replacement[:3], (1, 2, False))

        reporter.scan([0] * 10)
        self.assertEqual(reporter.next_report()[:3], (1, 2, True))

    def test_alarm_scan_skips_reserved_and_unknown_values(self) -> None:
        reporter = sim.AlarmReporterModel()
        values = [9] * 10
        reporter.scan(values)
        self.assertIsNone(reporter.next_report())

    def test_alarm_scan_round_robins_multiple_addresses(self) -> None:
        reporter = sim.AlarmReporterModel()
        values = [0] * 10
        values[1] = 3
        values[2] = 5
        values[4] = 1
        reporter.scan(values)
        reports = [reporter.next_report() for _ in range(3)]
        self.assertEqual([item[0] for item in reports], [1, 2, 4])

    def test_multi_segment_length_includes_command(self) -> None:
        power = bytes.fromhex(self.vectors["tlv_examples"][0]["hex"])
        door = bytes.fromhex(self.vectors["tlv_examples"][1]["hex"])
        power_segment = sim.decode_tlv_frame(power).segments[0]
        door_segment = sim.decode_tlv_frame(door).segments[0]
        frame = sim.encode_tlv_frame(
            sim.TLV_CMD_PROPERTY,
            [
                (power_segment.struct_type, power_segment.fields),
                (door_segment.struct_type, door_segment.fields),
            ],
        )
        self.assertEqual(frame[2:4], b"\x00\x26")
        self.assertEqual(len(frame), 42)
        self.assertEqual(len(frame), 4 + int.from_bytes(frame[2:4], "big"))
        self.assertEqual(len(sim.decode_tlv_frame(frame).segments), 2)

    def test_snapshot_and_bootstrap_share_command_by_direction(self) -> None:
        snapshot = sim.encode_snapshot(
            [(0x05, [sim.field_binary64(0x01, 25.3),
                     sim.field_binary64(0x02, 55.0)])] + [
                (struct_type, [sim.field_u8(0x01, struct_type & 1)])
                for struct_type in range(0x61, 0x6A)
            ]
        )
        decoded = sim.decode_tlv_frame(snapshot)
        self.assertEqual(decoded.command, sim.TLV_CMD_SNAPSHOT)
        self.assertEqual(
            [segment.struct_type for segment in decoded.segments],
            [0x05, *range(0x61, 0x6A)],
        )
        with self.assertRaises(sim.ProtocolError):
            sim.decode_bootstrap_result(snapshot)
        with self.assertRaises(ValueError):
            sim.encode_snapshot([(0x61, [sim.field_u8(0x01, 1)])])

    def test_2048_byte_tlv_boundary(self) -> None:
        field = sim.TlvField(0xFE, b"X" * 2037)
        frame = sim.encode_tlv_frame(0x35, [(0xEE, [field])])
        self.assertEqual(len(frame), 2048)
        self.assertEqual(frame[2:4], b"\x07\xFC")
        self.assertEqual(sim.decode_tlv_frame(frame).segments[0].fields[0], field)

    def test_tlv_oversize_rejected(self) -> None:
        with self.assertRaises(ValueError):
            sim.encode_tlv_frame(0x35, [(0xEE, [sim.TlvField(1, b"X" * 2038)])])

    def test_unknown_type_and_tag_are_preserved(self) -> None:
        frame = sim.encode_tlv_frame(0x35, [(0xFE, [sim.TlvField(0xFD, b"raw")])])
        segment = sim.decode_tlv_frame(frame).segments[0]
        self.assertEqual(segment.struct_type, 0xFE)
        self.assertEqual(segment.fields[0], sim.TlvField(0xFD, b"raw"))

    def test_binary64_is_network_order(self) -> None:
        field = sim.field_binary64(2, 30.0)
        self.assertEqual(field.value, bytes.fromhex("40 3E 00 00 00 00 00 00"))
        self.assertEqual(struct.unpack(">d", field.value)[0], 30.0)

    def test_truncated_and_overflowing_fields_are_rejected(self) -> None:
        valid = bytearray(
            sim.encode_tlv_frame(0x35, [(0x61, [sim.field_u8(1, 1)])])
        )
        with self.assertRaises(sim.ProtocolError):
            sim.decode_tlv_frame(bytes(valid[:-1]))
        valid[9:11] = b"\x00\x02"
        with self.assertRaises(sim.ProtocolError):
            sim.decode_tlv_frame(bytes(valid))

    def test_bootstrap_document_example_and_cache(self) -> None:
        vector = next(
            item
            for item in self.vectors["tlv_examples"]
            if item["id"] == "TLV-BOOTSTRAP-RESULT"
        )
        raw = bytes.fromhex(vector["hex"])
        self.assertEqual(len(raw), 114)
        self.assertEqual(raw[2:4], b"\x00\x6E")
        self.assertEqual(raw[6:8], b"\x00\x6A")

        cache = sim.BootstrapResultCache()
        self.assertIsNone(cache.get())
        result = cache.receive(raw)
        self.assertIs(cache.get(), result)
        self.assertEqual(result.device_sn, b"MQXQ-PET-CN-2026-000002")
        self.assertEqual(result.ble_id, b"AVUU69")
        self.assertEqual(result.api_endpoint, b"https://test-api-cn.mcqx.pet")
        self.assertEqual(result.bind_status, b"UNBOUND")
        self.assertEqual(result.qr_url, b"https://b.mcqx.pet?b=AVUU69")

    def test_bootstrap_missing_unknown_duplicate_empty_and_truncation(self) -> None:
        frame = sim.encode_tlv_frame(
            sim.TLV_CMD_BOOTSTRAP_RESULT,
            [
                (
                    sim.TLV_STRUCT_BOOTSTRAP_RESULT,
                    [
                        sim.TlvField(0xFE, b"ignored"),
                        sim.TlvField(0x01, b"S" * 64),
                        sim.TlvField(0x02, b"first"),
                        sim.TlvField(0x02, b"last"),
                        sim.TlvField(0x04, b""),
                    ],
                )
            ],
        )
        result = sim.decode_bootstrap_result(frame)
        self.assertEqual(result.device_sn, b"S" * 63)
        self.assertEqual(result.ble_id, b"last")
        self.assertEqual(result.api_endpoint, b"")
        self.assertEqual(result.bind_status, b"")
        self.assertEqual(result.qr_url, b"")

    def test_bootstrap_last_segment_wins_and_bad_frame_is_atomic(self) -> None:
        cache = sim.BootstrapResultCache()
        valid = sim.encode_tlv_frame(
            sim.TLV_CMD_BOOTSTRAP_RESULT,
            [
                (sim.TLV_STRUCT_BOOTSTRAP_RESULT, [sim.field_text(0x02, "OLD")]),
                (sim.TLV_STRUCT_BOOTSTRAP_RESULT, [sim.field_text(0x02, "NEW")]),
            ],
        )
        self.assertEqual(cache.receive(valid).ble_id, b"NEW")

        first = sim.encode_segment(
            sim.TLV_STRUCT_BOOTSTRAP_RESULT, [sim.field_text(0x02, "BROKEN")]
        )
        malformed_payload = b"\x01\x00\x02X"
        malformed = bytes([0x61]) + len(malformed_payload).to_bytes(2, "big")
        malformed += malformed_payload
        segments = first + malformed
        bad = b"\xAA\x55" + (len(segments) + 1).to_bytes(2, "big")
        bad += bytes([sim.TLV_CMD_BOOTSTRAP_RESULT]) + segments
        with self.assertRaises(sim.ProtocolError):
            cache.receive(bad)
        self.assertEqual(cache.get().ble_id, b"NEW")

    def test_bootstrap_qr_vp_payload_boundaries(self) -> None:
        self.assertEqual(sim.BOOTSTRAP_QR_VP_ADDR, 0x5500)
        self.assertEqual(sim.BOOTSTRAP_QR_VP_WORDS, 64)

        url = b"https://b.mcqx.pet?b=AVUU69"
        payload = sim.bootstrap_qr_vp_payload(sim.BootstrapResult(qr_url=url))
        self.assertEqual(len(payload), 128)
        self.assertEqual(payload[: len(url)], url)
        self.assertEqual(payload[len(url) :], bytes(128 - len(url)))

        maximum = b"Q" * 127
        self.assertEqual(
            sim.bootstrap_qr_vp_payload(sim.BootstrapResult(qr_url=maximum)),
            maximum + b"\x00",
        )
        for invalid in (b"", b"Q" * 128, b"Q" * 255):
            with self.subTest(length=len(invalid)):
                self.assertEqual(
                    sim.bootstrap_qr_vp_payload(
                        sim.BootstrapResult(qr_url=invalid)
                    ),
                    bytes(128),
                )


class WifiAndOtaTests(unittest.TestCase):
    def test_monitor_defaults_to_uart4_baudrate(self) -> None:
        args = sim.build_parser().parse_args(["monitor", "--port", "COM8"])
        self.assertEqual(args.baud, 115200)

    def test_wifi_scan_uses_r11_length_semantics(self) -> None:
        frame = sim.encode_wifi_scan(2)
        self.assertEqual(frame, bytes.fromhex("AA 55 00 03 C0 0A 05"))
        self.assertEqual(len(frame), 4 + int.from_bytes(frame[2:4], "big"))

    def test_wifi_connect_has_no_four_trailing_bytes(self) -> None:
        frame = sim.encode_wifi_connect("TestNet", "1234")
        self.assertEqual(
            frame,
            bytes.fromhex(
                "AA 55 00 11 C1 01 00 07 54 65 73 74 4E 65 74 00 04 31 32 33 34"
            ),
        )
        self.assertEqual(len(frame), 4 + int.from_bytes(frame[2:4], "big"))

    def test_wifi_status_frame(self) -> None:
        raw = bytes.fromhex("AA 55 00 05 C5 00 00 00 02")
        decoded = sim.decode_wifi_frame(raw)
        self.assertEqual(decoded.command, 0xC5)
        self.assertEqual(decoded.payload[-1], 2)

    def test_4124_byte_ota_boundary(self) -> None:
        frame = sim.encode_ota_frame(0x05, b"D" * 4119)
        self.assertEqual(len(frame), 4124)
        self.assertEqual(sim.decode_ota_frame(frame).payload, b"D" * 4119)
        with self.assertRaises(ValueError):
            sim.encode_ota_frame(0x05, b"D" * 4120)

    def test_crc_known_vector_and_corruption(self) -> None:
        self.assertEqual(sim.crc16_modbus(b"123456789"), 0x4B37)
        payload = b"ota-data"
        crc = sim.crc16_modbus(payload)
        packet = payload + crc.to_bytes(2, "little")
        self.assertEqual(sim.crc16_modbus(packet[:-2]), int.from_bytes(packet[-2:], "little"))
        corrupted = bytes([packet[0] ^ 1]) + packet[1:]
        self.assertNotEqual(
            sim.crc16_modbus(corrupted[:-2]),
            int.from_bytes(corrupted[-2:], "little"),
        )

    def test_event_alarm_uses_38_6d(self) -> None:
        frame = sim.encode_event_alarm("FILTER_LIFE_EXHAUSTED", "MEDIUM", False)
        decoded = sim.decode_tlv_frame(frame)
        self.assertEqual(decoded.command, 0x38)
        self.assertEqual(decoded.segments[0].struct_type, 0x6D)
        self.assertEqual(
            decoded.segments[0].fields,
            (
                sim.field_u8(0x01, 1),
                sim.field_text(0x02, "FILTER_LIFE_EXHAUSTED"),
                sim.field_text(0x03, "MEDIUM"),
                sim.field_u8(0x04, 0),
            ),
        )


class StreamRecoveryTests(unittest.TestCase):
    def test_noise_fragmentation_and_sticky_frames(self) -> None:
        tlv = sim.encode_tlv_frame(0x35, [(0x61, [sim.field_u8(1, 1)])])
        wifi = sim.encode_wifi_scan(0)
        ota = sim.encode_ota_frame(0x04)
        decoder = sim.FrameStreamDecoder()
        self.assertEqual(decoder.feed(b"noise\xAA"), [])
        frames = decoder.feed(tlv[1:7])
        self.assertEqual(frames, [])
        frames = decoder.feed(tlv[7:] + wifi + ota)
        self.assertEqual([frame.kind for frame in frames], ["tlv", "wifi", "ota"])
        self.assertEqual(decoder.dropped_bytes, 5)

    def test_bootstrap_and_event_alarm_are_stream_commands(self) -> None:
        bootstrap = sim.encode_tlv_frame(
            sim.TLV_CMD_BOOTSTRAP_RESULT,
            [(sim.TLV_STRUCT_BOOTSTRAP_RESULT, [sim.field_text(0x04, "BOUND")])],
        )
        alarm = sim.encode_event_alarm("TEMP_HIGH", "HIGH", False)
        frames = sim.FrameStreamDecoder().feed(bootstrap + alarm)
        self.assertEqual([frame.command for frame in frames], [0x37, 0x38])

    def test_bad_bootstrap_then_valid_bootstrap_recovers(self) -> None:
        bad = bytearray(
            sim.encode_tlv_frame(
                sim.TLV_CMD_BOOTSTRAP_RESULT,
                [(sim.TLV_STRUCT_BOOTSTRAP_RESULT, [sim.field_text(0x02, "BAD")])],
            )
        )
        bad[9:11] = b"\x00\x04"
        good = sim.encode_tlv_frame(
            sim.TLV_CMD_BOOTSTRAP_RESULT,
            [(sim.TLV_STRUCT_BOOTSTRAP_RESULT, [sim.field_text(0x02, "GOOD")])],
        )
        decoder = sim.FrameStreamDecoder()
        frames = decoder.feed(bytes(bad) + good)
        self.assertEqual(decoder.invalid_frames, 1)
        self.assertEqual(len(frames), 1)
        self.assertEqual(sim.decode_bootstrap_result(frames[0].raw).ble_id, b"GOOD")

    def test_bad_tlv_then_valid_frame_recovers(self) -> None:
        bad = bytearray(sim.encode_tlv_frame(0x35, [(0x61, [sim.field_u8(1, 1)])]))
        bad[9:11] = b"\x00\x02"
        good = sim.encode_wifi_scan(1)
        decoder = sim.FrameStreamDecoder()
        frames = decoder.feed(bytes(bad) + good)
        self.assertEqual(decoder.invalid_frames, 1)
        self.assertEqual(len(frames), 1)
        self.assertEqual(frames[0].command, 0xC0)

    def test_invalid_length_header_resynchronizes(self) -> None:
        bad_header = bytes.fromhex("AA 55 FF FF 35")
        good = sim.encode_ota_frame(0x04)
        decoder = sim.FrameStreamDecoder()
        frames = decoder.feed(bad_header + good)
        self.assertEqual([frame.kind for frame in frames], ["ota"])


if __name__ == "__main__":
    unittest.main()

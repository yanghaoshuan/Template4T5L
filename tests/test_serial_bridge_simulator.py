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

    def test_multi_segment_length_excludes_command(self) -> None:
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
        self.assertEqual(frame[2:4], b"\x00\x25")
        self.assertEqual(len(frame), 42)
        self.assertEqual(len(sim.decode_tlv_frame(frame).segments), 2)

    def test_2048_byte_tlv_boundary(self) -> None:
        field = sim.TlvField(0xFE, b"X" * 2037)
        frame = sim.encode_tlv_frame(0x35, [(0xEE, [field])])
        self.assertEqual(len(frame), 2048)
        self.assertEqual(frame[2:4], b"\x07\xFB")
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
        self.assertEqual(raw[2:4], b"\x00\x6D")
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
        bad = b"\xAA\x55" + len(segments).to_bytes(2, "big")
        bad += bytes([sim.TLV_CMD_BOOTSTRAP_RESULT]) + segments
        with self.assertRaises(sim.ProtocolError):
            cache.receive(bad)
        self.assertEqual(cache.get().ble_id, b"NEW")


class WifiAndOtaTests(unittest.TestCase):
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

    def test_private_ota_status_uses_38_6d(self) -> None:
        frame = sim.encode_tlv_frame(
            sim.TLV_CMD_OTA_STATUS,
            [
                (
                    sim.TLV_STRUCT_OTA_STATUS,
                    [sim.field_u8(0x01, 3), sim.field_u8(0x02, 100)],
                )
            ],
        )
        decoded = sim.decode_tlv_frame(frame)
        self.assertEqual(decoded.command, 0x38)
        self.assertEqual(decoded.segments[0].struct_type, 0x6D)


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

    def test_bootstrap_and_private_ota_status_are_stream_commands(self) -> None:
        bootstrap = sim.encode_tlv_frame(
            sim.TLV_CMD_BOOTSTRAP_RESULT,
            [(sim.TLV_STRUCT_BOOTSTRAP_RESULT, [sim.field_text(0x04, "BOUND")])],
        )
        status = sim.encode_tlv_frame(
            sim.TLV_CMD_OTA_STATUS,
            [(sim.TLV_STRUCT_OTA_STATUS, [sim.field_u8(0x02, 100)])],
        )
        frames = sim.FrameStreamDecoder().feed(bootstrap + status)
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

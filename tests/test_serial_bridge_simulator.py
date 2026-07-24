from __future__ import annotations

import sys
import threading
import time
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT))

from tools import t5l_serial_simulator as simulator  # noqa: E402


class FakeSerial:
    def __init__(self) -> None:
        self.rx = bytearray()
        self.tx = bytearray()
        self.lock = threading.Lock()

    @property
    def in_waiting(self) -> int:
        with self.lock:
            return len(self.rx)

    def feed(self, data: bytes) -> None:
        with self.lock:
            self.rx.extend(data)

    def read(self, size: int = 1) -> bytes:
        deadline = time.monotonic() + 0.05
        while time.monotonic() < deadline:
            with self.lock:
                if self.rx:
                    count = min(size, len(self.rx))
                    data = bytes(self.rx[:count])
                    del self.rx[:count]
                    return data
            time.sleep(0.001)
        return b""

    def write(self, data: bytes) -> int:
        with self.lock:
            self.tx.extend(data)
        return len(data)

    def flush(self) -> None:
        return


class CrcTests(unittest.TestCase):
    def test_modbus_known_vector(self) -> None:
        self.assertEqual(simulator.crc16_modbus(b"123456789"), 0x4B37)


class BleFrameTests(unittest.TestCase):
    def test_single_frame_round_trip(self) -> None:
        payload = simulator.compact_json(
            {"cmd": "get_device_info", "request_id": "REQ-1", "data": {}}
        )
        frames = simulator.encode_ble_chunks(payload, msg_id=0x1234)
        self.assertEqual(len(frames), 1)
        decoded = simulator.decode_ble_frame(frames[0])
        self.assertEqual(decoded.payload, payload)
        self.assertEqual(decoded.metadata["msg_id"], 0x1234)
        self.assertEqual(decoded.metadata["chunk_index"], 0)
        self.assertEqual(decoded.metadata["chunk_total"], 1)
        self.assertEqual(decoded.metadata["flags"], simulator.BLE_LAST_FLAG)

    def test_2000_bytes_uses_nine_chunks(self) -> None:
        payload = simulator.json_payload(
            {
                "cmd": "configure_network",
                "request_id": "BOUNDARY",
                "data": {},
            },
            target_length=2000,
            padding_path="data.padding",
        )
        frames = simulator.encode_ble_chunks(payload, msg_id=1)
        self.assertEqual(len(payload), 2000)
        self.assertEqual(len(frames), 9)
        self.assertTrue(all(len(frame) <= simulator.BLE_ATT_PAYLOAD_MAX for frame in frames))
        reassembled = b"".join(
            simulator.decode_ble_frame(frame).payload for frame in frames
        )
        self.assertEqual(reassembled, payload)

    def test_oversize_rejected(self) -> None:
        with self.assertRaises(ValueError):
            simulator.encode_ble_chunks(b"X" * 2001, msg_id=1)

    def test_bad_crc_mutation_is_detected(self) -> None:
        action = {
            "port": "ble",
            "kind": "json",
            "msg_id": 1,
            "mutations": ["bad_crc"],
            "json": {"cmd": "get_device_info", "request_id": "CRC", "data": {}},
        }
        frames, _ = simulator.materialize_action(action)
        with self.assertRaises(simulator.ProtocolError):
            simulator.decode_ble_frame(frames[0])


class V851FrameTests(unittest.TestCase):
    def test_frame_round_trip_and_big_endian_crc(self) -> None:
        payload = simulator.compact_json(
            {"msg_id": "HB", "msg_type": "device.heartbeat", "data": {}}
        )
        frame = simulator.encode_v851(payload)
        decoded = simulator.decode_v851_frame(frame)
        self.assertEqual(decoded.payload, payload)
        self.assertEqual(frame[:2], b"\xAA\x55")
        self.assertEqual(
            int.from_bytes(frame[-2:], "big"),
            simulator.crc16_modbus(frame[4:-2]),
        )

    def test_2000_byte_frame_fits_protocol_limit(self) -> None:
        payload = simulator.json_payload(
            {"msg_id": "BOUNDARY", "msg_type": "device.heartbeat", "data": {}},
            target_length=2000,
            padding_path="data.padding",
        )
        frame = simulator.encode_v851(payload)
        self.assertEqual(len(frame), 2007)
        self.assertEqual(int.from_bytes(frame[2:4], "big"), 2003)
        self.assertEqual(simulator.decode_v851_frame(frame).payload, payload)

    def test_oversize_rejected(self) -> None:
        with self.assertRaises(ValueError):
            simulator.encode_v851(b"X" * 2001)

class PeerSimulationTests(unittest.TestCase):
    def test_ble_timeout_diagnostic_distinguishes_no_data(self) -> None:
        peer = simulator.BlePeer(FakeSerial())
        self.assertIn("未收到任何字节", peer.receive_diagnostic())
        with peer.rx_lock:
            peer.rx_total = 2
            peer.rx_recent.extend(b"\x00\xFF")
        diagnostic = peer.receive_diagnostic()
        self.assertIn("共收到2字节", diagnostic)
        self.assertIn("00 FF", diagnostic)

    def test_ble_peer_handles_at_initialization_and_reassembles_frames(self) -> None:
        serial_port = FakeSerial()
        peer = simulator.BlePeer(serial_port)
        peer.start()
        try:
            commands = [
                "AT",
                "AT+BLEMODE=9",
                "AT+BLESERUUID=TEST",
                "AT+BLETXUUID=TEST",
                "AT+BLERXUUID=TEST",
                "AT+BLEMTU=240",
                "AT+BLEMAC?",
                "AT+BLEAUTH=123456",
                "AT+BLENAME=MQXQ-TEST",
                "AT+BLEADVDATA=TEST",
                "AT+BLEMODE=0",
                "AT+TRANSENTER",
            ]
            for command in commands:
                serial_port.feed((command + "\r\n").encode("ascii"))
                time.sleep(0.005)
            self.assertTrue(peer.wait_transparent(1.0))

            with serial_port.lock:
                response = bytes(serial_port.tx)
            self.assertIn(
                f"+BLEMAC:{simulator.PB03F_TEST_MAC}\r\nOK\r\n".encode("ascii"),
                response,
            )
            self.assertEqual(peer.at_history, commands)

            payload = simulator.json_payload(
                {"cmd": "network_status", "data": {}},
                target_length=300,
                padding_path="data.padding",
            )
            for frame in simulator.encode_ble_chunks(payload, msg_id=99):
                serial_port.feed(frame)
                time.sleep(0.01)
            received = peer.messages.get(timeout=1.0)
            self.assertEqual(received.payload, payload)
            self.assertEqual(received.metadata["chunk_total"], 2)
        finally:
            peer.close()

    def test_v851_peer_decodes_stream_frame(self) -> None:
        serial_port = FakeSerial()
        peer = simulator.V851Peer(serial_port)
        peer.start()
        try:
            payload = simulator.compact_json(
                {"cmd": "network_status", "data": {"connected": True}}
            )
            serial_port.feed(b"\x00\x01" + simulator.encode_v851(payload))
            received = peer.messages.get(timeout=1.0)
            self.assertEqual(received.payload, payload)
            self.assertEqual(received.json_value["cmd"], "network_status")
        finally:
            peer.close()

    def test_runner_can_start_with_v851_only(self) -> None:
        serial_port = FakeSerial()
        runner = simulator.HardwareRunner(
            None, serial_port, startup_delay_ms=0
        )
        runner.start()
        try:
            self.assertIsNone(runner.ble)
            self.assertIsNotNone(runner.v851)
            self.assertEqual(
                runner._check_expected(
                    {
                        "port": "ble",
                        "kind": "no_frame",
                        "description": "absent side is skipped",
                    }
                ),
                [],
            )
        finally:
            runner.close()


class DatasetTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.vectors = simulator.load_vectors()

    def test_case_ids_are_unique(self) -> None:
        ids = [case["id"] for case in self.vectors["cases"]]
        self.assertEqual(len(ids), len(set(ids)))
        self.assertGreaterEqual(len(ids), 40)

    def test_standalone_mode_selection(self) -> None:
        ble_cases = simulator._select_cases(
            self.vectors, None, "all", mode="ble"
        )
        v851_cases = simulator._select_cases(
            self.vectors, None, "all", mode="v851"
        )
        self.assertEqual(
            [case["id"] for case in ble_cases],
            self.vectors["standalone_cases"]["ble"],
        )
        self.assertEqual(
            [case["id"] for case in v851_cases],
            self.vectors["standalone_cases"]["v851"],
        )
        with self.assertRaises(ValueError):
            simulator._select_cases(
                self.vectors, "BLE-NET-001", None, mode="ble"
            )

    def test_v851_prerequisites_are_added_for_single_case(self) -> None:
        selected = simulator._select_cases(
            self.vectors, "V851-EXPIRED-001", None, mode="v851"
        )
        prepared = simulator._add_prerequisite_cases(
            self.vectors, selected, mode="v851"
        )
        self.assertEqual(
            [case["id"] for case in prepared],
            ["V851-ID-001", "V851-TIME-001", "V851-EXPIRED-001"],
        )

    def test_all_actions_materialize(self) -> None:
        for case in self.vectors["cases"]:
            for action in case.get("actions", []):
                with self.subTest(case=case["id"], kind=action["kind"]):
                    frames, _ = simulator.materialize_action(action)
                    if action["kind"] == "json":
                        self.assertTrue(frames)

    def test_boundary_vectors_have_exact_lengths(self) -> None:
        by_id = {case["id"]: case for case in self.vectors["cases"]}
        ble_action = by_id["BLE-BOUNDARY-2000"]["actions"][0]
        ble_frames, ble_payload = simulator.materialize_action(ble_action)
        self.assertEqual(len(ble_payload or b""), 2000)
        self.assertEqual(len(ble_frames), 9)

        v851_action = by_id["V851-BOUNDARY-2000"]["actions"][0]
        v851_frames, v851_payload = simulator.materialize_action(v851_action)
        self.assertEqual(len(v851_payload or b""), 2000)
        self.assertEqual(len(v851_frames[0]), 2007)

    def test_recovery_vector_contains_noise_bad_frame_and_valid_frame(self) -> None:
        by_id = {case["id"]: case for case in self.vectors["cases"]}
        action = by_id["V851-RECOVERY-001"]["actions"][0]
        wire = simulator.materialize_action(action)[0][0]
        self.assertEqual(wire[:3], b"\x00\xff\x12")

        bad_offset = 3
        bad_len = int.from_bytes(wire[bad_offset + 2 : bad_offset + 4], "big") + 4
        with self.assertRaises(simulator.ProtocolError):
            simulator.decode_v851_frame(
                wire[bad_offset : bad_offset + bad_len]
            )

        good = simulator.decode_v851_frame(wire[bad_offset + bad_len :])
        self.assertEqual(good.json_value["data"]["command_id"], "CMD-RECOVER")

    def test_generated_markdown_is_current(self) -> None:
        expected = simulator.render_markdown(self.vectors)
        actual = simulator.DEFAULT_DOCUMENT.read_text(encoding="utf-8")
        self.assertEqual(actual, expected)


class PortDiscoveryTests(unittest.TestCase):
    def test_resolve_port_is_case_insensitive(self) -> None:
        rows = [
            {
                "device": "COM7",
                "description": "USB Serial",
                "hwid": "TEST",
                "status": "未探测",
            }
        ]
        self.assertEqual(simulator._resolve_port("com7", rows), "COM7")
        with self.assertRaises(RuntimeError):
            simulator._resolve_port("COM8", rows)

    def test_monitor_command_is_exposed(self) -> None:
        args = simulator.build_parser().parse_args(
            ["monitor", "--port", "COM7", "--seconds", "5"]
        )
        self.assertEqual(args.command, "monitor")
        self.assertEqual(args.port, "COM7")
        self.assertEqual(args.baud, 115200)


class FirmwareMappingTests(unittest.TestCase):
    def test_pb03f_has_no_hardcoded_uart5(self) -> None:
        source = (REPO_ROOT / "modules" / "pb03f_ble.c").read_text(encoding="utf-8")
        self.assertNotIn("Uart5", source)
        self.assertIn("PB03F_BLE_UART", source)

    def test_test_configuration_selects_uart2(self) -> None:
        config = (
            REPO_ROOT / "include" / "T5L" / "T5LOSConfig.h"
        ).read_text(encoding="utf-8")
        self.assertIn("#define blePB03F_UART_ID                 2", config)
        self.assertIn("#define sysDGUS_AUTO_UPLOAD_ENABLED      0", config)


if __name__ == "__main__":
    unittest.main()

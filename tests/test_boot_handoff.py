from __future__ import annotations

import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]


def build_file_info(
    *,
    command: int = 0x04,
    total: int = 1,
    index: int = 0,
    all_size: int = 4096,
    file_size: int = 4096,
    name: bytes = b"22.bin",
    status: int = 0,
) -> bytes:
    body = bytearray([command, total, index])
    body += all_size.to_bytes(4, "little")
    body += bytes([2, 1])
    body += (22).to_bytes(2, "little")
    body += file_size.to_bytes(4, "little")
    body += bytes([len(name)]) + name + bytes([status])
    body += (0x12345678).to_bytes(4, "little")
    return b"\xAB\xCD" + len(body).to_bytes(2, "big") + body


def is_upgrade_frame(frame: bytes) -> bool:
    if not 25 <= len(frame) <= 280 or frame[:2] != b"\xAB\xCD":
        return False
    if int.from_bytes(frame[2:4], "big") != len(frame) - 4:
        return False
    if frame[4] != 0x04 or not 1 <= frame[5] <= 20 or frame[6] != 0:
        return False
    status_index = 20 + frame[19]
    crc_index = status_index + 1
    return (
        crc_index + 4 == len(frame)
        and frame[status_index] == 0
        and int.from_bytes(frame[7:11], "little") != 0
        and int.from_bytes(frame[15:19], "little") != 0
    )


class BootHandoffTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.source = (REPO_ROOT / "modules/boot_handoff.c").read_text(encoding="utf-8")
        cls.header = (REPO_ROOT / "modules/boot_handoff.h").read_text(encoding="utf-8")
        cls.uart = (REPO_ROOT / "source/uart.c").read_text(encoding="utf-8")

    def test_complete_valid_first_04_is_accepted(self) -> None:
        frame = build_file_info()
        self.assertTrue(is_upgrade_frame(frame))
        self.assertEqual(int.from_bytes(frame[2:4], "big"), len(frame) - 4)
        self.assertEqual(int.from_bytes(frame[13:15], "little"), 22)

    def test_malformed_or_non_first_frames_are_rejected(self) -> None:
        cases = [
            build_file_info(command=0x05),
            build_file_info(total=0),
            build_file_info(total=21),
            build_file_info(index=1),
            build_file_info(all_size=0),
            build_file_info(file_size=0),
            build_file_info(status=1),
            build_file_info()[:-1],
            build_file_info() + b"\x00",
        ]
        bad_length = bytearray(build_file_info())
        bad_length[3] ^= 1
        cases.append(bytes(bad_length))
        for frame in cases:
            with self.subTest(frame=frame.hex()):
                self.assertFalse(is_upgrade_frame(frame))

    def test_handoff_persists_control_before_soft_reset(self) -> None:
        persist = self.source.index("DgusToFlash(flashMAIN_BLOCK_ORDER")
        reset = self.source.index("write_dgus_vp(BOOT_HANDOFF_RESET_ADDR")
        self.assertLess(persist, reset)
        self.assertIn("{0x5AU, 0xA5U, 0x5AU, 0xA5U}", self.source)
        self.assertIn("{0x55U, 0xAAU, 0x5AU, 0xA5U}", self.source)
        self.assertIn("BOOT_HANDOFF_CONTROL_ADDR         0x0020U", self.source)
        self.assertIn("BOOT_HANDOFF_RESET_ADDR           0x0004U", self.source)

    def test_handoff_never_replies_with_05(self) -> None:
        self.assertNotIn("UartSendData", self.source)
        self.assertNotIn("0x05U", self.source)
        self.assertNotIn("V851ProtocolSendOtaFrame", self.source)

    def test_uart_only_hands_abcd_to_handoff_on_uart4(self) -> None:
        self.assertIn("else if((uart == &Uart4)", self.uart)
        self.assertIn("BOOT_HANDOFF_FILE_INFO_MAX - 4U", self.uart)
        self.assertIn("BootHandoffIsUpgradeFrame", self.uart)
        self.assertNotIn("OtaReceive", self.uart)
        self.assertIn("#define BOOT_HANDOFF_FILE_INFO_MAX       280U", self.header)


if __name__ == "__main__":
    unittest.main()

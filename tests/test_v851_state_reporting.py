from __future__ import annotations

import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]


class V851StateAndWifiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.protocol = (REPO_ROOT / "modules/v851_protocol.c").read_text(encoding="utf-8")
        cls.wifi = (REPO_ROOT / "modules/v851_wifi.c").read_text(encoding="utf-8")
        cls.wifi_h = (REPO_ROOT / "modules/v851_wifi.h").read_text(encoding="utf-8")
        cls.ota = (REPO_ROOT / "modules/ota.c").read_text(encoding="utf-8")
        cls.r11 = (REPO_ROOT / "modules/r11_common.c").read_text(encoding="utf-8")

    def test_initial_snapshot_and_coalesced_local_updates(self) -> None:
        self.assertIn("v851_state_snapshot_pending = 1U", self.protocol)
        self.assertIn("V851ProtocolQueueStateMask(0x00FFU)", self.protocol)
        self.assertIn("V851_STATE_SCAN_INTERVAL_MS               500UL", self.protocol)
        self.assertIn("V851_STATE_REPORT_INTERVAL_MS             5000UL", self.protocol)
        self.assertIn("v851_state_dirty_mask |= V851ControlInfoScanChanged()", self.protocol)
        self.assertIn("v851_state_dirty_mask &= (uint16_t)~sent_mask", self.protocol)

    def test_two_slot_tlv_queue_and_priority_order(self) -> None:
        self.assertIn("#define V851_TLV_TX_DEPTH                         2U", self.protocol)
        ota = self.protocol.index("if(v851_ota_tx_pending != 0U)")
        wifi = self.protocol.index("if(v851_wifi_tx_count != 0U)", ota)
        tlv = self.protocol.index("if(v851_tlv_tx_count != 0U)", wifi)
        self.assertLess(ota, wifi)
        self.assertLess(wifi, tlv)

    def test_ota_status_is_37_6c_tlv(self) -> None:
        self.assertIn("V851_TLV_CMD_OTA_STATUS", self.protocol)
        self.assertIn("V851_TLV_STRUCT_OTA_STATUS", self.protocol)
        self.assertIn("V851_TLV_TAG_OTA_STAGE", self.protocol)
        self.assertIn("V851_TLV_TAG_OTA_PROGRESS", self.protocol)
        self.assertIn("V851_TLV_TAG_OTA_ERROR_CODE", self.protocol)
        self.assertIn("v851_post_ota_success_remaining = (OtaCompleteFlag != 0U) ? 3U : 0U", self.protocol)
        self.assertIn("OtaAcknowledgeComplete();", self.protocol)

    def test_ota_reports_install_verify_reboot_and_failure(self) -> None:
        for stage in (
            "V851_OTA_STAGE_INSTALLING",
            "V851_OTA_STAGE_VERIFYING",
            "V851_OTA_STAGE_REBOOTING",
            "V851_OTA_STAGE_FAILED",
        ):
            self.assertIn(stage, self.ota)

    def test_wifi_uses_r11_addresses_and_commands(self) -> None:
        for value in ("0x0600UL", "0x04B0UL", "0x04C0UL", "0x05B8UL", "0x05BEUL", "0x06D8UL"):
            self.assertIn(value, self.wifi_h)
        for command in ("0xC0U", "0xC1U", "0xC5U"):
            self.assertIn(command, self.wifi_h)
        self.assertIn("V851_WIFI_PAGE_SIZE                      5U", self.wifi)
        self.assertIn("V851_WIFI_KEY_PREVIOUS", self.wifi)
        self.assertIn("V851_WIFI_KEY_NEXT", self.wifi)

    def test_wifi_connect_length_is_exact(self) -> None:
        self.assertIn("declared_length = (uint16_t)(offset - 4U)", self.wifi)
        self.assertIn("V851ProtocolSendWifiFrame(frame, offset)", self.wifi)
        self.assertNotIn("offset + 4U", self.wifi)
        self.assertIn("UartSendData(&Uart_R11, r11_send_buf, now_len2)", self.r11)
        self.assertNotIn("UartSendData(&Uart_R11,r11_send_buf,now_len2 + 4)", self.r11)

    def test_c5_updates_connected_or_disconnected_vp(self) -> None:
        self.assertIn("status_word = (frame[8] == 2U) ? 2U : 1U", self.wifi)
        self.assertIn("write_dgus_vp(V851_WIFI_STATUS_ADDR", self.wifi)


if __name__ == "__main__":
    unittest.main()

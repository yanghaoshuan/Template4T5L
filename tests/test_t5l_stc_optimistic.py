from __future__ import annotations

import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]


class T5lStcOptimisticStateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.header = (REPO_ROOT / "modules/t5l_stc.h").read_text(encoding="utf-8")
        cls.source = (REPO_ROOT / "modules/t5l_stc.c").read_text(encoding="gbk")

    def _function(self, signature: str, next_signature: str) -> str:
        start = self.source.index(signature)
        end = self.source.index(next_signature, start)
        return self.source[start:end]

    def test_mapped_report_addresses_and_sync_api_are_public(self) -> None:
        addresses = {
            "EXHAUST": "0x301A",
            "LIGHT": "0x301F",
            "UVB": "0x3024",
            "ANION": "0x3029",
            "PLASMA": "0x302E",
            "CLIMATE": "0x3033",
            "HUMIDIFIER": "0x3038",
            "INLET_FAN": "0x303D",
            "FILTER": "0x3042",
        }
        for name, address in addresses.items():
            with self.subTest(name=name):
                self.assertIn(f"T5L_STC_REPORT_{name}_VP", self.header)
                self.assertIn(address, self.header)
        self.assertIn("T5lStcMappedControl", self.header)
        self.assertIn("T5lStcSyncMappedControl", self.header)
        self.assertIn("T5L_STC_MAPPED_FIELD_ENABLED", self.header)
        self.assertIn("T5L_STC_MAPPED_FIELD_SECONDARY", self.header)
        self.assertIn("T5L_STC_MAPPED_FIELD_TERTIARY", self.header)

    def test_sync_updates_memory_and_report_without_running_inference(self) -> None:
        sync = self._function("uint8_t T5lStcSyncMappedControl", "void Queue_Init")
        for expression in (
            "G_Device_Ctrl.Exhaust.enable = enabled",
            "G_Device_Ctrl.Exhaust.speed = secondary",
            "G_Device_Ctrl.Light.enable = enabled",
            "G_Device_Ctrl.Light.brightness = secondary",
            "G_Device_Ctrl.UVB.enable = enabled",
            "G_Device_Ctrl.UVB.brightness = secondary",
            "G_Device_Ctrl.UVB.running_time_h = tertiary",
            "G_Device_Ctrl.Anion.enable = enabled",
            "G_Device_Ctrl.Plasma.enable = enabled",
            "G_Device_Ctrl.Heater.enable = enabled",
            "G_Device_Ctrl.Heater.set_temp = secondary",
            "G_Device_Ctrl.Humidifier.enable = enabled",
            "G_Device_Ctrl.Humidifier.interval_time_h = secondary",
            "G_Device_Ctrl.Humidifier.running_time_h = tertiary",
            "G_Device_Ctrl.Inlet_Fan.enable = enabled",
            "G_Device_Ctrl.Inlet_Fan.speed = secondary",
        ):
            with self.subTest(expression=expression):
                self.assertIn(expression, sync)
        self.assertNotIn(".running =", sync)
        self.assertIn("write_dgus_vp(report_vp", sync)

    def test_local_controls_sync_before_uart2_command(self) -> None:
        functions = (
            ("void Exhaust_On()", "void Exhaust_Off", "T5L_STC_MAPPED_EXHAUST"),
            ("void Exhaust_Off", "void Humidifier_On", "T5L_STC_MAPPED_EXHAUST"),
            ("void Humidifier_On()", "void Humidifier_Off", "T5L_STC_MAPPED_HUMIDIFIER"),
            ("void Humidifier_Off", "void UVB_On", "T5L_STC_MAPPED_HUMIDIFIER"),
            ("void UVB_On", "void UVB_Off", "T5L_STC_MAPPED_UVB"),
            ("void UVB_Off", "void UVC_On", "T5L_STC_MAPPED_UVB"),
            ("void Heater_On", "void Heater_Off", "T5L_STC_MAPPED_CLIMATE"),
            ("void Heater_Off", "void InWind_On", "T5L_STC_MAPPED_CLIMATE"),
            ("void InWind_On", "void InWind_Off", "T5L_STC_MAPPED_INLET_FAN"),
            ("void InWind_Off", "void Light_On", "T5L_STC_MAPPED_INLET_FAN"),
            ("void Light_On", "void Light_Off", "T5L_STC_MAPPED_LIGHT"),
            ("void Light_Off", "uint8_t check_passwd_format", "T5L_STC_MAPPED_LIGHT"),
        )
        for signature, next_signature, control in functions:
            with self.subTest(signature=signature):
                body = self._function(signature, next_signature)
                self.assertIn(control, body)
                self.assertLess(body.index("T5lStcSyncMappedControl"), body.index("Send_Cmd_Ctrl"))

        key_scan = self._function("void key_scanf", "void SysCfg_Init")
        for control, command in (
            ("T5L_STC_MAPPED_ANION", "ANION_CMDWORD"),
            ("T5L_STC_MAPPED_PLASMA", "PLASMA_CMDWORD"),
        ):
            sync = key_scan.index(control)
            send = key_scan.index(f"Send_Cmd_Ctrl({command}", sync)
            self.assertLess(sync, send)

    def test_ack_does_not_overwrite_mapped_fields(self) -> None:
        ack = self._function("void T5l_Stc_UartRxProcess", "void Stc_FlashBackup")
        for assignment in (
            "G_Device_Ctrl.Exhaust.enable =",
            "G_Device_Ctrl.Exhaust.speed =",
            "G_Device_Ctrl.Inlet_Fan.enable =",
            "G_Device_Ctrl.Inlet_Fan.speed =",
            "G_Device_Ctrl.Light.enable =",
            "G_Device_Ctrl.Light.brightness =",
            "G_Device_Ctrl.UVB.enable =",
            "G_Device_Ctrl.Anion.enable =",
            "G_Device_Ctrl.Plasma.enable =",
            "G_Device_Ctrl.Heater.enable =",
            "G_Device_Ctrl.Heater.set_temp =",
            "G_Device_Ctrl.Humidifier.enable =",
        ):
            with self.subTest(assignment=assignment):
                self.assertNotIn(assignment, ack)
        self.assertIn("run_state = T5lStcAcknowledgedRunState(addr)", ack)
        self.assertIn("G_Device_Ctrl.Exhaust.running =", ack)
        self.assertIn("G_Device_Ctrl.Heater.running =", ack)
        self.assertIn("G_Device_Ctrl.Humidifier.running =", ack)

    def test_timeout_does_not_roll_back_optimistic_state(self) -> None:
        queue = self._function("void Queue_Sta_Poll", "uint32_t Get_Interval_Run_Sec")
        failure = queue[queue.index("case STATE_WAIT_ACK_FAILURE"):]
        self.assertNotIn("T5lStcSyncMappedControl", failure)
        self.assertNotIn("write_dgus_vp(T5L_STC_REPORT_", failure)


if __name__ == "__main__":
    unittest.main()

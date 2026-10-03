"""Checks for the PC-side port-opening probe; no connected hardware needed."""
import importlib.util
from pathlib import Path
import unittest
from unittest.mock import Mock

SPEC = importlib.util.spec_from_file_location(
    "serial_probe", Path(__file__).resolve().parents[1] / "tools" / "serial_probe.py"
)
probe = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(probe)


class ProbeTests(unittest.TestCase):
    def test_control_lines_are_off_before_port_opens_and_nothing_is_sent(self):
        module = Mock()
        port = module.Serial.return_value

        def opened():
            self.assertEqual(port.port, "COM12")
            self.assertIs(port.dtr, False)
            self.assertIs(port.rts, False)

        port.open.side_effect = opened
        result = probe.open_passive_port(module, "COM12", 38400)
        self.assertIs(result, port)
        module.Serial.assert_called_once_with(
            port=None, baudrate=38400, bytesize=8, parity="N", stopbits=1,
            timeout=0.2, xonxoff=False, rtscts=False, dsrdtr=False,
        )
        port.open.assert_called_once_with()
        port.write.assert_not_called()

    def test_observation_reads_binary_without_transmitting(self):
        port = Mock()
        port.read.side_effect = [b"boot\r\n", b"\x00\xff", b""]
        output = []
        ticks = iter([0.0, 0.0, 0.2, 0.3, 0.5, 0.6, 1.1])
        probe.observe(port, 1.0, output.append, lambda: next(ticks))
        self.assertEqual(len(output), 2)
        self.assertIn("62 6F 6F 74 0D 0A", output[0])
        self.assertIn("00 FF", output[1])
        port.write.assert_not_called()


if __name__ == "__main__":
    unittest.main()

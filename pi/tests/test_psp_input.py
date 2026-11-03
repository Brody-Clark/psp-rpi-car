import struct
import unittest

from pi.psp_input import (
    PSPButton,
    PSPInput,
)


class PSPInputTests(unittest.TestCase):
    def test_parses_wire_format_without_interpreting_controls(self):
        buttons = PSPButton.UP | PSPButton.RIGHT | PSPButton.R | PSPButton.TRIANGLE
        packet = struct.pack("<IBB", buttons, 121, 132)

        command = PSPInput.from_packet(packet)

        self.assertEqual(command.buttons, buttons)
        self.assertEqual((command.analog_x, command.analog_y), (121, 132))

    def test_rejects_packet_with_invalid_size(self):
        for packet in (b"", b"\0" * 5, b"\0" * 7):
            with self.subTest(packet_size=len(packet)):
                with self.assertRaises(ValueError):
                    PSPInput.from_packet(packet)


if __name__ == "__main__":
    unittest.main()

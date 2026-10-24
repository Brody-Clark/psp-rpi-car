import struct
import unittest

from pi.psp_input import (
    ControlDirection,
    PSPButton,
    PSPInput,
    VehicleControls,
)


class PSPInputTests(unittest.TestCase):
    def test_parses_wire_format_and_maps_all_requested_controls(self):
        buttons = (
            PSPButton.UP
            | PSPButton.RIGHT
            | PSPButton.R
            | PSPButton.TRIANGLE
        )
        packet = struct.pack("<IBB", buttons, 121, 132)

        command = PSPInput.from_packet(packet)

        self.assertEqual(command.buttons, buttons)
        self.assertEqual((command.analog_x, command.analog_y), (121, 132))
        self.assertEqual(
            command.controls,
            VehicleControls(
                camera_horizontal=ControlDirection.POSITIVE,
                camera_vertical=ControlDirection.POSITIVE,
                steering=ControlDirection.POSITIVE,
                throttle=ControlDirection.POSITIVE,
            ),
        )

    def test_maps_opposite_buttons_to_negative_directions(self):
        packet = struct.pack(
            "<IBB",
            PSPButton.LEFT | PSPButton.DOWN | PSPButton.L | PSPButton.X,
            0,
            0,
        )

        controls = PSPInput.from_packet(packet).controls

        self.assertEqual(controls.camera_horizontal, ControlDirection.NEGATIVE)
        self.assertEqual(controls.camera_vertical, ControlDirection.NEGATIVE)
        self.assertEqual(controls.steering, ControlDirection.NEGATIVE)
        self.assertEqual(controls.throttle, ControlDirection.NEGATIVE)

    def test_opposing_buttons_cancel_to_neutral(self):
        packet = struct.pack(
            "<IBB",
            PSPButton.LEFT
            | PSPButton.RIGHT
            | PSPButton.UP
            | PSPButton.DOWN
            | PSPButton.L
            | PSPButton.R
            | PSPButton.TRIANGLE
            | PSPButton.X,
            0,
            0,
        )

        controls = PSPInput.from_packet(packet).controls

        self.assertEqual(controls.camera_horizontal, ControlDirection.NEUTRAL)
        self.assertEqual(controls.camera_vertical, ControlDirection.NEUTRAL)
        self.assertEqual(controls.steering, ControlDirection.NEUTRAL)
        self.assertEqual(controls.throttle, ControlDirection.NEUTRAL)

    def test_rejects_packet_with_invalid_size(self):
        for packet in (b"", b"\0" * 5, b"\0" * 7):
            with self.subTest(packet_size=len(packet)):
                with self.assertRaises(ValueError):
                    PSPInput.from_packet(packet)


if __name__ == "__main__":
    unittest.main()

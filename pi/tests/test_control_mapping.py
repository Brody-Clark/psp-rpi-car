import struct
import unittest

from pi.control_mapping import ControlDirection, DEFAULT_CONTROL_PROFILE
from pi.psp_input import PSPButton, PSPInput


class ControlMappingTests(unittest.TestCase):
    def make_input(self, buttons):
        return PSPInput.from_packet(struct.pack("<IBB", buttons, 128, 128))

    def test_default_profile_maps_standard_controls_and_square_action(self):
        controls = DEFAULT_CONTROL_PROFILE.map_input(
            self.make_input(
                PSPButton.UP
                | PSPButton.RIGHT
                | PSPButton.R
                | PSPButton.TRIANGLE
                | PSPButton.CIRCLE
                | PSPButton.SQUARE
            )
        )

        self.assertIs(controls.camera_horizontal, ControlDirection.POSITIVE)
        self.assertIs(controls.camera_vertical, ControlDirection.POSITIVE)
        self.assertIs(controls.steering, ControlDirection.POSITIVE)
        self.assertIs(controls.throttle, ControlDirection.POSITIVE)
        self.assertTrue(controls.brake)
        self.assertTrue(controls.center_steering)

    def test_opposing_buttons_cancel_directional_controls(self):
        controls = DEFAULT_CONTROL_PROFILE.map_input(
            self.make_input(
                PSPButton.LEFT
                | PSPButton.RIGHT
                | PSPButton.UP
                | PSPButton.DOWN
                | PSPButton.L
                | PSPButton.R
                | PSPButton.TRIANGLE
                | PSPButton.X
            )
        )

        self.assertIs(controls.camera_horizontal, ControlDirection.NEUTRAL)
        self.assertIs(controls.camera_vertical, ControlDirection.NEUTRAL)
        self.assertIs(controls.steering, ControlDirection.NEUTRAL)
        self.assertIs(controls.throttle, ControlDirection.NEUTRAL)


if __name__ == "__main__":
    unittest.main()

import unittest

from pi.psp_input import PSPButton
from pi.tools.mock_client import buttons_from_keys


class MockClientMappingTests(unittest.TestCase):
    def test_wasd_controls_camera(self):
        self.assertEqual(
            buttons_from_keys({"w", "a", "s", "d"}),
            PSPButton.UP | PSPButton.DOWN | PSPButton.LEFT | PSPButton.RIGHT,
        )

    def test_arrows_control_forward_reverse_and_space_brakes(self):
        self.assertEqual(buttons_from_keys({"up"}), PSPButton.TRIANGLE)
        self.assertEqual(buttons_from_keys({"down"}), PSPButton.X)
        self.assertEqual(buttons_from_keys({"space", "up"}), PSPButton.CIRCLE | PSPButton.TRIANGLE)

    def test_q_and_e_control_left_and_right_shoulders(self):
        self.assertEqual(buttons_from_keys({"q", "e"}), PSPButton.L | PSPButton.R)


if __name__ == "__main__":
    unittest.main()

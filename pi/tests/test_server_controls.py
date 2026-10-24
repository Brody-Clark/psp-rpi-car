import logging
import socket
import struct
import sys
import tempfile
import threading
import unittest
from dataclasses import replace
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

from pi.psp_input import PSPButton, PSPInput
from pi.robot_controller import RobotController
from pi.robot_state import DriveStage
from pi.server import (
    handle_input_command,
    load_config,
    receive_inputs,
    stream_video,
)


class FakeShield:
    CMD_DIR1 = 6
    CMD_DIR2 = 7
    CMD_PWM1 = 4
    CMD_PWM2 = 5
    CMD_SERVO1 = 0
    CMD_SERVO2 = 1
    CMD_SERVO3 = 2

    def __init__(self):
        self.writes = []

    def writeReg(self, command, value):
        self.writes.append((command, value))


class FakeRobotController:
    def __init__(self):
        self.speed = 0
        self.is_forward = True
        self.is_moving = False
        self.steering_angle = 90
        self.camera_pan = 90
        self.camera_tilt = 90
        self.calls = []

    def set_speed(self, speed):
        self.speed = max(0, min(100, speed))
        self.calls.append(("set_speed", self.speed))

    def adjust_speed(self, delta):
        self.speed = max(0, min(100, self.speed + delta))
        self.calls.append(("adjust_speed", delta))

    def set_direction(self, is_forward):
        self.is_forward = is_forward
        self.calls.append(("set_direction", is_forward))

    def start(self):
        self.is_moving = True
        self.calls.append(("start",))

    def stop(self):
        self.is_moving = False
        self.calls.append(("stop",))

    def adjust_steering(self, delta):
        self.steering_angle = max(0, min(180, self.steering_angle + delta))
        self.calls.append(("adjust_steering", delta))

    def adjust_camera_pan(self, delta):
        self.camera_pan = max(0, min(180, self.camera_pan + delta))
        self.calls.append(("adjust_camera_pan", delta))

    def adjust_camera_tilt(self, delta):
        self.camera_tilt = max(0, min(180, self.camera_tilt + delta))
        self.calls.append(("adjust_camera_tilt", delta))


class ServerControlTests(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp_dir.cleanup)
        config_path = Path(self.temp_dir.name) / "server.ini"
        config_path.write_text(
            "[server]\nhost=127.0.0.1\nport=12345\n\n"
            "[video]\nenabled=false\n\n"
            "[controls]\nsteering_step=7\ncamera_pan_step=3\n"
            "camera_tilt_step=4\ndrive_speed=10\n"
            "held_control_repeat_ms=100\nacceleration_duration_ms=300\n\n"
            "[logging]\nenabled=false\n"
        )
        self.config = load_config(config_path)
        self.controller = FakeRobotController()
        self.controller.speed = self.config.drive_speed
        self.logger = logging.getLogger("server-control-test")
        self.held_control_timers = {}

    def make_input(self, buttons):
        return PSPInput.from_packet(struct.pack("<IBB", buttons, 128, 128))

    def apply(self, buttons, previous=PSPButton.NONE, now=0.0):
        return handle_input_command(
            self.make_input(buttons),
            previous,
            self.controller,
            self.config,
            self.logger,
            self.held_control_timers,
            now,
        )

    def test_dpad_and_shoulder_press_adjust_by_configured_steps(self):
        self.apply(PSPButton.LEFT | PSPButton.UP | PSPButton.L)

        self.assertEqual(self.controller.camera_pan, 87)
        self.assertEqual(self.controller.camera_tilt, 94)
        self.assertEqual(self.controller.steering_angle, 83)
        self.assertEqual(
            self.controller.calls,
            [
                ("adjust_camera_pan", -3),
                ("adjust_camera_tilt", 4),
                ("adjust_steering", -7),
            ],
        )

    def test_held_controls_repeat_on_interval_and_release_resets_timer(self):
        buttons = PSPButton.LEFT | PSPButton.L
        self.apply(buttons, now=0.0)
        self.apply(buttons, now=0.05)
        self.assertEqual(self.controller.camera_pan, 87)
        self.assertEqual(self.controller.steering_angle, 83)

        previous = self.apply(buttons, previous=buttons, now=0.1)
        self.assertEqual(self.controller.camera_pan, 84)
        self.assertEqual(self.controller.steering_angle, 76)

        self.apply(PSPButton.NONE, previous=previous, now=0.11)
        self.apply(buttons, now=0.12)
        self.assertEqual(self.controller.camera_pan, 81)
        self.assertEqual(self.controller.steering_angle, 69)

    def test_held_control_direction_change_steps_immediately(self):
        self.apply(PSPButton.LEFT, now=0.0)
        self.apply(PSPButton.RIGHT, now=0.01)

        self.assertEqual(self.controller.camera_pan, 90)
        self.assertEqual(
            self.controller.calls,
            [("adjust_camera_pan", -3), ("adjust_camera_pan", 3)],
        )

    def test_holding_forward_uses_set_speed_and_release_preserves_it(self):
        previous = self.apply(PSPButton.TRIANGLE)
        self.assertEqual(self.controller.speed, 100)
        self.assertTrue(self.controller.is_moving)
        self.assertTrue(self.controller.is_forward)

        previous = self.apply(PSPButton.TRIANGLE, previous)
        self.assertEqual(self.controller.speed, 100)
        self.assertEqual(self.controller.calls.count(("start",)), 1)

        previous = self.apply(PSPButton.NONE, previous)
        self.assertEqual(self.controller.speed, 100)
        self.assertFalse(self.controller.is_moving)

        self.apply(PSPButton.TRIANGLE, previous)
        self.assertTrue(self.controller.is_moving)
        self.assertEqual(self.controller.speed, 100)

    def test_reverse_uses_set_speed_and_switches_direction_while_held(self):
        previous = self.apply(PSPButton.TRIANGLE)
        self.apply(PSPButton.X, previous)

        self.assertFalse(self.controller.is_forward)
        self.assertEqual(self.controller.speed, 100)
        self.assertTrue(self.controller.is_moving)

    def test_release_and_circle_stop_without_changing_speed_or_direction(self):
        previous = self.apply(PSPButton.TRIANGLE)
        self.apply(PSPButton.CIRCLE, previous)

        self.assertEqual(self.controller.speed, 100)
        self.assertTrue(self.controller.is_forward)
        self.assertFalse(self.controller.is_moving)
        self.assertIn(("stop",), self.controller.calls)

    def test_held_circle_blocks_throttle(self):
        previous = self.apply(PSPButton.CIRCLE)
        self.apply(PSPButton.CIRCLE | PSPButton.TRIANGLE, previous)

        self.assertFalse(self.controller.is_moving)
        self.assertNotIn(("start",), self.controller.calls)

    def test_configured_drive_speed_is_applied_when_server_starts(self):
        shield = FakeShield()
        controller = RobotController(shield=shield)

        controller.set_speed(self.config.drive_speed)

        self.assertEqual(controller.state.speed, 100)
        self.assertFalse(controller.state.is_moving)
        self.assertEqual(
            shield.writes[-2:],
            [
                (FakeShield.CMD_PWM1, 0),
                (FakeShield.CMD_PWM2, 0),
            ],
        )

    def test_drive_speed_level_maps_endpoints_and_rejects_out_of_range(self):
        config_path = Path(self.temp_dir.name) / "speed.ini"
        config_path.write_text("[controls]\ndrive_speed=1\n")
        self.assertEqual(load_config(config_path).drive_speed, 35)
        config_path.write_text("[controls]\ndrive_speed=10\n")
        self.assertEqual(load_config(config_path).drive_speed, 100)
        config_path.write_text("[controls]\ndrive_speed=11\n")
        with self.assertRaisesRegex(ValueError, "drive_speed must be between 1 and 10"):
            load_config(config_path)

    def test_socket_input_routes_commands_and_disconnect_stops_vehicle(self):
        server_socket, client_socket = socket.socketpair()
        stop_event = threading.Event()
        receiver = threading.Thread(
            target=receive_inputs,
            args=(
                server_socket,
                stop_event,
                self.controller,
                self.config,
                self.logger,
            ),
        )
        receiver.start()
        try:
            client_socket.sendall(struct.pack("<IBB", PSPButton.TRIANGLE, 128, 128))
            client_socket.sendall(struct.pack("<IBB", PSPButton.NONE, 128, 128))
            client_socket.shutdown(socket.SHUT_WR)
            receiver.join(timeout=2)

            self.assertFalse(receiver.is_alive())
            self.assertEqual(self.controller.speed, self.config.drive_speed)
            self.assertFalse(self.controller.is_moving)
            self.assertEqual(
                self.controller.calls[:2],
                [
                    ("set_direction", True),
                    ("start",),
                ],
            )
            self.assertEqual(self.controller.calls[-1], ("stop",))
        finally:
            client_socket.close()
            server_socket.close()

    def test_config_rejects_nonpositive_steps(self):
        config_path = Path(self.temp_dir.name) / "invalid.ini"
        config_path.write_text(
            "[server]\n" "[controls]\nsteering_step=0\n" "[logging]\nenabled=false\n"
        )

        with self.assertRaisesRegex(ValueError, "step values must be positive"):
            load_config(config_path)

    def test_robot_controller_applies_persistent_state_to_shield(self):
        shield = FakeShield()
        controller = RobotController(shield=shield)

        controller.set_direction(False)
        controller.set_speed(10)
        controller.adjust_steering(-5)
        controller.start()

        self.assertEqual(controller.state.speed, 10)
        self.assertFalse(controller.state.is_forward)
        self.assertTrue(controller.state.is_moving)
        self.assertEqual(controller.state.steering_angle, 85)
        self.assertEqual(
            shield.writes,
            [
                (FakeShield.CMD_DIR1, 0),
                (FakeShield.CMD_DIR2, 0),
                (FakeShield.CMD_PWM1, 0),
                (FakeShield.CMD_PWM2, 0),
                (FakeShield.CMD_DIR1, 0),
                (FakeShield.CMD_DIR2, 0),
                (FakeShield.CMD_PWM1, 0),
                (FakeShield.CMD_PWM2, 0),
                (FakeShield.CMD_SERVO1, 1444),
                (FakeShield.CMD_DIR1, 0),
                (FakeShield.CMD_DIR2, 0),
                (FakeShield.CMD_PWM1, 100),
                (FakeShield.CMD_PWM2, 100),
            ],
        )

    def test_stop_zeros_pwm_but_preserves_configured_speed(self):
        shield = FakeShield()
        controller = RobotController(shield=shield)
        controller.set_speed(35)
        controller.set_direction(True)
        controller.start()
        controller.stop()

        self.assertEqual(controller.state.speed, 35)
        self.assertFalse(controller.state.is_moving)
        self.assertEqual(
            shield.writes[-2:],
            [
                (FakeShield.CMD_PWM1, 0),
                (FakeShield.CMD_PWM2, 0),
            ],
        )

    def test_controller_accelerates_through_stages_and_stop_cancels_ramp(self):
        shield = FakeShield()
        controller = RobotController(shield=shield, acceleration_duration_ms=90)
        controller.set_speed(100)
        controller.start()
        drive_thread = controller._drive_thread
        self.assertIsNotNone(drive_thread)
        drive_thread.join(timeout=1)

        self.assertFalse(drive_thread.is_alive())
        pwm_outputs = [
            value for command, value in shield.writes if command == FakeShield.CMD_PWM1
        ]
        self.assertEqual(pwm_outputs, [0, 350, 570, 780, 1000])
        self.assertEqual(controller.state.drive_stage, DriveStage.CRUISING)
        self.assertEqual(controller.state.applied_speed, 100)
        controller.close()

        cancel_shield = FakeShield()
        cancel_controller = RobotController(
            shield=cancel_shield, acceleration_duration_ms=300
        )
        cancel_controller.set_speed(100)
        cancel_controller.start()
        cancelled_thread = cancel_controller._drive_thread
        cancel_controller.stop()
        cancelled_thread.join(timeout=1)

        self.assertFalse(cancelled_thread.is_alive())
        self.assertEqual(
            [
                value
                for command, value in cancel_shield.writes
                if command == FakeShield.CMD_PWM1
            ],
            [0, 350, 0],
        )
        self.assertEqual(cancel_controller.state.speed, 100)
        cancel_controller.close()

    def test_video_stream_sends_length_prefixed_opencv_jpeg_and_releases_camera(self):
        stop_event = threading.Event()

        class FakeCapture:
            def __init__(self):
                self.released = False

            def isOpened(self):
                return True

            def set(self, _property, _value):
                return True

            def read(self):
                return True, b"frame"

            def release(self):
                self.released = True

        capture = FakeCapture()
        fake_cv2 = SimpleNamespace(
            VideoCapture=lambda _device, _backend: capture,
            CAP_V4L2=7,
            CAP_PROP_FRAME_WIDTH=3,
            CAP_PROP_FOURCC=8,
            CAP_PROP_FRAME_HEIGHT=4,
            CAP_PROP_FPS=5,
            IMWRITE_JPEG_QUALITY=6,
            INTER_AREA=10,
            VideoWriter_fourcc=lambda *_args: 9,
            resize=lambda frame, _size, interpolation: frame,
            imencode=lambda *_args: (True, SimpleNamespace(tobytes=lambda: b"jpeg")),
        )

        class FakeConnection:
            def __init__(self):
                self.packets = []

            def sendall(self, packet):
                self.packets.append(packet)
                stop_event.set()

        connection = FakeConnection()
        with patch.dict(sys.modules, {"cv2": fake_cv2}):
            stream_video(
                connection,
                stop_event,
                replace(self.config, video_enabled=True),
                self.logger,
            )

        self.assertEqual(connection.packets, [struct.pack("!I", 4) + b"jpeg"])
        self.assertTrue(capture.released)


if __name__ == "__main__":
    unittest.main()

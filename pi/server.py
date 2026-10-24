#!/usr/bin/env python3
"""Accept PSP clients and route controller and video data."""

import argparse
import configparser
import logging
import socket
import struct
import threading
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

if __package__:
    from .psp_input import ControlDirection, PSPButton, PSPInput
    from .robot_controller import RobotController
else:
    from psp_input import ControlDirection, PSPButton, PSPInput
    from robot_controller import RobotController


@dataclass(frozen=True)
class ServerConfig:
    host: str
    port: int
    backlog: int
    video_enabled: bool
    camera_device: int | str
    video_width: int
    video_height: int
    video_fps: float
    steering_step: int
    camera_pan_step: int
    camera_tilt_step: int
    held_control_repeat_ms: int
    drive_speed: int
    acceleration_duration_ms: int
    logging_enabled: bool
    logging_level: str
    logging_file: Optional[Path]


def load_config(config_path: Path) -> ServerConfig:
    """Load and validate server settings from an INI file."""
    parser = configparser.ConfigParser(interpolation=None)
    if not parser.read(config_path):
        raise FileNotFoundError(f"Could not read config file: {config_path}")

    try:
        host = parser.get("server", "host", fallback="0.0.0.0")
        port = parser.getint("server", "port", fallback=12345)
        backlog = parser.getint("server", "backlog", fallback=1)
        video_enabled = parser.getboolean("video", "enabled", fallback=True)
        camera_device_setting = parser.get("video", "device", fallback="0").strip()
        camera_device: int | str = (
            int(camera_device_setting)
            if camera_device_setting.isdecimal()
            else camera_device_setting
        )
        video_width = parser.getint("video", "width", fallback=480)
        video_height = parser.getint("video", "height", fallback=272)
        video_fps = parser.getfloat("video", "fps", fallback=15)
        steering_step = parser.getint("controls", "steering_step", fallback=5)
        camera_pan_step = parser.getint("controls", "camera_pan_step", fallback=5)
        camera_tilt_step = parser.getint("controls", "camera_tilt_step", fallback=5)
        held_control_repeat_ms = parser.getint(
            "controls", "held_control_repeat_ms", fallback=100
        )
        drive_speed_level = parser.getint("controls", "drive_speed", fallback=1)
        acceleration_duration_ms = parser.getint(
            "controls", "acceleration_duration_ms", fallback=300
        )
        logging_enabled = parser.getboolean("logging", "enabled", fallback=True)
        logging_level = parser.get("logging", "level", fallback="INFO").upper()
        logging_file_setting = parser.get(
            "logging", "file", fallback="logs/server.log"
        ).strip()
    except configparser.Error as error:
        raise ValueError(f"Invalid setting in {config_path}: {error}") from error

    if not 1 <= port <= 65535:
        raise ValueError("server.port must be between 1 and 65535")
    if backlog < 1:
        raise ValueError("server.backlog must be at least 1")
    if video_width < 1 or video_height < 1 or video_fps <= 0:
        raise ValueError("Video width, height, and fps must be positive")
    if min(steering_step, camera_pan_step, camera_tilt_step) < 1:
        raise ValueError("All control step values must be positive")
    if held_control_repeat_ms < 1:
        raise ValueError("controls.held_control_repeat_ms must be positive")
    if acceleration_duration_ms < 1:
        raise ValueError("controls.acceleration_duration_ms must be positive")
    if not 1 <= drive_speed_level <= 10:
        raise ValueError("controls.drive_speed must be between 1 and 10")
    if not isinstance(logging.getLevelName(logging_level), int):
        raise ValueError(f"Unknown logging level: {logging_level}")

    logging_file = None
    if logging_file_setting:
        logging_file = Path(logging_file_setting)
        if not logging_file.is_absolute():
            logging_file = config_path.parent / logging_file

    return ServerConfig(
        host=host,
        port=port,
        backlog=backlog,
        video_enabled=video_enabled,
        camera_device=camera_device,
        video_width=video_width,
        video_height=video_height,
        video_fps=video_fps,
        steering_step=steering_step,
        camera_pan_step=camera_pan_step,
        camera_tilt_step=camera_tilt_step,
        held_control_repeat_ms=held_control_repeat_ms,
        drive_speed=round(35 + (drive_speed_level - 1) * 65 / 9),  # map 1-10 to 35-100
        acceleration_duration_ms=acceleration_duration_ms,
        logging_enabled=logging_enabled,
        logging_level=logging_level,
        logging_file=logging_file,
    )


def configure_logging(config: ServerConfig) -> logging.Logger:
    """Configure console and optional file logging."""
    logger = logging.getLogger("psp_rpi_car")
    logger.handlers.clear()
    logger.propagate = False

    if not config.logging_enabled:
        logger.addHandler(logging.NullHandler())
        return logger

    logger.setLevel(config.logging_level)
    formatter = logging.Formatter(
        "%(asctime)s %(levelname)s %(threadName)s: %(message)s"
    )
    console_handler = logging.StreamHandler()
    console_handler.setFormatter(formatter)
    logger.addHandler(console_handler)

    if config.logging_file is not None:
        config.logging_file.parent.mkdir(parents=True, exist_ok=True)
        file_handler = logging.FileHandler(config.logging_file)
        file_handler.setFormatter(formatter)
        logger.addHandler(file_handler)

    return logger


def handle_input_command(
    command: PSPInput,
    previous_buttons: PSPButton,
    controller: RobotController,
    config: ServerConfig,
    logger: logging.Logger,
    held_control_timers: Optional[dict[str, tuple[ControlDirection, float]]] = None,
    current_time: Optional[float] = None,
) -> PSPButton:
    """Apply button state and repeat camera/steering steps while controls are held."""
    if held_control_timers is None:
        held_control_timers = {}
    if current_time is None:
        current_time = time.monotonic()

    controls = command.controls
    previous_direction = _drive_direction(previous_buttons)
    current_direction = _drive_direction(command.buttons)

    repeat_interval = config.held_control_repeat_ms / 1000
    _repeat_control(
        "camera_pan",
        controls.camera_horizontal,
        config.camera_pan_step,
        controller.adjust_camera_pan,
        held_control_timers,
        current_time,
        repeat_interval,
    )
    _repeat_control(
        "camera_tilt",
        controls.camera_vertical,
        config.camera_tilt_step,
        controller.adjust_camera_tilt,
        held_control_timers,
        current_time,
        repeat_interval,
    )
    _repeat_control(
        "steering",
        controls.steering,
        config.steering_step,
        controller.adjust_steering,
        held_control_timers,
        current_time,
        repeat_interval,
    )

    if current_direction != previous_direction:
        if current_direction is None:
            controller.stop()
        else:
            controller.set_direction(current_direction)
            controller.start()

    logger.debug(
        "Input buttons=0x%x camera_horizontal=%s camera_vertical=%s "
        "steering=%s throttle=%s (analog_x=%d analog_y=%d)",
        int(command.buttons),
        controls.camera_horizontal.name.lower(),
        controls.camera_vertical.name.lower(),
        controls.steering.name.lower(),
        controls.throttle.name.lower(),
        command.analog_x,
        command.analog_y,
    )
    return command.buttons


def _repeat_control(
    name: str,
    direction: ControlDirection,
    step: int,
    apply_step,
    timers: dict[str, tuple[ControlDirection, float]],
    current_time: float,
    repeat_interval_seconds: float,
) -> None:
    if direction is ControlDirection.NEUTRAL:
        timers.pop(name, None)
        return

    next_repeat = timers.get(name)
    if (
        next_repeat is None
        or next_repeat[0] is not direction
        or current_time >= next_repeat[1]
    ):
        apply_step(int(direction) * step)
        timers[name] = (direction, current_time + repeat_interval_seconds)


def _drive_direction(buttons: PSPButton) -> Optional[bool]:
    """Return forward/reverse motion, or None when throttle is released/braked."""
    if buttons & PSPButton.CIRCLE:
        return None

    triangle_pressed = bool(buttons & PSPButton.TRIANGLE)
    x_pressed = bool(buttons & PSPButton.X)
    if triangle_pressed == x_pressed:
        return None
    return triangle_pressed


def receive_inputs(
    conn: socket.socket,
    stop_event: threading.Event,
    controller: RobotController,
    config: ServerConfig,
    logger: logging.Logger,
) -> None:
    """Receive six-byte PSP commands until the client disconnects."""
    pending_data = bytearray()
    previous_buttons = PSPButton.NONE
    held_control_timers: dict[str, tuple[ControlDirection, float]] = {}
    try:
        while not stop_event.is_set():
            data = conn.recv(4096)
            if not data:
                logger.info("PSP disconnected")
                break

            pending_data.extend(data)
            while len(pending_data) >= 6:
                command = PSPInput.from_packet(bytes(pending_data[:6]))
                del pending_data[:6]
                previous_buttons = handle_input_command(
                    command,
                    previous_buttons,
                    controller,
                    config,
                    logger,
                    held_control_timers,
                )
    except OSError as error:
        if not stop_event.is_set():
            logger.warning("Controller input connection failed: %s", error)
    except Exception:
        logger.exception("Could not apply controller input")
    finally:
        if not stop_event.is_set():
            try:
                controller.stop()
                logger.info("Stopped vehicle after PSP disconnect")
            except Exception:
                logger.exception("Could not stop vehicle after PSP disconnect")
        stop_event.set()
        try:
            conn.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        logger.debug("Input worker ended")


def _open_video_capture(device: int | str, width: int, height: int, fps: float):
    try:
        import cv2
    except ImportError as error:
        raise RuntimeError(
            "OpenCV is required for camera streaming; install python3-opencv"
        ) from error

    capture = cv2.VideoCapture(device, cv2.CAP_V4L2)
    if not capture.isOpened():
        capture.release()
        raise RuntimeError(f"Could not open camera device {device!r}")

    capture.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc("Y", "U", "Y", "V"))
    capture.set(cv2.CAP_PROP_FRAME_WIDTH, width)
    capture.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
    capture.set(cv2.CAP_PROP_FPS, fps)
    return cv2, capture


def _send_video_frames(
    conn: socket.socket,
    stop_event: threading.Event,
    config: ServerConfig,
    logger: logging.Logger,
) -> None:
    """Capture USB camera frames with OpenCV and stream length-prefixed JPEGs."""
    capture = None
    try:
        cv2, capture = _open_video_capture(
            config.camera_device,
            config.video_width,
            config.video_height,
            config.video_fps,
        )

        read_error_logged = False
        while not stop_event.is_set():
            read_ok, frame = capture.read()
            if not read_ok:
                if not read_error_logged:
                    logger.warning("OpenCV could not read a camera frame")
                    read_error_logged = True
                stop_event.wait(0.05)
                continue
            read_error_logged = False
            frame = cv2.resize(
                frame,
                (config.video_width, config.video_height),
                interpolation=cv2.INTER_AREA,
            )
            encoded_ok, encoded_frame = cv2.imencode(
                ".jpg", frame, [cv2.IMWRITE_JPEG_QUALITY, 80]
            )
            if not encoded_ok:
                logger.warning("OpenCV could not encode a camera frame")
                continue
            logger.debug("Successfully encoded to jpeg")
            jpeg_data = encoded_frame.tobytes()
            conn.sendall(struct.pack("!I", len(jpeg_data)) + jpeg_data)
            logger.debug("Sent OpenCV JPEG frame (%d bytes)", len(jpeg_data))

    except (OSError, RuntimeError) as error:
        if not stop_event.is_set():
            logger.error("Video stream failed: %s", error)
    except Exception:
        if not stop_event.is_set():
            logger.exception("Unexpected OpenCV video streaming failure")
    finally:
        if capture is not None:
            capture.release()
        logger.debug("Video worker ended")


def stream_video(
    conn: socket.socket,
    stop_event: threading.Event,
    config: ServerConfig,
    logger: logging.Logger,
) -> None:
    """Run the optional video transport for the active PSP connection."""
    if not config.video_enabled:
        logger.info("Video streaming is disabled by configuration")
        return
    _send_video_frames(conn, stop_event, config, logger)


def handle_client(
    conn: socket.socket,
    address: tuple,
    controller: RobotController,
    config: ServerConfig,
    logger: logging.Logger,
) -> None:
    """Run input and video threads for connection."""
    logger.info("PSP connected from %s:%d", *address)
    stop_event = threading.Event()
    input_thread = threading.Thread(
        target=receive_inputs,
        args=(conn, stop_event, controller, config, logger),
        name="controller-input",
    )
    video_thread = threading.Thread(
        target=stream_video,
        args=(conn, stop_event, config, logger),
        name="video-stream",
    )

    input_thread.start()
    video_thread.start()
    try:
        input_thread.join()
        video_thread.join()
    finally:
        stop_event.set()
        try:
            conn.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        input_thread.join()
        video_thread.join()
        logger.info("Connection from %s:%d closed", *address)


def run_server(
    controller: RobotController, config: ServerConfig, logger: logging.Logger
) -> None:
    """Keep listening and accept another PSP after every disconnect."""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind((config.host, config.port))
        server.listen(config.backlog)
        logger.info("Listening on %s:%d", config.host, config.port)

        while True:
            try:
                conn, address = server.accept()
            except OSError:
                logger.exception("Could not accept a client connection")
                raise
            with conn:
                handle_client(conn, address, controller, config, logger)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--config",
        type=Path,
        default=Path(__file__).with_name("server.ini"),
        help="INI configuration file (default: raspberry_pi/server.ini)",
    )
    args = parser.parse_args()

    config = load_config(args.config.resolve())
    logger = configure_logging(config)
    controller = RobotController(
        acceleration_duration_ms=config.acceleration_duration_ms
    )
    try:
        controller.set_speed(config.drive_speed)
        run_server(controller, config, logger)
    except KeyboardInterrupt:
        logger.info("Server stopped")
    finally:
        controller.close()


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Accept PSP clients and route controller and video data."""

import argparse
import configparser
import logging
import select
import socket
import struct
import subprocess
import threading
from dataclasses import dataclass
from pathlib import Path
from typing import Optional


@dataclass(frozen=True)
class ServerConfig:
    host: str
    port: int
    backlog: int
    video_enabled: bool
    video_source: str
    video_width: int
    video_height: int
    video_fps: float
    ffmpeg: str
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
        video_source = parser.get("video", "source", fallback="SPIDER.avi")
        video_width = parser.getint("video", "width", fallback=480)
        video_height = parser.getint("video", "height", fallback=272)
        video_fps = parser.getfloat("video", "fps", fallback=15)
        ffmpeg = parser.get("video", "ffmpeg", fallback="ffmpeg")
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
    if not isinstance(logging.getLevelName(logging_level), int):
        raise ValueError(f"Unknown logging level: {logging_level}")

    video_source_path = Path(video_source)
    if (
        not video_source_path.is_absolute()
        and "://" not in video_source
    ):
        video_source = str(config_path.parent / video_source_path)

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
        video_source=video_source,
        video_width=video_width,
        video_height=video_height,
        video_fps=video_fps,
        ffmpeg=ffmpeg,
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


def handle_input_command(buttons: int, analog_x: int, analog_y: int,
                         logger: logging.Logger) -> None:
    """Route one decoded PSP command; replace this seam with car controls."""
    logger.debug(
        "Input buttons=0x%x analog_x=%d analog_y=%d",
        buttons,
        analog_x,
        analog_y,
    )


def receive_inputs(conn: socket.socket, stop_event: threading.Event,
                   logger: logging.Logger) -> None:
    """Receive six-byte PSP commands until the client disconnects."""
    pending_data = bytearray()
    try:
        while not stop_event.is_set():
            data = conn.recv(4096)
            if not data:
                logger.info("PSP disconnected")
                break

            pending_data.extend(data)
            while len(pending_data) >= 6:
                buttons, analog_x, analog_y = struct.unpack_from(
                    "<IBB", pending_data
                )
                del pending_data[:6]
                handle_input_command(buttons, analog_x, analog_y, logger)
    except OSError as error:
        if not stop_event.is_set():
            logger.warning("Controller input connection failed: %s", error)
    finally:
        stop_event.set()
        try:
            conn.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        logger.debug("Input worker ended")


def _send_video_frames(conn: socket.socket, stop_event: threading.Event,
                       config: ServerConfig, logger: logging.Logger) -> None:
    """Encode a configured source as MJPEG and send length-prefixed frames."""
    command = [
        config.ffmpeg,
        "-i",
        config.video_source,
        "-vf",
        f"scale={config.video_width}:{config.video_height}",
        "-c:v",
        "mjpeg",
        "-pix_fmt",
        "yuvj420p",
        "-r",
        str(config.video_fps),
        "-f",
        "mjpeg",
        "pipe:1",
    ]
    process = None
    try:
        process = subprocess.Popen(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            bufsize=0,
        )
        if process.stdout is None:
            raise RuntimeError("FFmpeg stdout pipe was not created")

        frame_buffer = bytearray()
        while not stop_event.is_set():
            readable, _, _ = select.select([process.stdout], [], [], 0.25)
            if not readable:
                if process.poll() is not None:
                    break
                continue

            chunk = process.stdout.read(4096)
            if not chunk:
                break
            frame_buffer.extend(chunk)

            while not stop_event.is_set():
                start = frame_buffer.find(b"\xff\xd8")
                if start < 0:
                    frame_buffer[:] = frame_buffer[-1:]
                    break
                if start:
                    del frame_buffer[:start]

                end = frame_buffer.find(b"\xff\xd9", 2)
                if end < 0:
                    break

                frame = bytes(frame_buffer[:end + 2])
                del frame_buffer[:end + 2]
                conn.sendall(struct.pack("!I", len(frame)) + frame)
                logger.debug("Sent MJPEG frame (%d bytes)", len(frame))

    except (OSError, RuntimeError) as error:
        if not stop_event.is_set():
            logger.warning("Video stream failed: %s", error)
    finally:
        if process is not None:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
            if process.stdout is not None:
                process.stdout.close()
        logger.debug("Video worker ended")


def stream_video(conn: socket.socket, stop_event: threading.Event,
                 config: ServerConfig, logger: logging.Logger) -> None:
    """Run the optional video transport for the active PSP connection."""
    if not config.video_enabled:
        logger.info("Video streaming is disabled by configuration")
        return
    _send_video_frames(conn, stop_event, config, logger)


def handle_client(conn: socket.socket, address: tuple, config: ServerConfig,
                  logger: logging.Logger) -> None:
    """Run input and optional video workers for one connection."""
    logger.info("PSP connected from %s:%d", *address)
    stop_event = threading.Event()
    input_thread = threading.Thread(
        target=receive_inputs,
        args=(conn, stop_event, logger),
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


def serve_forever(config: ServerConfig, logger: logging.Logger) -> None:
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
                handle_client(conn, address, config, logger)


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
    try:
        serve_forever(config, logger)
    except KeyboardInterrupt:
        logger.info("Server stopped")


if __name__ == "__main__":
    main()

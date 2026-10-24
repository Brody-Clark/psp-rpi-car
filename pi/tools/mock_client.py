#!/usr/bin/env python3
"""Keyboard teleoperation client for the Raspberry Pi car server."""

from __future__ import annotations

import argparse
from collections.abc import Set
from io import BytesIO
import queue
import socket
import struct
import threading
import time

from pi.psp_input import PSPButton

MAX_JPEG_BYTES = 1024 * 1024
ANALOG_CENTER = 128
KEY_BINDINGS = {
    "w": PSPButton.UP,
    "a": PSPButton.LEFT,
    "s": PSPButton.DOWN,
    "d": PSPButton.RIGHT,
    "q": PSPButton.L,
    "e": PSPButton.R,
    "up": PSPButton.TRIANGLE,
    "down": PSPButton.X,
    "space": PSPButton.CIRCLE,
}


def buttons_from_keys(keys: Set[str]) -> PSPButton:
    """Translate held keyboard key names into the PSP button bitmask."""
    buttons = PSPButton.NONE
    for key in keys:
        buttons |= KEY_BINDINGS.get(key.lower(), PSPButton.NONE)
    return buttons


def receive_exact(connection: socket.socket, length: int) -> bytes:
    """Read exactly length bytes or raise when the server disconnects."""
    chunks = bytearray()
    while len(chunks) < length:
        data = connection.recv(length - len(chunks))
        if not data:
            raise ConnectionError("Server closed the connection")
        chunks.extend(data)
    return bytes(chunks)


def video_reader(
    connection: socket.socket,
    stop_event: threading.Event,
    frames: queue.Queue[bytes],
    errors: queue.Queue[Exception],
) -> None:
    """Receive length-prefixed JPEG frames without blocking keyboard input."""
    try:
        while not stop_event.is_set():
            (frame_size,) = struct.unpack("!I", receive_exact(connection, 4))
            if frame_size == 0 or frame_size > MAX_JPEG_BYTES:
                raise ValueError(f"Invalid JPEG frame size: {frame_size}")
            jpeg_data = receive_exact(connection, frame_size)
            if frames.full():
                try:
                    frames.get_nowait()
                except queue.Empty:
                    pass
            frames.put_nowait(jpeg_data)
    except (ConnectionError, OSError, ValueError) as error:
        if not stop_event.is_set():
            errors.put(error)
            stop_event.set()


def run_client(host: str, port: int) -> None:
    try:
        import pygame
    except ImportError as error:
        raise RuntimeError(
            "The mock client requires pygame. Install it with: "
            "python3 -m pip install pygame"
        ) from error

    connection = socket.create_connection((host, port), timeout=10)
    connection.settimeout(None)
    stop_event = threading.Event()
    frames: queue.Queue[bytes] = queue.Queue(maxsize=1)
    errors: queue.Queue[Exception] = queue.Queue()
    reader = threading.Thread(
        target=video_reader,
        args=(connection, stop_event, frames, errors),
        name="video-reader",
        daemon=True,
    )
    reader.start()

    pygame.init()
    window = pygame.display.set_mode((640, 400))
    pygame.display.set_caption("PSP Raspberry Pi Car - Mock Client")
    font = pygame.font.Font(None, 22)
    clock = pygame.time.Clock()
    held_keys: set[str] = set()
    latest_frame = None
    next_send = 0.0

    print(f"Connected to {host}:{port}. Press Escape or close the window to exit.")
    print("WASD: camera | Up/Down: drive | Q/E: steering | Space: stop")
    try:
        while not stop_event.is_set():
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    stop_event.set()
                elif event.type == pygame.KEYDOWN:
                    if event.key == pygame.K_ESCAPE:
                        stop_event.set()
                    else:
                        held_keys.add(pygame.key.name(event.key).lower())
                elif event.type == pygame.KEYUP:
                    held_keys.discard(pygame.key.name(event.key).lower())
                elif event.type == pygame.WINDOWFOCUSLOST:
                    held_keys.clear()

            now = time.monotonic()
            if now >= next_send and not stop_event.is_set():
                buttons = int(buttons_from_keys(held_keys))
                connection.sendall(
                    struct.pack("<IBB", buttons, ANALOG_CENTER, ANALOG_CENTER)
                )
                next_send = now + 1 / 60

            while not frames.empty():
                jpeg_data = frames.get_nowait()
                try:
                    latest_frame = pygame.image.load(BytesIO(jpeg_data))
                except pygame.error as error:
                    raise RuntimeError(
                        f"Could not decode server JPEG frame: {error}"
                    ) from error

            window.fill((24, 24, 24))
            if latest_frame is not None:
                frame = pygame.transform.scale(latest_frame, (640, 360))
                window.blit(frame, (0, 0))
            help_text = font.render(
                "WASD camera | Up/Down drive | Q/E steer | Space stop | Esc exit",
                True,
                (255, 255, 255),
            )
            window.blit(help_text, (8, 370))
            pygame.display.flip()
            clock.tick(60)

        if not errors.empty():
            raise RuntimeError(f"Video stream ended: {errors.get()}")
    finally:
        stop_event.set()
        try:
            connection.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        connection.close()
        reader.join(timeout=1)
        pygame.quit()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="192.168.2.1")
    parser.add_argument("--port", type=int, default=12345)
    args = parser.parse_args()
    run_client(args.host, args.port)


if __name__ == "__main__":
    main()

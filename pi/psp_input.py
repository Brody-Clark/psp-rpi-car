"""Typed parsing for raw PSP controller packets."""

import struct
from dataclasses import dataclass
from enum import IntFlag


class PSPButton(IntFlag):
    """PSP controller button masks used by the input packet."""

    NONE = 0
    UP = 0x00000010
    RIGHT = 0x00000020
    DOWN = 0x00000040
    LEFT = 0x00000080
    L = 0x00000100
    R = 0x00000200
    TRIANGLE = 0x00001000
    CIRCLE = 0x00002000
    X = 0x00004000
    SQUARE = 0x008000

@dataclass(frozen=True)
class PSPInput:
    """One six-byte PSP command, including its analog stick sample."""

    buttons: PSPButton
    analog_x: int
    analog_y: int

    @classmethod
    def from_packet(cls, packet: bytes) -> "PSPInput":
        """Parse the PSP wire format: little-endian buttons, X, then Y."""
        if len(packet) != 6:
            raise ValueError(f"PSP input packet must be 6 bytes, got {len(packet)}")

        buttons, analog_x, analog_y = struct.unpack("<IBB", packet)
        return cls(PSPButton(buttons), analog_x, analog_y)

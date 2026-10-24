"""Typed PSP controller packet parsing and vehicle-control mapping."""

import struct
from dataclasses import dataclass
from enum import IntEnum, IntFlag


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


class ControlDirection(IntEnum):
    """A normalized control direction."""

    NEGATIVE = -1
    NEUTRAL = 0
    POSITIVE = 1


@dataclass(frozen=True)
class VehicleControls:
    """Normalized controls with positive meaning right, up, right, and forward."""

    camera_horizontal: ControlDirection
    camera_vertical: ControlDirection
    steering: ControlDirection
    throttle: ControlDirection


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

    @property
    def controls(self) -> VehicleControls:
        """Map PSP buttons to camera, vehicle heading, and throttle controls."""
        return VehicleControls(
            camera_horizontal=_direction(self.buttons, PSPButton.RIGHT, PSPButton.LEFT),
            camera_vertical=_direction(self.buttons, PSPButton.UP, PSPButton.DOWN),
            steering=_direction(self.buttons, PSPButton.R, PSPButton.L),
            throttle=_direction(self.buttons, PSPButton.TRIANGLE, PSPButton.X),
        )


def _direction(
    buttons: PSPButton,
    positive_button: PSPButton,
    negative_button: PSPButton,
) -> ControlDirection:
    positive_pressed = bool(buttons & positive_button)
    negative_pressed = bool(buttons & negative_button)

    if positive_pressed == negative_pressed:
        return ControlDirection.NEUTRAL
    if positive_pressed:
        return ControlDirection.POSITIVE
    return ControlDirection.NEGATIVE

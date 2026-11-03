"""Map raw PSP input to logical vehicle controls."""

from dataclasses import dataclass, fields
from enum import IntEnum

if __package__:
    from .psp_input import PSPButton, PSPInput
else:
    from psp_input import PSPButton, PSPInput


class ControlDirection(IntEnum):
    """A normalized control direction."""

    NEGATIVE = -1
    NEUTRAL = 0
    POSITIVE = 1


@dataclass(frozen=True)
class VehicleControls:
    """Logical vehicle actions independent of PSP button assignments."""

    camera_horizontal: ControlDirection
    camera_vertical: ControlDirection
    steering: ControlDirection
    throttle: ControlDirection
    brake: bool
    center_steering: bool


@dataclass(frozen=True)
class ControlProfile:
    """Button bindings that translate a PSP input state into vehicle actions."""

    camera_pan_positive: PSPButton = PSPButton.RIGHT
    camera_pan_negative: PSPButton = PSPButton.LEFT
    camera_tilt_positive: PSPButton = PSPButton.UP
    camera_tilt_negative: PSPButton = PSPButton.DOWN
    steering_positive: PSPButton = PSPButton.R
    steering_negative: PSPButton = PSPButton.L
    throttle_positive: PSPButton = PSPButton.TRIANGLE
    throttle_negative: PSPButton = PSPButton.X
    brake: PSPButton = PSPButton.CIRCLE
    center_steering: PSPButton = PSPButton.SQUARE

    def map_input(self, command: PSPInput) -> VehicleControls:
        """Map one parsed PSP command to logical vehicle actions."""
        return self.map_buttons(command.buttons)

    def map_buttons(self, buttons: PSPButton) -> VehicleControls:
        """Map a PSP button state to logical vehicle actions."""
        return VehicleControls(
            camera_horizontal=_direction(
                buttons, self.camera_pan_positive, self.camera_pan_negative
            ),
            camera_vertical=_direction(
                buttons, self.camera_tilt_positive, self.camera_tilt_negative
            ),
            steering=_direction(
                buttons, self.steering_positive, self.steering_negative
            ),
            throttle=_direction(
                buttons, self.throttle_positive, self.throttle_negative
            ),
            brake=bool(buttons & self.brake),
            center_steering=bool(buttons & self.center_steering),
        )


DEFAULT_CONTROL_PROFILE = ControlProfile()
CONTROL_PROFILE_FIELDS = tuple(field.name for field in fields(ControlProfile))


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

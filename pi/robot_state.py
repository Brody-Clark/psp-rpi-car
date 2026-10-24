from enum import Enum, auto


class DriveStage(Enum):
    IDLE = auto()
    ACCELERATING = auto()
    CRUISING = auto()


class RobotState:

    def __init__(self):
        self.speed = 0  # 0 to 100
        self.steering_angle = 90  # 0 to 180 (90 is center)
        self.camera_pan = 90  # 0 to 180 (90 is center)
        self.camera_tilt = 90  # 0 to 180 (90 is center)
        self.is_forward = True  # True = forward, False = backward
        self.is_moving = False
        self.drive_stage = DriveStage.IDLE
        self.applied_speed = 0

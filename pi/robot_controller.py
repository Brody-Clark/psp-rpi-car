import threading
import time
from typing import Optional, Protocol

if __package__:
  from .robot_state import DriveStage, RobotState
else:
  from robot_state import DriveStage, RobotState


class ShieldDriver(Protocol):
  """Minimal Freenove shield interface used by the robot controller."""

  CMD_DIR1: int
  CMD_DIR2: int
  CMD_PWM1: int
  CMD_PWM2: int
  CMD_SERVO1: int
  CMD_SERVO2: int
  CMD_SERVO3: int

  def writeReg(self, command: int, value: int) -> None: ...

class RobotController:

  def __init__(
      self,
      servo1_reversed=False,
      servo2_reversed=False,
      servo3_reversed=False,
      shield: Optional[ShieldDriver] = None,
      acceleration_duration_ms=300,
  ):
    if shield is None:
      if __package__:
        from .freenove.Server.mDev import mdev
      else:
        from freenove.Server.mDev import mdev
      shield = mdev

    self.mdev = shield
    self.state = RobotState()
    self.servo1_reversed = servo1_reversed
    self.servo2_reversed = servo2_reversed
    self.servo3_reversed = servo3_reversed
    self.acceleration_duration_ms = max(1, acceleration_duration_ms)
    self._lock = threading.RLock()
    self._drive_generation = 0
    self._drive_thread = None

  def _num_map(self, value, from_low, from_high, to_low, to_high):
    return int(
        (value - from_low) * (to_high - to_low) / (from_high - from_low) + to_low
    )

  def set_speed(self, speed):
    with self._lock:
      self.state.speed = max(0, min(100, speed))
      moving = self.state.is_moving
      if not moving:
        self._apply_drive_motors_locked()
    if moving:
      self.start()

  def adjust_speed(self, delta):
    """Increment/decrement speed"""
    self.state.speed = max(0, min(100, self.state.speed + delta))
    self._apply_drive_motors()

  def set_direction(self, is_forward: bool):
    with self._lock:
      self.state.is_forward = is_forward
      if not self.state.is_moving:
        self._apply_drive_motors_locked()

  def start(self):
    """Start driving and ramp from minimum motor speed to the setpoint."""
    with self._lock:
      if self.state.speed <= 0:
        self.state.is_moving = False
        self.state.drive_stage = DriveStage.IDLE
        self._apply_drive_motors_locked()
        return

      self._drive_generation += 1
      generation = self._drive_generation
      start_speed = min(35, self.state.speed)
      self.state.is_moving = True
      self.state.drive_stage = (
          DriveStage.ACCELERATING
          if start_speed < self.state.speed
          else DriveStage.CRUISING
      )
      self._write_drive_output_locked(start_speed)

      if start_speed == self.state.speed:
        return

      self._drive_thread = threading.Thread(
          target=self._accelerate_to_setpoint,
          args=(generation, start_speed, self.state.speed),
          name="robot-acceleration",
          daemon=True,
      )
      self._drive_thread.start()

  def stop(self):
    """Stop the motors without changing the configured speed."""
    with self._lock:
      self._drive_generation += 1
      self.state.is_moving = False
      self.state.drive_stage = DriveStage.IDLE
      self._apply_drive_motors_locked()

  def set_acceleration_duration(self, duration_ms):
    """Set total time for the acceleration ramp."""
    if duration_ms < 1:
      raise ValueError("Acceleration duration must be positive")
    with self._lock:
      self.acceleration_duration_ms = duration_ms

  def close(self):
    """Stop motor output and wait for the acceleration worker to finish."""
    self.stop()
    drive_thread = self._drive_thread
    if drive_thread is not None and drive_thread is not threading.current_thread():
      drive_thread.join(timeout=self.acceleration_duration_ms / 1000 + 0.5)

  def steer(self, angle):
    """Set absolute steering angle (or from analog stick)"""
    self.state.steering_angle = max(0, min(180, angle))
    self._apply_steering()

  def adjust_steering(self, delta):
    """Step steering angle (e.g., D-Pad left/right click)"""
    with self._lock:
      self.state.steering_angle = max(
          0, min(180, self.state.steering_angle + delta)
      )
      self._apply_steering_locked()

  def center_steering(self):
    self.state.steering_angle = 90
    self._apply_steering()

  def adjust_camera_pan(self, delta):
    with self._lock:
      self.state.camera_pan = max(0, min(180, self.state.camera_pan + delta))
    self._apply_camera()

  def adjust_camera_tilt(self, delta):
    with self._lock:
      self.state.camera_tilt = max(0, min(180, self.state.camera_tilt + delta))
    self._apply_camera()


  def _apply_drive_motors(self):
    with self._lock:
      self._apply_drive_motors_locked()

  def _apply_drive_motors_locked(self):
    output_speed = self.state.applied_speed if self.state.is_moving else 0
    self._write_drive_output_locked(output_speed)

  def _write_drive_output_locked(self, speed):
    val = speed * 10
    direction = 1 if self.state.is_forward else 0
    self.mdev.writeReg(self.mdev.CMD_DIR1, direction)
    self.mdev.writeReg(self.mdev.CMD_DIR2, direction)
    self.mdev.writeReg(self.mdev.CMD_PWM1, val)
    self.mdev.writeReg(self.mdev.CMD_PWM2, val)
    self.state.applied_speed = speed

  def _accelerate_to_setpoint(self, generation, start_speed, target_speed):
    stage_count = 3
    interval_seconds = self.acceleration_duration_ms / 1000 / stage_count
    for stage in range(1, stage_count + 1):
      time.sleep(interval_seconds)
      with self._lock:
        if (
            generation != self._drive_generation
            or not self.state.is_moving
        ):
          return
        speed = round(
            start_speed + (target_speed - start_speed) * stage / stage_count
        )
        self.state.drive_stage = (
            DriveStage.CRUISING
            if stage == stage_count
            else DriveStage.ACCELERATING
        )
        self._write_drive_output_locked(speed)

  def _apply_steering(self):
    with self._lock:
      self._apply_steering_locked()

  def _apply_steering_locked(self):
    # Apply inversion rules
    angle = self.state.steering_angle
    if self.servo1_reversed:
      angle = 180 - angle

    pwm_val = self._num_map(angle, 0, 180, 500, 2500)
    self.mdev.writeReg(self.mdev.CMD_SERVO1, pwm_val)

  def _apply_camera(self):
    with self._lock:
      self._apply_camera_locked()

  def _apply_camera_locked(self):
    pan = (
        self.state.camera_pan
        if not self.servo2_reversed
        else 180 - self.state.camera_pan
    )
    tilt = (
        self.state.camera_tilt
        if not self.servo3_reversed
        else 180 - self.state.camera_tilt
    )

    self.mdev.writeReg(
        self.mdev.CMD_SERVO2, self._num_map(pan, 0, 180, 500, 2500)
    )
    self.mdev.writeReg(
        self.mdev.CMD_SERVO3, self._num_map(tilt, 0, 180, 500, 2500)
    )

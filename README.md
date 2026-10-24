# PSP Raspberry Pi Car

psp-rpi-car (PSP Raspberry Pi Car) demonstrates the usage of a Sony PSP, along with the pspsdk libraries, to control a Raspberry Pi car and display its streamed camera data.

## Contents

- [Overview](#overview)
- [Project Structure](#project-structure)
- [Requirements](#requirements)
- [Getting Started](#getting-started)
- [Usage](#usage)
- [Raspberry Pi Server Configuration](#raspberry-pi-server-configuration)
- [Building from Source](#building-from-source)
- [Troubleshooting](#troubleshooting)
- [Development](CONTRIBUTING.md)
- [License](#license)

## Overview

This project provides a small PSP application (EBOOT.PBP) and a Raspberry Pi server that together demonstrate two-way data transfer: PSP button inputs can be sent to the Pi, and the Pi can stream camera and sensor data back to the PSP. This project uses the Raspberry Pi server as a driver for a Freenove three-wheeled car but can be repurposed for other uses such as drones, IoT devices, and other robotics projects.

## Project Structure

```text
psp-rpi-car/
├── docs/
├── pi/
│   ├── freenove/
│   ├── scripts/
│   ├── tests/
│   ├── tools/
│   ├── psp_input.py
│   ├── robot_controller.py
│   ├── robot_state.py
│   ├── server.ini
│   └── server.py
├── psp/
│   ├── include/
│   ├── src/
│   └── tests/
├── CONTRIBUTING.md
└── README.md
```

### PSP Client

The PSP client reads controller input, sends six-byte control packets, receives
JPEG frames, and presents the camera image. Its modules separate input,
networking, video decoding, display, and PSP lifecycle responsibilities.

The client is built with the PSPSDK libraries. It uses PSPGU for utility-dialog
rendering, PSP networking libraries for the connection, and PSP JPEG/MPEG
libraries for video decoding.

### Pi Server

The Pi server accepts the PSP connection, interprets its control packets, and
routes movement to the Freenove shield driver. It also captures camera frames
and streams them to the client as JPEG images. The server remains available
for a client reconnect.

The server uses Python's standard library for configuration, networking, and
logging. OpenCV handles USB camera capture and JPEG encoding. The retained
Freenove driver provides the shield interface.

### Client and Server Relationship

The PSP and Pi communicate over TCP. The PSP sends controller packets and the
Pi returns length-prefixed JPEG frames. The [protocol description](docs/protocol.md)
documents the packet formats.

## Requirements

- A Raspberry Pi running Raspberry Pi OS (tested on Pi 3/Pi 4)
- The Freenove three-wheeled car kit and shield (Can be purchased from the official Freenove store [here](https://store.freenove.com/products/fnk0021?srsltid=AU7gw4WxGUVLTF7dpSA2eTFnrtBII-kZL_LF3ODinYbY7OUMYRSHbQqb))
- A USB camera supported by OpenCV (Should be included in Freenove kit)
- A Sony PSP capable of running homebrew or unsigned EBOOT.PBP files
- A way to transfer files to the PSP memory stick
- Ethernet or a second Wi-Fi adapter if you need remote access while the Pi's
  built-in Wi-Fi is configured as an access point
  - pspsdk toolchain (only needed if building from source)

## Getting Started

### Set Up the Raspberry Pi

Download `pi-car-server.zip` from GitHub Releases and extract it on the Pi.
Alternatively, clone the repository:

```bash
git clone https://github.com/Brody-Clark/psp-rpi-car.git
cd psp-rpi-car
```

Inspect `pi/scripts/setup_server.sh` for the SSID and password defaults.
> [!IMPORTANT]
> keep note of the default SSID and password or change them if you want. These will be needed later to connect to the Pi from your PSP.

Run the setup script from the extracted release folder:

```bash
sudo bash ./pi-car-server/scripts/setup_server.sh
```

For a repository checkout, use:

```bash
sudo bash ./pi/scripts/setup_server.sh
```

> [!WARNING]
> This script will disable the Pi's regular Wi-Fi connectivity. This will prevent SSH communication. If you need an SSH connection you can:
> 1. Use an ethernet connection
> 2. Connect an additional USB wifi adapter to the Pi

### Install the PSP Client

Download `psp-car-client.zip` from GitHub Releases and extract it. Copy the
included `PSP_RPI_CAR/` folder to the PSP memory stick under `PSP/GAME/`.
Confirm the final file path is `PSP/GAME/PSP_RPI_CAR/EBOOT.PBP`.

### Connect

On the PSP, open **Settings > Network Settings > Infrastructure Mode** and
connect to the Pi access point. Then launch **PSP_RPI_CAR** from
**Game > Memory Stick**.

## Usage

### PSP Controls

| PSP input | Action |
| --- | --- |
| D-pad Up / Down | Tilt the camera up or down |
| D-pad Left / Right | Pan the camera left or right |
| L / R shoulder | Turn the vehicle left or right |
| Triangle (hold) | Drive forward at the configured speed |
| X (hold) | Drive in reverse at the configured speed |
| Circle | Stop the motors |
| HOME | Exit the application |

## Raspberry Pi Server Configuration

The server reads `server.ini` from its installation directory. In a repository
checkout, edit `pi/server.ini`. In the release archive, edit
`pi-car-server/server.ini`.

After changing a setting, restart the service:

```bash
sudo systemctl restart psp-rpi-car.service
```
The config supports the following options:

| Section | Option | Default | Description |
| --- | --- | --- | --- |
| `[server]` | `host` | `0.0.0.0` | Network address for incoming clients |
| `[server]` | `port` | `12345` | TCP port used by the PSP and server |
| `[server]` | `backlog` | `1` | Number of pending TCP connections |
| `[video]` | `enabled` | `true` | Enable camera capture and video streaming |
| `[video]` | `device` | `0` | USB camera index or device path such as `/dev/video0` |
| `[video]` | `width` | `480` | Requested camera image width in pixels |
| `[video]` | `height` | `272` | Requested camera image height in pixels |
| `[video]` | `fps` | `15` | Requested camera frame rate |
| `[controls]` | `drive_speed` | `5` | Speed level from 1 to 10 |
| `[controls]` | `steering_step` | `5` | Steering angle change per repeat |
| `[controls]` | `camera_pan_step` | `5` | Camera pan change per repeat |
| `[controls]` | `camera_tilt_step` | `5` | Camera tilt change per repeat |
| `[controls]` | `held_control_repeat_ms` | `100` | Repeat interval for held directional controls |
| `[controls]` | `acceleration_duration_ms` | `300` | Duration of the controller acceleration ramp |
| `[logging]` | `enabled` | `true` | Set to `false` to disable application logs |
| `[logging]` | `level` | `INFO` | Log detail level such as `DEBUG`, `INFO`, or `WARNING` |
| `[logging]` | `file` | `logs/server.log` | Optional log file path, relative to the INI file |

Set the logging file option to an empty value to log to the service journal
only. View service status with:

```bash
sudo systemctl status psp-rpi-car.service
```

When file logging is disabled or unavailable, view logs with:

```bash
sudo journalctl -u psp-rpi-car.service
```

## Building from Source

To build the PSP client from source you will need the pspsdk toolchain:

- Install instructions: https://pspdev.github.io/
- pspdev repo: https://github.com/pspdev/pspdev
- pspsdk repo: https://github.com/pspdev/pspsdk

Build the psp client project with the PSPSDK `psp-cmake` wrapper:
```bash
cd psp/
mkdir -p build
cd build/
psp-cmake ..
make
```

This will produce the EBOOT.PBP that needs to be added to the `PSP/GAME/PSP_RPI_CAR/` folder.

## Troubleshooting

### PSP Cannot Connect

Check that the Pi access point and server service are running. Confirm the
configured SSID and password and verify the client and server use the same TCP
port.

If the access point stops working after setup, restart its services:

```bash
sudo systemctl restart hostapd dnsmasq
```

### Remote Access to the Pi Is Unavailable

The setup script configures the Pi's built-in Wi-Fi for access-point use. Use
Ethernet or a second Wi-Fi adapter to keep remote access.

### Camera or Video Problems

Check that the USB camera is connected and available at the configured device.
Review the service logs for OpenCV camera errors and confirm the requested
resolution is supported by the camera.

## Development

For development setup, see [CONTRIBUTING.md](CONTRIBUTING.md).

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE).

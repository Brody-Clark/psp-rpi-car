# PSP Rasberry Pi Link

PSP Raspberry Pi Link (psp-rpi-link) lets you remotely connect a Sony PSP to a Raspberry Pi host to stream camera/sensor data and forward button inputs. It's designed for hobby projects that need a lightweight wired/wireless bridge between a PSP and a Raspberry Pi.

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Requirements](#requirements)
- [Getting started](#getting-started)
- [Building from source](#building-from-source)
- [Usage](#usage)
- [Troubleshooting](#troubleshooting)
- [Contributing](#contributing)
- [License](#license)
- [About / Contact](#about--contact)

## Overview

This project provides a small PSP application (EBOOT.PBP) and a Raspberry Pi server that together enable two-way data transfer: PSP button inputs can be sent to the Pi, and the Pi can stream camera and sensor data back to the PSP. Typical uses include remote control experiments, telemetry, and retro handheld integration projects.

## Features

- Two-way data transfer between PSP and Raspberry Pi
- PSP input forwarding (buttons, analog)
- Camera feed streaming from Pi to PSP (basic)
- Sensor telemetry (e.g., GPIO, I2C sensors on the Pi)
- Scripts to configure the Pi as a Wi-Fi access point and start the server

## Requirements

- Raspberry Pi (tested on Pi 3 / Pi 4) with Raspbian / Raspberry Pi OS
- A Sony PSP capable of running homebrew (custom firmware / ability to run unsigned EBOOT.PBP)
- A USB connection to transfer `EBOOT.PBP` to the PSP (or memory stick access)
- (Optional) Etherne or additional wifi-adapter to SSH into your pi when setup script disables onboard Wi-Fi
- pspsdk toolchain (only needed if building from source)

## Getting Started

To get started, follow these steps:

1. Clone the repo:
    ```bash
    git clone https://github.com/Brody-Clark/psp-rpi-link.git
    cd psp-rpi-link
    ```

2. Prepare the Raspberry Pi:
   - Inspect `raspberry_pi/scripts/setup_host.sh` for the SSID and password defaults.
        > [!NOTE]
        > keep note of the default SSID and password or change them as you wish. These will be needed later to connect to the Pi from your PSP.
   - Run (on the Pi):
        ```bash
            sudo bash ./raspberry_pi/scripts/setup_host.sh
        ```

> [!IMPORTANT]
> This script will disable the Pi's regular Wi-Fi connectivity. This will prevent SSH communication. If you need an SSH connection you can:
> 1. Use an ethernet connection
> 2. Use an additional wifi adapter on the Pi

3. Prepare the PSP:
   - Download the latest release (EBOOT.PBP) from Releases. If building from source, follow [Building from source](#building-from-source).
   - Connect PSP to your PC via USB or access the memory stick.
   - Copy `EBOOT.PBP` to `PSP/GAME/PSP_RPI_LINK/EBOOT.PBP` (create the folder if needed).

4. Connect and run:
   - On PSP: `Settings > Network Settings > Infrastructure Mode > [New Connection] > Scan`, then connect to the Pi's SSID.
   - On PSP: select `Game > Memory Stick > PSP_RPI_LINK` to start the app.

6. From you PSP, navigate to `Game > Memory Stick` and select the app called `PSP_RPI_LINK`


## Building from Source

To build the PSP client from source you will need the pspsdk toolchain:

- Install instructions: https://pspdev.github.io/
- pspdev repo: https://github.com/pspdev/pspdev
- pspsdk repo: https://github.com/pspdev/pspsdk

If you run into toolchain or build issues, open an issue with the build log.


# Usage

TODO


## Troubleshooting

- "I lost SSH after setup": The Access Point setup disables the Pi's wifi client mode. Use ethernet, a secondary wi-fi adapterm, or re-enable Wi-Fi manually after setup.
- PSP won't connect: Verify the SSID and password printed by the Pi, confirm PSP firmware supports homebrew / EBOOT loading.
- Build errors: Ensure pspsdk is installed and in PATH and you are compiling within WSL if on Windows.

## Contributing

Contributions welcome. Please see [CONTRIBUTING.md](/CONTRIBUTING.md) for more details.
 
## License

This project is licensed under the MIT License. See the included [LICENSE](LICENSE) file for full details.

## About / Contact

PSP Raspberry Pi Link was created to help hobbyists and makers connect a Sony PSP to a Raspberry Pi for experiments and retro integrations. If you have questions, want to report a bug, or request a feature, please open an issue in this repository: https://github.com/Brody-Clark/psp-rpi-link/issues

For direct contact or to view other projects by the maintainer, visit the owner's GitHub profile: https://github.com/Brody-Clark




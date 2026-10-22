# PSP Rasberry Pi Car

PSP Raspberry Pi Car (psp-rpi-car) lets you remotely connect a Sony PSP to a Raspberry Pi host to stream camera/sensor data and forward button inputs. This project demonstrates the ability to control a Raspberry Pi car using the PSP as the controller and camera monitor.

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

This project provides a small PSP application (EBOOT.PBP) and a Raspberry Pi server that together demonstrate two-way data transfer: PSP button inputs can be sent to the Pi, and the Pi can stream camera and sensor data back to the PSP. This project uses the Rapsberry Pi server as a driver for a Freenove three-wheeled car but can be repurposed for other uses such as drones, IoT devices, and other robotics experiments.


## Project Structure

```text
psp-pi-car/
├── README.md                
├── CONTRIBUTING.md           # Guidelines for contributing developers
├── scripts/                  # Automation and setup scripts
│   └── setup_server.sh       # Automates AP mode, dependencies, and service config
├── docs/                     # Schematics, pinouts, and network protocols
│   └── protocol.md
├── psp/                      # PSP client source code & CMake build
│   ├── CMakeLists.txt
│   ├── include/               # Public module interfaces
│   └── src/                   # Application and PSP platform modules
└── raspberry_pi/             # Raspberry Pi Server source code
    ├── server.py             # Main network listener & command router
    ├── camera/               # Video streaming module
    ├── motors/               # Freenove motor abstraction layer
    └── freenove/             # Original vendor scripts
```

The PSP client keeps application flow in `psp/src/app.c`; controller input,
network transport, JPEG decoding, display output, and PSP lifecycle services
are implemented in separate modules with public interfaces in `psp/include/`.

## Requirements

- Raspberry Pi (tested on Pi 3 / Pi 4) with Raspbian / Raspberry Pi OS
- Freenove three-wheeled car kit (Can be purchased from the official Freenove store [here](https://store.freenove.com/products/fnk0021?srsltid=AU7gw4WxGUVLTF7dpSA2eTFnrtBII-kZL_LF3ODinYbY7OUMYRSHbQqb))
- A Sony PSP capable of running homebrew (custom firmware / ability to run unsigned EBOOT.PBP)
- A USB connection to transfer `EBOOT.PBP` to the PSP (or memory stick access)
- (Optional) Ethernet or additional wifi-adapter to SSH into your pi when setup script disables onboard Wi-Fi
- pspsdk toolchain (only needed if building from source)

## Getting Started

To get started, follow these steps:

1. Clone the repo:
    ```bash
    git clone https://github.com/Brody-Clark/psp-rpi-car.git
    cd psp-rpi-car
    ```

2. Prepare the Raspberry Pi:
   - Inspect `raspberry_pi/scripts/setup_server.sh` for the SSID and password defaults.
        > [!NOTE]
        > keep note of the default SSID and password or change them as you wish. These will be needed later to connect to the Pi from your PSP.
   -  Setup the Pi as an access point host:
        ```bash
            sudo bash ./raspberry_pi/scripts/setup_server.sh
        ```
      > [!IMPORTANT]
      > This script will disable the Pi's regular Wi-Fi connectivity. This will prevent SSH communication. If you need an SSH connection you can:
      > 1. Use an ethernet connection
      > 2. Use an additional wifi adapter on the Pi

   - Run the car server:
      ```bash
      cd raspberry_pi
      python3 server.py
      ```

3. Prepare the PSP Client:
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

Configure and build with the PSPSDK `psp-cmake` wrapper:
```bash
psp-cmake -S psp -B build-psp
cmake --build build-psp
```

## Troubleshooting

- "I lost SSH after setup": The Access Point setup disables the Pi's wifi client mode. Use ethernet, a secondary wi-fi adapter, or re-enable Wi-Fi manually after setup.
- PSP won't connect: Verify the SSID and password printed by the Pi, confirm PSP firmware supports homebrew / EBOOT loading.
- Build errors: Ensure pspsdk is installed and in PATH and you are compiling within WSL if on Windows.

## Contributing

Contributions welcome. Please see [CONTRIBUTING.md](/CONTRIBUTING.md) for more details.
 
## License

This project is licensed under the MIT License. See the included [LICENSE](LICENSE) file for full details.

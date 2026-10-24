# Contributing

## Repository Setup

Clone the repository and work from its root directory:

```bash
git clone https://github.com/Brody-Clark/psp-rpi-car.git
cd psp-rpi-car
```

The main code areas are:

- `psp/src/` and `psp/include/` contain the PSP application and module APIs.
- `psp/tests/` contains host-buildable tests for the PSP wire protocol.
- `pi/` contains the Raspberry Pi server, configuration, and controller.
- `pi/tests/` contains Pi server and input tests.
- `pi/tools/` contains developer tools, including the keyboard client.

## Building the PSP Client

Install PSPSDK by following the
[PSPSDK installation instructions](https://pspdev.github.io/). Build the PSP
application from the repository root:

```bash
mkdir -p build/psp
cd build/psp
psp-cmake ../../psp
make
```

The build creates `EBOOT.PBP` in the PSP build output. The PSP Release archive
should include a top-level `PSP_RPI_CAR/` folder containing that file. The
folder can then be copied into `PSP/GAME/` on the memory stick.

## Running the Pi Unit Tests

The Pi tests use Python's built-in `unittest` framework. Run them from the
repository root:

```bash
python3 -m unittest discover -s pi/tests -v
```

These tests cover packet parsing, server control behavior, camera stream
framing, and keyboard mapping. Hardware is mocked where needed, so a Raspberry
Pi, shield, and camera are not required to run the suite.

## Running the PSP Protocol Test

The protocol test uses the host C compiler and does not require PSPSDK:

```bash
cmake -S psp/tests -B build/psp-tests
cmake --build build/psp-tests
ctest --test-dir build/psp-tests --output-on-failure
```

## Using the Keyboard Mock Client

The mock client sends the server the same six-byte controller packets as the
PSP. It also receives and displays the JPEG camera stream. It is intended for
development on a desktop computer.

Install Pygame in the Python environment used to run the client:

```bash
python3 -m pip install pygame
```

Start the Pi server and enable video streaming. Then run the client from the
repository root:

```bash
python3 -m pi.tools.mock_client --host 192.168.2.1 --port 12345
```

Use `--host` and `--port` to match the server configuration when it differs
from the defaults.

| Key | Control |
| --- | --- |
| W / A / S / D | Camera up / left / down / right |
| Up / Down arrow | Forward / reverse |
| Q / E | Steering left / right |
| Space | Stop the motors |
| Escape | Close the client |

The client sends updates continuously while keys are held. Closing the client
releases the connection, which causes the server to stop the vehicle.

## Pi Server Development

The server entry point is `pi/server.py`. It parses controller packets into
`PSPInput` objects and routes normalized controls through `RobotController`.
Camera capture and JPEG streaming use OpenCV. Shield output goes through the
retained Freenove driver in `pi/freenove/Server/mDev.py`.

For development on a Raspberry Pi, install the same hardware dependencies
listed by `pi/scripts/setup_server.sh`. To run the server directly, use:

```bash
python3 pi/server.py --config pi/server.ini
```

The default configuration binds to all interfaces on port `12345`. Review
`pi/server.ini` before running with vehicle hardware. See the README's
[server configuration](README.md#raspberry-pi-server-configuration) section
for option details.

## Contribution Guidelines

Keep changes focused and follow the existing module boundaries. Add or update
tests for behavior changes and run the relevant test commands before submitting
changes.

Avoid requiring optional desktop tools for server operation. Pygame is only
needed for the keyboard client and is not a Pi server dependency.

When changing the client-server wire format, update the PSP and Pi code
together, adjust the protocol tests, and document the change in
`docs/protocol.md`.

## Release Archive Layout

Prepare the PSP client archive with this structure:

```text
psp-car-client.zip
└── PSP_RPI_CAR/
    └── EBOOT.PBP
```

Prepare the Pi server archive with its installable directory at the root:

```text
pi-car-server.zip
└── pi-car-server/
    ├── freenove/
    ├── scripts/
    ├── server.ini
    ├── server.py
    ├── psp_input.py
    ├── robot_controller.py
    └── robot_state.py
```

The Pi archive should include the files referenced by the setup script and
should not include generated Python caches or local log files.
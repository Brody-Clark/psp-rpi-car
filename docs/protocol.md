
# Network Protocol Specification

This document defines the communication protocol between the **PSP Client** (Controller & Video Display) and the **Raspberry Pi Server** (Car Control & Camera Stream). 

To ensure low latency and real-time responsiveness over Wi-Fi, the system uses a **dual-channel** or **multiplexed socket** architecture:
* **Control Channel (UDP or TCP):** Low-overhead, asynchronous packets sent from PSP $\rightarrow$ Pi for motor and steering commands.
* **Video Channel (TCP or UDP):** Dedicated stream sent from Pi $\rightarrow$ PSP carrying camera frames.


## 1. Control Protocol (PSP $\rightarrow$ Raspberry Pi)

Control commands are triggered by polling the PSP controller state (`sceCtrlReadBufferPositive`) in the main game loop. Commands are sent whenever button states change or at a fixed interval (e.g., 30Hz) to prevent connection timeout.

### Packet Structure
Commands can be sent as compact comma-separated ASCII strings or fixed-size binary structs. For ease of debugging, a compact text format is recommended:

```text
CMD:<THROTTLE>:<STEERING>:<AUX_FLAGS>\n

```

* **`CMD`**: Magic header bytes (3 bytes) to validate packet integrity.
* **`THROTTLE`**: Integer value ranging from `-100` (full reverse) to `100` (full forward). `0` is stopped.
* **`STEERING`**: Integer value ranging from `-100` (full left) to `100` (full right). `0` is centered.
* **`AUX_FLAGS`**: Bitmask integer representing auxiliary buttons (e.g., headlights, turbo, horn).

#### Example Control Packet

```text
CMD:75:-20:1\n

```

*(Meaning: 75% forward throttle, 20% left steering, auxiliary flag 1 active)*



## 2. Video Streaming Protocol (Raspberry Pi $\rightarrow$ PSP)

The Raspberry Pi captures frames from the camera module, resizes them to fit the PSP's native resolution ($480 \times 272$ pixels), compresses them (e.g., as JPEG frames or custom raw RGB565 chunks), and streams them to the PSP.

### Stream Architecture Options

#### Option A: MJPEG over HTTP (Easiest to implement)

* The Pi runs a lightweight HTTP stream endpoint (e.g., `/stream.mjpg`).
* The PSP network code opens an HTTP socket, parses the multipart boundary, and extracts sequential JPEG binary blocks.
* **Pros:** Highly reliable, easy to test in a standard web browser.
* **Cons:** HTTP header overhead per frame.

#### Option B: Custom Binary Frame Framing (Lowest Latency)

For maximum performance, use a custom TCP or UDP packet structure where each frame is prefixed with a fixed-size header:

```
+-------------------+-------------------+-------------------------+
| Magic Bytes (2B)  | Frame Size (4B)   | JPEG/Raw Payload (nB)   |
| [0x50, 0x53]      | (UInt32 Big Endian) | (Variable Length)     |
+-------------------+-------------------+-------------------------+

```

* **Magic Bytes (`0x50, 0x53`)**: ASCII `"PS"` to signify the start of a video frame packet.
* **Frame Size**: 4-byte integer indicating the exact byte length of the incoming image payload.
* **Payload**: The raw image data (e.g., JPEG bytes) ready to be read into memory and decoded by the PSP rendering pipeline.

---

## 3. Connection Handshake & Lifecycle

1. **Discovery / Connection:**
* The PSP connects to the Raspberry Pi's Wi-Fi Access Point (configured via `setup_server.sh`).
* The PSP client establishes a socket connection to the Pi server IP (default static IP, e.g., `192.168.4.1`) on designated ports:
* Control Port: `8080` (UDP/TCP)
* Video Port: `8081` (TCP)




2. **Streaming Loop:**
* **Client:** Continuously polls inputs and streams command packets.
* **Server:** Reads control packets, updates motor states via `motors/drive.py`, and continuously pushes camera frames over the video stream.


3. **Disconnection / Timeout:**
* If the Pi server does not receive a control packet within a set threshold (e.g., 500ms), it triggers a **failsafe stop** (`THROTTLE=0`, `STEERING=0`) to prevent runaway cars if the PSP loses Wi-Fi connection or battery dies.


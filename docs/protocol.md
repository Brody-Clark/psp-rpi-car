
# PSP Client Protocol

The PSP client and Raspberry Pi server use one TCP connection to the server's
configured address (`192.168.4.1:12345` by default). The connection carries
controller commands from the PSP and video frames from the Pi.

## Controller command (PSP to Pi)

Each command is exactly six bytes:

| Offset | Size | Value |
| --- | ---: | --- |
| 0 | 4 bytes | PSP controller button bitmask, little-endian |
| 4 | 1 byte | Analog stick X sample |
| 5 | 1 byte | Analog stick Y sample |

Commands are sent at the video frame cadence. There is no text prefix or
additional packet header.

## Video frame (Pi to PSP)

Each frame consists of a four-byte unsigned payload length in network byte
order (big-endian), followed by that many JPEG bytes. TCP may split either
part across reads; the client reads each field until it is complete. The PSP
client accepts JPEG payloads up to `480 * 272 * 4` bytes and decodes them to
the PSP's `480 x 272` display dimensions.

## Connection lifecycle

The PSP connects to the configured infrastructure access point, opens one TCP
connection to the server, then alternates sending a controller command and
receiving a complete JPEG frame. A disconnect or malformed/oversized frame
ends the client loop.

#ifndef PSP_CONFIG_H
#define PSP_CONFIG_H

#define PSP_SERVER_IP "192.168.4.1"
#define PSP_SERVER_PORT 12345

#define PSP_SCREEN_WIDTH 480
#define PSP_SCREEN_HEIGHT 272
#define PSP_DISPLAY_BUFFER_WIDTH 512
#define PSP_PIXEL_BYTES 4
#define PSP_RGBA_FRAME_BYTES \
	(PSP_SCREEN_WIDTH * PSP_SCREEN_HEIGHT * PSP_PIXEL_BYTES)

/* Allow for JPEG framing overhead while keeping a bounded receive buffer. */
#define PSP_MAX_JPEG_BYTES PSP_RGBA_FRAME_BYTES

#endif

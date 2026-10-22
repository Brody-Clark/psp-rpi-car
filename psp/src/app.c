#include "app.h"

#include <stdint.h>

#include "config.h"
#include "display.h"
#include "input.h"
#include "network.h"
#include "platform.h"
#include "protocol.h"
#include "video.h"

static uint8_t jpeg_buffer[PSP_MAX_JPEG_BYTES] __attribute__((aligned(64)));
static uint32_t pixel_buffer[PSP_SCREEN_WIDTH * PSP_SCREEN_HEIGHT]
	__attribute__((aligned(64)));

/**
 * Own subsystem startup, frame processing, and reverse-order shutdown.
 */
int app_run(void)
{
	bool platform_ready = false;
	bool display_ready = false;
	bool video_ready = false;
	bool network_ready = false;
	int result = 0;

	if (!platform_initialize())
	{
		return -1;
	}
	platform_ready = true;
	platform_clear_console();

	if (!display_initialize())
	{
		platform_log("Display initialization failed.\n");
		result = -1;
		goto cleanup;
	}
	display_ready = true;

	if (!input_initialize())
	{
		platform_log("Controller initialization failed.\n");
		result = -1;
		goto cleanup;
	}

	if (!video_initialize())
	{
		platform_log("JPEG decoder initialization failed.\n");
		result = -1;
		goto cleanup;
	}
	video_ready = true;

	if (!network_initialize())
	{
		platform_log("Network initialization failed.\n");
		result = -1;
		goto cleanup;
	}
	network_ready = true;

	platform_log("Connect to the Raspberry Pi access point.\n");
	if (!network_connect_access_point() || !network_connect_server())
	{
		platform_log("Could not connect to the Raspberry Pi server.\n");
		result = -1;
		goto cleanup;
	}

	platform_clear_console();
	platform_log("Connected. Press HOME to exit.\n");

	while (platform_is_running())
	{
		psp_command_t command;
		size_t jpeg_size = 0;

		if (!network_access_point_is_connected())
		{
			platform_log("Access point disconnected.\n");
			result = -1;
			break;
		}

		if (!input_read_command(&command) ||
			!network_send_command(&command))
		{
			platform_log("Failed to send controller input.\n");
			result = -1;
			break;
		}

		if (!network_receive_frame(jpeg_buffer, sizeof(jpeg_buffer),
								  &jpeg_size))
		{
			platform_log("Failed to receive a video frame.\n");
			result = -1;
			break;
		}

		if (!video_decode_jpeg(jpeg_buffer, jpeg_size, pixel_buffer,
							  sizeof(pixel_buffer) / sizeof(pixel_buffer[0])))
		{
			platform_log("Failed to decode a video frame.\n");
			result = -1;
			break;
		}

		if (!display_present_rgba8888(pixel_buffer, PSP_SCREEN_WIDTH,
									  PSP_SCREEN_HEIGHT))
		{
			platform_log("Failed to present a video frame.\n");
			result = -1;
			break;
		}
	}

cleanup:
	if (network_ready)
	{
		network_shutdown();
	}
	if (video_ready)
	{
		video_shutdown();
	}
	if (display_ready)
	{
		display_shutdown();
	}
	if (platform_ready)
	{
		platform_shutdown();
	}

	return result;
}

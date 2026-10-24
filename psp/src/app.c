#include "app.h"

#include <stdint.h>

#include <pspkernel.h>

#include "config.h"
#include "display.h"
#include "input.h"
#include "network.h"
#include "platform.h"
#include "protocol.h"
#include "video.h"

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
	platform_wait_s(2);
	{
		bool access_point_connected = network_connect_access_point();

		display_shutdown();
		display_ready = false;
		platform_reset_console();
		if (!access_point_connected)
		{
			platform_log("Could not connect to the access point.\n");
			result = -1;
			goto cleanup;
		}
	}
	if (!network_connect_server(PSP_SERVER_CONNECT_TIMEOUT_SECONDS))
	{
		result = -1;
		goto cleanup;
	}
	if (!network_start_workers())
	{
		platform_log("Could not start network worker threads.\n");
		result = -1;
		goto cleanup;
	}

	platform_log("Connected. Press HOME to exit.\n");

	while (platform_is_running())
	{
		psp_command_t command;
		const uint8_t *jpeg_data;
		size_t jpeg_size;
		static bool frame_error_logged;

		if (!network_access_point_is_connected() ||
			!network_send_worker_is_running())
		{
			platform_log("Network connection lost.\n");
			result = -1;
			break;
		}

		if (!input_read_command(&command) ||
			!network_set_command(&command))
		{
			platform_log("Could not update controller command.\n");
			result = -1;
			break;
		}

		if (network_acquire_frame(&jpeg_data, &jpeg_size))
		{
			bool frame_ok = video_decode_jpeg(
				jpeg_data, jpeg_size, pixel_buffer,
				sizeof(pixel_buffer) / sizeof(pixel_buffer[0]));

			if (frame_ok)
			{
				frame_ok = display_present_rgba8888(
					pixel_buffer, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT);
			}
			network_release_frame();

			if (!frame_ok)
			{
				if (!frame_error_logged)
				{
					platform_log("Could not decode or display a video frame; "
								 "continuing with the stream.\n");
					frame_error_logged = true;
				}
			}
		}

		sceKernelDelayThread(10000);
	}

cleanup:
	platform_wait_s(3);
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

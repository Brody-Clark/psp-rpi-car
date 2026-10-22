#include "display.h"

#include <string.h>

#include <pspdisplay.h>
#include <pspkernel.h>

#include "config.h"
#include "platform.h"

static uint32_t display_buffer[PSP_DISPLAY_BUFFER_WIDTH * PSP_SCREEN_HEIGHT]
	__attribute__((aligned(64)));

/**
 * Configure the display backend and clear its initial frame.
 */
bool display_initialize(void)
{
	memset(display_buffer, 0,
		   sizeof(display_buffer));

	if (sceDisplaySetMode(0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT) < 0)
	{
		platform_log("Could not set display mode.\n");
		return false;
	}

	return true;
}

/** Release display resources owned by this backend. */
void display_shutdown(void)
{
	sceDisplaySetFrameBuf(NULL, 0, PSP_DISPLAY_PIXEL_FORMAT_8888,
						  PSP_DISPLAY_SETBUF_IMMEDIATE);
}

/**
 * Copy a tightly packed RGBA8888 image into the PSP display buffer.
 */
bool display_present_rgba8888(const uint32_t *pixels, size_t width,
							  size_t height)
{
	size_t y;

	if (pixels == NULL || width != PSP_SCREEN_WIDTH ||
		height != PSP_SCREEN_HEIGHT)
	{
		return false;
	}

	for (y = 0; y < height; ++y)
	{
		memcpy(&display_buffer[y * PSP_DISPLAY_BUFFER_WIDTH],
			   &pixels[y * width], width * sizeof(*pixels));
	}

	sceKernelDcacheWritebackAll();
	if (sceDisplaySetFrameBuf(display_buffer, PSP_DISPLAY_BUFFER_WIDTH,
							  PSP_DISPLAY_PIXEL_FORMAT_8888,
							  PSP_DISPLAY_SETBUF_NEXTVSYNC) < 0)
	{
		platform_log("Could not submit display frame.\n");
		return false;
	}

	return true;
}

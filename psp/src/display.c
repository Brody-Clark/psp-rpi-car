#include "display.h"

#include <string.h>

#include <pspdisplay.h>
#include <pspgu.h>
#include <pspkernel.h>

#include "config.h"
#include "platform.h"

#define PSP_GU_COMMAND_LIST_WORDS 262144 // 256 KB

static unsigned int gu_command_list[PSP_GU_COMMAND_LIST_WORDS]
	__attribute__((aligned(16)));
static uint32_t display_buffer[PSP_DISPLAY_BUFFER_WIDTH * PSP_SCREEN_HEIGHT]
	__attribute__((aligned(64)));
static bool gu_initialized;

/**
 * Configure the display backend and clear its initial frame.
 */
bool display_initialize(void)
{
	int result;

	if (sceDisplaySetMode(0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT) < 0)
	{
		platform_log("Could not set display mode.\n");
		return false;
	}

	memset(display_buffer, 0, sizeof(display_buffer));

	result = sceGuInit();
	if (result < 0)
	{
		platform_log("Could not initialize the PSP GU (%d).\n", result);
		return false;
	}
	gu_initialized = true;

	sceGuStart(GU_DIRECT, gu_command_list);
	sceGuDrawBuffer(GU_PSM_8888, (void *)0, PSP_DISPLAY_BUFFER_WIDTH);
	sceGuDispBuffer(PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT,
					(void *)(uintptr_t)PSP_GU_FRAME_BYTES,
					PSP_DISPLAY_BUFFER_WIDTH);
	sceGuDepthBuffer((void *)(uintptr_t)(PSP_GU_FRAME_BYTES * 2),
					 PSP_DISPLAY_BUFFER_WIDTH);
	sceGuOffset(2048 - (PSP_SCREEN_WIDTH / 2),
				2048 - (PSP_SCREEN_HEIGHT / 2));
	sceGuViewport(2048, 2048, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT);
	sceGuScissor(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuFinish();
	sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
	sceDisplayWaitVblankStart();
	sceGuDisplay(GU_TRUE);

	return true;
}

/** Release display resources owned by this backend. */
void display_shutdown(void)
{
	if (gu_initialized)
	{
		sceGuDisplay(GU_FALSE);
		sceGuTerm();
		gu_initialized = false;
	}
}

bool display_begin_utility_frame(void)
{
	if (!gu_initialized || sceGuStart(GU_DIRECT, gu_command_list) < 0)
	{
		return false;
	}

	sceGuClearColor(0xff000000);
	sceGuClearDepth(0);
	sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
	sceGuFinish();
	sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);

	return true;
}

void display_end_utility_frame(void)
{
	sceDisplayWaitVblankStart();
	sceGuSwapBuffers();
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

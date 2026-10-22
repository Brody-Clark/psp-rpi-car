#include "video.h"
#include <stdint.h>
#include "../include/display.h"

#define BUF_WIDTH (512)

static u8 g_decodeBuf[480 * 272 * 4] __attribute__((aligned(64)));
static u8 g_displayBuf[512 * 272 * 4] __attribute__((aligned(64)));




void init_video()
{
    sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
	sceUtilityLoadNetModule(PSP_NET_MODULE_INET);
	sceUtilityLoadAvModule(PSP_AV_MODULE_AVCODEC);
	sceUtilityLoadAvModule(PSP_AV_MODULE_MPEGBASE);

	int ret = sceMpegInit();
	if (ret != 0)
	{
		printf("FAILED to load mpeg\n");
	}
	else
	{
		printf("Mpeg module init succeeded.\n");
	}

	ret = sceJpegInitMJpeg();
	if (ret != 0)
	{
		printf("FAILED to load mjpeg\n");
	}
	else
	{
		printf("MJpeg module init succeeded\n");
	}

	ret =  sceJpegCreateMJpeg(SCR_WIDTH, SCR_HEIGHT);
	if(ret < 0)
	{
		printf("Failed creat mjpeg??? 0x%08X\n", ret);
	}
}
void shutdown_video(){
    sceJpegDeleteMJpeg();
	sceJpegFinishMJpeg();
	sceMpegFinish();
}


void draw_jpeg(u8 *jpeg_frame, int size)
{	
	int ret = sceJpegDecodeMJpeg(jpeg_frame, size, g_decodeBuf, 0);
	if (ret < 0)
	{
		printf("JPEG decode failed: 0x%08X\n", ret);
		return;
	}
	
	for (int y = 0; y < 272; y++)
    {
        memcpy(
            g_displayBuf + (y * 512 * 4),
            g_decodeBuf  + (y * 480 * 4),
            480 * 4
        );
    }

	if (sceDisplaySetFrameBuf((void *)g_displayBuf, BUF_WIDTH,
						  PSP_DISPLAY_PIXEL_FORMAT_8888,
						  PSP_DISPLAY_SETBUF_NEXTVSYNC) != 0)
						  {
							printf("Failed to draw display frame buffer.\n");

						  }
}
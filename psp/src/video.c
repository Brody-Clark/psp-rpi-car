#include "video.h"

#include <limits.h>

#include <pspjpeg.h>
#include <psputility.h>
#include <pspmpeg.h>
#include <pspmpegbase.h>

#include "config.h"
#include "platform.h"

static bool jpeg_decoder_initialized;
static bool jpeg_instance_created;

bool video_initialize(void)
{
	int result = sceUtilityLoadAvModule(PSP_AV_MODULE_AVCODEC);
    sceUtilityLoadAvModule(PSP_AV_MODULE_MPEGBASE);

	if (result < 0)
	{
		platform_log("Could not load the PSP AVCODEC module (%d).\n", result);
		return false;
	}

    result = sceMpegInit();
	if (result != 0)
	{
		platform_log("Could not load PSP Mpeg (%d).\n", result);
        return false;
	}

	result = sceJpegInitMJpeg();
	if (result < 0)
	{
		platform_log("Could not initialize the PSP JPEG decoder (%d).\n",
					 result);
		return false;
	}
	jpeg_decoder_initialized = true;

	result = sceJpegCreateMJpeg(PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT);
	if (result < 0)
	{
		platform_log("Could not create the PSP JPEG decoder (%d).\n", result);
		video_shutdown();
		return false;
	}
	jpeg_instance_created = true;

	return true;
}

void video_shutdown(void)
{
	if (jpeg_instance_created)
	{
		sceJpegDeleteMJpeg();
		jpeg_instance_created = false;
	}
	if (jpeg_decoder_initialized)
	{
		sceJpegFinishMJpeg();
		jpeg_decoder_initialized = false;
	}
}

bool video_decode_jpeg(const uint8_t *jpeg_data, size_t jpeg_size,
					   uint32_t *output, size_t output_capacity)
{
	size_t required_pixels = PSP_SCREEN_WIDTH * PSP_SCREEN_HEIGHT;
	int result;

	if (!jpeg_decoder_initialized || jpeg_data == NULL || output == NULL ||
		jpeg_size == 0 || jpeg_size > INT_MAX ||
		output_capacity < required_pixels)
	{
		return false;
	}

	result = sceJpegDecodeMJpeg((u8 *)jpeg_data, (SceSize)jpeg_size,
								(u8 *)output, 0);
	if (result < 0)
	{
		platform_log("JPEG decode failed (%d).\n", result);
		return false;
	}

	return true;
}

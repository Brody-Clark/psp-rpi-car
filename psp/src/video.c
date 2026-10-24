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
static bool decode_error_logged;

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
		if (!decode_error_logged)
		{
			platform_log(
				"Invalid JPEG decode arguments (size=%lu, capacity=%lu).\n",
				(unsigned long)jpeg_size, (unsigned long)output_capacity);
			decode_error_logged = true;
		}
		return false;
	}
	result = sceJpegDecodeMJpeg((u8 *)jpeg_data, (SceSize)jpeg_size,
								(u8 *)output, 0);
	if (result < 0)
	{
		if (!decode_error_logged)
		{
			platform_log(
				"JPEG decode failed (%d), size=%lu, header=%02x %02x %02x "
				"%02x.\n",
				result, (unsigned long)jpeg_size,
				(unsigned int)jpeg_data[0],
				(unsigned int)(jpeg_size > 1 ? jpeg_data[1] : 0),
				(unsigned int)(jpeg_size > 2 ? jpeg_data[2] : 0),
				(unsigned int)(jpeg_size > 3 ? jpeg_data[3] : 0));
			decode_error_logged = true;
		}
		return false;
	}

	decode_error_logged = false;
	return true;
}

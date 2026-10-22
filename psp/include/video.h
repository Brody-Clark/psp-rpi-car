#ifndef PSP_VIDEO_H
#define PSP_VIDEO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Initialize the PSP JPEG decoder.
 *
 * @return true when the decoder is ready.
 */
bool video_initialize(void);

/** Release JPEG decoder resources. */
void video_shutdown(void);

/**
 * Decode a JPEG frame into tightly packed RGBA8888 pixels.
 *
 * @param jpeg_data Encoded JPEG payload.
 * @param jpeg_size Payload size in bytes.
 * @param output Destination pixel buffer.
 * @param output_capacity Capacity of output, in pixels.
 * @return true when decoding succeeds.
 */
bool video_decode_jpeg(const uint8_t *jpeg_data, size_t jpeg_size,
					   uint32_t *output, size_t output_capacity);

#endif

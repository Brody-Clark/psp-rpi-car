#ifndef PSP_DISPLAY_H
#define PSP_DISPLAY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Initialize the PSP display backend.
 *
 * @return true when the display is ready.
 */
bool display_initialize(void);

/** Release resources owned by the display backend. */
void display_shutdown(void);

/**
 * Present a tightly packed RGBA8888 image.
 *
 * @param pixels Image pixels.
 * @param width Image width in pixels.
 * @param height Image height in pixels.
 * @return true when the image was submitted for display.
 */
bool display_present_rgba8888(const uint32_t *pixels, size_t width,
							  size_t height);

#endif

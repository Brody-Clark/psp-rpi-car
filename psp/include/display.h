#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>

static inline uint32_t ARGB(uint8_t a, uint8_t r, uint8_t g, uint8_t b)
{
	return ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static inline uint32_t COLOR_BLACK(ARGB(255,0,0,0));
static inline uint32_t COLOR_WHITE(ARGB(255,255,255,255));

void init_display();

void shutdown_display();

void draw_start();

void draw_end();


#ifndef PSP_PROTOCOL_H
#define PSP_PROTOCOL_H

#include <stdint.h>

/**
 * Controller packet sent to the server.
 *
 * The packed layout is part of the current wire protocol: buttons are a
 * little-endian 32-bit value followed by the analog X and Y bytes.
 */
typedef struct __attribute__((packed))
{
	uint32_t buttons;
	uint8_t analog_x;
	uint8_t analog_y;
} psp_command_t;

_Static_assert(sizeof(psp_command_t) == 6,
			   "Controller command must remain six bytes on the wire");

#endif

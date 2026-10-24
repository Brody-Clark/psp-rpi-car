#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "protocol.h"

int main(void)
{
	const psp_command_t command = {
		.buttons = UINT32_C(0x00005020),
		.analog_x = 121,
		.analog_y = 132,
	};
	const uint8_t expected[6] = {0x20, 0x50, 0x00, 0x00, 121, 132};
	uint8_t packet[sizeof(command)];

	if (sizeof(command) != sizeof(expected) ||
		offsetof(psp_command_t, buttons) != 0 ||
		offsetof(psp_command_t, analog_x) != 4 ||
		offsetof(psp_command_t, analog_y) != 5)
	{
		return 1;
	}

	memcpy(packet, &command, sizeof(packet));
	return memcmp(packet, expected, sizeof(expected)) == 0 ? 0 : 1;
}

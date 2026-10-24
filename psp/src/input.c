#include "input.h"

#include <stddef.h>

#include <pspctrl.h>

bool input_initialize(void)
{
	if (sceCtrlSetSamplingCycle(0) < 0)
	{
		return false;
	}

	return sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG) >= 0;
}

bool input_read_command(psp_command_t *command)
{
	SceCtrlData pad;

	if (command == NULL)
	{
		return false;
	}

	if (sceCtrlReadBufferPositive(&pad, 1) < 1)
	{
		return false;
	}
	command->buttons = pad.Buttons;
	command->analog_x = pad.Lx;
	command->analog_y = pad.Ly;

	return true;
}

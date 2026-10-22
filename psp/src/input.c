#include "input.h"

PSPCommand convertPadToStruct(SceCtrlData pad)
{
	PSPCommand cmd;
	cmd.buttons = pad.Buttons;
	cmd.lx = pad.Lx;
	cmd.ly = pad.Ly;
	return cmd;
}

PSPCommand get_inputs(){
    sceCtrlReadBufferPositive(&pad, 1);
	return convertPadToStruct(pad);
}
#include "protocol.h"

bool init_inputs(){
    sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
}

PSPCommand get_inputs();

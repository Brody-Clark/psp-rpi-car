#include <pspkernel.h>

#include "app.h"

PSP_MODULE_INFO("PSP Raspberry Pi Car", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);
PSP_HEAP_SIZE_KB(8 * 1024);

int main(void)
{
	return app_run();
}

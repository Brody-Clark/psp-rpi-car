#include "platform.h"

#include <stdarg.h>
#include <stdio.h>

#include <pspdebug.h>
#include <pspkernel.h>

static volatile int application_running;
static volatile int callback_thread_status;

static int exit_callback(int argument_1, int argument_2, void *common)
{
	(void)argument_1;
	(void)argument_2;
	(void)common;
	application_running = 0;
	return 0;
}

static int callback_thread(SceSize argument_size, void *argument)
{
	SceUID callback_id;

	(void)argument_size;
	(void)argument;

	callback_id = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
	if (callback_id < 0)
	{
		callback_thread_status = -1;
		application_running = 0;
		return callback_id;
	}
	if (sceKernelRegisterExitCallback(callback_id) < 0)
	{
		sceKernelDeleteCallback(callback_id);
		callback_thread_status = -1;
		application_running = 0;
		return -1;
	}

	callback_thread_status = 1;
	sceKernelSleepThreadCB();
	sceKernelDeleteCallback(callback_id);
	return 0;
}

void platform_wait_s(uint8_t seconds)
{
	sceKernelDelayThread((int)seconds * 1000000);
}

bool platform_initialize(void)
{
	SceUID callback_thread_id;

	pspDebugScreenInit();
	pspDebugScreenSetXY(0, 0);
	pspDebugScreenClear();
	application_running = 1;
	callback_thread_status = 0;

	callback_thread_id = sceKernelCreateThread(
		"exit_callback_thread", callback_thread, 0x11, 0xFA0, 0, NULL);
	if (callback_thread_id < 0)
	{
		platform_log("Could not create exit callback thread (%d).\n",
					 callback_thread_id);
		return false;
	}

	if (sceKernelStartThread(callback_thread_id, 0, NULL) < 0)
	{
		platform_log("Could not start exit callback thread.\n");
		sceKernelDeleteThread(callback_thread_id);
		return false;
	}

	for (int attempt = 0; attempt < 100 && callback_thread_status == 0;
		 ++attempt)
	{
		sceKernelDelayThread(10000);
	}
	if (callback_thread_status != 1)
	{
		platform_log("Could not register the PSP exit callback.\n");
		application_running = 0;
		SceUInt timeout = 100000;
		if (sceKernelWaitThreadEnd(callback_thread_id, &timeout) < 0)
		{
			sceKernelTerminateDeleteThread(callback_thread_id);
		}
		else
		{
			sceKernelDeleteThread(callback_thread_id);
		}
		return false;
	}

	return true;
}

bool platform_is_running(void)
{
	return application_running != 0;
}

void platform_request_exit(void)
{
	application_running = 0;
}

void platform_shutdown(void)
{
	application_running = 0;
	sceKernelExitGame();
}

void platform_clear_console(void)
{
	pspDebugScreenSetXY(0, 0);
	pspDebugScreenClear();
}

void platform_log(const char *format, ...)
{
	char message[512];
	va_list arguments;

	va_start(arguments, format);
	vsnprintf(message, sizeof(message), format, arguments);
	va_end(arguments);

	pspDebugScreenPrintf("%s", message);
}

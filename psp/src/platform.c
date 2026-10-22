#include "platform.h"

#include <stdarg.h>
#include <stdio.h>

#include <pspdebug.h>
#include <pspkernel.h>

static volatile int application_running;
static SceUID callback_thread_id = -1;
static SceUID exit_callback_id = -1;

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
	(void)argument_size;
	(void)argument;
	sceKernelSleepThreadCB();
	return 0;
}

bool platform_initialize(void)
{
	pspDebugScreenInit();
	pspDebugScreenSetXY(0, 0);
	pspDebugScreenClear();
	application_running = 1;

	exit_callback_id = sceKernelCreateCallback(
		"Exit Callback", exit_callback, NULL);
	if (exit_callback_id < 0)
	{
		platform_log("Could not create exit callback (%d).\n",
					 exit_callback_id);
		return false;
	}
	if (sceKernelRegisterExitCallback(exit_callback_id) < 0)
	{
		platform_log("Could not register exit callback.\n");
		sceKernelDeleteCallback(exit_callback_id);
		exit_callback_id = -1;
		return false;
	}

	callback_thread_id = sceKernelCreateThread(
		"exit_callback_thread", callback_thread, 0x11, 0xFA0, 0, NULL);
	if (callback_thread_id < 0)
	{
		platform_log("Could not create exit callback thread (%d).\n",
					 callback_thread_id);
		sceKernelDeleteCallback(exit_callback_id);
		exit_callback_id = -1;
		return false;
	}

	if (sceKernelStartThread(callback_thread_id, 0, NULL) < 0)
	{
		platform_log("Could not start exit callback thread.\n");
		sceKernelDeleteThread(callback_thread_id);
		callback_thread_id = -1;
		sceKernelDeleteCallback(exit_callback_id);
		exit_callback_id = -1;
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
	if (callback_thread_id >= 0)
	{
		sceKernelTerminateDeleteThread(callback_thread_id);
		callback_thread_id = -1;
	}
	if (exit_callback_id >= 0)
	{
		sceKernelDeleteCallback(exit_callback_id);
		exit_callback_id = -1;
	}
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

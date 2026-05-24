#include <pspkernel.h>
#include <pspsysmem.h>
#include <pspdebug.h>
#include <pspdisplay.h>


#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdatomic.h>
#include <stdbool.h>

#define printf pspDebugScreenPrintf

#define SERVER_IP "192.168.4.1" // Change to your Pi's IP
#define SERVER_PORT 12345
// Thread priorities (lower number = higher priority)
#define THREAD_PRIORITY_NET_RECEIVE 0x18
#define THREAD_PRIORITY_NET_SEND 0x19
#define THREAD_PRIORITY_MAIN 0x20
#define FRAME_BUF_SIZE 512				  // 512
#define MAX_FRAME (FRAME_BUF_SIZE * 1024) // KB

#define BUF_WIDTH (FRAME_BUF_SIZE)
#define SCR_WIDTH (480)
#define SCR_HEIGHT (272)
#define PIXEL_SIZE (4)
#define FRAME_SIZE (BUF_WIDTH * SCR_HEIGHT * PIXEL_SIZE)
#define STREAM_FRAME_SIZE (320 * 180 * 2)


PSP_MODULE_INFO("Test", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(8*1024); // This is weird, number here is not freed memory for this app. That is MAX - this # = available heap

static int running = 1;
SceUID g_recvThid = -1;
SceUID g_sendThid = -1;
static int g_serverSocket = -1;
// static unsigned int __attribute__((aligned(16))) list[262144]; // 262144


/* Exit callback */
int exit_callback(int arg1, int arg2, void *common)
{
	running = 0;
	return 0;
}

/* Callback thread */
int CallbackThread(SceSize args, void *argp)
{
	int cbid;
	cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
	sceKernelRegisterExitCallback(cbid);
	sceKernelSleepThreadCB();

	return 0;
}

/* Sets up the callback thread and returns its thread id */
int SetupCallbacks(void)
{
	int thid = 0;
	thid = sceKernelCreateThread("update_thread", CallbackThread, 0x11, 0xFA0, 0, 0);

	if (thid >= 0)
		sceKernelStartThread(thid, 0, 0);

	return thid;
}




void printAvailableMemory(SceUID thid)
{
	int freesize = sceKernelGetThreadStackFreeSize(thid);
	printf("free size on thread id %i : %i\n", thid, freesize);
	SceSize size = sceKernelTotalFreeMemSize();
	printf("HEAP: %i\n", size);
	size = sceKernelMaxFreeMemSize();
	printf("LARGEST FREE MEM BLOCK: %i\n", size);

}


/* main routine */
int main(void)
{
	SetupCallbacks();

	pspDebugScreenInit();
	pspDebugScreenSetXY(0, 0);
	pspDebugScreenClear();

	
	SceUID thid = sceKernelGetThreadId();

	printAvailableMemory(thid);
	
	sceKernelDelayThread(20000000);


	printf("Shutting down...");
	sceKernelDelayThread(3000000);

	return 0;
}
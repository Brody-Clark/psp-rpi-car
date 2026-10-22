#include "./include/network.h"
#include "./include/protocol.h"
#include "./include/video.h"
#include "./include/config.h"
#include "./include/platform.h"
#include <pspkernel.h>
#include <pspsysmem.h>
#include <pspdebug.h>

#include <pspctrl.h>
#include <psputility.h>



#include <psptypes.h>


#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdatomic.h>
#include <stdbool.h>


// Thread priorities (lower number = higher priority)
#define THREAD_PRIORITY_NET_RECEIVE 0x18
#define THREAD_PRIORITY_NET_SEND 0x19
#define THREAD_PRIORITY_MAIN 0x20
// #define FRAME_BUF_SIZE 512				  // 512
// #define MAX_FRAME (FRAME_BUF_SIZE * 1024) // KB



#define STREAM_FRAME_SIZE (FRAME_SIZE)


PSP_HEAP_SIZE_KB(8 * 1024);


atomic_int g_activeFrameIndex = -1; // -1 = no frame ready
SceUID g_frameSema;					// semaphore to signal frame ready
volatile PSPCommand g_latestInput;	// updated by main, sent by TX

PSP_MODULE_INFO("Hello World", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);

static int running = 1;
SceUID g_recvThid = -1;
SceUID g_sendThid = -1;
static int g_serverSocket = -1;

int exit_callback(int arg1, int arg2, void *common)
{
	running = 0;
	return 0;
}

int CallbackThread(SceSize args, void *argp)
{
	int cbid;
	cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
	sceKernelRegisterExitCallback(cbid);
	sceKernelSleepThreadCB();

	return 0;
}

int SetupCallbacks(void)
{
	int thid = 0;
	thid = sceKernelCreateThread("update_thread", CallbackThread, 0x11, 0xFA0, 0, 0);

	if (thid >= 0)
		sceKernelStartThread(thid, 0, 0);

	return thid;
}


PSPCommand convertPadToStruct(SceCtrlData pad)
{
	PSPCommand cmd;
	cmd.buttons = pad.Buttons;
	cmd.lx = pad.Lx;
	cmd.ly = pad.Ly;
	return cmd;
}



int receiveThread(SceSize args, void *argp)
{
	while (running)
	{
		FrameBuffer *buf = get_back(frame_buffer);

		int size = readFrame(g_serverSocket, buf->data, STREAM_FRAME_SIZE);
		if (size > 0)
		{
			buf->size = size;
			swap_buffers(frame_buffer);
			sceKernelSignalSema(g_frameSema, 1);
		}
		else
		{
			printf("Failed to receive frame or connection closed\n");
			sceKernelDelayThread(3000000);
			sceKernelExitDeleteThread(0);
			break;
		}
	}
	sceKernelExitDeleteThread(0);
	return 0;
}

int sendThread(SceSize args, void *argp)
{
	size_t bytes_sent = -1;
	while (running)
	{

		bytes_sent = sceNetInetSend(g_serverSocket, (const void *)&g_latestInput, sizeof(g_latestInput), 0);
		if (bytes_sent <= 0)
		{
			int err = sceNetInetGetErrno();
			printf("Net send error: %i\n", err);
			break;
		}
		sceKernelDelayThread(16000); // ~60 Hz
	}
	sceKernelExitDeleteThread(0);
	return 0;
}



void shutdown(){
	shutdown_client();
	shutdown_display();
	shutdown_networking();
	shutdown_video();

	sceKernelExitGame();
}

int main(void)
{
	SetupCallbacks();
	
	debug_clear_screen();

	// Initialize frame semaphore
	g_frameSema = sceKernelCreateSema("frame_sema", 0, 0, 1, NULL);

	printf("Loading modules...\n");
	init_display();
	init_networking();
	init_video();
	init_inputs();

	printf("Connecting to AP...\n");

	if (!connectToAccessPoint())
	{
		printf("Failed to connect\n");
		shutdown();
		return 0;
	}

	debug_clear_screen();
	
	printf("Connected to server\n");

	g_latestInput.buttons = 0;
	g_latestInput.lx = 0;
	g_latestInput.ly = 0;
	
	printf("Started send thread\n");
	printf("Setup complete, entering main loop...\n");
	sceKernelDelayThread(MICROSEC_TO_SEC(1));
	
	debug_clear_screen();

	SceCtrlData pad;
	int ap_state = -1;

	FrameDoubleBuffer frame_buf;
	while (running)
	{
		sceCtrlReadBufferPositive(&pad, 1);
		g_latestInput = convertPadToStruct(pad);

		draw_start();
		get_ap_status(&ap_state);
		if (ap_state < 0)
		{
			printf("Failed to get AP ctl status.\n");
			running = false;
			break;
		}
		if (ap_state == PSP_NET_APCTL_STATE_DISCONNECTED)
		{
			printf("AP Disconnected...Exiting.\n");
			running = false;
			break;
		}

		//pspDebugScreenSetXY(0, 0);
		// printf("buttons: %u, lx: %u, ly: %u\n", g_latestInput.buttons, g_latestInput.lx, g_latestInput.ly);

		// Try to get a frame
		if (sceKernelPollSema(g_frameSema, 1) == 0 && g_activeFrameIndex >= 0)
		{
			FrameBuffer* buf = get_front(frame_buf)
			if (buf != nullptr)
			{
				draw_jpeg(buf->data, buf->size);
				swap_buffers(frame_buf);
			}
		}

		draw_end();
	}

	draw_end();
	printf("Shutting down...");
	shutdown();

	return 0;
}
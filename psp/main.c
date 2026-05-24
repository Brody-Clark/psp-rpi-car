#include <pspkernel.h>
#include <pspsysmem.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <psputility.h>
#include <pspnet.h>
#include <pspnet_apctl.h>
#include <pspmpeg.h>
#include <pspmpegbase.h>
#include <pspjpeg.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <pspnet_resolver.h>
#include <pspnet_inet.h>
#include <psptypes.h>
#include <pspgu.h>
#include <pspgum.h>

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
#define FRAME_SIZE (SCR_WIDTH * SCR_HEIGHT * PIXEL_SIZE)
#define STREAM_FRAME_SIZE (FRAME_SIZE)
#define MICROSEC_TO_SEC(s) s * 1000000

PSP_HEAP_SIZE_KB(8 * 1024);

typedef struct __attribute__((packed))
{
	u32 buttons;
	u8 lx;
	u8 ly;
} PSPCommand;

typedef struct
{
	int index;
	int size;
	u8 data[FRAME_SIZE];
} FrameBuffer;

// Buffers
volatile FrameBuffer frameBuffers[2]; // double-buffer for video
static u8 g_decodeBuf[480 * 272 * 4] __attribute__((aligned(64)));
static u8 g_displayBuf[512 * 272 * 4] __attribute__((aligned(64)));
atomic_int g_activeFrameIndex = -1; // -1 = no frame ready
SceUID g_frameSema;					// semaphore to signal frame ready
volatile PSPCommand g_latestInput;	// updated by main, sent by TX

PSP_MODULE_INFO("Hello World", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);

static int running = 1;
SceUID g_recvThid = -1;
SceUID g_sendThid = -1;
static int g_serverSocket = -1;
static unsigned int __attribute__((aligned(16))) list[262144]; // 262144

static inline uint32_t ARGB(uint8_t a, uint8_t r, uint8_t g, uint8_t b)
{
	return ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

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

static void setupGu()
{
	sceGuInit();

	sceGuStart(GU_DIRECT, list);
	sceGuDrawBuffer(GU_PSM_8888, (void *)0, BUF_WIDTH);
	sceGuDispBuffer(SCR_WIDTH, SCR_HEIGHT, (void *)FRAME_SIZE, BUF_WIDTH);
	sceGuDepthBuffer((void *)(FRAME_SIZE * 2), BUF_WIDTH);
	sceGuOffset(2048 - (SCR_WIDTH / 2), 2048 - (SCR_HEIGHT / 2));
	sceGuViewport(2048, 2048, SCR_WIDTH, SCR_HEIGHT);
	sceGuDepthRange(0xc350, 0x2710);
	sceGuScissor(0, 0, SCR_WIDTH, SCR_HEIGHT);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuDepthFunc(GU_GEQUAL);
	sceGuEnable(GU_DEPTH_TEST);
	sceGuFrontFace(GU_CW);
	sceGuShadeModel(GU_SMOOTH);
	sceGuEnable(GU_CULL_FACE);
	sceGuEnable(GU_CLIP_PLANES);
	sceGuFinish();
	sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);

	sceDisplayWaitVblankStart();
	sceGuDisplay(GU_TRUE);
}
bool connectToAccessPoint()
{

	int done = 0;

	pspUtilityNetconfData data;

	memset(&data, 0, sizeof(data));
	data.base.size = sizeof(data);
	data.base.language = PSP_SYSTEMPARAM_LANGUAGE_ENGLISH;
	data.base.buttonSwap = PSP_UTILITY_ACCEPT_CROSS;
	data.base.graphicsThread = 17;
	data.base.accessThread = 19;
	data.base.fontThread = 18;
	data.base.soundThread = 16;
	data.action = PSP_NETCONF_ACTION_CONNECTAP;

	struct pspUtilityNetconfAdhoc adhocparam;
	memset(&adhocparam, 0, sizeof(adhocparam));
	data.adhocparam = &adhocparam;

	sceUtilityNetconfInitStart(&data);

	while (running)
	{
		sceGuStart(GU_DIRECT, list);
		sceGuClearColor(0xff554433);
		sceGuClearDepth(0);
		sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
		sceGuFinish();
		sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
		switch (sceUtilityNetconfGetStatus())
		{
		case PSP_UTILITY_DIALOG_NONE:
			break;

		case PSP_UTILITY_DIALOG_VISIBLE:
			sceUtilityNetconfUpdate(1);
			break;

		case PSP_UTILITY_DIALOG_QUIT:
			sceUtilityNetconfShutdownStart();
			break;

		case PSP_UTILITY_DIALOG_FINISHED:
			done = 1;
			break;

		default:
			break;
		}

		sceDisplayWaitVblankStart();
		sceGuSwapBuffers();

		if (done)
		{
			printf("Connected to AP\n");
			sceGuStart(GU_DIRECT, list);
			sceGuClearColor(ARGB(255, 0, 0, 0));
			sceGuClearDepth(0);
			sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
			sceGuFinish();
			sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
			sceDisplayWaitVblankStart();
			sceGuSwapBuffers();
			
			return true;
		}
	}

	return false;
}

void netInit(void)
{
	printf("Initializing network...\n");
	sceNetInit(128 * 1024, 42, 4 * 1024, 42, 4 * 1024);

	sceNetInetInit();

	sceNetApctlInit(0x8000, 48);
	printf("Network initialized\n");
}

void netTerm(void)
{
	sceNetApctlTerm();

	sceNetInetTerm();

	sceNetTerm();
}

PSPCommand convertPadToStruct(SceCtrlData pad)
{
	PSPCommand cmd;
	cmd.buttons = pad.Buttons;
	cmd.lx = pad.Lx;
	cmd.ly = pad.Ly;
	return cmd;
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
bool connectToServer(const char *server_ip, int server_port, int *out_socket)
{
	struct sockaddr_in server_addr;
	int sock;

	sock = sceNetInetSocket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
	{
		printf("Failed to create socket\n");
		return false;
	}

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(server_port);
	server_addr.sin_addr.s_addr = inet_addr(server_ip);

	printf("Connecting to server %s:%d...\n", server_ip, server_port);
	if (sceNetInetConnect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
	{
		printf("Failed to connect to server\n");
		int err = sceNetInetGetErrno();
		printf("...errno: %d\n", err);
		sceNetInetClose(sock);
		return false;
	}

	int flag = 1;
	sceNetInetSetsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

	*out_socket = sock;
	return true;
}

volatile FrameBuffer *getInactiveBuffer()
{
	int inactiveIndex = (atomic_load_explicit(&g_activeFrameIndex, memory_order_acquire) == 0) ? 1 : 0;
	return &frameBuffers[inactiveIndex];
}

static int recvAll(int socket, char *buf, int len)
{
	int total = 0;
	while (total < len)
	{
		int r = sceNetInetRecv(socket, buf + total, len - total, 0);
		if (r <= 0)
			return r;
		total += r;
	}
	return total;
}

int readFrame(int socket, u8 *buffer, int maxSize)
{
	uint32_t netFrameSize;
	if (recvAll(socket, (char *)&netFrameSize, sizeof(netFrameSize)) != sizeof(netFrameSize))
	{
		printf("recvAll not equal to expected\n");
		return -1;
	}

	int frameSize = ntohl(netFrameSize);
	if (frameSize <= 0 || frameSize > maxSize)
	{
		printf("Frame size %d exceeds buffer size %d\n", frameSize, maxSize);
		return -1;
	}

	if (recvAll(socket, (char *)buffer, frameSize) != frameSize)
	{
		printf("Error, did not receive right frame size: %i \n", frameSize);
		return -1;
	}
	return frameSize;
}

int receiveThread(SceSize args, void *argp)
{
	while (running)
	{
		int inactiveIndex = (atomic_load_explicit(&g_activeFrameIndex, memory_order_acquire) == 0) ? 1 : 0;
		FrameBuffer *buf = &frameBuffers[inactiveIndex];

		int size = readFrame(g_serverSocket, buf->data, STREAM_FRAME_SIZE);
		if (size > 0)
		{
			// printf("received frame size: %i\n", size);
			buf->size = size;
			atomic_store_explicit(&g_activeFrameIndex, inactiveIndex, memory_order_release);
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

void blitFrame(u8 *frameData, int size)
{	
	int ret = sceJpegDecodeMJpeg(frameData, size, g_decodeBuf, 0);
	if (ret < 0)
	{
		printf("JPEG decode failed: 0x%08X\n", ret);
		return;
	}
	
	for (int y = 0; y < 272; y++)
    {
        memcpy(
            g_displayBuf + (y * 512 * 4),
            g_decodeBuf  + (y * 480 * 4),
            480 * 4
        );
    }

    sceDisplayWaitVblankStart();

	if (sceDisplaySetFrameBuf((void *)g_displayBuf, BUF_WIDTH,
						  PSP_DISPLAY_PIXEL_FORMAT_8888,
						  PSP_DISPLAY_SETBUF_NEXTVSYNC) != 0)
						  {
							printf("Failed to draw display frame buffer.\n");

						  }
}

void shutdown_client()
{
	if (g_recvThid > 0)
	{
		sceKernelWaitThreadEnd(g_recvThid, 0);
		sceKernelDeleteThread(g_recvThid);
	}

	if (g_sendThid > 0)
	{
		sceKernelWaitThreadEnd(g_sendThid, 0);
		sceKernelDeleteThread(g_sendThid);
	}

	if (g_serverSocket > 0)
	{
		sceNetInetClose(g_serverSocket);
	}
	sceNetInetShutdown(g_serverSocket, 2); // 2 is for both send and receive
	netTerm();
	
	sceJpegDeleteMJpeg();
	sceJpegFinishMJpeg();
	sceMpegFinish();

	sceGuTerm();

	sceKernelExitGame();
}

void dumpThreadStatus(SceUID thid)
{
	SceKernelThreadInfo status;
	int ret;

	memset(&status, 0, sizeof(SceKernelThreadInfo));
	printf("Thread ID: %08X\n", thid);
	status.size = sizeof(SceKernelThreadInfo);
	ret = sceKernelReferThreadStatus(thid, &status);
	printf("Get Thread Status: %08X\n", ret);
	if (ret == 0)
	{
		printf("Name: %s\n", status.name);
		printf("Thread Addr: %p\n", status.entry);
		printf("Stack Addr: %p\n", status.stack);
		printf("Stack Size: %08X\n", status.stackSize);
		printf("gp: %p\n", status.gpReg);
		printf("Initial Pri: %x\n", status.initPriority);
		printf("Current Pri: %x\n", status.currentPriority);
	}
}

/* main routine */
int main(void)
{
	SetupCallbacks();
	setupGu();
	pspDebugScreenInit();
	pspDebugScreenSetXY(0, 0);
	pspDebugScreenClear();

	// Initialize frame semaphore
	g_frameSema = sceKernelCreateSema("frame_sema", 0, 0, 1, NULL);

	// SceUID thid = sceKernelGetThreadId();
	// printAvailableMemory(thid);

	// #if defined(_PSP_FW_VERSION) && _PSP_FW_VERSION >= 200
	sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
	sceUtilityLoadNetModule(PSP_NET_MODULE_INET);
	// printAvailableMemory(thid);
	sceUtilityLoadAvModule(PSP_AV_MODULE_AVCODEC);
	sceUtilityLoadAvModule(PSP_AV_MODULE_MPEGBASE);
	
	netInit();
	sceKernelDelayThread(2000000);

	printf("Loading modules...\n");

	int ret = sceMpegInit();
	if (ret != 0)
	{
		printf("FAILED to load mpeg\n");
	}
	else
	{
		printf("Mpeg module init succeeded.\n");
	}

	ret = sceJpegInitMJpeg();
	if (ret != 0)
	{
		printf("FAILED to load mjpeg\n");
	}
	else
	{
		printf("MJpeg module init succeeded\n");
	}

	ret =  sceJpegCreateMJpeg(SCR_WIDTH, SCR_HEIGHT);
	if(ret < 0)
	{
		printf("Failed creat mjpeg??? 0x%08X\n", ret);
	}

	sceDisplaySetMode(0, SCR_WIDTH, SCR_HEIGHT);

	printf("Connecting to AP...\n");
	sceKernelDelayThread(2000000);

	// Sign in to an access point via dialog
	if (!connectToAccessPoint())
	{
		printf("Failed to connect\n");
		shutdown_client();
		return 0;
	}

	pspDebugScreenInit();
	pspDebugScreenSetXY(0, 0);
	pspDebugScreenClear();

	// Check connection status first
	int state;
	sceNetApctlGetState(&state);

	uint8_t retry = 0;
	while (true)
	{
		if (!connectToServer(SERVER_IP, SERVER_PORT, &g_serverSocket))
		{
			printf("Failed to connect to server...Retrying...\n");

			sceKernelDelayThread(3000000);
			retry++;
			if (retry > 10)
			{
				printf("Failed to connect to server...Aborting...\n");
				sceKernelDelayThread(3000000);
				shutdown_client();
				return 0;
			}
		}
		else
		{
			break;
		}
	}
	printf("Connected to server\n");

	// if (!allocBuffers())
	// {
	// 	printf("Failed to allocate frame buffers\n");
	// 	sceKernelDelayThread(5000000);
	// 	shutdown_client();
	// 	return 0;
	// }

	// Initialize controller
	sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

	g_latestInput.buttons = 0;
	g_latestInput.lx = 0;
	g_latestInput.ly = 0;

	// printAvailableMemory(thid);

	// Start the send and receive threads
	g_recvThid = sceKernelCreateThread("receive_thread", receiveThread, THREAD_PRIORITY_NET_RECEIVE, 0x10000, PSP_THREAD_ATTR_USER, NULL);
	if (g_recvThid <= 0)
	{
		printf("Faield to create receive thread...\n");

		printf("ERROR CODE: %i", g_recvThid);
		sceKernelDelayThread(20000000);
		shutdown_client();
		return 0;
	}
	int success = sceKernelStartThread(g_recvThid, 0, NULL);
	if (success < 0)
	{
		printf("Failed to start receive thread...\n");
		printf("ERROR CODE: %i", success);
		sceKernelDelayThread(20000000);
		shutdown_client();
		return 0;
	}
	printf("Started receive thread\n");

	g_sendThid = sceKernelCreateThread("send_thread", sendThread, THREAD_PRIORITY_NET_SEND, 0x10000, PSP_THREAD_ATTR_USER, NULL);
	if (g_sendThid <= 0)
	{
		printf("Failed to create send thread...\n");

		printf("ERROR CODE: %i", g_sendThid);
		sceKernelDelayThread(20000000);
		shutdown_client();
		return 0;
	}
	success = sceKernelStartThread(g_sendThid, 0, NULL);
	if (success < 0)
	{
		printf("Failed to start send thread...\n");
		printf("ERROR CODE: %i", success);
		sceKernelDelayThread(20000000);
		shutdown_client();
		return 0;
	}
	printf("Started send thread\n");
	printf("Setup complete, entering main loop...\n");
	sceKernelDelayThread(1000000);

	pspDebugScreenClear();

	SceCtrlData pad;
	int ap_state = -1;

	while (running)
	{
		if (sceNetApctlGetState(&ap_state) < 0)
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

		// Capture inputs
		sceCtrlReadBufferPositive(&pad, 1);
		g_latestInput = convertPadToStruct(pad);

		//pspDebugScreenSetXY(0, 0);
		// printf("buttons: %u, lx: %u, ly: %u\n", g_latestInput.buttons, g_latestInput.lx, g_latestInput.ly);

		// Try to get a frame
		if (sceKernelPollSema(g_frameSema, 1) == 0 && g_activeFrameIndex >= 0)
		{
			int idx = atomic_load_explicit(&g_activeFrameIndex, memory_order_acquire);
			if (idx >= 0)
			{
				FrameBuffer *buf = (FrameBuffer *)&frameBuffers[idx];
				// printf("Blit frame size: %i\n", buf->size);
				blitFrame(buf->data, buf->size);

				atomic_store_explicit(&g_activeFrameIndex, -1, memory_order_release);
			}
		}

	}

	sceDisplayWaitVblankStart();
	sceGuSwapBuffers();
	printf("Shutting down...");
	shutdown_client();

	return 0;
}
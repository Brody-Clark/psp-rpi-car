#include "network.h"

void init_networking(){
	sceNetInit(128 * 1024, 42, 4 * 1024, 42, 4 * 1024);

	sceNetInetInit();

	sceNetApctlInit(0x8000, 48);
}

void shutdown_networking()
{
	sceNetApctlTerm();
	sceNetInetTerm();
	sceNetTerm();
}

bool start_client(){
	g_recvThid = sceKernelCreateThread("receive_thread", receiveThread, THREAD_PRIORITY_NET_RECEIVE, 0x10000, PSP_THREAD_ATTR_USER, NULL);
	if (g_recvThid <= 0)
	{
		printf("Failed to create receive thread...\n");

		printf("ERROR CODE: %i", g_recvThid);
		shutdown_client();
		return 0;
	}
	int success = sceKernelStartThread(g_recvThid, 0, NULL);
	if (success < 0)
	{
		printf("Failed to start receive thread...\n");
		printf("ERROR CODE: %i", success);
		shutdown_client();
		return 0;
	}
	printf("Started receive thread\n");

	g_sendThid = sceKernelCreateThread("send_thread", sendThread, THREAD_PRIORITY_NET_SEND, 0x10000, PSP_THREAD_ATTR_USER, NULL);
	if (g_sendThid <= 0)
	{
		printf("Failed to create send thread...\n");

		printf("ERROR CODE: %i", g_sendThid);
		shutdown_client();
		return 0;
	}
	success = sceKernelStartThread(g_sendThid, 0, NULL);
	if (success < 0)
	{
		printf("Failed to start send thread...\n");
		printf("ERROR CODE: %i", success);
		shutdown_client();
		return 0;
	}
}

void shutdown_client(){
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
		shutdown_networking();
}

bool connect(const char *server_ip, int server_port, int *out_socket){
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


bool connect_to_access_point(){   
	bool done = false;

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

	while (!done)
	{
		draw_start();

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
			done = true;
			break;

		default:
			break;
		}
		draw_end();
		if (done)
		{
			return true;
		}
	}
	return false;
}


bool connectToServer(const char *server_ip, int server_port, int *out_socket, u8 retries=0, u8 wait_sec=0)
{
	for(u8 i=0; i<=retries; i++)
	{
		if (!connect(server_ip, server_port, &out_socket, 10, 3))
		{
			printf("Failed to connect to server...Retrying...\n");
			sceKernelDelayThread(SEC_TO_MICROSEC(wait_sec));
		}
		else
		{
			return true;
		}
	}
}

static int recieve_data(int socket, char *buf, int len)
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

int read_frame(int socket, u8 *buffer, int maxSize)
{
	uint32_t netFrameSize;
	if (recieve_data(socket, (char *)&netFrameSize, sizeof(netFrameSize)) != sizeof(netFrameSize))
	{
		printf("recieved data not equal to expected size\n");
		return -1;
	}

	int frameSize = ntohl(netFrameSize);
	if (frameSize <= 0 || frameSize > maxSize)
	{
		printf("Frame size %d exceeds buffer size %d\n", frameSize, maxSize);
		return -1;
	}

	if (recieve_data(socket, (char *)buffer, frameSize) != frameSize)
	{
		printf("Error, did not receive right frame size: %i \n", frameSize);
		return -1;
	}
	return frameSize;
}

int get_ap_status(int& ap_status){
	return sceNetApctlGetState(ap_status);
}
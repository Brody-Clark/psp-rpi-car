#include "network.h"

#include <limits.h>
#include <string.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pspkernel.h>
#include <pspnet.h>
#include <pspnet_apctl.h>
#include <pspnet_inet.h>
#include <psputility.h>

#include "config.h"
#include "platform.h"

static int server_socket = -1;
static bool net_initialized;
static bool inet_initialized;
static bool apctl_initialized;

static bool receive_exact(uint8_t *buffer, size_t length)
{
	size_t received = 0;

	while (received < length)
	{
		int result = sceNetInetRecv(server_socket, buffer + received,
									(int)(length - received), 0);
		if (result <= 0)
		{
			platform_log("Network receive failed (%d).\n",
						 sceNetInetGetErrno());
			return false;
		}
		received += (size_t)result;
	}

	return true;
}

static bool send_exact(const uint8_t *buffer, size_t length)
{
	size_t sent = 0;

	while (sent < length)
	{
		int result = sceNetInetSend(server_socket, buffer + sent,
									(int)(length - sent), 0);
		if (result <= 0)
		{
			platform_log("Network send failed (%d).\n",
						 sceNetInetGetErrno());
			return false;
		}
		sent += (size_t)result;
	}

	return true;
}

bool network_initialize(void)
{
    sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
	sceUtilityLoadNetModule(PSP_NET_MODULE_INET);
	int result = sceNetInit(128 * 1024, 42, 4 * 1024, 42, 4 * 1024);
	if (result < 0)
	{
		platform_log("sceNetInit failed (%d).\n", result);
		return false;
	}
	net_initialized = true;

	result = sceNetInetInit();
	if (result < 0)
	{
		platform_log("sceNetInetInit failed (%d).\n", result);
		network_shutdown();
		return false;
	}
	inet_initialized = true;

	result = sceNetApctlInit(0x8000, 48);
	if (result < 0)
	{
		platform_log("sceNetApctlInit failed (%d).\n", result);
		network_shutdown();
		return false;
	}
	apctl_initialized = true;

	return true;
}

void network_shutdown(void)
{
	if (server_socket >= 0)
	{
		sceNetInetShutdown(server_socket, 2);
		sceNetInetClose(server_socket);
		server_socket = -1;
	}
	if (apctl_initialized)
	{
		sceNetApctlTerm();
		apctl_initialized = false;
	}
	if (inet_initialized)
	{
		sceNetInetTerm();
		inet_initialized = false;
	}
	if (net_initialized)
	{
		sceNetTerm();
		net_initialized = false;
	}
}

bool network_connect_access_point(void)
{
	pspUtilityNetconfData data;
	struct pspUtilityNetconfAdhoc adhoc_parameter;
	memset(&data, 0, sizeof(data));
	data.base.size = sizeof(data);
	data.base.language = PSP_SYSTEMPARAM_LANGUAGE_ENGLISH;
	data.base.buttonSwap = PSP_UTILITY_ACCEPT_CROSS;
	data.base.graphicsThread = 17;
	data.base.accessThread = 19;
	data.base.fontThread = 18;
	data.base.soundThread = 16;
	data.action = PSP_NETCONF_ACTION_CONNECTAP;
	memset(&adhoc_parameter, 0, sizeof(adhoc_parameter));
	data.adhocparam = &adhoc_parameter;

	if (sceUtilityNetconfInitStart(&data) < 0)
	{
		platform_log("Could not start the network connection dialog.\n");
		return false;
	}

    bool complete = false;
    bool shutdown_started = false;
	while (!complete)
	{
		switch (sceUtilityNetconfGetStatus())
		{
		case PSP_UTILITY_DIALOG_NONE:
			break;
		case PSP_UTILITY_DIALOG_VISIBLE:
			sceUtilityNetconfUpdate(1);
			break;
		case PSP_UTILITY_DIALOG_QUIT:
			if (!shutdown_started)
			{
				sceUtilityNetconfShutdownStart();
				shutdown_started = true;
			}
			break;
		case PSP_UTILITY_DIALOG_FINISHED:
			complete = true;
			break;
		default:
			sceKernelDelayThread(10000);
			break;
		}
		sceKernelDelayThread(10000);
	}

    int access_point_state = PSP_NET_APCTL_STATE_DISCONNECTED;
	return sceNetApctlGetState(&access_point_state) >= 0 &&
		   access_point_state == PSP_NET_APCTL_STATE_GOT_IP;
}

bool network_connect_server(void)
{
	struct sockaddr_in server_address;
	int socket_fd;
	int no_delay = 1;

	if (server_socket >= 0)
	{
		return true;
	}

	socket_fd = sceNetInetSocket(AF_INET, SOCK_STREAM, 0);
	if (socket_fd < 0)
	{
		platform_log("Could not create server socket (%d).\n", socket_fd);
		return false;
	}

	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(PSP_SERVER_PORT);
	server_address.sin_addr.s_addr = inet_addr(PSP_SERVER_IP);

	platform_log("Connecting to %s:%d...\n", PSP_SERVER_IP, PSP_SERVER_PORT);
	if (sceNetInetConnect(socket_fd, (struct sockaddr *)&server_address,
						  sizeof(server_address)) < 0)
	{
		platform_log("Could not connect to server (%d).\n",
					 sceNetInetGetErrno());
		sceNetInetClose(socket_fd);
		return false;
	}

	if (sceNetInetSetsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY,
							 &no_delay, sizeof(no_delay)) < 0)
	{
		platform_log("Could not disable TCP Nagle buffering (%d).\n",
					 sceNetInetGetErrno());
		sceNetInetClose(socket_fd);
		return false;
	}

	server_socket = socket_fd;
	return true;
}

bool network_send_command(const psp_command_t *command)
{
	if (server_socket < 0 || command == NULL)
	{
		return false;
	}

	return send_exact((const uint8_t *)command, sizeof(*command));
}

bool network_receive_frame(uint8_t *buffer, size_t capacity,
						   size_t *frame_size)
{
	uint32_t network_size;
	uint32_t payload_size;

	if (server_socket < 0 || buffer == NULL || frame_size == NULL ||
		capacity == 0 || capacity > INT_MAX)
	{
		return false;
	}

	if (!receive_exact((uint8_t *)&network_size, sizeof(network_size)))
	{
		return false;
	}

	payload_size = ntohl(network_size);
	if (payload_size == 0 || payload_size > capacity)
	{
		platform_log("Invalid JPEG frame size: %lu bytes.\n",
					 (unsigned long)payload_size);
		return false;
	}

	if (!receive_exact(buffer, payload_size))
	{
		return false;
	}

	*frame_size = payload_size;
	return true;
}

bool network_access_point_is_connected(void)
{
	int state = PSP_NET_APCTL_STATE_DISCONNECTED;

	return sceNetApctlGetState(&state) >= 0 &&
		   state == PSP_NET_APCTL_STATE_GOT_IP;
}

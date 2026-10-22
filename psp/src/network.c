#include "network.h"

#include <errno.h>
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
#include "display.h"
#include "platform.h"

#define NETWORK_SEND_INTERVAL_US 16000
#define NETWORK_THREAD_PRIORITY_RECEIVE 0x18
#define NETWORK_THREAD_PRIORITY_SEND 0x19
#define NETWORK_THREAD_STACK_SIZE 0x10000
#define NETWORK_WORKER_JOIN_TIMEOUT_US 250000

static int server_socket = -1;
static bool net_initialized;
static bool inet_initialized;
static bool apctl_initialized;
static bool common_module_loaded;
static bool inet_module_loaded;
static volatile int send_worker_running;
static volatile int receive_worker_running;
static SceUID receive_thread_id = -1;
static SceUID send_thread_id = -1;
static SceUID command_mutex = -1;
static SceUID frame_ready_semaphore = -1;
static SceUID frame_consumed_semaphore = -1;
static psp_command_t latest_command;

typedef struct
{
	size_t size;
	uint8_t data[PSP_MAX_JPEG_BYTES];
} network_frame_t;

static network_frame_t frame_buffers[2] __attribute__((aligned(64)));
static int producer_buffer_index;
static int ready_buffer_index;

#define CONNECT_POLL_INTERVAL_MS 1000

static void stop_workers(void);

static void signal_receive_failure(void)
{
	receive_worker_running = 0;
}

static void signal_send_failure(void)
{
	send_worker_running = 0;
}

static bool receive_exact(uint8_t *buffer, size_t length)
{
	size_t received = 0;

	while (received < length && receive_worker_running)
	{
		int result = sceNetInetRecv(server_socket, buffer + received,
									(int)(length - received), 0);
		if (result <= 0)
		{
			int error = sceNetInetGetErrno();
			if (error == EAGAIN || error == EWOULDBLOCK || error == EINTR)
			{
				continue;
			}
			if (receive_worker_running)
			{
				platform_log("Network receive failed (%d).\n", error);
			}
			return false;
		}
		received += (size_t)result;
	}

	return true;
}

static bool send_exact(const uint8_t *buffer, size_t length)
{
	size_t sent = 0;

	while (sent < length && send_worker_running)
	{
		int result = sceNetInetSend(server_socket, buffer + sent,
									(int)(length - sent), 0);
		if (result <= 0)
		{
			int error = sceNetInetGetErrno();
			if (error == EAGAIN || error == EWOULDBLOCK || error == EINTR)
			{
				continue;
			}
			if (send_worker_running)
			{
				platform_log("Network send failed (%d).\n", error);
			}
			return false;
		}
		sent += (size_t)result;
	}

	return true;
}

static bool receive_frame_payload(uint8_t *buffer, size_t capacity,
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

static int receive_worker(SceSize argument_size, void *argument)
{
	(void)argument_size;
	(void)argument;

	while (receive_worker_running)
	{
		network_frame_t *frame = &frame_buffers[producer_buffer_index];

		if (sceKernelWaitSema(frame_consumed_semaphore, 1, NULL) < 0)
		{
			break;
		}
		if (!receive_worker_running)
		{
			break;
		}

		if (!receive_frame_payload(frame->data, sizeof(frame->data),
								   &frame->size))
		{
			if (receive_worker_running)
			{
				platform_log("Video receive worker stopped.\n");
				signal_receive_failure();
			}
			break;
		}

		ready_buffer_index = producer_buffer_index;
		producer_buffer_index = 1 - producer_buffer_index;
		if (sceKernelSignalSema(frame_ready_semaphore, 1) < 0)
		{
			platform_log("Could not signal the frame-ready semaphore.\n");
			signal_receive_failure();
			break;
		}
	}

	return 0;
}

static int send_worker(SceSize argument_size, void *argument)
{
	(void)argument_size;
	(void)argument;

	while (send_worker_running)
	{
		psp_command_t command;

		if (sceKernelWaitSema(command_mutex, 1, NULL) < 0)
		{
			break;
		}
		command = latest_command;
		sceKernelSignalSema(command_mutex, 1);

		if (!send_exact((const uint8_t *)&command, sizeof(command)))
		{
			if (send_worker_running)
			{
				platform_log("Controller send worker stopped.\n");
				signal_send_failure();
			}
			break;
		}

		sceKernelDelayThread(NETWORK_SEND_INTERVAL_US);
	}

	return 0;
}

static void delete_worker_semaphores(void)
{
	if (command_mutex >= 0)
	{
		sceKernelDeleteSema(command_mutex);
		command_mutex = -1;
	}
	if (frame_ready_semaphore >= 0)
	{
		sceKernelDeleteSema(frame_ready_semaphore);
		frame_ready_semaphore = -1;
	}
	if (frame_consumed_semaphore >= 0)
	{
		sceKernelDeleteSema(frame_consumed_semaphore);
		frame_consumed_semaphore = -1;
	}
}

static void stop_workers(void)
{
	send_worker_running = 0;
	receive_worker_running = 0;

	if (server_socket >= 0)
	{
		sceNetInetShutdown(server_socket, 2);
	}
	if (frame_consumed_semaphore >= 0)
	{
		sceKernelSignalSema(frame_consumed_semaphore, 1);
	}

	if (receive_thread_id >= 0)
	{
		SceUInt timeout = NETWORK_WORKER_JOIN_TIMEOUT_US;
		if (sceKernelWaitThreadEnd(receive_thread_id, &timeout) < 0)
		{
			sceKernelTerminateDeleteThread(receive_thread_id);
		}
		else
		{
			sceKernelDeleteThread(receive_thread_id);
		}
		receive_thread_id = -1;
	}
	if (send_thread_id >= 0)
	{
		SceUInt timeout = NETWORK_WORKER_JOIN_TIMEOUT_US;
		if (sceKernelWaitThreadEnd(send_thread_id, &timeout) < 0)
		{
			sceKernelTerminateDeleteThread(send_thread_id);
		}
		else
		{
			sceKernelDeleteThread(send_thread_id);
		}
		send_thread_id = -1;
	}

	if (server_socket >= 0)
	{
		sceNetInetClose(server_socket);
		server_socket = -1;
	}

	delete_worker_semaphores();
}

static void wait_before_connect_retry(int timeout_seconds,
									  SceInt64 start_time_us)
{
	SceInt64 retry_delay_us = 1000000;
	const SceInt64 sleep_slice_us = 100000;

	if (timeout_seconds != -1)
	{
		SceInt64 elapsed_us = sceKernelGetSystemTimeWide() - start_time_us;
		SceInt64 remaining_us =
			(SceInt64)timeout_seconds * 1000000 - elapsed_us;

		if (remaining_us <= 0)
		{
			return;
		}
		if (remaining_us < retry_delay_us)
		{
			retry_delay_us = remaining_us;
		}
	}

	while (retry_delay_us > 0 && platform_is_running())
	{
		SceInt64 sleep_us =
			retry_delay_us < sleep_slice_us ? retry_delay_us : sleep_slice_us;
		sceKernelDelayThread((unsigned int)sleep_us);
		retry_delay_us -= sleep_us;
	}
}

bool network_initialize(void)
{
	int result = sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
	if (result < 0)
	{
		platform_log("Could not load the PSP common network module (%d).\n",
					 result);
		return false;
	}
	common_module_loaded = true;

	result = sceUtilityLoadNetModule(PSP_NET_MODULE_INET);
	if (result < 0)
	{
		platform_log("Could not load the PSP internet network module (%d).\n",
					 result);
		network_shutdown();
		return false;
	}
	inet_module_loaded = true;

	result = sceNetInit(128 * 1024, 42, 4 * 1024, 42, 4 * 1024);
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
	stop_workers();

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
	if (inet_module_loaded)
	{
		sceUtilityUnloadNetModule(PSP_NET_MODULE_INET);
		inet_module_loaded = false;
	}
	if (common_module_loaded)
	{
		sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
		common_module_loaded = false;
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
		if (!display_begin_utility_frame())
		{
			platform_log("Could not prepare a frame for the network dialog.\n");
			sceUtilityNetconfShutdownStart();
			return false;
		}

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
		display_end_utility_frame();
	}

    int access_point_state = PSP_NET_APCTL_STATE_DISCONNECTED;
	return sceNetApctlGetState(&access_point_state) >= 0 &&
		   access_point_state == PSP_NET_APCTL_STATE_GOT_IP;
}

bool network_connect_server(int timeout_seconds)
{
	struct sockaddr_in server_address;
	SceInt64 start_time_us;
	SceInt64 timeout_us;
	unsigned int attempts = 0;
	int last_error = 0;
	int no_delay = 1;

	if (server_socket >= 0)
	{
		return true;
	}

	if (timeout_seconds < -1)
	{
		platform_log("Invalid server connection timeout: %d seconds.\n",
					 timeout_seconds);
		return false;
	}

	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(PSP_SERVER_PORT);
	server_address.sin_addr.s_addr = inet_addr(PSP_SERVER_IP);

	platform_log("Connecting to %s:%d (timeout: %s)...\n",
				 PSP_SERVER_IP, PSP_SERVER_PORT,
				 timeout_seconds == -1 ? "infinite" : "configured");

	start_time_us = sceKernelGetSystemTimeWide();
	timeout_us = (SceInt64)timeout_seconds * 1000000;

	while (platform_is_running() &&
		   (timeout_seconds == -1 ||
		   attempts == 0 ||
		   sceKernelGetSystemTimeWide() - start_time_us < timeout_us))
	{
		SceNetInetPollfd poll_descriptor;
		socklen_t socket_error_length;
		SceInt64 remaining_us = timeout_us -
			(sceKernelGetSystemTimeWide() - start_time_us);
		int poll_timeout_ms = CONNECT_POLL_INTERVAL_MS;
		int socket_fd;
		int connect_result;
		int socket_error = 0;
		int result;
		int nonblocking = 1;
		bool connected;

		if (timeout_seconds != -1)
		{
			if (remaining_us <= 0)
			{
				poll_timeout_ms = 0;
			}
			else if (remaining_us / 1000 < poll_timeout_ms)
			{
				poll_timeout_ms = (int)((remaining_us + 999) / 1000);
			}
		}

		++attempts;
		socket_fd = sceNetInetSocket(AF_INET, SOCK_STREAM, 0);
		if (socket_fd < 0)
		{
			last_error = sceNetInetGetErrno();
			wait_before_connect_retry(timeout_seconds, start_time_us);
			continue;
		}

		if (sceNetInetSetsockopt(socket_fd, SOL_SOCKET, SO_NONBLOCK,
								 &nonblocking, sizeof(nonblocking)) < 0)
		{
			last_error = sceNetInetGetErrno();
			sceNetInetClose(socket_fd);
			wait_before_connect_retry(timeout_seconds, start_time_us);
			continue;
		}

		connect_result = sceNetInetConnect(
			socket_fd, (struct sockaddr *)&server_address,
			sizeof(server_address));
		connected = connect_result == 0;
		if (connect_result < 0)
		{
			poll_descriptor.fd = socket_fd;
			poll_descriptor.events = SCE_NET_INET_POLLOUT;
			poll_descriptor.revents = 0;
			result = sceNetInetPoll(&poll_descriptor, 1, poll_timeout_ms);
			if (result > 0)
			{
				socket_error_length = sizeof(socket_error);
				if (sceNetInetGetsockopt(socket_fd, SOL_SOCKET, SO_ERROR,
										 &socket_error,
										 &socket_error_length) < 0)
				{
					last_error = sceNetInetGetErrno();
				}
				else if (socket_error != 0)
				{
					last_error = socket_error;
				}
				else
				{
					connected = true;
				}
			}
			else
			{
				last_error = result < 0 ? sceNetInetGetErrno() : ETIMEDOUT;
			}

			if (!connected)
			{
				sceNetInetClose(socket_fd);
				if (result > 0)
				{
					wait_before_connect_retry(timeout_seconds, start_time_us);
				}
				continue;
			}
		}

		nonblocking = 0;
		if (sceNetInetSetsockopt(socket_fd, SOL_SOCKET, SO_NONBLOCK,
								 &nonblocking, sizeof(nonblocking)) < 0)
		{
			last_error = sceNetInetGetErrno();
			sceNetInetClose(socket_fd);
			wait_before_connect_retry(timeout_seconds, start_time_us);
			continue;
		}

		if (sceNetInetSetsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY,
								 &no_delay, sizeof(no_delay)) < 0)
		{
			last_error = sceNetInetGetErrno();
			sceNetInetClose(socket_fd);
			wait_before_connect_retry(timeout_seconds, start_time_us);
			continue;
		}

		server_socket = socket_fd;
		return true;
	}

	platform_log("Could not connect to server after %u attempts "
				 "(last error: %d).\n", attempts, last_error);
	return false;
}

bool network_access_point_is_connected(void)
{
	int state = PSP_NET_APCTL_STATE_DISCONNECTED;

	return sceNetApctlGetState(&state) >= 0 &&
		   state == PSP_NET_APCTL_STATE_GOT_IP;
}

bool network_start_workers(void)
{
	int result;

	if (server_socket < 0 || receive_thread_id >= 0 || send_thread_id >= 0)
	{
		return false;
	}

	command_mutex = sceKernelCreateSema("command_mutex", 0, 1, 1, NULL);
	frame_ready_semaphore =
		sceKernelCreateSema("frame_ready", 0, 0, 1, NULL);
	frame_consumed_semaphore =
		sceKernelCreateSema("frame_consumed", 0, 1, 1, NULL);
	if (command_mutex < 0 || frame_ready_semaphore < 0 ||
		frame_consumed_semaphore < 0)
	{
		platform_log("Could not create network worker semaphores.\n");
		delete_worker_semaphores();
		return false;
	}

	memset(&latest_command, 0, sizeof(latest_command));
	producer_buffer_index = 0;
	ready_buffer_index = 0;
	send_worker_running = 1;
	receive_worker_running = 1;

	receive_thread_id = sceKernelCreateThread(
		"video_receive", receive_worker, NETWORK_THREAD_PRIORITY_RECEIVE,
		NETWORK_THREAD_STACK_SIZE, PSP_THREAD_ATTR_USER, NULL);
	if (receive_thread_id < 0)
	{
		platform_log("Could not create video receive worker (%d).\n",
					 receive_thread_id);
		stop_workers();
		return false;
	}

	result = sceKernelStartThread(receive_thread_id, 0, NULL);
	if (result < 0)
	{
		platform_log("Could not start video receive worker (%d).\n", result);
		sceKernelDeleteThread(receive_thread_id);
		receive_thread_id = -1;
		stop_workers();
		return false;
	}

	send_thread_id = sceKernelCreateThread(
		"controller_send", send_worker, NETWORK_THREAD_PRIORITY_SEND,
		NETWORK_THREAD_STACK_SIZE, PSP_THREAD_ATTR_USER, NULL);
	if (send_thread_id < 0)
	{
		platform_log("Could not create controller send worker (%d).\n",
					 send_thread_id);
		stop_workers();
		return false;
	}

	result = sceKernelStartThread(send_thread_id, 0, NULL);
	if (result < 0)
	{
		platform_log("Could not start controller send worker (%d).\n", result);
		sceKernelDeleteThread(send_thread_id);
		send_thread_id = -1;
		stop_workers();
		return false;
	}

	return true;
}

bool network_set_command(const psp_command_t *command)
{
	if (!send_worker_running || command_mutex < 0 || command == NULL)
	{
		return false;
	}

	if (sceKernelWaitSema(command_mutex, 1, NULL) < 0)
	{
		return false;
	}
	latest_command = *command;

	return sceKernelSignalSema(command_mutex, 1) >= 0;
}

bool network_acquire_frame(const uint8_t **frame_data, size_t *frame_size)
{
	network_frame_t *frame;

	if (!receive_worker_running || frame_ready_semaphore < 0 ||
		frame_data == NULL || frame_size == NULL ||
		sceKernelPollSema(frame_ready_semaphore, 1) < 0)
	{
		return false;
	}

	frame = &frame_buffers[ready_buffer_index];
	*frame_data = frame->data;
	*frame_size = frame->size;
	return true;
}

void network_release_frame(void)
{
	if (frame_consumed_semaphore >= 0)
	{
		sceKernelSignalSema(frame_consumed_semaphore, 1);
	}
}

bool network_send_worker_is_running(void)
{
	return send_worker_running != 0;
}

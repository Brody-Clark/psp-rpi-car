#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <pspnet_resolver.h>
#include <pspnet_inet.h>
#include <pspnet.h>
#include <pspnet_apctl.h>
#include <stdint.h>

void init_networking(void);

void shutdown_client();
	
bool start_client();

bool connect_to_access_point();

bool connectToServer(const char *server_ip, int server_port, int *out_socket, u8 retries=0, u8 wait_sec=0);

static int recieve_data(int socket, char *buf, int len);

int read_frame(int socket, u8 *buffer, int maxSize);

int get_ap_status(int& ap_status);
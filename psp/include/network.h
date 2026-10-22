#ifndef PSP_NETWORK_H
#define PSP_NETWORK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "protocol.h"

/**
 * Initialize PSP networking services.
 *
 * @return true when all required network services are ready.
 */
bool network_initialize(void);

/** Close the server connection and release initialized network services. */
void network_shutdown(void);

/**
 * Show the PSP infrastructure connection dialog.
 *
 * @return true when the dialog completes successfully.
 */
bool network_connect_access_point(void);

/**
 * Connect to the configured Raspberry Pi server.
 *
 * @return true when the TCP connection succeeds.
 */
bool network_connect_server(void);

/**
 * Send one controller command to the connected server.
 *
 * @param command Command to send in the protocol's six-byte format.
 * @return true when the complete command was sent.
 */
bool network_send_command(const psp_command_t *command);

/**
 * Receive one length-prefixed JPEG frame.
 *
 * @param buffer Destination for the JPEG payload.
 * @param capacity Destination capacity in bytes.
 * @param frame_size Receives the payload size on success.
 * @return true when one complete frame was received.
 */
bool network_receive_frame(uint8_t *buffer, size_t capacity,
						   size_t *frame_size);

/**
 * Report whether the infrastructure connection is currently active.
 *
 * @return true when PSP reports a connected access point.
 */
bool network_access_point_is_connected(void);

#endif

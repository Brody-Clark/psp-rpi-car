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
 * @param timeout_seconds Maximum time to retry, in seconds; -1 retries forever.
 * @return true when the TCP connection succeeds.
 */
bool network_connect_server(int timeout_seconds);

/**
 * Start the controller-send and video-receive worker threads.
 *
 * @return true when both workers start successfully.
 */
bool network_start_workers(void);

/**
 * Replace the latest controller command sent by the network worker.
 *
 * @param command Command in the protocol's six-byte format.
 * @return true when the command was accepted.
 */
bool network_set_command(const psp_command_t *command);

/**
 * Acquire a newly received JPEG frame without blocking.
 *
 * The returned pointer remains valid until network_release_frame is called.
 *
 * @param frame_data Receives a pointer to the JPEG payload.
 * @param frame_size Receives the payload size in bytes.
 * @return true when a frame was available.
 */
bool network_acquire_frame(const uint8_t **frame_data, size_t *frame_size);

/** Release the frame acquired by network_acquire_frame. */
void network_release_frame(void);

/** Report whether the controller-send worker remains available. */
bool network_send_worker_is_running(void);

/**
 * Report whether the infrastructure connection is currently active.
 *
 * @return true when PSP reports a connected access point.
 */
bool network_access_point_is_connected(void);

#endif

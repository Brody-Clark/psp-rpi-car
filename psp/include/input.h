#ifndef PSP_INPUT_H
#define PSP_INPUT_H

#include <stdbool.h>

#include "protocol.h"

/**
 * Configure PSP controller sampling.
 *
 * @return true when controller sampling was configured successfully.
 */
bool input_initialize(void);

/**
 * Read the current controller state into the network command format.
 *
 * @param command Receives the sampled controller state.
 * @return true when a sample was read.
 */
bool input_read_command(psp_command_t *command);

#endif

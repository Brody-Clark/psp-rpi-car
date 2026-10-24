#ifndef PSP_PLATFORM_H
#define PSP_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

/**
 * Initialize platform services, including the PSP exit callback.
 *
 * @return true when the callback thread is ready.
 */
bool platform_initialize(void);

/** Return whether the application should continue running. */
bool platform_is_running(void);

/** Request that the application leave its main loop. */
void platform_request_exit(void);

/** Release platform resources and return to the PSP menu. */
void platform_shutdown(void);

/** Clear the PSP debug text console. */
void platform_clear_console(void);

/** Reinitialize the PSP debug text console after another display mode was used. */
void platform_reset_console(void);

/** Write a formatted message to the PSP debug text console. */
void platform_log(const char *format, ...);

/** Wait a set number of seconds. */
void platform_wait_s(uint8_t seconds);

#endif

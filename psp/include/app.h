#ifndef PSP_APP_H
#define PSP_APP_H

/**
 * Run the PSP client application.
 *
 * Initializes each subsystem, runs the input/video loop, and shuts down
 * initialized subsystems before returning.
 *
 * @return 0 when the application exits normally, or a negative value on error.
 */
int app_run(void);

#endif

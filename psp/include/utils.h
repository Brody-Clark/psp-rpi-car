#ifndef PSP_UTILS_H
#define PSP_UTILS_H

#include <pspkernel.h>

/** Print free heap and thread stack memory to the PSP debug console. */
void utils_print_available_memory(SceUID thread_id);

/** Print PSP kernel information for a thread to the debug console. */
void utils_dump_thread_status(SceUID thread_id);

#endif

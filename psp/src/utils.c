#include "utils.h"

#include <string.h>

#include <pspsysmem.h>

#include "platform.h"

void utils_print_available_memory(SceUID thread_id)
{
	int stack_free = sceKernelGetThreadStackFreeSize(thread_id);
	SceSize total_free = sceKernelTotalFreeMemSize();
	SceSize largest_free = sceKernelMaxFreeMemSize();

	platform_log("Free stack for thread %d: %d bytes\n", thread_id, stack_free);
	platform_log("Free heap: %u bytes\n", (unsigned int)total_free);
	platform_log("Largest free block: %u bytes\n", (unsigned int)largest_free);
}

void utils_dump_thread_status(SceUID thread_id)
{
	SceKernelThreadInfo status;
	int result;

	memset(&status, 0, sizeof(status));
	status.size = sizeof(status);
	result = sceKernelReferThreadStatus(thread_id, &status);
	if (result < 0)
	{
		platform_log("Could not read thread status for %d (%d).\n",
					 thread_id, result);
		return;
	}

	platform_log("Thread %d: %s\n", thread_id, status.name);
	platform_log("Entry: %p, stack: %p, stack size: %u\n",
				 status.entry, status.stack, (unsigned int)status.stackSize);
	platform_log("Initial priority: %d, current priority: %d\n",
				 status.initPriority, status.currentPriority);
}

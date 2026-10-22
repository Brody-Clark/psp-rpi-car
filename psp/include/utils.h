#define MICROSEC_TO_SEC(s) (s / 1000000)
#define SEC_TO_MICROSEC(s) (s * 1000000)
#define KB_TO_BYTES(kb) (n*1024)

void printAvailableMemory(SceUID thid)
{
	int freesize = sceKernelGetThreadStackFreeSize(thid);
	printf("free size on thread id %i : %i\n", thid, freesize);
	SceSize size = sceKernelTotalFreeMemSize();
	printf("HEAP: %i\n", size);
	size = sceKernelMaxFreeMemSize();
	printf("LARGEST FREE MEM BLOCK: %i\n", size);
}

void dumpThreadStatus(SceUID thid)
{
	SceKernelThreadInfo status;
	int ret;

	memset(&status, 0, sizeof(SceKernelThreadInfo));
	printf("Thread ID: %08X\n", thid);
	status.size = sizeof(SceKernelThreadInfo);
	ret = sceKernelReferThreadStatus(thid, &status);
	printf("Get Thread Status: %08X\n", ret);
	if (ret == 0)
	{
		printf("Name: %s\n", status.name);
		printf("Thread Addr: %p\n", status.entry);
		printf("Stack Addr: %p\n", status.stack);
		printf("Stack Size: %08X\n", status.stackSize);
		printf("gp: %p\n", status.gpReg);
		printf("Initial Pri: %x\n", status.initPriority);
		printf("Current Pri: %x\n", status.currentPriority);
	}
}
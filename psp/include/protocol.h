#include <stdatomic.h>

typedef struct __attribute__((packed))
{
	u32 buttons;
	u8 lx;
	u8 ly;
} PSPCommand;

typedef struct
{
	int index;
	int size;
	u8 data[FRAME_SIZE];
} FrameBuffer;

typedef struct{
    volatile FrameBuffer frame_buffers[2];
    atomic_int active_index = -1;
} FrameDoubleBuffer;


volatile FrameBuffer *get_back(FrameDoubleBuffer& frame_double_buffer)
{
	int inactiveIndex = (atomic_load_explicit(&g_activeFrameIndex, memory_order_acquire) == 0) ? 1 : 0;
	return &frame_double_buffer[inactiveIndex];
}

volatile FrameBuffer *get_front(FrameDoubleBuffer& frame_double_buffer)
{
	int active_idx = atomic_load_explicit(&g_activeFrameIndex, memory_order_acquire);
    if(active_idx >-1){
        return &frame_double_buffer[active_idx];
    }
    else{
        return nullptr;
    }
	
}


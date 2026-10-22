#include <pspmpeg.h>
#include <pspmpegbase.h>
#include <pspjpeg.h>


void init_video();

void shutdown_video();

void blit_frame(u8 *frameData, int size);

bool frame_ready
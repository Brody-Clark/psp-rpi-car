#define SCR_WIDTH (480)
#define SCR_HEIGHT (272)
#define PIXEL_SIZE (4)
#define FRAME_SIZE (SCR_WIDTH * SCR_HEIGHT * PIXEL_SIZE)

#define printf pspDebugScreenPrintf

static void debug_clear_screen(){
    pspDebugScreenInit();
	pspDebugScreenSetXY(0, 0);
	pspDebugScreenClear();
}
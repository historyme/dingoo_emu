#include "framebuffer.h"
#include "utils.h"


uint32_t VM_LCD_FB_ADDRESS = 0x90000000;
uint8_t s_LcdFrameBufferPtr[VM_LCD_FB_SIZE] = { 0 };


int InitFb(uc_engine* uc)
{
	uc_err err = uc_mem_map_ptr(uc, VM_LCD_FB_ADDRESS, sizeof(s_LcdFrameBufferPtr), UC_PROT_ALL, s_LcdFrameBufferPtr);
	if (err) {
		printf("Failed mem map s_LcdFrameBufferPtr: %u (%s)\n", err, uc_strerror(err));
		return -1;
	}

	return 0;
}

uint32_t _lcd_get_frame(void)
{
	return VM_LCD_FB_ADDRESS;
}

void * getFramebuffPtr(void)
{
	return s_LcdFrameBufferPtr;
}


/*
uint32_t s_VmLcdFrameBufferPtr = NULL;
void* s_LcdFrameBufferPtr = NULL;

int InitFb(uc_engine* uc, app* _app)
{
	uint32_t p = vm_malloc(sizeof(uint16_t) * SCREEN_WIDTH * SCREEN_HEIGHT);
	s_LcdFrameBufferPtr = toHostPtr(p);

	if (!s_LcdFrameBufferPtr)
	{
		printf("Failed InitFb(), p = 0x%08x\n", p);
		assert(0);
	}

	s_VmLcdFrameBufferPtr = p;

	return 0;
}

uint32_t _lcd_get_frame(void)
{
	return s_VmLcdFrameBufferPtr;
}

void * getFramebuffPtr(void)
{
	return s_LcdFrameBufferPtr;
}
*/
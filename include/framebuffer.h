#ifndef _FRAMEBUFFER_H_
#define _FRAMEBUFFER_H_

#include <stdint.h>
#include <unicorn/unicorn.h>
#include "config.h"
#include "app.h"

#define VM_LCD_FB_SIZE  0x00026000 //ALIGN((sizeof(uint16_t) * SCREEN_WIDTH * SCREEN_HEIGHT), 0x1000);

int InitFb(uc_engine* uc);

uint32_t _lcd_get_frame(void);

void* getFramebuffPtr(void);

#endif
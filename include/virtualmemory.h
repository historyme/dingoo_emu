#ifndef _VIRTUAL_MEMORY_H_
#define _VIRTUAL_MEMORY_H_

#include <stdint.h>
#include "app.h"
#include <unicorn/unicorn.h>


int InitVmMem(uc_engine* uc, app* _app);

int InitVmMemSubTask(uc_engine* uc);

uint32_t vm_malloc(uint32_t len);
void vm_free(uint32_t addr);
uint32_t vm_realloc(uint32_t addr, uint32_t len);

void* toHostPtr(uint32_t addr);
uint32_t toVmPtr(void* ptr);

#endif
#ifndef _VIRTUAL_FILE_SYS_H_
#define _VIRTUAL_FILE_SYS_H_

#include <stdint.h>
#include <stdbool.h>
#include "app.h"
#include <unicorn/unicorn.h>

typedef enum {
    _file_type_file,
    _file_type_mem
} _file_type_e;

typedef struct {
    uint32_t/* _file_type_e */ type;
    uint32_t/* void* */ data;
    uint32_t /*bool*/ eof;
} _file_t;

typedef struct {
    uint32_t /*uintptr_t*/ base;
	uint32_t /* void* */size;
	uint32_t /*uintptr_t*/ offset;
    uint32_t /*bool*/      read, write;
    uint32_t /*bool*/      alloc;
} _file_mem_t;


typedef void FSYS_FILE;
extern uint32_t fsys_fopen(const char* name, const char* mode);
extern uint32_t vm_fread(void* ptr, uint32_t size, uint32_t count, uint32_t stream);
extern uint32_t fsys_fclose(uint32_t stream);
extern uint32_t fsys_fseek(uint32_t stream, uint32_t offset, uint32_t origin);
extern uint32_t fsys_ftell(uint32_t stream);
extern uint32_t fsys_fwrite(void* ptr, uint32_t size, uint32_t count, uint32_t stream);
extern uint32_t fsys_feof(uint32_t stream);

#endif
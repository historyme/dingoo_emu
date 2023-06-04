#include "virtualmemory.h"
#include <assert.h>
#include <inttypes.h>
#include "utils.h"
#include "framebuffer.h"
#include <pthread.h>
// JZ4740芯片物理地址映射关系 jz4740_03_emc_spec.pdf 
//Figure 1-1 Physical Address Space Map
//Table 1 - 2 Physical Address Space Map
//|Start Address | End Address  |Connectable Memory  |Capacity
//|H’0000 0000  | H’07FF FFFF |SDRAM space         |128 MB
//|H’0800 0000  | H’0FFF FFFF |Static memory space |128 MB
//|H’1000 0000  | H’13FF FFFF |Internal I / O space|64 MB
//|H’1400 0000  | H’1BFF FFFF |Static memory space |128MB
//|H’1C00 0000  | H’1FBF FFFF |Un - used           |60MB
//|H’1FC0 0000  | H’1FC0 0FFF |On - chip boot ROM  |4KB
//|H’1FC0 1000  | H’1FFF FFFF |Un - used           |4095KB
//|H’2000 0000  | H’BFFF FFFF |SDRAM space         |2944 MB
//|H’D000 0000  | H’FFFF FFFF |Reserved space      |512 MB



//系统的虚拟内存分配映射
//|----------------|--------------------------|----------------------|-------------------------------------|
//|0x8000_0000     |0x80a0_0000               |                      |             |                       |0xFFFF_FFFF
//|起始地址        |app程序地址               |                      |             |
//                 |代码段 |  数据段 |  bss   |   堆    |    |  栈   |             |
//                                            |  16MB   |    |  8MB  |0xa000_0000  |                        |映射内存必须小于0xc000_0000
//                                                         |LCD_FB 0x9000_0000     |
//                                                                                 |CPU寄存器地址0xb000_0000
const uint32_t CPU_REGISTER_BASE_ADDR = 0xB0000000;
const uint32_t CPU_REGISTER_SIZE = 0x04000000;
const uint32_t VM_HEAP_SIZE = 64 * 1024 * 1024;
const uint32_t VM_STACK_SIZE = 16 * 1024 * 1024;
const uint32_t VM_STACK_UPPER_ADDRESS = 0xA0000000;
const uint32_t VM_APP_BEGIN_ADDRESS = 0x80a00000;
void* s_App_Prog_Ptr = 0;
uint32_t s_App_Prog_Size = 0;
uint32_t s_Heap_Begin_Address = 0;

//固定分配内存
uint8_t s_HeapMemPtr[VM_HEAP_SIZE] = { 0 };
uint8_t s_StackMemPtr[VM_STACK_SIZE] = { 0 };
uint8_t s_RegisterMemPtr[CPU_REGISTER_SIZE] = { 0 };

typedef struct {
    size_t next;
    size_t len;
} LG_mem_free_t;

uint32_t LG_mem_min;  // 从未分配过的长度？
uint32_t LG_mem_top;  // 动态申请到达的最高内存值
LG_mem_free_t LG_mem_free;
void* LG_mem_base;
uint32_t LG_mem_len;
void* Origin_LG_mem_base;
uint32_t Origin_LG_mem_len;
void* LG_mem_end;
uint32_t LG_mem_left;  // 剩余内存

#define realLGmemSize(x) (((x) + 7) & (0xfffffff8))

#define MEM_DEBUG

void initMemoryManager(void * baseAddress, uint32_t len)
{
	printf("initMemoryManager: baseAddress:0x%" PRIx64 " len: 0x%08x\n", (size_t)baseAddress, len);
	Origin_LG_mem_base = baseAddress;
	Origin_LG_mem_len = len;

	LG_mem_base = (void*)((size_t)((size_t)Origin_LG_mem_base + 3) & (~3));
	LG_mem_len = (Origin_LG_mem_len - ((size_t)LG_mem_base - (size_t)Origin_LG_mem_base)) & (~3);
	LG_mem_end = (void *)((size_t)LG_mem_base + LG_mem_len);
	LG_mem_free.next = 0;
	LG_mem_free.len = 0;
	((LG_mem_free_t*)LG_mem_base)->next = LG_mem_len;
	((LG_mem_free_t*)LG_mem_base)->len = LG_mem_len;
	LG_mem_left = LG_mem_len;
#ifdef MEM_DEBUG
	LG_mem_min = LG_mem_len;
	LG_mem_top = 0;
#endif
}
void printMemoryInfo() {
    printf(".......total:%d, min:%d, free:%d, top:%d\n", LG_mem_len, LG_mem_min, LG_mem_left, LG_mem_top);
    printf(".......base:%p, end:%p\n", LG_mem_base, LG_mem_end);
    printf(".......obase:%p, olen:%d\n", Origin_LG_mem_base, Origin_LG_mem_len);
}

void* my_malloc(uint32_t len)
{
    LG_mem_free_t* previous, * nextfree, * l;
    void* ret;

    len = (uint32_t)realLGmemSize(len);
    if (len >= LG_mem_left) {
        printf("my_malloc no memory: len %08x\n", len);
        goto err;
    }
    if (!len) {
        printf("my_malloc invalid memory request");
        goto err;
    }
    if ((size_t)LG_mem_base + LG_mem_free.next > (size_t)LG_mem_end) {
        printf("my_malloc corrupted memory");
        goto err;
    }
    previous = &LG_mem_free;
    nextfree = (LG_mem_free_t*)((size_t)LG_mem_base + previous->next);
    while ((char*)nextfree < LG_mem_end) {
        if (nextfree->len == len) {
            previous->next = nextfree->next;
            LG_mem_left -= len;
#ifdef MEM_DEBUG
            if (LG_mem_left < LG_mem_min)
                LG_mem_min = LG_mem_left;
            if (LG_mem_top < previous->next)
                LG_mem_top = previous->next;
#endif
            ret = (void*)nextfree;
            goto end;
        }
        if (nextfree->len > len) {
            l = (LG_mem_free_t*)((char*)nextfree + len);
            l->next = nextfree->next;
            l->len = (size_t)(nextfree->len - len);
            previous->next += len;
            LG_mem_left -= len;
#ifdef MEM_DEBUG
            if (LG_mem_left < LG_mem_min)
                LG_mem_min = LG_mem_left;
            if (LG_mem_top < previous->next)
                LG_mem_top = previous->next;
#endif
            ret = (void*)nextfree;
            goto end;
        }
        previous = nextfree;
        nextfree = (LG_mem_free_t*)((size_t)LG_mem_base + nextfree->next);
    }
    printf("my_malloc no memory: len %08x\n", len);
err:
    return 0;
end:
    return ret;
}

void my_free(void* p, uint32_t len) {
    LG_mem_free_t* free, * n;
    len = (uint32_t)realLGmemSize(len);
#ifdef MEM_DEBUG
    if (!len || !p || (char*)p < LG_mem_base || (char*)p >= LG_mem_end || (char*)p + len > LG_mem_end || (char*)p + len <= LG_mem_base) {
        printf("my_free invalid\n");
        printf("p=%" PRIXPTR ", l=%d, base=%" PRIXPTR ",LG_mem_end=%" PRIXPTR "\n", (size_t)p, len, (size_t)LG_mem_base, (size_t)LG_mem_end);
        return;
    }
#endif
    free = &LG_mem_free;
    n = (LG_mem_free_t*)((size_t)LG_mem_base + free->next);
    while (((char*)n < LG_mem_end) && ((void*)n < p)) {
        free = n;
        n = (LG_mem_free_t*)((size_t)LG_mem_base + n->next);
    }
#ifdef MEM_DEBUG
    if (p == (void*)free || p == (void*)n) {
        printf("my_free:already free\n");
        return;
    }
#endif
    if ((free != &LG_mem_free) && ((char*)free + free->len == p)) {
        free->len += len;
    }
    else {
        free->next = (size_t)((char*)p - LG_mem_base);
        free = (LG_mem_free_t*)p;
        free->next = (size_t)((char*)n - LG_mem_base);
        free->len = len;
    }
    if (((char*)n < LG_mem_end) && ((char*)p + len == (char*)n)) {
        free->next = n->next;
        free->len += n->len;
    }
    LG_mem_left += len;
}

void* my_realloc(void* p, uint32_t oldlen, uint32_t len) {
    unsigned long minsize = (oldlen > len) ? len : oldlen;
    void* newblock;
    if (p == NULL) {
        return my_malloc(len);
    }
    if (len == 0) {
        my_free(p, oldlen);
        return NULL;
    }
    newblock = my_malloc(len);
    if (newblock == NULL) {
        return newblock;
    }
    memmove(newblock, p, minsize);
    my_free(p, oldlen);
    return newblock;
}

int InitVmMem(uc_engine *uc, app *_app)
{
	uc_err err;

	if (VM_APP_BEGIN_ADDRESS != _app->origin)
	{
		printf("InitVmMem invalid origin 0x%08x \n", _app->origin);
		return -1;
	}

    s_App_Prog_Ptr = _app->bin_data;
    s_App_Prog_Size = _app->bin_size;

	s_Heap_Begin_Address = ALIGN((_app->prog_size + _app->origin) ,4096);

	memset(s_HeapMemPtr, 0x00, VM_HEAP_SIZE);
	initMemoryManager(s_HeapMemPtr, VM_HEAP_SIZE);

	err = uc_mem_map_ptr(uc, s_Heap_Begin_Address, VM_HEAP_SIZE, UC_PROT_ALL, s_HeapMemPtr);
	if (err) {
		printf("Failed mem map s_HeapMemPtr: %u (%s)\n", err, uc_strerror(err));
		return -1;
	}


	memset(s_StackMemPtr, 0x00, VM_STACK_SIZE);
	err = uc_mem_map_ptr(uc, VM_STACK_UPPER_ADDRESS - VM_STACK_SIZE, VM_STACK_SIZE, UC_PROT_ALL, s_StackMemPtr);
	if (err) {
		printf("Failed mem map s_StackMemPtr: %u (%s)\n", err, uc_strerror(err));
		return -1;
	}

	uint32_t value = VM_STACK_UPPER_ADDRESS;
	uc_reg_write(uc, UC_MIPS_REG_SP, &value);


    //Register
    memset(s_RegisterMemPtr, 0x00, CPU_REGISTER_SIZE);
    err = uc_mem_map_ptr(uc, CPU_REGISTER_BASE_ADDR - CPU_REGISTER_SIZE, CPU_REGISTER_SIZE, UC_PROT_ALL, s_RegisterMemPtr);
    if (err) {
        printf("Failed mem map s_RegisterMemPtr: %u (%s)\n", err, uc_strerror(err));
        return -1;
    }

	return 0;
}

int InitVmMemSubTask(uc_engine* uc)
{
    uc_err err;

    err = uc_mem_map_ptr(uc, s_Heap_Begin_Address, VM_HEAP_SIZE, UC_PROT_ALL, s_HeapMemPtr);
    if (err)
    {
        printf("Failed mem map s_HeapMemPtr: %u (%s)\n", err, uc_strerror(err));
        return -1;
    }

    err = uc_mem_map_ptr(uc, VM_STACK_UPPER_ADDRESS - VM_STACK_SIZE, VM_STACK_SIZE, UC_PROT_ALL, s_StackMemPtr);
    if (err)
    {
        printf("Failed mem map s_StackMemPtr: %u (%s)\n", err, uc_strerror(err));
        return -1;
    }

    //Register
    err = uc_mem_map_ptr(uc, CPU_REGISTER_BASE_ADDR - CPU_REGISTER_SIZE, CPU_REGISTER_SIZE, UC_PROT_ALL, s_RegisterMemPtr);
    if (err)
    {
        printf("Failed mem map s_RegisterMemPtr: %u (%s)\n", err, uc_strerror(err));
        return -1;
    }

    return 0;
}

void* my_mallocExt(uint32_t len) {
    void* p = NULL;
    if (len == 0) {
        return NULL;
    }
    p = my_malloc(len + sizeof(size_t));
    if (p) {
        ((size_t *)p)[0] = len;
        return (void*)((size_t *)p + 1);
    }
    return p;
}
void my_freeExt(void* p) {
    if (p) {
        size_t* t = (size_t*)p - 1;
        my_free(t, *t + sizeof(size_t));
    }
}

void* my_reallocExt(void* p, uint32_t newLen) {
    if (p == NULL) {
        return my_mallocExt(newLen);
    }
    else if (newLen == 0) {
        my_freeExt(p);
        return NULL;
    }
    else {
        size_t oldlen = *((size_t*)p - 1) + sizeof(size_t);
        size_t minsize = (oldlen < newLen) ? oldlen : newLen;
        void* newblock = my_mallocExt(newLen);
        if (newblock == NULL) {
            return newblock;
        }
        memmove(newblock, p, minsize);
        my_freeExt(p);
        return newblock;
    }
}


uint32_t vm_malloc(uint32_t len)
{
    //printf("malloc(len = %d) = ", len);
    void *p = my_mallocExt(len);
    if (!p)
    {
        return NULL;
    }
    uint32_t ret =  (uint32_t)(((size_t)p - (size_t)s_HeapMemPtr) + s_Heap_Begin_Address);
    //printf("%08x \n", ret);
    return ret;
}

void vm_free(uint32_t addr)
{
    //printf("free(addr = %08x) \n", addr);
    void* p = (void *)((size_t)addr - (size_t)s_Heap_Begin_Address + (size_t)s_HeapMemPtr);
    my_freeExt((void *)p);
}

uint32_t vm_realloc(uint32_t addr, uint32_t len)
{
    void* p = (void*)((size_t)addr - (size_t)s_Heap_Begin_Address + (size_t)s_HeapMemPtr);
    void * retPtr = my_reallocExt((void*)p, len);
    return (uint32_t)(((size_t)retPtr - (size_t)s_HeapMemPtr) + s_Heap_Begin_Address);
}

//framebuffer
extern uint32_t VM_LCD_FB_ADDRESS;
extern uint8_t s_LcdFrameBufferPtr[VM_LCD_FB_SIZE];

void * toHostPtr(uint32_t addr)
{
    //heap
    if (addr >= s_Heap_Begin_Address && addr < s_Heap_Begin_Address + VM_HEAP_SIZE)
    {
        void* p = (void*)((size_t)addr - (size_t)s_Heap_Begin_Address + (size_t)s_HeapMemPtr);
        return p;
    }

    //stack
    if (addr <= VM_STACK_UPPER_ADDRESS && addr > VM_STACK_UPPER_ADDRESS - VM_STACK_SIZE)
    {
        void* p = (void*)((size_t)addr - (size_t)(VM_STACK_UPPER_ADDRESS - VM_STACK_SIZE) + (size_t)s_StackMemPtr);
        return p;
    }

    //code
    if (addr >= VM_APP_BEGIN_ADDRESS && addr < VM_APP_BEGIN_ADDRESS + s_App_Prog_Size)
    {
        void* p = (void*)((size_t)addr - (size_t)VM_APP_BEGIN_ADDRESS + (size_t)s_App_Prog_Ptr);
        return p;
    }
    //framebuffer
    if (addr >= VM_LCD_FB_ADDRESS && addr < VM_LCD_FB_ADDRESS + VM_LCD_FB_SIZE)
    {
        void* p = (void*)((size_t)addr - (size_t)VM_LCD_FB_ADDRESS + (size_t)s_LcdFrameBufferPtr);
        return p;
    }

    printf("ERR: toHostPtr 0x%08x\n", addr);
    return NULL;
}

uint32_t toVmPtr(void* ptr)
{
    //heap
    if ((size_t)ptr >= (size_t)s_HeapMemPtr && (size_t)ptr < (size_t)s_HeapMemPtr + VM_HEAP_SIZE)
    {
        return (uint32_t)(((size_t)ptr - (size_t)s_HeapMemPtr) + s_Heap_Begin_Address);
    }

    //stack
    if ((size_t)ptr >= (size_t)s_StackMemPtr && (size_t)ptr > (size_t)ptr + VM_STACK_SIZE)
    {
        return (uint32_t)(((size_t)ptr - (size_t)s_StackMemPtr) + (VM_STACK_UPPER_ADDRESS - VM_STACK_SIZE));
    }

    //code
    if ((size_t)ptr >= VM_APP_BEGIN_ADDRESS && (size_t)ptr > VM_APP_BEGIN_ADDRESS + s_App_Prog_Size)
    {
        return (uint32_t)(((size_t)ptr - (size_t)s_App_Prog_Ptr) + VM_APP_BEGIN_ADDRESS);
    }
    //framebuffer
    if ((size_t)ptr >= VM_LCD_FB_ADDRESS && (size_t)ptr > VM_LCD_FB_ADDRESS + VM_LCD_FB_SIZE)
    {
        return (uint32_t)(((size_t)ptr - (size_t)s_LcdFrameBufferPtr) + VM_LCD_FB_ADDRESS);
    }
    printf("ERR: toVmPtr 0x%x\n", ptr);
    return NULL;
}
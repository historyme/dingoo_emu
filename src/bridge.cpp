#include "bridge.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unicorn/unicorn.h>
#include "utils.h"
#include "app.h"
#include "virtualmemory.h"
#include "control.h"
#include "pthread.h"
#include "semaphore.h"
#include <assert.h>
#include "framebuffer.h"
#include "virtualfilesys.h"
#include "virtualthread.h"
#include "sound.h"
#include "mysprintf.h"
#include <pthread.h>
#include <string>
#include <locale.h>

//void* malloc(size_t len);
static void br_malloc(uc_engine* uc)
{
    uint32_t len;
    //dumpREG(uc);
    uc_reg_read(uc, UC_MIPS_REG_A0, &len);
    uint32_t p = vm_malloc(len);
    if (len == 0)
    {
        printf("vm_malloc 0\n");
    }
    if (!p && len != 0)
    {
        dumpREG(uc);
        dumpStackCall(uc, 0xa0000000);
        dumpAsm(uc);
        assert(0);
    }
    uc_reg_write(uc, UC_MIPS_REG_V0, &p);

    uint32_t ra;
    uc_reg_read(uc, UC_MIPS_REG_RA, &ra);
    uc_reg_write(uc, UC_MIPS_REG_PC, &ra);
}
//void free(void*);
static void br_free(uc_engine* uc)
{
    uint32_t addr;
    uint32_t pc;
    uc_reg_read(uc, UC_MIPS_REG_A0, &addr);
    vm_free(addr);
    uc_reg_read(uc, UC_MIPS_REG_RA, &pc);
    uc_reg_write(uc, UC_MIPS_REG_PC, &pc);
}
//void* realloc(void*, size_t);
static void br_realloc(uc_engine* uc)
{
    uint32_t addr;
    uc_reg_read(uc, UC_MIPS_REG_A0, &addr);

    uint32_t size;
    uc_reg_read(uc, UC_MIPS_REG_A1, &size);

    uint32_t p = vm_realloc(addr, size);
    uc_reg_write(uc, UC_MIPS_REG_V0, &p);

    uint32_t pc;
    uc_reg_read(uc, UC_MIPS_REG_RA, &pc);
    uc_reg_write(uc, UC_MIPS_REG_PC, &pc);
}

//void __func(void);
static void br_common(uc_engine* uc)
{
    uint32_t pc;
    uc_reg_read(uc, UC_MIPS_REG_RA, &pc);
    uc_reg_write(uc, UC_MIPS_REG_PC, &pc);
}

char  __to_locale_ansi_string[256];

char* __to_locale_ansi(wchar_t* inInput){
    uintptr_t i;
    char* tempString = (char*)inInput;
    for(i = 0; (i < 255) && (tempString[i] != '\0'); i += 2)
        __to_locale_ansi_string[i] = tempString[i];
    tempString[i] = '\0';
    return tempString;
}

static void br__to_locale_ansi(uc_engine* uc)
{
    uint32_t inInputpPtr;
    uc_reg_read(uc, UC_MIPS_REG_A0, &inInputpPtr);

    uc_err err;
    err = uc_mem_read(uc, inInputpPtr, __to_locale_ansi_string, sizeof(__to_locale_ansi_string));
    if (err) {
        printf("Failed on uc_mem_read() with error returned: %u (%s)\n", err, uc_strerror(err));
        return;
    }

    char * out = __to_locale_ansi((wchar_t*)__to_locale_ansi_string);

    uint32_t ret = inInputpPtr;
    uc_reg_write(uc, UC_MIPS_REG_V0, &ret);

    uint32_t pc;
    uc_reg_read(uc, UC_MIPS_REG_RA, &pc);
    uc_reg_write(uc, UC_MIPS_REG_PC, &pc);
}


static uint64_t s_tempTicks = 0;
// time ms
uint32_t OSTimeGet(void)
{
    if (s_tempTicks == 0)
    {
        s_tempTicks = SDL_GetTicks64();
    }

    //输出tick单位是10ms  也就是 =  1000 / OS_TICKS_PER_SEC
    //输入tempTicks单位是1ms 
    uint64_t tempTicks = SDL_GetTicks64() - s_tempTicks;
    //printf("%lld\n", tempTicks);
    //static uint64_t ticks = 0;
    //printf("%lld\n", tempTicks - ticks);
    //ticks = tempTicks;

    
    tempTicks *= OS_TICKS_PER_SEC;
    tempTicks /= 1000;
    
    return (uint32_t)tempTicks;
}


static void br_GetTickCount(uc_engine* uc)
{
    FILETIME ft;
    uint64_t tmpres = 0;
    static uint64_t res = 0;

    GetSystemTimeAsFileTime(&ft);

    tmpres |= ft.dwHighDateTime;
    tmpres <<= 32;
    tmpres |= ft.dwLowDateTime;

    if (res == 0)
    {
        res = tmpres;
        tmpres = 0;
    }
    else
    {
        tmpres = tmpres - res;
    }

    uint32_t ret = tmpres & 0xFFFFFFFF;
    uc_reg_write(uc, UC_MIPS_REG_V0, &ret);

    uint32_t pc;
    uc_reg_read(uc, UC_MIPS_REG_RA, &pc);
    uc_reg_write(uc, UC_MIPS_REG_PC, &pc);
}
static void br_OSTimeGet(uc_engine* uc)
{
    uint32_t tick_time_10ms = OSTimeGet();

    uint32_t ret = tick_time_10ms;
    uc_reg_write(uc, UC_MIPS_REG_V0, &ret);

    uint32_t pc;
    uc_reg_read(uc, UC_MIPS_REG_RA, &pc);
    uc_reg_write(uc, UC_MIPS_REG_PC, &pc);
}



static void br__kbd_get_status(uc_engine* uc)
{
    uint32_t ksPtr;
    uc_reg_read(uc, UC_MIPS_REG_A0, &ksPtr);

    KEY_STATUS* ks = (KEY_STATUS*)toHostPtr(ksPtr);
    _kbd_get_status(ks);

    uint32_t pc;
    uc_reg_read(uc, UC_MIPS_REG_RA, &pc);
    uc_reg_write(uc, UC_MIPS_REG_PC, &pc);
}

sem_t* s_semaphore_map[128] = { NULL };

//typedef void OS_EVENT;
//OS_EVENT* OSSemCreate(uint16_t cnt);
static void br_OSSemCreate(uc_engine* uc)
{
    uint32_t index = 1;
    uint32_t cnt;
    uc_reg_read(uc, UC_MIPS_REG_A0, &cnt);

    //信号量
    sem_t* sem = (sem_t*)malloc(sizeof(sem_t));
    int sem_ret = sem_init(sem, 0, cnt);
    if (sem_ret)
    {
        printf("Failed sem_init with error : %u \n", errno);
        assert(0);
    }
    for (; index < sizeof(s_semaphore_map) / sizeof(s_semaphore_map[0]); ++index)
    {
        if (s_semaphore_map[index] == NULL)
        {
            break;
        }
    }
    if (index >= sizeof(s_semaphore_map) / sizeof(s_semaphore_map[0]))
    {
        printf("Failed sem_init with error : %u, index %d \n", errno, index);
        assert(0);
    }
    s_semaphore_map[index] = sem;

    uint32_t ret = index;
    uc_reg_write(uc, UC_MIPS_REG_V0, &ret);

    uint32_t pc;
    uc_reg_read(uc, UC_MIPS_REG_RA, &pc);
    uc_reg_write(uc, UC_MIPS_REG_PC, &pc);
}

/*
extern OS_EVENT* OSSemDel(OS_EVENT *event, uint8_t option, uint8_t* error);
extern void      OSSemPend(OS_EVENT* event, uint16_t timeout, uint8_t* error);
extern uint8_t   OSSemPost(OS_EVENT* event);
*/

static void br_OSSemPend(uc_engine* uc)
{
    uint32_t eventVal;
    uint32_t timeout;
    uint32_t errorPtr;
    uc_reg_read(uc, UC_MIPS_REG_A0, &eventVal);
    uc_reg_read(uc, UC_MIPS_REG_A1, &timeout);
    uc_reg_read(uc, UC_MIPS_REG_A2, &errorPtr);

    sem_t * sem = s_semaphore_map[eventVal];

    int ret = sem_wait(sem);
#ifdef _WIN32
    DWORD tid = GetCurrentThreadId();
#else
    uint32_t tid = pthread_self();
#endif
    //printf("tid: %x, sem_wait(%lx) index %d, ret %d \n", tid, (size_t)sem, eventVal, ret);

    *((uint32_t*)toHostPtr(errorPtr)) = OS_NO_ERR;

    uint32_t pc;
    uc_reg_read(uc, UC_MIPS_REG_RA, &pc);
    uc_reg_write(uc, UC_MIPS_REG_PC, &pc);
}

static void br_OSSemPost(uc_engine* uc)
{
    uint32_t eventVal;
    uc_reg_read(uc, UC_MIPS_REG_A0, &eventVal);

    sem_t *sem = s_semaphore_map[eventVal];

    int ret = sem_post(sem);

#ifdef _WIN32
    DWORD tid = GetCurrentThreadId();
#else
    uint32_t tid = pthread_self();
#endif
    //printf("tid: %x, sem_post(%lx) index %d, ret %d \n", tid, (size_t)sem, eventVal, ret);

    uint32_t retVal = OS_NO_ERR;
    uc_reg_write(uc, UC_MIPS_REG_V0, &retVal);

    uint32_t pc;
    uc_reg_read(uc, UC_MIPS_REG_RA, &pc);
    uc_reg_write(uc, UC_MIPS_REG_PC, &pc);
}

//uint8_t   OSTaskCreate(void (*task)(void* data), void* data, OS_STK* stack, uint8_t priority);
static void br_OSTaskCreate(uc_engine* uc)
{
    /*
    dumpREG(uc);
    dumpStackCall(uc);
    dumpAsm(uc);
    exit(0);
    */

    //创建线程，在线程中启动uc_emu_start

    uc_err err;
    uint32_t taskFuncAddr;
    uint32_t dataPtr;
    uint32_t stackPtr;
    uint32_t priority;
    uc_reg_read(uc, UC_MIPS_REG_A0, &taskFuncAddr);
    uc_reg_read(uc, UC_MIPS_REG_A1, &dataPtr);
    uc_reg_read(uc, UC_MIPS_REG_A2, &stackPtr);
    uc_reg_read(uc, UC_MIPS_REG_A3, &priority);

    printf("br_OSTaskCreate(0x%x, 0x%x, 0x%x, 0x%x)\n", taskFuncAddr, dataPtr, stackPtr, priority);

    uint32_t ret = OSTaskCreate(taskFuncAddr, dataPtr, stackPtr, priority);

    uc_reg_write(uc, UC_MIPS_REG_V0, &ret);
    
    uint32_t pc;
    uc_reg_read(uc, UC_MIPS_REG_RA, &pc);
    uc_reg_write(uc, UC_MIPS_REG_PC, &pc);
}

//extern void* waveout_open(waveout_args* args);
static void br_waveout_open(uc_engine* uc)
{
    uint32_t argsPtr;
    uint32_t ret = 0;
    uc_reg_read(uc, UC_MIPS_REG_A0, &argsPtr);

    waveout_args* args = (waveout_args*)toHostPtr(argsPtr);
    waveout_args* argsCpy = (waveout_args*)malloc(sizeof(waveout_args));
    if (args != NULL && argsCpy != NULL)
    {
        memcpy(argsCpy, args, sizeof(waveout_args));
        ret = waveout_open(argsCpy);
    }

    uc_reg_write(uc, UC_MIPS_REG_V0, &ret);

    uint32_t pc;
    uc_reg_read(uc, UC_MIPS_REG_RA, &pc);
    uc_reg_write(uc, UC_MIPS_REG_PC, &pc);
}

//extern int waveout_write(waveout_inst* inst, char* buffer, int count);
static void br_waveout_write(uc_engine* uc)
{
    uint32_t instPtr;
    uint32_t bufferPtr;
    uint32_t count;
    uc_reg_read(uc, UC_MIPS_REG_A0, &instPtr);
    uc_reg_read(uc, UC_MIPS_REG_A1, &bufferPtr);
    uc_reg_read(uc, UC_MIPS_REG_A2, &count);

    //dumpMem(toHostPtr(bufferPtr), count);
    char* buff = (char*)malloc(count);
    memcpy(buff, toHostPtr(bufferPtr), count);
    uint32_t ret = waveout_write(instPtr, buff, count);
    uc_reg_write(uc, UC_MIPS_REG_V0, &ret);

    uint32_t pc;
    uc_reg_read(uc, UC_MIPS_REG_RA, &pc);
    uc_reg_write(uc, UC_MIPS_REG_PC, &pc);
}

//typedef void waveout_inst;
//int waveout_close(waveout_inst*);
static void br_waveout_close(uc_engine* uc)
{
    uint32_t ptr;
    uc_reg_read(uc, UC_MIPS_REG_A0, &ptr);

    uint32_t ret = 0;
    uc_reg_write(uc, UC_MIPS_REG_V0, &ret);

    uint32_t pc;
    uc_reg_read(uc, UC_MIPS_REG_RA, &pc);
    uc_reg_write(uc, UC_MIPS_REG_PC, &pc);
}

//int waveout_can_write();
static void br_waveout_can_write(uc_engine* uc)
{
    uint32_t ret = waveout_can_write();
    uc_reg_write(uc, UC_MIPS_REG_V0, &ret);

    uint32_t pc;
    uc_reg_read(uc, UC_MIPS_REG_RA, &pc);
    uc_reg_write(uc, UC_MIPS_REG_PC, &pc);
}

static void br_waveout_set_volume(uc_engine* uc)
{
    uint32_t vol;
    uc_reg_read(uc, UC_MIPS_REG_A0, &vol);

    uint32_t ret = waveout_set_volume(vol);
    uc_reg_write(uc, UC_MIPS_REG_V0, &ret);

    uint32_t pc;
    uc_reg_read(uc, UC_MIPS_REG_RA, &pc);
    uc_reg_write(uc, UC_MIPS_REG_PC, &pc);
}



//void* _lcd_get_frame()
static void br__lcd_get_frame(uc_engine* uc)
{
    uint32_t ptr = _lcd_get_frame();
    uc_reg_write(uc, UC_MIPS_REG_V0, &ptr);

    uint32_t pc;
    uc_reg_read(uc, UC_MIPS_REG_RA, &pc);
    uc_reg_write(uc, UC_MIPS_REG_PC, &pc);
}
//void* lcd_get_frame()
static void br_lcd_get_frame(uc_engine* uc)
{
    br__lcd_get_frame(uc);
}

extern void updateFb(void);

//void _lcd_set_frame()
static void br__lcd_set_frame(uc_engine* uc)
{
    //update fb
    updateFb();

    br_common(uc);
}

static void br_lcd_set_frame(uc_engine* uc)
{
    br__lcd_set_frame(uc);
}



//int fread(void* ptr, size_t size, size_t count, FILE* stream);
static void br_fread(uc_engine* uc)
{
    uc_err err;
    uint32_t ptr;
    uint32_t size;
    uint32_t count;
    uint32_t stream;
    uint32_t read_size;
    uint32_t read_ret = -1;
    uc_reg_read(uc, UC_MIPS_REG_A0, &ptr); 
    uc_reg_read(uc, UC_MIPS_REG_A1, &size);   
    uc_reg_read(uc, UC_MIPS_REG_A2, &count);
    uc_reg_read(uc, UC_MIPS_REG_A3, &stream);

    read_size = size * count;

    _file_t *_file = (_file_t*)toHostPtr(stream);
    if (!_file)
    {
        read_ret =  -1;
    }
    else
    {
        if (_file->type == _file_type_mem)
        {
            _file_mem_t* _file_mem = (_file_mem_t*)toHostPtr(_file->data);
            if (!_file)
            {
                read_ret = -1;
            }
            else if (_file_mem->read)
            {
                void* buff = toHostPtr(_file_mem->base + _file_mem->offset);
                void* distPtr = toHostPtr(ptr);
                if (!buff || !distPtr)
                {
                    read_ret = -1;
                }
                else
                {
                    memcpy(distPtr, buff, read_size);
                    read_ret = read_size;

                    if (read_size > 0)
                    {
                        _file_mem->offset += read_size;
                    }
                }
            }
        }
        else if (_file->type == _file_type_file)
        {
            void* buff = toHostPtr(ptr);
            if (buff)
            {
                read_ret = vm_fread(buff, size, count, _file->data);
                static int debug_fread = 0;
                if (debug_fread)
                {
                    char* out = (char *)malloc(read_size*3 + 1);
                    if (out)
                    {
                        memset(out, 0x00, read_size);
                        toHexString(buff, read_size, out);
                        printf("vm_fread(buff = %s, size = %d, count = %d, stream = %d) = %d\n", out, size, count, _file->data, read_ret);
                        free(out);
                    }
                }
            }
            else
            {
                read_ret = -1;
            }
        }
        else
        {
            printf("Failed br_fread with: %d\n", _file->type);
            assert(0);
        }
    }

    uint32_t ret = read_ret;
    uc_reg_write(uc, UC_MIPS_REG_V0, &ret);

    uint32_t pc;
    uc_reg_read(uc, UC_MIPS_REG_RA, &pc);
    uc_reg_write(uc, UC_MIPS_REG_PC, &pc);
}

//int sprintf(char* buff, const char* fmt, char * va_list);
uint32_t vm_sprintf(uc_engine* uc, uint32_t buffPtr, uint32_t fmtPtr, uint32_t val1Ptr, uint32_t val2Ptr )
{
    char* buff = (char*)toHostPtr(buffPtr);
    char* fmt = (char*)toHostPtr(fmtPtr);
    char* val1 = (char *)toHostPtr(val1Ptr);
    char* val2 = (char*)toHostPtr(val2Ptr);

    if (NULL == val1 && NULL != val2)
    {
        return sprintf(buff, fmt, val1Ptr, val2);
    }
    else if (NULL == val1 && NULL == val2)
    {
        return sprintf(buff, fmt, val1Ptr, val2Ptr);
    }

    return sprintf(buff, fmt, val1, val2);
}

static void br_sprintf(uc_engine* uc)
{
    /*
    uc_err err;
    uint32_t buffPtr;
    uint32_t fmtPtr;
    uint32_t var1Ptr;
    uint32_t var2Ptr;
    uint32_t sp;

    uc_reg_read(uc, UC_MIPS_REG_A0, &buffPtr);
    uc_reg_read(uc, UC_MIPS_REG_A1, &fmtPtr);
    uc_reg_read(uc, UC_MIPS_REG_A2, &var1Ptr);
    uc_reg_read(uc, UC_MIPS_REG_A3, &var2Ptr);
    uc_reg_read(uc, UC_MIPS_REG_SP, &sp);


    //模拟参数表
    char va_list_buff[128];
    char* buff = (char*)toHostPtr(buffPtr);
    char* fmt = (char*)toHostPtr(fmtPtr);
    char* var1 = (char*)toHostPtr(var1Ptr);
    char* var2 = (char*)toHostPtr(var2Ptr);
    dumpMem(toHostPtr(sp-128), 128);
    */

    my_sprintf(uc);

    /*
    uint32_t ret = 0;
    uc_reg_write(uc, UC_MIPS_REG_V0, &ret);

    uint32_t pc;
    uc_reg_read(uc, UC_MIPS_REG_RA, &pc);
    uc_reg_write(uc, UC_MIPS_REG_PC, &pc);
    */
}

//typedef void FSYS_FILE;
//extern FSYS_FILE* fsys_fopen(const char* name, const char* mode);
static void br_fsys_fopen(uc_engine* uc)
{
    uint32_t namePtr;
    uint32_t modePtr;
    uc_reg_read(uc, UC_MIPS_REG_A0, &namePtr);
    uc_reg_read(uc, UC_MIPS_REG_A1, &modePtr);

    char* name = (char*)toHostPtr(namePtr);
    char* mode = (char*)toHostPtr(modePtr);
    uint32_t fpPtr = fsys_fopen(name, mode);
    printf("fsys_fopen(\"%s\", \"%s\") = %d\n", name, mode, fpPtr);
    uint32_t ret = fpPtr;
    uc_reg_write(uc, UC_MIPS_REG_V0, &ret);
    uint32_t pc;
    uc_reg_read(uc, UC_MIPS_REG_RA, &pc);
    uc_reg_write(uc, UC_MIPS_REG_PC, &pc);
}


//typedef void FSYS_FILE;
//extern int fsys_fclose(FSYS_FILE*);
static void br_fsys_fclose(uc_engine* uc)
{
    uint32_t file;
    uc_reg_read(uc, UC_MIPS_REG_A0, &file);

    uint32_t ret = fsys_fclose(file);

    uc_reg_write(uc, UC_MIPS_REG_V0, &ret);
    uint32_t pc;
    uc_reg_read(uc, UC_MIPS_REG_RA, &pc);
    uc_reg_write(uc, UC_MIPS_REG_PC, &pc);
}
//int fsys_fseek(FSYS_FILE*, int offset, int origin);
static void br_fsys_fseek(uc_engine* uc)
{
    uint32_t file;
    uint32_t offset;
    uint32_t origin;
    uc_reg_read(uc, UC_MIPS_REG_A0, &file);
    uc_reg_read(uc, UC_MIPS_REG_A1, &offset);
    uc_reg_read(uc, UC_MIPS_REG_A2, &origin);

    uint32_t ret = fsys_fseek(file, offset, origin);

    //printf("fsys_fseek(%08x, %d,  %d,) = %d\n", file, offset, origin, ret);
    uc_reg_write(uc, UC_MIPS_REG_V0, &ret);

    uint32_t pc;
    uc_reg_read(uc, UC_MIPS_REG_RA, &pc);
    uc_reg_write(uc, UC_MIPS_REG_PC, &pc);
}

static void br_fsys_ftell(uc_engine* uc)
{
    uint32_t file;
    uc_reg_read(uc, UC_MIPS_REG_A0, &file);

    uint32_t ret = fsys_ftell(file);

    //printf("fsys_ftell(%d) = %08x\n", file,ret);
    uc_reg_write(uc, UC_MIPS_REG_V0, &ret);

    uint32_t pc;
    uc_reg_read(uc, UC_MIPS_REG_RA, &pc);
    uc_reg_write(uc, UC_MIPS_REG_PC, &pc);
}

//uint32_t fsys_fwrite(void* ptr, uint32_t size, uint32_t count, uint32_t stream)
static void br_fsys_fwrite(uc_engine* uc)
{
    uc_err err;
    uint32_t ret = (uint32_t) - 1;
    uint32_t ptr,size,count,stream;
    uc_reg_read(uc, UC_MIPS_REG_A0, &ptr);
    uc_reg_read(uc, UC_MIPS_REG_A1, &size);
    uc_reg_read(uc, UC_MIPS_REG_A2, &count);
    uc_reg_read(uc, UC_MIPS_REG_A3, &stream);

    void* buff = toHostPtr(ptr);
    if (buff)
    {
        ret = fsys_fwrite(buff, size, count, stream);
        static int debug_fwrite = 1;
        if (debug_fwrite)
        {
            uint32_t read_size = size * count;
            char* out = (char*)malloc((size_t)(read_size * 3 + 1));
            memset(out, 0x00, read_size);
            toHexString(buff, read_size, out);
            printf("fsys_fwrite(buff = %s, size = %d, count = %d, stream = %d) = %d\n", out, size, count, stream, ret);
            free(out);
        }
    }
    
    uc_reg_write(uc, UC_MIPS_REG_V0, &ret);

    uint32_t pc;
    uc_reg_read(uc, UC_MIPS_REG_RA, &pc);
    uc_reg_write(uc, UC_MIPS_REG_PC, &pc);
}

//uint32_t fsys_fread(void* ptr, uint32_t size, uint32_t count, uint32_t stream)
static void br_fsys_fread(uc_engine* uc)
{
    uc_err err;
    uint32_t read_ret = -1;
    uint32_t ptr, size, count, stream;
    uc_reg_read(uc, UC_MIPS_REG_A0, &ptr);
    uc_reg_read(uc, UC_MIPS_REG_A1, &size);
    uc_reg_read(uc, UC_MIPS_REG_A2, &count);
    uc_reg_read(uc, UC_MIPS_REG_A3, &stream);

    void* buff = toHostPtr(ptr);
    if (buff)
    {
        read_ret = vm_fread(buff, size, count, stream);
        static int debug_fread = 0;
        if (debug_fread)
        {
            int read_size = size * count;
            char* out = (char*)malloc(read_size * 3 + 1);
            memset(out, 0x00, read_size);
            toHexString(buff, read_size, out);
            printf("vm_fread(buff = %s, size = %d, count = %d, stream = %d) = %d\n", out, size, count, stream, read_ret);
            free(out);
        }
    }
    else
    {
        read_ret = -1;
    }

    uc_reg_write(uc, UC_MIPS_REG_V0, &read_ret);

    uint32_t pc;
    uc_reg_read(uc, UC_MIPS_REG_RA, &pc);
    uc_reg_write(uc, UC_MIPS_REG_PC, &pc);
}

static void br_fsys_feof(uc_engine* uc)
{
    uc_err err;
    uint32_t file;
    uc_reg_read(uc, UC_MIPS_REG_A0, &file);

    uint32_t ret = fsys_feof(file);

    printf("fsys_feof(%d) = %08x\n", file, ret);
    uc_reg_write(uc, UC_MIPS_REG_V0, &ret);

    uint32_t pc;
    uc_reg_read(uc, UC_MIPS_REG_RA, &pc);
    uc_reg_write(uc, UC_MIPS_REG_PC, &pc);
}



//extern int _sys_judge_event(void* in);
static void br__sys_judge_event(uc_engine* uc)
{
    uc_err err;
    uint32_t inPtr;
    uc_reg_read(uc, UC_MIPS_REG_A0, &inPtr);

    //void* in = toHostPtr(inPtr);

    // 0 继续， 小于 0 退出循环
    uint32_t ret = 0;

    uc_reg_write(uc, UC_MIPS_REG_V0, &ret);

    uint32_t pc;
    uc_reg_read(uc, UC_MIPS_REG_RA, &pc);
    uc_reg_write(uc, UC_MIPS_REG_PC, &pc);
}


// extern void OSTimeDly(uint16_t ticks);
static void br_OSTimeDly(uc_engine* uc)
{
    uc_err err;
    uint32_t ticks;
    uc_reg_read(uc, UC_MIPS_REG_A0, &ticks);

    //tick 单位是10ms 也就是 =  1000 / OS_TICKS_PER_SE
    ticks *= 1000;
    ticks /= OS_TICKS_PER_SEC;
    SDL_Delay(ticks);

    uint32_t pc;
    uc_reg_read(uc, UC_MIPS_REG_RA, &pc);
    uc_reg_write(uc, UC_MIPS_REG_PC, &pc);
}

//extern void *memset(void *outDest, int inValue, size_t inLength);
static void br_memset(uc_engine* uc)
{
    uint32_t ret = 0;
    uc_err err;
    uint32_t outDestPtr;
    uc_reg_read(uc, UC_MIPS_REG_A0, &outDestPtr);
    uint32_t inValue;
    uc_reg_read(uc, UC_MIPS_REG_A1, &inValue);
    uint32_t inLength;
    uc_reg_read(uc, UC_MIPS_REG_A2, &inLength);

    void* in = toHostPtr(outDestPtr);
    if (in)
    {
        void* out = memset(in, inValue, inLength);
        ret = toVmPtr(out);
    }
    else
    {
        ret = 0;
    }

    uc_reg_write(uc, UC_MIPS_REG_V0, &ret);

    uint32_t pc;
    uc_reg_read(uc, UC_MIPS_REG_RA, &pc);
    uc_reg_write(uc, UC_MIPS_REG_PC, &pc);
}

//extern void* memcpy(void* outDest, const void* inSrc, size_t inLength);
static void br_memcpy(uc_engine* uc)
{
    uint32_t ret = 0;
    uc_err err;
    uint32_t outDestPtr;
    uc_reg_read(uc, UC_MIPS_REG_A0, &outDestPtr);
    uint32_t inSrcPtr;
    uc_reg_read(uc, UC_MIPS_REG_A1, &inSrcPtr);
    uint32_t inLength;
    uc_reg_read(uc, UC_MIPS_REG_A2, &inLength);

    void* outDest = toHostPtr(outDestPtr);
    void* inSrc = toHostPtr(inSrcPtr);
    if (outDest && inSrc)
    {
        void* out = memcpy(outDest, inSrc, inLength);
        ret = toVmPtr(out);
    }
    else
    {
        ret = 0;
    }

    uc_reg_write(uc, UC_MIPS_REG_V0, &ret);

    uint32_t pc;
    uc_reg_read(uc, UC_MIPS_REG_RA, &pc);
    uc_reg_write(uc, UC_MIPS_REG_PC, &pc);
}



//extern int fseek(FILE* stream, long int offset, int origin);
static void br_fseek(uc_engine* uc)
{
    uc_err err;
    uint32_t origin;
    uint32_t offset;
    uint32_t stream;
    uint32_t read_ret = -1;
    uc_reg_read(uc, UC_MIPS_REG_A2, &origin);
    uc_reg_read(uc, UC_MIPS_REG_A1, &offset);
    uc_reg_read(uc, UC_MIPS_REG_A0, &stream);

    _file_t* _file = (_file_t*)toHostPtr(stream);
    if (_file)
    {
        if (_file->type == _file_type_mem)
        {
            _file_mem_t* _file_mem = (_file_mem_t*)toHostPtr(stream);
            if (!_file)
            {
                read_ret = -1;
            }
        }
        else if (_file->type == _file_type_file)
        {
            read_ret = fsys_fseek(_file->data, offset, origin);
            static int debug_fseek = 0;
            if (debug_fseek)
            {
                printf("fsys_fseek(stream = %d, offset = %d , origin = %d) = %d\n", _file->type, offset, origin, read_ret);
            }
        }
        else
        {
            printf("Failed br_fseek with: %d\n", _file->type);
            assert(0);
        }
    }

    uint32_t ret = read_ret;
    uc_reg_write(uc, UC_MIPS_REG_V0, &ret);

    uint32_t pc;
    uc_reg_read(uc, UC_MIPS_REG_RA, &pc);
    uc_reg_write(uc, UC_MIPS_REG_PC, &pc);
}

static void br_get_current_language(uc_engine* uc)
{
    uint32_t val0;
    uint32_t val1;
    uint32_t val2;
    uint32_t val3;
    uc_reg_read(uc, UC_MIPS_REG_A0, &val0);
    uc_reg_read(uc, UC_MIPS_REG_A1, &val1);
    uc_reg_read(uc, UC_MIPS_REG_A2, &val2);
    uc_reg_read(uc, UC_MIPS_REG_A3, &val3);

    //char* _val0 = (char*)toHostPtr(val0);
    //char* _val1 = (char*)toHostPtr(val1);

    uint32_t ret = 0;
    uc_reg_write(uc, UC_MIPS_REG_V0, &ret);

    uint32_t pc;
    uc_reg_read(uc, UC_MIPS_REG_RA, &pc);
    uc_reg_write(uc, UC_MIPS_REG_PC, &pc);
}


static void br_fsys_fopenW(uc_engine* uc)
{
    uc_err err;
    uint32_t namePtr;
    uint32_t modePtr;
    uc_reg_read(uc, UC_MIPS_REG_A0, &namePtr);
    uc_reg_read(uc, UC_MIPS_REG_A1, &modePtr);

    wchar_t* name = (wchar_t*)toHostPtr(namePtr);
    wchar_t* mode = (wchar_t*)toHostPtr(modePtr);

    std::string namestr = WString2String(name);
    std::string modestr = WString2String(mode);

    uint32_t fpPtr = fsys_fopen(namestr.c_str(), modestr.c_str());
    printf("fsys_fopenW(\"%s\", \"%s\") = %d\n", namestr.c_str(), modestr.c_str(), fpPtr);
    uint32_t ret = fpPtr;
    uc_reg_write(uc, UC_MIPS_REG_V0, &ret);
    uint32_t pc;
    uc_reg_read(uc, UC_MIPS_REG_RA, &pc);
    uc_reg_write(uc, UC_MIPS_REG_PC, &pc);
}



#define br_none NULL

typedef void (*br_func)(uc_engine* uc);

struct _hook_code_func_
{
    uint32_t offset;
    const char* name;
    br_func func;
    uint32_t lock;
    uint32_t trigger_times;
}_hook_code_func_map[] =
{
    {0,"OSTimeGet",br_OSTimeGet , 1},
    {0,"fread",br_fread, 1},
    {0,"memcpy",br_memcpy, 1},
    {0,"malloc",br_malloc , 1},
    {0,"free",br_free , 1},
    {0,"_lcd_get_frame",br__lcd_get_frame, 1},
    {0,"_lcd_set_frame",br__lcd_set_frame, 1},
    {0,"_sys_judge_event",br__sys_judge_event, 1},
    {0,"_kbd_get_status",br__kbd_get_status, 1},
    {0,"__dcache_writeback_all",br_common , 1},
    {0,"ap_lcd_set_frame",br_none, 1},
    {0,"lcd_set_frame",br_lcd_set_frame, 1},
    {0,"lcd_get_frame",br_lcd_get_frame, 1},
    {0,"delay_ms",br_none, 1},
    {0,"lcd_get_bpp",br_none, 1},
    {0,"lcd_get_cframe",br_none, 1},
    {0,"lcd_flip",br_none, 1},
    {0,"kbd_get_key",br_none, 1},
    {0,"kbd_get_status",br_none, 1},
    {0,"open_gui_key_msg",br_none, 1},
    {0,"tv_get_openflag",br_none, 1},
    {0,"tv_set_openflag",br_none, 1},
    {0,"tv_get_closeflag",br_none, 1},
    {0,"tv_set_closeflag",br_none, 1},
    {0,"tv_disable_switch",br_none, 1},
    {0,"tv_enable_switch",br_none, 1},
    {0,"Read_Acc0",br_none, 1},
    {0,"Memsic_SerialCommInit",br_none, 1},
    {0,"Read_Acc",br_none, 1},
    {0,"Custom_Memsic_test",br_none, 1},
    {0,"Get_X",br_none, 1},
    {0,"Get_Y",br_none, 1},
    {0,"sys_judge_event",br_none, 1},
    {0,"SysDisableBkLight",br_none, 1},
    {0,"SysEnableShutDownPower",br_none, 1},
    {0,"SysDisableCloseBkLight",br_none, 1},
    {0,"_kbd_get_key",br_none, 1},
    {0,"_waveout_open",br_none, 1},
    {0,"_waveout_set_volume",br_waveout_set_volume, 1},
    {0,"jz_pm_pllconvert",br_none, 1},
    {0,"strncasecmp",br_none, 1},
    {0,"sys_get_ccpmp_config",br_none, 1},
    {0,"vxGoHome",br_none, 1},
    {0,"cmGetSysModel",br_none, 1},
    {0,"cmGetSysVersion",br_none, 1},
    {0,"fsys_fopen_flash",br_none, 1},
    {0,"fsys_fclose_flash",br_none, 1},
    {0,"get_dl_handle",br_none, 1},
    {0,"get_game_vol",br_none, 1},
    {0,"get_current_language",br_get_current_language, 1},
    {0,"fsys_fopen",br_fsys_fopen, 1},
    {0,"fsys_fclose",br_fsys_fclose, 1},
    {0,"fsys_fread",br_fsys_fread, 1},
    {0,"fsys_remove",br_none, 1},
    {0,"fsys_fwrite",br_fsys_fwrite, 1},
    {0,"fsys_fseek",br_fsys_fseek, 1},
    {0,"fsys_ftell",br_fsys_ftell, 1},
    {0,"fsys_feof",br_fsys_feof, 1},
    {0,"fsys_ferror",br_none, 1},
    {0,"fsys_clearerr",br_none, 1},
    {0,"fsys_findfirst",br_none, 1},
    {0,"fsys_findnext",br_none, 1},
    {0,"fsys_findclose",br_none, 1},
    {0,"fsys_mkdir",br_none, 1},
    {0,"fsys_rename",br_none, 1},
    {0,"fsys_flush_cache",br_none, 1},
    {0,"fsys_RefreshCache",br_none, 1},
    {0,"fsys_fopenW",br_fsys_fopenW, 1},
    {0,"fsys_fcloseW",br_none, 1},
    {0,"fsys_removeW",br_none, 1},
    {0,"fsys_renameW",br_none, 1},
    {0,"USB_Connect",br_none, 1},
    {0,"USB_No_Connect",br_none, 1},
    {0,"tv_open",br_none, 1},
    {0,"tv_close",br_none, 1},
    {0,"isTVON",br_none, 1},
    {0,"pcm_ioctl",br_none, 1},
    {0,"mdelay",br_none, 1},
    {0,"HP_Mute_sw",br_none, 1},
    {0,"pcm_can_write",br_none, 1},
    {0,"waveout_open",br_waveout_open, 1},
    {0,"waveout_close_at_once",br_none, 1},
    {0,"waveout_write",br_waveout_write, 0},
    {0,"waveout_close",br_waveout_close, 1},
    {0,"waveout_can_write",br_waveout_can_write, 0},
    {0,"waveout_set_volume",br_waveout_set_volume, 1},
    {0,"av_reg_object",br_none, 1},
    {0,"av_unreg_object",br_none, 1},
    {0,"av_queue_get",br_none, 1},
    {0,"av_uft8_2_unicode",br_none, 1},
    {0,"av_resize_packet",br_none, 1},
    {0,"av_upper_4cc",br_none, 1},
    {0,"av_begin_thread",br_none, 1},
    {0,"av_end_thread",br_none, 1},
    {0,"av_create_sem",br_none, 1},
    {0,"av_wait_sem",br_none, 1},
    {0,"av_wait_sem2",br_none, 1},
    {0,"av_give_sem",br_none, 1},
    {0,"av_destroy_sem",br_none, 1},
    {0,"av_create_flag",br_none, 1},
    {0,"av_wait_flag",br_none, 1},
    {0,"av_give_flag",br_none, 1},
    {0,"av_destroy_flag",br_none, 1},
    {0,"av_delay",br_none, 1},
    {0,"av_queue_init",br_none, 1},
    {0,"av_queue_flush",br_none, 1},
    {0,"av_queue_abort",br_none, 1},
    {0,"av_queue_end",br_none, 1},
    {0,"av_queue_put",br_none, 1},
    {0,"dl_load",br_none, 1},
    {0,"dl_free",br_none, 1},
    {0,"dl_res_open",br_none, 1},
    {0,"dl_res_get_size",br_none, 1},
    {0,"dl_res_get_data",br_none, 1},
    {0,"dl_res_close",br_none, 1},
    {0,"dl_get_proc",br_none, 1},
    {0,"memset",br_memset , 1},
    {0,"abort",br_none, 1},
    {0,"fprintf",br_none, 1},
    {0,"fseek",br_fseek, 1},
    {0,"fwrite",br_none, 1},
    {0,"printf",br_none, 1},
    {0,"realloc",br_realloc , 1},
    {0,"sprintf",br_sprintf , 1},
    {0,"sscanf",br_none, 1},
    {0,"vsprintf",br_none, 1},
    {0,"GUI_Exec",br_none, 1},
    {0,"GUI_Lock",br_none, 1},
    {0,"GUI_TIMER_Create",br_none, 1},
    {0,"GUI_TIMER_Delete",br_none, 1},
    {0,"GUI_TIMER_SetPeriod",br_none, 1},
    {0,"GUI_TIMER_Restart",br_none, 1},
    {0,"LCD_Color2Index",br_none, 1},
    {0,"LCD_GetXSize",br_none, 1},
    {0,"LCD_GetYSize",br_none, 1},
    {0,"WM_CreateWindow",br_none, 1},
    {0,"WM_DeleteWindow",br_none, 1},
    {0,"WM_SelectWindow",br_none, 1},
    {0,"WM_DefaultProc",br_none, 1},
    {0,"WM__SendMessage",br_none, 1},
    {0,"WM_SetFocus",br_none, 1},
    {0,"U8TOU32",br_none, 1},
    {0,"__icache_invalidate_all",br_common , 1},
    {0,"LcdGetDisMode",br_none, 1},
    {0,"free_irq",br_none, 1},
    {0,"spin_lock_irqsave",br_none, 1},
    {0,"spin_unlock_irqrestore",br_none, 1},
    {0,"detect_clock",br_none, 1},
    {0,"udelay",br_none, 1},
    {0,"serial_putc",br_none, 1},
    {0,"serial_puts",br_none, 1},
    {0,"serial_getc",br_none, 1},
    {0,"TaskMediaFunStop",br_none, 1},
    {0,"StartSwTimer",br_none, 1},
    {0,"GetTickCount",br_GetTickCount , 1},
    {0,"OSCPUSaveSR",br_none, 1},
    {0,"OSCPURestoreSR",br_none, 1},
    {0,"OSFlagPost",br_none, 1},
    {0,"OSQCreate",br_none, 1},
    {0,"OSSemDel",br_none , 0},
    {0,"OSSemPend",br_OSSemPend, 0},
    {0,"OSSemPost",br_OSSemPost, 0},
    {0,"OSSemCreate",br_OSSemCreate , 1},
    {0,"OSTaskCreate",br_OSTaskCreate , 1},
    {0,"OSTaskDel",br_none, 1},
    {0,"OSTimeDly",br_OSTimeDly , 0},
    {0,"U8TOU16",br_none, 1},
    {0,"_tcscmp",br_none, 1},
    {0,"_tcscpy",br_none, 1},
    {0,"__to_unicode_le",br_none, 1},
    {0,"__to_locale_ansi",br__to_locale_ansi , 1},
    {0,"udc_attached",br_none, 1},
};

static void  debugCallFunc(uc_engine* uc,const char* name, uint32_t address)
{
    uint32_t ra;
    uc_reg_read(uc, UC_MIPS_REG_RA, &ra);
#ifdef _WIN32
    DWORD tid = GetCurrentThreadId();
#else
    uint32_t tid = pthread_self();
#endif
    printf("T:%x, %lld\t%08x %s\n", tid,  SDL_GetTicks64(), ra, name);
}

pthread_mutex_t hook_code_mutex = PTHREAD_MUTEX_INITIALIZER;

static void hook_code(uc_engine* uc, uint64_t address, uint32_t size, void* user_data)
{


    for (int j = 0; j < sizeof(_hook_code_func_map) / sizeof(_hook_code_func_map[0]); ++j)
    {
        if (_hook_code_func_map[j].offset == address)
        {
            if (_hook_code_func_map[j].func)
            {
                if (_hook_code_func_map[j].lock)
                {
                    pthread_mutex_lock(&hook_code_mutex);
                }
                _hook_code_func_map[j].func(uc);
                _hook_code_func_map[j].trigger_times++;

                if (_hook_code_func_map[j].lock)
                {
                    pthread_mutex_unlock(&hook_code_mutex);
                }

                if (0)
                {
                    for (int i = 0; i < sizeof(_hook_code_func_map) / sizeof(_hook_code_func_map[0]); ++i)
                    {
                        if (_hook_code_func_map[i].func)
                        {
                            printf("times: %10d \t%s\n", _hook_code_func_map[i].trigger_times, _hook_code_func_map[i].name);
                        }
                    }
                }

                static int debugger = 0;
                if (debugger)
                {
                    debugCallFunc(uc, _hook_code_func_map[j].name, address);
                    //dumpREG(uc);
                }
            }
            else
            {
                printf("-----------------br_none:%s----------------\n", _hook_code_func_map[j].name);
                dumpREG(uc);
                //dumpStackCall(uc);
                //dumpAsm(uc);
                exit(0);
            }
            break;
        }
    }
}

void uc_cb_hookintr(uc_engine* uc, uint32_t intno, void* user_data)
{
    app* _app = (app*)user_data;
}

static void hook_block(uc_engine* uc, uint64_t address, uint32_t size,
    void* user_data)
{
    printf(">>> Tracing basic block at 0x%" PRIx64 ", block size = 0x%x\n",
        address, size);
}

static void hooks_init(uc_engine* uc, app* _app)
{
    for (int i = 0; i < _app->import_count; ++i)
    {
        app_import_entry* entry = _app->import_data[i];
        const char* name = entry->name;
        for (int j = 0; j < sizeof(_hook_code_func_map) / sizeof(_hook_code_func_map[0]); ++j)
        {
            if (strcmp(name, _hook_code_func_map[j].name) == 0)
            {
                _hook_code_func_map[j].offset = entry->offset;
                _hook_code_func_map[j].trigger_times = 0;
                break;
            }
        }
    }

    uc_err err;
    uc_hook trace;
    uint64_t offset_begin = _app->import_data[0]->offset;
    uint64_t offset_end = _app->import_data[_app->import_count - 1]->offset;
    err = uc_hook_add(uc, &trace, UC_HOOK_CODE, hook_code, (void*)_app, offset_begin, offset_end, 0);
    if (err != UC_ERR_OK) {
        printf("add hook err %u (%s)\n", err, uc_strerror(err));
        return;
    }
}

uc_err bridge_init(uc_engine* uc, app* _app)
{
	hooks_init(uc, _app);

	return UC_ERR_OK;
}
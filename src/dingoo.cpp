#include "dingoo.h"
#include "app.h"
#include <unicorn/unicorn.h>
#include <capstone/capstone.h>
#include "bridge.h"
#include "virtualmemory.h"
#include "framebuffer.h"
#include <pthread.h>
#include <assert.h>
#include "mysprintf.h"
#include <SDL2/SDL.h>

#define APP_NAME "7day"

uint32_t s_AppDataAddr = 0;
uint32_t s_AppDataBuffSize = 0;
void *s_AppDataBuff = 0;
app* s_app = NULL;

app* loadApp(char * appName)
{
    app* _app = NULL;
    char tempPath[1024];
    sprintf(tempPath, "%s.app", appName);
    FILE* tempFile = fopen(tempPath, "rb");
    if (tempFile == NULL) {
        printf("Error: Couldn't open \"%s.app\".\n", appName);
        return NULL;
    }
    fseek(tempFile, 0, SEEK_END);
    uintptr_t tempBodyLen = ftell(tempFile);
    fseek(tempFile, 0, SEEK_SET);

    _app = app_create(tempFile, tempBodyLen);
    if (_app == NULL) {
        printf("Error: Couldn't create app struct.\n");
        return NULL;
    }

    fclose(tempFile);

    return _app;
}

void freeApp(app* pstApp)
{
    app_delete(pstApp);
}

static uint32_t breakpoint = 0x80A4A218;
static uint32_t start_debug = 0;
FILE* fp = stdout;
int use_stdout = 1;

void tryInitLog()
{
    if (use_stdout)
    {
        return;
    }

    static int init_debug_log = 0;
    if (!init_debug_log)
    {
        fp = fopen("hook_code_debug.txt", "w+");
        if (fp)
        {
            init_debug_log = 1;
        }
    }
}

void hook_code_dingoo_debug(uc_engine* uc, uint64_t address, uint32_t size, void* user_data)
{
    //debug
    dingoo_debug(uc);
}


void hook_code_debug(uc_engine* uc, uint64_t address, uint32_t size, void* user_data)
{
    if (breakpoint == address)
    {
        start_debug = 1;
    }

    if (!start_debug)
    {
        return;
    }

    //return;

    tryInitLog();

    if (size <= 4)
    {
        cs_insn* insn;
        uint32_t binary;
        size_t count;
        csh handle = 0;
        char str[60];
        uint32_t pc;

        if (cs_open(CS_ARCH_MIPS, CS_MODE_MIPS32, &handle) != CS_ERR_OK)
        {
            printf("debug cs_open() fail.");
            exit(1);
        }

        uc_reg_read(uc, UC_MIPS_REG_PC, &pc);
        uc_mem_read(uc, address, &binary, size);
        count = cs_disasm(handle, (uint8_t*)&binary, size, address, 1, &insn);
        if (count > 0) 
        {
            for (size_t j = 0; j < count; j++)
            {
                static int print_flag = 1;
                if (print_flag)
                {
                    if (fp)
                    {
                        fprintf(fp, "%lx %08X:    %08x    %s\t%s\n", SDL_GetTicks64(), pc, binary, insn[j].mnemonic, insn[j].op_str);
                        dumpREG2File(uc, fp);
                        //dumpStackCall(uc, 0xa0000000);
                        fflush(fp);
                    }
                    //dumpREG(uc);
                    //printf("%08X:    %08x    %s\t%s\n", pc, binary, insn[j].mnemonic, insn[j].op_str);
                }
            }
            cs_free(insn, count);
        }
        else 
        {
            if (fp)
            {
                fprintf(fp, "%08X:     %08x    0x%" PRIXPTR "    %d]> ", pc, binary, address, size);
                fflush(fp);
            }
            printf("[PC:0x%X, mem:0x%" PRIXPTR ", size:%d]> ", pc, address, size);
        }
        cs_close(&handle);
    }
}

static void hook_mem_valid(uc_engine* uc, uc_mem_type type, uint64_t address, int size, int64_t value, void* user_data)
{
    tryInitLog();

    fprintf(fp, ">>> Tracing mem_valid mem_type:%s at 0x%" PRIx64 ", size:0x%x, value:0x%" PRIx64 "\n",
        memTypeStr(type), address, size, value);
    if (type == UC_MEM_READ && size <= 4) {
        uint32_t v, pc;
        uc_mem_read(uc, address, &v, size);
        uc_reg_read(uc, UC_MIPS_REG_PC, &pc);
        fprintf(fp, "PC:0x%X,read:0x%X\n", pc, v);
    }
}

static bool hook_mem_invalid(uc_engine* uc, uc_mem_type type, uint64_t address, int size, int64_t value, void* user_data)
{
    tryInitLog();
    fprintf(fp, ">>> Tracing mem_invalid mem_type:%s at 0x%" PRIx64 ", size:0x%x, value:0x%" PRIx64 "\n",
        memTypeStr(type), address, size, value);
    dumpREG2File(uc, fp);
    dumpAsm(uc);
    printf(">>> Tracing mem_invalid mem_type:%s at 0x%" PRIx64 ", size:0x%x, value:0x%" PRIx64 "\n",
        memTypeStr(type), address, size, value);
    dumpREG(uc);
    return false;
}

bool uc_cb_hookinsn_invalid(uc_engine* uc, void* user_data)
{
    printf(">>> Tracing uc_cb_hookinsn_invalid \n");
    dumpREG(uc);
    return false;
}


static void hook_block(uc_engine* uc, uint64_t address, uint32_t size, void* user_data)
{
    static int logger = 0;
    if (logger)
    {
        uint64_t tempTicks = GetTickCount64();
        printf("%lld 0x%" PRIx64 " 0x%x\n", tempTicks, address, size);
    }
}

void hook_code_hook_appMain(uc_engine* uc, uint64_t address, uint32_t size, void* user_data)
{
    printf(">>> Tracing hook_code_hook_appMain \n");

    uc_err err;

    string appPath = string(APP_NAME) + ".app";
    uint32_t respathPtr = vm_malloc(64);

    err = uc_mem_write(uc, respathPtr, String2WString(appPath).c_str(), 64);
    if (err)
    {
        printf("Failed on uc_mem_write() with error returned: %u (%s)\n", err, uc_strerror(err));
        exit(1);
    }
    err = uc_reg_write(uc, UC_MIPS_REG_A0, &respathPtr);
    if (err)
    {
        printf("Failed on uc_reg_write() with error returned: %u (%s)\n", err, uc_strerror(err));
        exit(1);
    }

    // AppMain只hook一次，进去就删除hook
    uc_hook *hh = (uc_hook *)user_data;
    err = uc_hook_del(uc, *hh);
    if (err)
    {
        printf("Failed on uc_hook_del() with error returned: %u (%s)\n", err, uc_strerror(err));
        exit(1);
    }

    free(user_data);
}


uc_engine* initDingoo()
{
    uc_engine* uc;
    uc_err err;
    uc_hook trace;

    err = uc_open(UC_ARCH_MIPS, UC_MODE_MIPS32, &uc);
    if (err) {
        printf("Failed on uc_open() with error returned: %u (%s)\n", err, uc_strerror(err));
        return NULL;
    }

    app* _app = loadApp(APP_NAME);
    size_t size = _app->bin_size;

    s_AppDataAddr = _app->origin;
    s_AppDataBuffSize = size;
    s_AppDataBuff = _app->bin_data;
    s_app = _app;

    err = uc_mem_map_ptr(uc, s_AppDataAddr, s_AppDataBuffSize, UC_PROT_ALL, s_AppDataBuff);
    if (err) {
        printf("Failed mem map: %u (%s)\n", err, uc_strerror(err));
        goto end;
    }

    err = bridge_init(uc, _app);
    if (err)
    {
        printf("Failed bridge_init(): %u (%s)\n", err, uc_strerror(err));
        goto end;
    }

    //uc_hook_add(uc, &trace, UC_HOOK_BLOCK, hook_block, NULL, 1, 0);
    uc_hook_add(uc, &trace, UC_HOOK_CODE, hook_code_debug, (void*)_app, 1, 0);
    //uc_hook_add(uc, &trace, UC_HOOK_INSN_INVALID, uc_cb_hookinsn_invalid, NULL, 1, 0);
    //uc_hook_add(uc, &trace, UC_HOOK_MEM_INVALID, hook_mem_invalid, NULL, 1, 0, 0);
    //uc_hook_add(uc, &trace, UC_HOOK_MEM_VALID, hook_mem_valid, NULL, 1, 0);

    uc_hook_add(uc, &trace, UC_HOOK_CODE, hook_code_dingoo_debug, NULL, 0x80B0F060, 0x80B0F060);
 
    if (InitVmMem(uc, _app))
    {
        printf("Failed InitVmMem()\n");
        goto end;
    }

    if (InitFb(uc))
    {
        printf("Failed InitFb()\n");
        goto end;
    }

    uint32_t value = 0;
    uc_reg_write(uc, UC_MIPS_REG_ZERO, &value);

    uint32_t appMainEntry = 0;
    for (int i = 0; i < _app->export_count; i++)
    {
        if (strcmp(_app->export_data[i]->name, "AppMain") == 0)
        {
            appMainEntry = _app->export_data[i]->offset;
            break;
        }
    }

    if (!appMainEntry)
    {
        printf("Failed load appMainEntry \n");
        exit(1);
    }

    err = uc_reg_write(uc, UC_MIPS_REG_RA, &appMainEntry);
    if (err)
    {
        printf("Failed on uc_reg_write() with error returned: %u (%s)\n", err, uc_strerror(err));
        exit(1);
    }

    uc_hook* hh =(uc_hook*)malloc(sizeof(uc_hook));
    //hook appMainEntry
    err = uc_hook_add(uc, hh, UC_HOOK_CODE, hook_code_hook_appMain, (void*)hh, appMainEntry, appMainEntry, 0);
    if (err != UC_ERR_OK) {
        printf("add hook hook_code_hook_appMain err %u (%s)\n", err, uc_strerror(err));
        exit(1);
    }

    uint32_t malloc_lcd_buff = 0;
    err = uc_reg_write(uc, UC_MIPS_REG_A1, &malloc_lcd_buff);
    if (err)
    {
        printf("Failed on uc_reg_write() with error returned: %u (%s)\n", err, uc_strerror(err));
        exit(1);
    }

    err = uc_emu_start(uc, _app->bin_entry, 0xFFFFFFFF, 0, 0);
    if (err)
    {
        printf("Failed on uc_emu_start() with error returned: %u (%s)\n", err, uc_strerror(err));
        exit(1);
    }

    return uc;
end:
    uc_close(uc);
    return NULL;
}

void* dingooRun(void* data)
{
    uc_engine* __uc = initDingoo();
    return 0;
}

void startDingoo(void)
{
    printf("startDingoo\n");

    pthread_t tid;
    int ret = pthread_create(&tid,
        NULL,
        dingooRun,
        NULL);
    if (ret)
    {
        printf("pthread_create dingooRun failed\n");
        assert(0);
    }
}
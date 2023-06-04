#include "virtualthread.h"
#include <assert.h>
#include "virtualmemory.h"
#include <pthread.h>
#include <unicorn/unicorn.h>
#include "framebuffer.h"
#include "bridge.h"
#include "app.h"
#include <capstone/capstone.h>

extern uint32_t s_AppDataAddr;
extern uint32_t s_AppDataBuffSize;
extern void* s_AppDataBuff;
extern app* s_app;

struct TaskStruct
{
    pthread_t tid;
    uint32_t taskFuncAddr;
    uint32_t dataPtr;
    uint32_t stackPtr;
    uint32_t priority;
};

static bool hook_mem_invalid(uc_engine* uc, uc_mem_type type, uint64_t address, int size, int64_t value, void* user_data)
{
    printf(">>> Tracing mem_invalid mem_type:%s at 0x%" PRIx64 ", size:0x%x, value:0x%" PRIx64 "\n",
        memTypeStr(type), address, size, value);
    dumpREG(uc);
    dumpAsm(uc);
    return false;
}


static void hook_mem_valid(uc_engine* uc, uc_mem_type type, uint64_t address, int size, int64_t value, void* user_data)
{
    printf(">>> Tracing mem_valid mem_type:%s at 0x%" PRIx64 ", size:0x%x, value:0x%" PRIx64 "\n",
        memTypeStr(type), address, size, value);
    if (type == UC_MEM_READ && size <= 4)
    {
        uint32_t v, pc;
        uc_mem_read(uc, address, &v, size);
        uc_reg_read(uc, UC_MIPS_REG_PC, &pc);
        printf( "PC:0x%X,read:0x%X\n", pc, v);
    }
}

static void hook_code_debug(uc_engine* uc, uint64_t address, uint32_t size, void* user_data)
{
    char str[60];
    char* ptr;
    int eqPos;
    uc_err err;
    uint32_t stack_start_address = *((uint32_t*)user_data);

    cs_mode mode;
    uint32_t pc;
    uc_reg_read(uc, UC_MIPS_REG_PC, &pc);

    if (size <= 4) {
        cs_insn* insn;
        uint32_t binary;
        size_t count;
        csh handle;
        uint32_t cpsr;

        if (cs_open(CS_ARCH_MIPS, CS_MODE_MIPS32, &handle) != CS_ERR_OK)
        {
            printf("debug cs_open() fail.");
            exit(1);
        }
        uc_mem_read(uc, address, &binary, size);
        count = cs_disasm(handle, (uint8_t*)&binary, size, address, 1, &insn);
        if (count > 0)
        {
            for (size_t j = 0; j < count; j++)
            {
                static int print_flag = 1;
                if (print_flag)
                {
                    printf("%08X:    %08x    %s\t%s\n", pc, binary, insn[j].mnemonic, insn[j].op_str);
                    //dumpREG(uc);
                    //dumpStackCall(uc, stack_start_address);
                }
            }
            cs_free(insn, count);
        }
        else
        {
            printf("%08X:     %08x    0x%" PRIXPTR "    %d]> ", pc, binary, address, size);
        }
        cs_close(&handle);
    }
}

void* subTaskRun(void* data)
{
    struct TaskStruct* taskStruct = (struct TaskStruct*)data;

    uint32_t entry = taskStruct->taskFuncAddr;

    uc_engine* uc;
    uc_err err;
    uc_hook trace;

    printf("subTaskRun start:entry 0x%08x, priority %d\n", entry, taskStruct->priority);

    err = uc_open(UC_ARCH_MIPS, UC_MODE_MIPS32, &uc);
    if (err)
    {
        printf("Failed on uc_open() with error returned: %u (%s)\n", err, uc_strerror(err));
        return NULL;
    }

    //内存初始化
    err = uc_mem_map_ptr(uc, s_AppDataAddr, s_AppDataBuffSize, UC_PROT_ALL, s_AppDataBuff);
    if (err)
    {
        printf("Failed mem map: %u (%s)\n", err, uc_strerror(err));
        exit(1);
    }

    if(InitVmMemSubTask(uc))
    {
        printf("Failed on InitVmMemSubTask\n");
        exit(1);
    }

    if (InitFb(uc))
    {
        printf("Failed on InitFb\n");
        exit(1);
    }

    // 初始化bridge
    err = bridge_init(uc, s_app);
    if (err)
    {
        printf("Failed bridge_init(): %u (%s)\n", err, uc_strerror(err));
        exit(1);
    }

    //uc_hook_add(uc, &trace, UC_HOOK_CODE, hook_code_debug, (void*)&(taskStruct->stackPtr), 1, 0);
    //uc_hook_add(uc, &trace, UC_HOOK_MEM_INVALID, hook_mem_invalid, NULL, 1, 0, 0);
    //uc_hook_add(uc, &trace, UC_HOOK_MEM_VALID, hook_mem_valid, NULL, 1, 0, 0);

    //初始化寄存器
    //堆栈地址需要改变，不能用父线程的
    uint32_t sp = taskStruct->stackPtr;
    uc_reg_write(uc, UC_MIPS_REG_SP, &sp);

    //参数
    uint32_t a0 = taskStruct->dataPtr;
    uc_reg_write(uc, UC_MIPS_REG_A0, &a0);


    // 启动线程
    err = uc_emu_start(uc, entry, 0xFFFFFFFF, 0, 0);
    if (err)
    {
        printf("Failed on uc_emu_start() with error returned: %u (%s)\n", err, uc_strerror(err));
        exit(1);
    }

    return 0;
}

uint32_t OSTaskCreate(uint32_t taskFuncAddr, uint32_t dataPtr, uint32_t stackPtr, uint32_t priority)
{
    struct TaskStruct* taskStruct =(struct TaskStruct*)malloc(sizeof(struct TaskStruct));
    if (taskStruct == NULL)
    {
        printf("OSTaskCreate malloc failed\n");
        return -1;
    }
    taskStruct->dataPtr = dataPtr;
    taskStruct->taskFuncAddr = taskFuncAddr;
    taskStruct->stackPtr = stackPtr;
    taskStruct->priority = priority;

    int ret = pthread_create(&taskStruct->tid, NULL, subTaskRun, taskStruct);
    if (ret)
    {
        printf("pthread_create dingooRun failed\n");
        assert(0);
    }
    
    return OS_NO_ERR;
}
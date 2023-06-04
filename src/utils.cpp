#include "utils.h"
#include <time.h>
#include "virtualmemory.h"
#include <capstone/capstone.h>

char* memTypeStr(uc_mem_type type)
{
    // clang-format off
    switch (type)
    {
    case UC_MEM_READ:return "UC_MEM_READ";
    case UC_MEM_WRITE:return "UC_MEM_WRITE";
    case UC_MEM_FETCH:return "UC_MEM_FETCH";
    case UC_MEM_READ_UNMAPPED:return "UC_MEM_READ_UNMAPPED";
    case UC_MEM_WRITE_UNMAPPED:return "UC_MEM_WRITE_UNMAPPED";
    case UC_MEM_FETCH_UNMAPPED:return "UC_MEM_FETCH_UNMAPPED";
    case UC_MEM_WRITE_PROT:return "UC_MEM_WRITE_PROT";
    case UC_MEM_READ_PROT:return "UC_MEM_READ_PROT";
    case UC_MEM_FETCH_PROT:return "UC_MEM_FETCH_PROT";
    case UC_MEM_READ_AFTER:return "UC_MEM_READ_AFTER";
    }
    // clang-format on
    return "<error type>";
}

void dumpStackCall(uc_engine* uc, uint32_t stack_start_address)
{
    uint32_t v;
    printf("==========================STACK=================================\n");
    uc_reg_read(uc, UC_MIPS_REG_SP, &v); printf("0x%08x:\t", v);
    void *stack = toHostPtr(v);
    int i = 0;
    for (int j = 0; j < stack_start_address - v; j += 4)
    {
        printf("%08x ", ((uint32_t*)stack)[++i]);
        if (j % 16 == 0)
        {
            printf("\n");
            printf("0x%08x:\t", v + j);
        }
    }
    printf("\n");

    printf("==============================================================\n");
}

void dumpREG(uc_engine* uc)
{
    dumpREG2File(uc, stdout);
}

void dumpREG2File(uc_engine* uc, FILE* fp)
{
    uint32_t v;

    fprintf(fp, "==========================REG=================================\n");
    //uc_reg_read(uc, UC_MIPS_REG_ZERO, &v); printf("ZERO=%08X\t", v);
    uc_reg_read(uc, UC_MIPS_REG_AT, &v); fprintf(fp, "AT=%08X\t", v);
    uc_reg_read(uc, UC_MIPS_REG_V0, &v); fprintf(fp, "V0=%08X\t", v);
    uc_reg_read(uc, UC_MIPS_REG_V1, &v); fprintf(fp, "V1=%08X\t\n", v);

    uc_reg_read(uc, UC_MIPS_REG_A0, &v); fprintf(fp, "A0=%08X\t", v);
    uc_reg_read(uc, UC_MIPS_REG_A1, &v); fprintf(fp, "A1=%08X\t", v);
    uc_reg_read(uc, UC_MIPS_REG_A2, &v); fprintf(fp, "A2=%08X\t", v);
    uc_reg_read(uc, UC_MIPS_REG_A3, &v); fprintf(fp, "A3=%08X\t\n", v);

    uc_reg_read(uc, UC_MIPS_REG_S0, &v); fprintf(fp, "S0=%08X\t", v);
    uc_reg_read(uc, UC_MIPS_REG_S1, &v); fprintf(fp, "S1=%08X\t", v);
    uc_reg_read(uc, UC_MIPS_REG_S2, &v); fprintf(fp, "S2=%08X\t", v);
    uc_reg_read(uc, UC_MIPS_REG_S3, &v); fprintf(fp, "S3=%08X\t\n", v);
    uc_reg_read(uc, UC_MIPS_REG_S4, &v); fprintf(fp, "S4=%08X\t", v);
    uc_reg_read(uc, UC_MIPS_REG_S5, &v); fprintf(fp, "S5=%08X\t", v);
    uc_reg_read(uc, UC_MIPS_REG_S6, &v); fprintf(fp, "S6=%08X\t", v);
    uc_reg_read(uc, UC_MIPS_REG_S7, &v); fprintf(fp, "S7=%08X\t\n", v);

    uc_reg_read(uc, UC_MIPS_REG_LO, &v); fprintf(fp, "LO=%08X\t", v);
    uc_reg_read(uc, UC_MIPS_REG_HI, &v); fprintf(fp, "HI=%08X\t\n", v);

    uc_reg_read(uc, UC_MIPS_REG_PC, &v); fprintf(fp, "PC=%08X\t", v);
    uc_reg_read(uc, UC_MIPS_REG_SP, &v); fprintf(fp, "SP=%08X\t", v);
    uc_reg_read(uc, UC_MIPS_REG_FP, &v); fprintf(fp, "FP=%08X\t", v);
    uc_reg_read(uc, UC_MIPS_REG_RA, &v); fprintf(fp, "RA=%08X\t\n", v);
    fprintf(fp, "==============================================================\n");
}

static void dumpDisasm(uc_engine* uc, uint32_t address)
{
    cs_insn* insn;
    uint32_t binary;
    size_t count;
    csh handle;
    uint32_t size = 4;

    if (cs_open(CS_ARCH_MIPS, CS_MODE_MIPS32, &handle) != CS_ERR_OK)
    {
        printf("debug cs_open() fail.");
        exit(1);
    }
    uc_mem_read(uc, address, &binary, size);
    count = cs_disasm(handle, (uint8_t*)&binary, size, address, 1, &insn);
    if (count > 0)
    {
        for (int j = 0; j < count; j++)
        {
            printf("%08X:    %08x    %s\t%s\n", address, binary, insn[j].mnemonic, insn[j].op_str);
        }
    }
    else
    {
        printf("%08X:    %08x  -----------disasm-error----------- \n", address, binary);
    }

    cs_close(&handle);
}

void dumpAsm(uc_engine* uc)
{
    uint32_t ra;
    uint32_t address = 0;
    printf("==========================DISASM==============================\n");
    uc_reg_read(uc, UC_MIPS_REG_RA, &ra); 

    address = ra - 256;
    while ((ra + 4) != address)
    {
        dumpDisasm(uc, address);
        address += 4;
    }

    printf("==============================================================\n");
}


void dumpMem(void * buffer, uint32_t count)
{
    uint8_t * d = (uint8_t*)buffer;
    for (int i = 0; i < count; ++i)
    {
        printf("%02x ", d[i]);
        if ((i + 1)%16 == 0)
        {
            printf("\n");
        }
    }

    printf("\n");
}

void toHexString(void* buff, int count, char* out)
{
    int i = 0;
    for (; i < count; i++)
    {
        sprintf(out + 3*i, "%02x ", (uint8_t)((char*)buff)[i]);
    }
}

//wstring=>string
std::string WString2String(const std::wstring& ws)
{
    std::string strLocale = setlocale(LC_ALL, "");
    const wchar_t* wchSrc = ws.c_str();
    size_t nDestSize = wcstombs(NULL, wchSrc, 0) + 1;
    char* chDest = new char[nDestSize];
    memset(chDest, 0, nDestSize);
    wcstombs(chDest, wchSrc, nDestSize);
    std::string strResult = chDest;
    delete[]chDest;
    setlocale(LC_ALL, strLocale.c_str());
    return strResult;
}
// string => wstring
std::wstring String2WString(const std::string& s)
{
    std::string strLocale = setlocale(LC_ALL, "");
    const char* chSrc = s.c_str();
    size_t nDestSize = mbstowcs(NULL, chSrc, 0) + 1;
    wchar_t* wchDest = new wchar_t[nDestSize];
    wmemset(wchDest, 0, nDestSize);
    mbstowcs(wchDest, chSrc, nDestSize);
    std::wstring wstrResult = wchDest;
    delete[]wchDest;
    setlocale(LC_ALL, strLocale.c_str());
    return wstrResult;
}
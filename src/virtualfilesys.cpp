#include "virtualfilesys.h"
#include <assert.h>


FILE* s_FILE_Map[128] = { 0 };

 uint32_t fsys_fopen(const char* name, const char* mode)
{
     uint32_t index = 0;
	 FILE* fp = NULL;
     if (name == NULL || mode == NULL)
     {
         return 0;
     }
	 fp = fopen(name, mode);
     if (!fp)
     {
         return 0;
     }

     for (index = 1; index < sizeof(s_FILE_Map) / sizeof(s_FILE_Map[0]); ++index)
     {
         if (s_FILE_Map[index] == NULL)
         {
             break;
         }
     }

     if (index >= sizeof(s_FILE_Map) / sizeof(s_FILE_Map[0]))
     {
         printf("Failed s_FILE_Map with error : %u, index %d \n", errno, index);
         assert(0);
     }

     s_FILE_Map[index] = fp;

     return index;
}
 //int fread(void* ptr, size_t size, size_t count, FILE* stream);
 uint32_t vm_fread(void *ptr, uint32_t size,uint32_t count, uint32_t stream)
 {
     if (stream <= 0)
     {
         return 0;
     }

     FILE *fp = s_FILE_Map[stream];
     return (uint32_t)fread(ptr, size, count, fp);
 }

uint32_t fsys_fclose(uint32_t stream)
{
    if (stream <= 0)
    {
        return 0;
    }

    FILE* fp = s_FILE_Map[stream];
    return fclose(fp);
}

// int fseek(FILE* stream, long int offset, int origin);
uint32_t fsys_fseek(uint32_t stream, uint32_t offset, uint32_t origin)
{
    if (stream <= 0)
    {
        return 0;
    }

    FILE* fp = s_FILE_Map[stream];
    return fseek(fp, offset, origin);
}

uint32_t fsys_ftell(uint32_t stream)
{
    if (stream <= 0)
    {
        return 0;
    }

    FILE* fp = s_FILE_Map[stream];
    return ftell(fp);
}

uint32_t fsys_fwrite(void* ptr, uint32_t size, uint32_t count, uint32_t stream)
{
    if (stream <= 0)
    {
        return 0;
    }

    FILE* fp = s_FILE_Map[stream];
    return (uint32_t)fwrite(ptr, size, count, fp);
}

uint32_t fsys_feof(uint32_t stream)
{
    if (stream <= 0)
    {
        return 0;
    }

    FILE* fp = s_FILE_Map[stream];
    return (uint32_t)feof(fp);
}
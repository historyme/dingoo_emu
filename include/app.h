#ifndef __app_h__
#define __app_h__


#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>


typedef struct {
	uint32_t  offset;
	char*     name;
} app_import_entry;

typedef struct {
	uint32_t offset;
	char*    name;
} app_export_entry;

typedef struct {
	uint32_t           import_count;
	app_import_entry** import_data;
	uint32_t           export_count;
	app_export_entry** export_data;
	uint32_t           bin_size;
	void*              bin_data;
	uint32_t           bin_entry;
	uint32_t		   origin;
	uint32_t		   prog_size;
	uint32_t           bin_bss;
} app;



extern app* app_create(FILE* tempFile, uint32_t inSize);
extern void app_delete(app* inApp);

extern bool app_import_add(app* inApp, const char* inName, uint32_t inOffset);
extern bool app_export_add(app* inApp, const char* inName, uint32_t inOffset);

extern bool app_save(app* inApp, const char* inPath);


#endif

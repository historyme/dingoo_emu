#include "app.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "utils.h"

#ifdef _WIN32
#pragma pack(1)
#endif
typedef struct {
	char     ident[4];
	uint8_t  unknown[20];
	uint8_t  padding[8];
#ifndef _WIN32
} __attribute__((__packed__)) _app_ccdl;
#else
} _app_ccdl;
#pragma pack()
#endif

_app_ccdl _app_ccdl_default = {
	{ 'C', 'C', 'D', 'L' },
	{
		0x00, 0x00, 0x01, 0x00,
		0x01, 0x00, 0x02, 0x00,
		0x04, 0x00, 0x00, 0x00,
		0x20, 0x09, 0x06, 0x24,
		0x19, 0x24, 0x42, 0x00
	},
	{
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00
	}
};


#ifdef _WIN32
#pragma pack(1)
#endif
typedef struct {
	char     ident[4];
	uint32_t unknown;
	uint32_t offset;
	uint32_t size;
	uint8_t  padding[16];
#ifndef _WIN32
} __attribute__((__packed__)) _app_impt;
#else
} _app_impt;
#pragma pack()
#endif

_app_impt _app_impt_default = {
	{ 'I', 'M', 'P', 'T' },
	0x00000008,
	0,
	0,
	{
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00
	}
};

#ifdef _WIN32
#pragma pack(1)
#endif
typedef struct {
	char     ident[4];
	uint32_t unknown;
	uint32_t offset;
	uint32_t size;
	uint8_t  padding[16];
#ifndef _WIN32
} __attribute__((__packed__)) _app_expt;
#else
} _app_expt;
#pragma pack()
#endif

_app_expt _app_expt_default = {
	{ 'E', 'X', 'P', 'T' },
	0x00000009,
	0,
	0,
	{
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00
	}
};


#ifdef _WIN32
#pragma pack(1)
#endif
typedef struct {
	char     ident[4];
	uint32_t unknown0;
	uint32_t offset;
	uint32_t size;
	uint32_t unknown1;
	uint32_t entry;
	uint32_t origin;
	uint32_t prog_size;
#ifndef _WIN32
} __attribute__((__packed__)) _app_rawd;
#else
} _app_rawd;
#pragma pack()
#endif

_app_rawd _app_rawd_default = {
	{ 'R', 'A', 'W', 'D' },
	0x00000001,
	0,
	0,
	0x00000000,
	0x00000000,
	0x80A00000,
	0
};


#ifdef _WIN32
#pragma pack(1)
#endif
typedef struct {
	uint32_t str_offset;
	uint32_t unknown[2];
	uint32_t offset;
#ifndef _WIN32
} __attribute__((__packed__)) _app_impt_entry;
#else
} _app_impt_entry;
#pragma pack()
#endif

_app_impt_entry _app_impt_entry_default = {
	0, { 0, 0x00020000 }, 0
};


#ifdef _WIN32
#pragma pack(1)
#endif
typedef struct {
	uint32_t str_offset;
	uint32_t unknown[2];
	uint32_t offset;
#ifndef _WIN32
} __attribute__((__packed__)) _app_expt_entry;
#else
} _app_expt_entry;
#pragma pack()
#endif

_app_expt_entry _app_expt_entry_default = {
	0, { 0, 0x00020000 }, 0
};




uintptr_t _app_strlen(const char* inString) {
	uintptr_t tempLen = strlen(inString);
	tempLen += (4 - (tempLen & 3));
	return tempLen;
}

app* app_create(FILE* tempFile, uint32_t inSize)
{
	int i = 0;
	app* tempApp = (app*)malloc(sizeof(app));
	if (!tempApp)
	{
		return NULL;
	}

	memset(tempApp, 0x00, sizeof(app));

	_app_ccdl tempCCDL;
	_app_impt tempIMPT;
	_app_expt tempEXPT;
	_app_rawd tempRAWD;

	fread(&tempCCDL, sizeof(_app_ccdl), 1, tempFile);
	fread(&tempIMPT, sizeof(_app_impt), 1, tempFile);
	fread(&tempEXPT, sizeof(_app_expt), 1, tempFile);
	fread(&tempRAWD, sizeof(_app_rawd), 1, tempFile);

	// read Import Table
	fseek(tempFile, tempIMPT.offset, SEEK_SET);
	_app_impt_entry tempIHeader = { 0, { 0, 0 }, 0 };
	fread(&tempIHeader, sizeof(_app_impt_entry), 1, tempFile);
	tempApp->import_count = tempIHeader.str_offset;
	_app_impt_entry *tempIEntry = (_app_impt_entry*)malloc(sizeof(_app_impt_entry) * tempApp->import_count);
	if (!tempIEntry)
	{
		assert(0);
	}
	for (i = 0; i < tempApp->import_count; i++) {
		fread(&tempIEntry[i], sizeof(_app_impt_entry), 1, tempFile);
	}

	tempApp->import_data = (app_import_entry**)malloc(sizeof(app_import_entry*) * tempApp->import_count);

	// read Import Strings
	for (i = 0; i < tempApp->import_count; i++)
	{
		app_import_entry* entry = (app_import_entry*)malloc(sizeof(app_import_entry));
		if (!entry)
		{
			assert(0);
		}
		memset(entry, 0x00, sizeof(app_import_entry));
		entry->offset = tempIEntry[i].offset;

		entry->name = (char*)malloc(32);
		if (!entry->name)
		{
			assert(0);
		}
		memset(entry->name, 0x00, 32);
		for (int j = 0; j < 32; j++)
		{
			fread(entry->name + j, 1, 1, tempFile);
			if (entry->name[j] == '\0')
			{
				int padding_len = (4 - ((j + 1) % 4)) % 4;
				fread(entry->name + j + 1, padding_len, 1, tempFile);
				break;
			}
		}

		tempApp->import_data[i] = entry;

		printf("import_data: offset 0x%08x, name %s\n", entry->offset, entry->name);
	}


	// read Export Table
	fseek(tempFile, tempEXPT.offset, SEEK_SET);
	_app_expt_entry tempEHeader = { 0, { 0, 0 }, 0 };
	fread(&tempEHeader, sizeof(_app_expt_entry), 1, tempFile);
	tempApp->export_count = tempEHeader.str_offset;
	_app_expt_entry* tempEEntry = (_app_expt_entry*)malloc(sizeof(_app_expt_entry) * tempApp->export_count);
	if (!tempEEntry)
	{
		assert(0);
	}
	for (i = 0; i < tempApp->export_count; i++) {
		fread(&tempEEntry[i], sizeof(_app_expt_entry), 1, tempFile);
	}
	tempApp->export_data = (app_export_entry**)malloc(sizeof(app_export_entry*) * tempApp->export_count);
	// read Export Strings
	for (i = 0; i < tempApp->export_count; i++)
	{
		app_export_entry* entry = (app_export_entry*)malloc(sizeof(app_export_entry));
		if (!entry)
		{
			assert(0);
		}
		memset(entry, 0x00, sizeof(app_export_entry));
		entry->offset = tempEEntry[i].offset;

		entry->name = (char*)malloc(32);
		if (!entry->name)
		{
			assert(0);
		}
		memset(entry->name, 0x00, 32);
		for (int j = 0; j < 32; j++)
		{
			fread(entry->name + j, 1, 1, tempFile);
			if (entry->name[j] == '\0')
			{
				int padding_len = (4 - ((j + 1) % 4)) % 4;
				fread(entry->name + j + 1, padding_len, 1, tempFile);
				break;
			}
		}

		tempApp->export_data[i] = entry;

		printf("export_data: offset 0x%08x, name %s\n", entry->offset, entry->name);
	}

	// read Binary Data
	fseek(tempFile, tempRAWD.offset, SEEK_SET);
	printf("bin offset: offset 0x%08x \n", tempRAWD.offset);
	tempApp->bin_size = tempRAWD.size;

	//4k ALIGN
	uint32_t memory_align = ALIGN(tempRAWD.prog_size, 4096);
	tempApp->bin_data = malloc(memory_align);
	if (!tempApp->bin_data)
	{
		assert(0);
	}
	memset(tempApp->bin_data, 0x00, memory_align);
	fread(tempApp->bin_data, tempApp->bin_size, 1, tempFile);

	tempApp->bin_size = memory_align;

	tempApp->bin_entry = tempRAWD.entry;
	tempApp->bin_bss = tempRAWD.prog_size - tempRAWD.size;
	tempApp->origin = tempRAWD.origin;
	tempApp->prog_size = tempRAWD.prog_size;

	return tempApp;
}


void app_delete(app* inApp) {
	if(inApp == NULL)
		return;

	uintptr_t i;

	if(inApp->import_data != NULL) {
		for(i = 0; i < inApp->import_count; i++)
			free(inApp->import_data[i]);
		free(inApp->import_data);
	}

	if(inApp->export_data != NULL) {
		for(i = 0; i < inApp->export_count; i++)
			free(inApp->export_data[i]);
		free(inApp->export_data);
	}

	if(inApp->bin_data != NULL)
		free(inApp->bin_data);

	free(inApp);
}



bool app_import_add(app* inApp, const char* inName, uint32_t inOffset) {
	if((inApp == NULL) || (inName == NULL))
		return false;

	app_import_entry* tempEntry = (app_import_entry*)malloc(sizeof(app_import_entry) + strlen(inName) + 1);
	if(tempEntry == NULL)
		return false;
	tempEntry->offset = inOffset;
	tempEntry->name = (char*)((uintptr_t)tempEntry + sizeof(app_import_entry));
	strcpy(tempEntry->name, inName);

	app_import_entry** tempRealloc = (app_import_entry**)realloc(inApp->import_data, (sizeof(app_import_entry*) * (inApp->import_count + 1)));
	if(tempRealloc == NULL) {
		free(tempEntry);
		return false;
	}

	inApp->import_data = tempRealloc;
	inApp->import_data[inApp->import_count] = tempEntry;
	inApp->import_count++;

	return true;
}

bool app_export_add(app* inApp, const char* inName, uint32_t inOffset) {
	if((inApp == NULL) || (inName == NULL))
		return false;

	app_export_entry* tempEntry = (app_export_entry*)malloc(sizeof(app_export_entry) + strlen(inName) + 1);
	if(tempEntry == NULL)
		return false;
	tempEntry->offset = inOffset;
	tempEntry->name = (char*)((uintptr_t)tempEntry + sizeof(app_export_entry));
	strcpy(tempEntry->name, inName);

	app_export_entry** tempRealloc = (app_export_entry**)realloc(inApp->export_data, (sizeof(app_export_entry*) * (inApp->export_count + 1)));
	if(tempRealloc == NULL) {
		free(tempEntry);
		return false;
	}

	inApp->export_data = tempRealloc;
	inApp->export_data[inApp->export_count] = tempEntry;
	inApp->export_count++;

	return true;
}



void _fprint_string(const char* inString, FILE* inStream) {
	uintptr_t tempLen = strlen(inString);
	fwrite(inString, 1, tempLen, inStream);
	uintptr_t i;
	for(i = 0; i < (4 - (tempLen & 3)); i++)
		fwrite(&inString[tempLen], 1, 1, inStream);
}

bool app_save(app* inApp, const char* inPath) {
	if((inApp == NULL) || (inPath == NULL))
		return false;

	uintptr_t i;

	_app_ccdl tempCCDL = _app_ccdl_default;
	_app_impt tempIMPT = _app_impt_default;
	_app_expt tempEXPT = _app_expt_default;
	_app_rawd tempRAWD = _app_rawd_default;

	uint32_t tempPadding[4] = { 0, 0, 0, 0 };

	tempIMPT.offset = sizeof(_app_ccdl) + sizeof(_app_impt) + sizeof(_app_expt) + sizeof(_app_rawd);

	tempIMPT.size = (sizeof(_app_impt_entry) * (inApp->import_count + 1));
	for(i = 0; i < inApp->import_count; i++)
		tempIMPT.size += _app_strlen(inApp->import_data[i]->name);
	uintptr_t tempIPad = (16 - (tempIMPT.size & 15)) & 15;
	tempIMPT.size += tempIPad;


	tempEXPT.offset = tempIMPT.offset + tempIMPT.size;
	tempEXPT.size = (sizeof(_app_expt_entry) * (inApp->export_count + 1));
	for(i = 0; i < inApp->export_count; i++)
		tempEXPT.size += _app_strlen(inApp->export_data[i]->name);
	uintptr_t tempEPad = (16 - (tempEXPT.size & 15)) & 15;
	tempEXPT.size += tempEPad;

	tempRAWD.offset = tempEXPT.offset + tempEXPT.size;
	tempRAWD.size   = (inApp->bin_size + 15) & 0xFFFFFFF0;
	tempRAWD.origin = 0x80A00000;
	tempRAWD.entry  = inApp->bin_entry;
	tempRAWD.prog_size = tempRAWD.size + inApp->bin_bss;

	FILE* tempFile = fopen(inPath, "wb");
	if(tempFile == NULL)
		return false;

	fwrite(&tempCCDL, sizeof(_app_ccdl), 1, tempFile);
	fwrite(&tempIMPT, sizeof(_app_impt), 1, tempFile);
	fwrite(&tempEXPT, sizeof(_app_expt), 1, tempFile);
	fwrite(&tempRAWD, sizeof(_app_rawd), 1, tempFile);
	// TODO - Consider extra field.



	// Write Import Table
	_app_impt_entry tempIHeader = { inApp->import_count, { 0, 0 }, 0 };
	fwrite(&tempIHeader, sizeof(_app_impt_entry), 1, tempFile);
	_app_impt_entry tempIEntry = _app_impt_entry_default;
	for(i = 0; i < inApp->import_count; i++) {
		tempIEntry.offset = inApp->import_data[i]->offset;
		fwrite(&tempIEntry, sizeof(_app_impt_entry), 1, tempFile);
		tempIEntry.str_offset += _app_strlen(inApp->import_data[i]->name);
	}

	// Write Import Strings
	for(i = 0; i < inApp->import_count; i++)
		_fprint_string(inApp->import_data[i]->name, tempFile);

	// Write Import Whitespace
	fwrite(tempPadding, 1, tempIPad, tempFile);



	// Write Export Table
	_app_expt_entry tempEHeader = { inApp->export_count, { 0, 0 }, 0 };
	fwrite(&tempEHeader, sizeof(_app_expt_entry), 1, tempFile);
	_app_expt_entry tempEEntry = _app_expt_entry_default;
	for(i = 0; i < inApp->export_count; i++) {
		tempEEntry.offset = inApp->export_data[i]->offset;
		fwrite(&tempEEntry, sizeof(_app_expt_entry), 1, tempFile);
		tempEEntry.str_offset += _app_strlen(inApp->export_data[i]->name);
	}

	// Write Export Strings
	for(i = 0; i < inApp->export_count; i++)
		_fprint_string(inApp->export_data[i]->name, tempFile);

	// Write Export Whitespace
	fwrite(tempPadding, 1, tempEPad, tempFile);



	// Write Binary Data
	fwrite(inApp->bin_data, 1, inApp->bin_size, tempFile);
	fwrite(tempPadding, 1, ((16 - (inApp->bin_size & 15)) & 15), tempFile);

	// TODO - Append res data.

	fclose(tempFile);
	return true;
}

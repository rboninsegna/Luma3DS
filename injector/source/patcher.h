#pragma once

#include <3ds/types.h>

#define PATH_MAX 255

#define CONFIG(a)        (((info.config >> (a + 16)) & 1) != 0)
#define MULTICONFIG(a)   ((info.config >> (a * 2 + 6)) & 3)
#define BOOTCONFIG(a, b) ((info.config >> a) & b)

// Symbolic option numbers
#define OPTION_REGION_FREE    9
#define OPTION_UPDATE_BYPASS 10
#define OPTION_SECUREINFO    11
#define OPTION_ERRDISP       12
#define OPTION_TESTMENU      13


typedef struct __attribute__((packed))
{
    char magic[4];
    
    u8 versionMajor;
    u8 versionMinor;
    u8 versionBuild;
    u8 flags; /* bit 0: dev branch; bit 1: is release */

    u32 commitHash;

    u32 config;
} CFWInfo;

void patchCode(u64 progId, u8 *code, u32 size);
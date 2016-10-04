/*
*   This file is part of Luma3DS
*   Copyright (C) 2016 Aurora Wright, TuxSH
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*   Additional Terms 7.b of GPLv3 applies to this file: Requiring preservation of specified
*   reasonable legal notices or author attributions in that material or in the Appropriate Legal
*   Notices displayed by works containing it.
*/

/*
*   ARM11 modules patching code originally by Subv
*/

#include "patches.h"
#include "fs.h"
#include "memory.h"
#include "config.h"
#include "../build/bundled.h"

u8 *getProcess9(u8 *pos, u32 size, u32 *process9Size, u32 *process9MemAddr)
{
    u8 *off = memsearch(pos, "ess9", size, 4);

    *process9Size = *(u32 *)(off - 0x60) * 0x200;
    *process9MemAddr = *(u32 *)(off + 0xC);

    //Process9 code offset (start of NCCH + ExeFS offset + ExeFS header size)
    return off - 0x204 + (*(u32 *)(off - 0x64) * 0x200) + 0x200;
}

u32 *getKernel11Info(u8 *pos, u32 size, u32 *baseK11VA, u8 **freeK11Space, u32 **arm11SvcHandler, u32 **arm11ExceptionsPage)
{    
    const u8 pattern[] = {0x00, 0xB0, 0x9C, 0xE5};

    *arm11ExceptionsPage = (u32 *)memsearch(pos, pattern, size, sizeof(pattern)) - 0xB;

    u32 svcOffset = (-(((*arm11ExceptionsPage)[2] & 0xFFFFFF) << 2) & (0xFFFFFF << 2)) - 8; //Branch offset + 8 for prefetch
    u32 pointedInstructionVA = 0xFFFF0008 - svcOffset;
    *baseK11VA = pointedInstructionVA & 0xFFFF0000; //This assumes that the pointed instruction has an offset < 0x10000, iirc that's always the case
    u32 *arm11SvcTable = (u32 *)(pos + *(u32 *)(pos + pointedInstructionVA - *baseK11VA + 8) - *baseK11VA); //SVC handler address
    *arm11SvcHandler = arm11SvcTable;
    while(*arm11SvcTable) arm11SvcTable++; //Look for SVC0 (NULL)

    const u8 pattern2[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    *freeK11Space = memsearch(pos, pattern2, size, sizeof(pattern2)) + 1;

    return arm11SvcTable;
}

void patchSignatureChecks(u8 *pos, u32 size)
{
    //Look for signature checks
    const u8 pattern[] = {0xC0, 0x1C, 0x76, 0xE7},
             pattern2[] = {0xB5, 0x22, 0x4D, 0x0C};

    u16 *off = (u16 *)memsearch(pos, pattern, size, sizeof(pattern)),
        *off2 = (u16 *)(memsearch(pos, pattern2, size, sizeof(pattern2)) - 1);

    *off = off2[0] = 0x2000;
    off2[1] = 0x4770;
}

void patchFirmlaunches(u8 *pos, u32 size, u32 process9MemAddr)
{
    //Look for firmlaunch code
    const u8 pattern[] = {0xE2, 0x20, 0x20, 0x90};

    u8 *off = memsearch(pos, pattern, size, sizeof(pattern)) - 0x13;

    //Firmlaunch function offset - offset in BLX opcode (A4-16 - ARM DDI 0100E) + 1
    u32 fOpenOffset = (u32)(off + 9 - (-((*(u32 *)off & 0x00FFFFFF) << 2) & (0xFFFFFF << 2)) - pos + process9MemAddr);

    //Copy firmlaunch code
    memcpy(off, reboot_bin, reboot_bin_size);

    //Put the fOpen offset in the right location
    u32 *pos_fopen = (u32 *)memsearch(off, "OPEN", reboot_bin_size, 4);
    *pos_fopen = fOpenOffset;

    if(CONFIG(USECUSTOMPATH))
    {
        const char pathPath[] = "/puma/path.txt";

        u32 pathSize = getFileSize(pathPath);

        if(pathSize > 5 && pathSize < 58)
        {
            u8 path[pathSize];
            fileRead(path, pathPath, 0);
            if(path[pathSize - 1] == 0xA) pathSize--;
            if(path[pathSize - 1] == 0xD) pathSize--;

            if(pathSize > 5 && pathSize < 56 && path[0] == '/' && memcmp(&path[pathSize - 4], ".bin", 4) == 0)
            {
                u16 finalPath[pathSize + 1];
                for(u32 i = 0; i < pathSize; i++)
                    finalPath[i] = (u16)path[i];
                finalPath[pathSize] = 0;

                u8 *pos_path = memsearch(off, u"sd", reboot_bin_size, 4) + 0xA;
                memcpy(pos_path, finalPath, (pathSize + 1) * 2);
            }
        }
    }
}

void patchFirmWrites(u8 *pos, u32 size)
{
    //Look for FIRM writing code
    u8 *off1 = memsearch(pos, "exe:", size, 4);
    const u8 pattern[] = {0x00, 0x28, 0x01, 0xDA};

    u16 *off2 = (u16 *)memsearch(off1 - 0x100, pattern, 0x100, sizeof(pattern));

    off2[0] = 0x2000;
    off2[1] = 0x46C0;
}

void patchOldFirmWrites(u8 *pos, u32 size)
{
    //Look for FIRM writing code
    const u8 pattern[] = {0x04, 0x1E, 0x1D, 0xDB};

    u16 *off = (u16 *)memsearch(pos, pattern, size, sizeof(pattern));

    off[0] = 0x2400;
    off[1] = 0xE01D;
}

void reimplementSvcBackdoor(u8 *pos, u32 *arm11SvcTable, u32 baseK11VA, u8 **freeK11Space)
{
    //Official implementation of svcBackdoor
    const u8 svcBackdoor[40] = {0xFF, 0x10, 0xCD, 0xE3,  //bic   r1, sp, #0xff
                                0x0F, 0x1C, 0x81, 0xE3,  //orr   r1, r1, #0xf00
                                0x28, 0x10, 0x81, 0xE2,  //add   r1, r1, #0x28
                                0x00, 0x20, 0x91, 0xE5,  //ldr   r2, [r1]
                                0x00, 0x60, 0x22, 0xE9,  //stmdb r2!, {sp, lr}
                                0x02, 0xD0, 0xA0, 0xE1,  //mov   sp, r2
                                0x30, 0xFF, 0x2F, 0xE1,  //blx   r0
                                0x03, 0x00, 0xBD, 0xE8,  //pop   {r0, r1}
                                0x00, 0xD0, 0xA0, 0xE1,  //mov   sp, r0
                                0x11, 0xFF, 0x2F, 0xE1}; //bx    r1

    if(!arm11SvcTable[0x7B])
    {
        memcpy(*freeK11Space, svcBackdoor, 40);

        arm11SvcTable[0x7B] = baseK11VA + *freeK11Space - pos;
        *freeK11Space += 40;
    }
}

void implementSvcGetCFWInfo(u8 *pos, u32 *arm11SvcTable, u32 baseK11VA, u8 **freeK11Space)
{
    memcpy(*freeK11Space, svcGetCFWInfo_bin, svcGetCFWInfo_bin_size);

    CFWInfo *info = (CFWInfo *)memsearch(*freeK11Space, "LUMA", svcGetCFWInfo_bin_size, 4);

    const char *rev = REVISION;

    info->commitHash = COMMIT_HASH;
    info->config = configData.config;
    info->versionMajor = (u8)(rev[1] - '0');
    info->versionMinor = (u8)(rev[3] - '0');

    bool isRelease;

    if(rev[4] == '.')
    {
        info->versionBuild = (u8)(rev[5] - '0');
        isRelease = rev[6] == 0;
    }
    else isRelease = rev[4] == 0;

    info->flags = isRelease ? 1 : 0;

    arm11SvcTable[0x2E] = baseK11VA + *freeK11Space - pos; //Stubbed svc
    *freeK11Space += svcGetCFWInfo_bin_size;
}

void patchTitleInstallMinVersionCheck(u8 *pos, u32 size)
{
    const u8 pattern[] = {0x0A, 0x81, 0x42, 0x02};

    u8 *off = memsearch(pos, pattern, size, sizeof(pattern));

    if(off != NULL) off[4] = 0xE0;
}

void applyLegacyFirmPatches(u8 *pos, FirmwareType firmType)
{
    const patchData twlPatches[] = {
        {{0x1650C0, 0x165D64}, {{ 6, 0x00, 0x20, 0x4E, 0xB0, 0x70, 0xBD }}, 0},
        {{0x173A0E, 0x17474A}, { .type1 = 0x2001 }, 1},
        {{0x174802, 0x17553E}, { .type1 = 0x2000 }, 2},
        {{0x174964, 0x1756A0}, { .type1 = 0x2000 }, 2},
        {{0x174D52, 0x175A8E}, { .type1 = 0x2001 }, 2},
        {{0x174D5E, 0x175A9A}, { .type1 = 0x2001 }, 2},
        {{0x174D6A, 0x175AA6}, { .type1 = 0x2001 }, 2},
        {{0x174E56, 0x175B92}, { .type1 = 0x2001 }, 1},
        {{0x174E58, 0x175B94}, { .type1 = 0x4770 }, 1}
    },
    agbPatches[] = {
        {{0x9D2A8, 0x9DF64}, {{ 6, 0x00, 0x20, 0x4E, 0xB0, 0x70, 0xBD }}, 0},
        {{0xD7A12, 0xD8B8A}, { .type1 = 0xEF26 }, 1}
    };

    /* Calculate the amount of patches to apply. Only count the boot screen patch for AGB_FIRM
       if the matching option was enabled (keep it as last) */
    u32 numPatches = firmType == TWL_FIRM ? (sizeof(twlPatches) / sizeof(patchData)) :
                                            (sizeof(agbPatches) / sizeof(patchData) - !CONFIG(SHOWGBABOOT));
    const patchData *patches = firmType == TWL_FIRM ? twlPatches : agbPatches;

    //Patch
    for(u32 i = 0; i < numPatches; i++)
    {
        switch(patches[i].type)
        {
            case 0:
                memcpy(pos + patches[i].offset[isN3DS ? 1 : 0], patches[i].patch.type0 + 1, patches[i].patch.type0[0]);
                break;
            case 2:
                *(u16 *)(pos + patches[i].offset[isN3DS ? 1 : 0] + 2) = 0;
            case 1:
                *(u16 *)(pos + patches[i].offset[isN3DS ? 1 : 0]) = patches[i].patch.type1;
                break;
        }
    }
}

void patchArm9ExceptionHandlersInstall(u8 *pos, u32 size)
{
    const u8 pattern[] = {0x03, 0xA0, 0xE3, 0x18};

    u32 *off = (u32 *)(memsearch(pos, pattern, size, sizeof(pattern)) + 0x13);

    for(u32 r0 = 0x08000000; *off != 0xE3A01040; off++) //Until mov r1, #0x40
    {
        //Discard everything that's not str rX, [r0, #imm](!)
        if((*off & 0xFE5F0000) != 0xE4000000) continue;

        u32 rD = (*off >> 12) & 0xF,
            offset = (*off & 0xFFF) * ((((*off >> 23) & 1) == 0) ? -1 : 1);
        bool writeback = ((*off >> 21) & 1) != 0,
             pre = ((*off >> 24) & 1) != 0;

        u32 addr = r0 + ((pre || !writeback) ? offset : 0);
        if((addr & 7) != 0 && addr != 0x08000014 && addr != 0x08000004) *off = 0xE1A00000; //nop
        else *off = 0xE5800000 | (rD << 12) | (addr & 0xFFF); //Preserve IRQ and SVC handlers

        if(!pre) addr += offset;
        if(writeback) r0 = addr;
    }
}

u32 getInfoForArm11ExceptionHandlers(u8 *pos, u32 size, u32 *codeSetOffset)
{
    const u8 pattern[] = {0xE3, 0xDC, 0x05, 0xC0}, //Get TitleID from CodeSet
             pattern2[] = {0xE1, 0x0F, 0x00, 0xBD}; //Call exception dispatcher

    u32 *loadCodeSet = (u32 *)(memsearch(pos, pattern, size, sizeof(pattern)) - 0xB);

    *codeSetOffset = *loadCodeSet & 0xFFF;

    return *(u32 *)(memsearch(pos, pattern2, size, sizeof(pattern2)) + 0xD);
}

void patchSvcBreak9(u8 *pos, u32 size, u32 kernel9Address)
{
    /* Stub svcBreak with "bkpt 65535" so we can debug the panic.
       Thanks @yellows8 and others for mentioning this idea on #3dsdev */

    //Look for the svc handler
    const u8 pattern[] = {0x00, 0xE0, 0x4F, 0xE1}; //mrs lr, spsr

    u32 *arm9SvcTable = (u32 *)memsearch(pos, pattern, size, sizeof(pattern));
    while(*arm9SvcTable) arm9SvcTable++; //Look for SVC0 (NULL)

    u32 *addr = (u32 *)(pos + arm9SvcTable[0x3C] - kernel9Address);
    *addr = 0xE12FFF7F;
}

void patchSvcBreak11(u8 *pos, u32 *arm11SvcTable)
{
    //Same as above, for NATIVE_FIRM ARM11
    u32 *addr = (u32 *)(pos + arm11SvcTable[0x3C] - 0xFFF00000);
    *addr = 0xE12FFF7F;
}

void patchKernel9Panic(u8 *pos, u32 size)
{
    const u8 pattern[] = {0xFF, 0xEA, 0x04, 0xD0};

    u32 *off = (u32 *)(memsearch(pos, pattern, size, sizeof(pattern)) - 0x12);
    *off = 0xE12FFF7E;
}

void patchKernel11Panic(u8 *pos, u32 size)
{
    const u8 pattern[] = {0x02, 0x0B, 0x44, 0xE2};

    u32 *off = (u32 *)memsearch(pos, pattern, size, sizeof(pattern));
    *off = 0xE12FFF7E;
}

void patchP9AccessChecks(u8 *pos, u32 size)
{
    const u8 pattern[] = {0xE0, 0x00, 0x40, 0x39};

    u16 *off = (u16 *)memsearch(pos, pattern, size, sizeof(pattern)) - 7;

    off[0] = 0x2001; //mov r0, #1
    off[1] = 0x4770; //bx lr
}

void patchArm11SvcAccessChecks(u32 *arm11SvcHandler)
{
    while(*arm11SvcHandler != 0xE11A0E1B) arm11SvcHandler++; //TST R10, R11,LSL LR
    *arm11SvcHandler = 0xE3B0A001; //MOVS R10, #1
}

void patchK11ModuleChecks(u8 *pos, u32 size, u8 **freeK11Space)
{
    /* We have to detour a function in the ARM11 kernel because builtin modules
       are compressed in memory and are only decompressed at runtime */

    //Check that we have enough free space
    if(*(u32 *)(*freeK11Space + k11modules_bin_size - 4) == 0xFFFFFFFF)
    {
        //Inject our code into the free space
        memcpy(*freeK11Space, k11modules_bin, k11modules_bin_size);

        //Look for the code that decompresses the .code section of the builtin modules
        const u8 pattern[] = {0xE5, 0x48, 0x00, 0x9D};

        u32 *off = (u32 *)(memsearch(pos, pattern, size, sizeof(pattern)) - 0xB);

        //Inject a jump (BL) instruction to our code at the offset we found
        *off = 0xEB000000 | (((((u32)*freeK11Space) - ((u32)off + 8)) >> 2) & 0xFFFFFF);

        *freeK11Space += k11modules_bin_size;
    }
}

void patchUnitInfoValueSet(u8 *pos, u32 size)
{
    //Look for UNITINFO value being set during kernel sync
    const u8 pattern[] = {0x01, 0x10, 0xA0, 0x13};

    u8 *off = memsearch(pos, pattern, size, sizeof(pattern));

    off[0] = isDevUnit ? 0 : 1;
    off[3] = 0xE3;
}
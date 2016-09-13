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

#include "fs.h"
#include "memory.h"
#include "strings.h"
#include "cache.h"
#include "screen.h"
#include "fatfs/ff.h"
#include "buttons.h"
#include "../build/loader.h"

static FATFS sdFs,
             nandFs;

void mountFs(void)
{
    f_mount(&sdFs, "0:", 1);
    f_mount(&nandFs, "1:", 0);
}

u32 fileRead(void *dest, const char *path, u32 maxSize)
{
    FIL file;
    u32 ret = 0;

    if(f_open(&file, path, FA_READ) == FR_OK)
    {
        u32 size = f_size(&file);
        if(dest == NULL) ret = size;
        else if(!(maxSize > 0 && size > maxSize))
            f_read(&file, dest, size, (unsigned int *)&ret);
        f_close(&file);
    }

    return ret;
}

u32 getFileSize(const char *path)
{
    return fileRead(NULL, path, 0);
}

bool fileWrite(const void *buffer, const char *path, u32 size)
{
    FIL file;

    FRESULT result = f_open(&file, path, FA_WRITE | FA_OPEN_ALWAYS);

    if(result == FR_OK)
    {
        unsigned int written;
        f_write(&file, buffer, size, &written);
        f_close(&file);

        return true;
    }

    if(result == FR_NO_PATH)
    {
        for(u32 i = 1; path[i] != 0; i++)
           if(path[i] == '/')
           {
                char folder[i + 1];
                memcpy(folder, path, i);
                folder[i] = 0;
                f_mkdir(folder);
           }

        return fileWrite(buffer, path, size);
    }

    return false;
}

void fileDelete(const char *path)
{
    f_unlink(path);
}

void loadPayload(u32 pressed)
{
    const char *pattern;

    if(pressed & BUTTON_LEFT) pattern = PATTERN("left");
    else if(pressed & BUTTON_RIGHT) pattern = PATTERN("right");
    else if(pressed & BUTTON_UP) pattern = PATTERN("up");
    else if(pressed & BUTTON_DOWN) pattern = PATTERN("down");
    else if(pressed & BUTTON_START) pattern = PATTERN("start");
    else if(pressed & BUTTON_B) pattern = PATTERN("b");
    else if(pressed & BUTTON_X) pattern = PATTERN("x");
    else if(pressed & BUTTON_Y) pattern = PATTERN("y");
    else if(pressed & BUTTON_R1) pattern = PATTERN("r");
    else if(pressed & BUTTON_A) pattern = PATTERN("a");
    else pattern = PATTERN("select");

    DIR dir;
    FILINFO info;
    char path[28] = "/puma/payloads";

    FRESULT result = f_findfirst(&dir, &info, path, pattern);

    f_closedir(&dir);

    if(result == FR_OK && info.fname[0])
    {
        u32 *loaderAddress = (u32 *)0x24FFFF00;
        u8 *payloadAddress = (u8 *)0x24F00000;

        memcpy(loaderAddress, loader, loader_size);

        concatenateStrings(path, "/");
        concatenateStrings(path, info.altname);

        u32 payloadSize = fileRead(payloadAddress, path, (u8 *)loaderAddress - payloadAddress);

        if(payloadSize > 0)
        {
            loaderAddress[1] = payloadSize;

            initScreens();

            flushDCacheRange(loaderAddress, loader_size);
            flushICacheRange(loaderAddress, loader_size);

            ((void (*)())loaderAddress)();
        }
    }
}

u32 firmRead(void *dest, u32 firmType)
{
    const char *firmFolders[4][2] = {{ "00000002", "20000002" },
                                    { "00000102", "20000102" },
                                    { "00000202", "20000202" },
                                    { "00000003", "20000003" }};

    char path[48] = "1:/title/00040138/";
    concatenateStrings(path, firmFolders[firmType][isN3DS ? 1 : 0]);
    concatenateStrings(path, "/content");

    DIR dir;
    FILINFO info;

    f_opendir(&dir, path);

    u32 firmVersion = 0xFFFFFFFF;

    //Parse the target directory
    while(f_readdir(&dir, &info) == FR_OK && info.fname[0])
    {
        //Not a cxi
        if(info.altname[9] != 'A') continue;

        //Convert the .app name to an integer
        u32 tempVersion = 0;
        for(char *tmp = info.altname; *tmp != '.'; tmp++)
        {
            tempVersion <<= 4;
            tempVersion += *tmp > '9' ? *tmp - 'A' + 10 : *tmp - '0';
        }

        //Found an older cxi
        if(tempVersion < firmVersion) firmVersion = tempVersion;
    }

    f_closedir(&dir);

    //Complete the string with the .app name
    concatenateStrings(path, "/00000000.app");

    //Convert back the .app name from integer to array
    hexItoa(firmVersion, &path[35], 8);

    fileRead(dest, path, 0);

    return firmVersion;
}

#ifdef DEV
void findDumpFile(const char *path, char *fileName)
{
    DIR dir;
    FILINFO info;
    u32 n = 0;

    while(f_findfirst(&dir, &info, path, fileName) == FR_OK && info.fname[0])
    {
        u32 i = 18,
            tmp = ++n;

        while(tmp > 0)
        {
            fileName[i--] = '0' + (tmp % 10);
            tmp /= 10;
        }
    }

    f_closedir(&dir);
}
#endif
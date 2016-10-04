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

#pragma once

#include "types.h"

#define CFG_BOOTENV    (*(vu32 *)0x10010000)
#define CFG_UNITINFO   (*(vu8  *)0x10010010)
#define PDN_MPCORE_CFG (*(vu32 *)0x10140FFC)
#define PDN_SPI_CNT    (*(vu32 *)0x101401C0)

//FIRM Header layout
typedef struct firmSectionHeader {
    u32 offset;
    u8 *address;
    u32 size;
    u32 procType;
    u8 hash[0x20];
} firmSectionHeader;

typedef struct firmHeader {
    u32 magic;
    u32 reserved1;
    u8 *arm11Entry;
    u8 *arm9Entry;
    u8 reserved2[0x30];
    firmSectionHeader section[4];
} firmHeader;
 
static inline u32 loadFirm(FirmwareType *firmType, FirmwareSource firmSource, bool loadFromSd);
static inline void patchNativeFirm(u32 firmVersion, FirmwareSource nandType, u32 emuHeader, u32 devMode);
static inline void patchLegacyFirm(FirmwareType firmType, u32 firmVersion, u32 devMode);
static inline void patch1x2xNativeAndSafeFirm(u32 devMode);
static inline void copySection0AndInjectSystemModules(FirmwareType firmType, bool loadFromSd);
static inline void launchFirm(FirmwareType firmType, bool loadFromSd);
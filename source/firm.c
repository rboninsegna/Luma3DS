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

#include "firm.h"
#include "config.h"
#include "utils.h"
#include "fs.h"
#include "patches.h"
#include "memory.h"
#include "strings.h"
#include "cache.h"
#include "emunand.h"
#include "crypto.h"
#include "draw.h"
#include "screen.h"
#include "buttons.h"
#include "pin.h"
#include "../build/injector.h"

#ifdef DEV
#include "exceptions.h"
#endif

extern u16 launchedFirmTidLow[8]; //Defined in start.s

static firmHeader *firm = (firmHeader *)0x24000000;
static const firmSectionHeader *section;

u32 emuOffset;
bool isN3DS,
     isDevUnit,
     isFirmlaunch;
CfgData configData;
FirmwareSource firmSource;

void main(void)
{
    bool isA9lh;
    u32 configTemp,
        emuHeader;
    FirmwareType firmType;
    FirmwareSource nandType;
    ConfigurationStatus needConfig;

    //Detect the console being used
    isN3DS = PDN_MPCORE_CFG == 7;

    //Detect dev units
    isDevUnit = CFG_UNITINFO != 0;

    //Mount filesystems. CTRNAND will be mounted only if/when needed
    mountFs();

    //Attempt to read the configuration file
    needConfig = readConfig() ? MODIFY_CONFIGURATION : CREATE_CONFIGURATION;

#ifdef DEV
    detectAndProcessExceptionDumps();
#endif

    //Determine if this is a firmlaunch boot
    if(launchedFirmTidLow[5] != 0)
    {
        isFirmlaunch = true;

        if(needConfig == CREATE_CONFIGURATION) mcuReboot();

        //'0' = NATIVE_FIRM, '1' = TWL_FIRM, '2' = AGB_FIRM
        firmType = launchedFirmTidLow[7] == u'3' ? SAFE_FIRM : (FirmwareType)(launchedFirmTidLow[5] - u'0');

        nandType = (FirmwareSource)BOOTCFG_NAND;
        firmSource = (FirmwareSource)BOOTCFG_FIRM;
        isA9lh = BOOTCFG_A9LH != 0;

#ifdef DEV
        if(isA9lh) installArm9Handlers();
#endif
    }
    else
    {
        isFirmlaunch = false;
        firmType = NATIVE_FIRM;

        //Determine if booting with A9LH
        isA9lh = !PDN_SPI_CNT;

#ifdef DEV
        if(isA9lh) installArm9Handlers();
#endif

        //Get pressed buttons
        u32 pressed = HID_PAD;

        //Save old options and begin saving the new boot configuration
        configTemp = (configData.config & 0xFFFFFE00) | ((u32)isA9lh << 6);

        //If it's a MCU reboot, try to force boot options
        if(isA9lh && CFG_BOOTENV)
        {
            //Always force a sysNAND boot when quitting AGB_FIRM
            if(CFG_BOOTENV == 7)
            {
                nandType = FIRMWARE_SYSNAND;
                firmSource = CONFIG(USESYSFIRM) ? FIRMWARE_SYSNAND : (FirmwareSource)BOOTCFG_FIRM;
                needConfig = DONT_CONFIGURE;

                //Flag to prevent multiple boot options-forcing
                configTemp |= 1 << 7;
            }

            /* Else, force the last used boot options unless a button is pressed
               or the no-forcing flag is set */
            else if(needConfig != CREATE_CONFIGURATION && !pressed && !BOOTCFG_NOFORCEFLAG)
            {
                nandType = (FirmwareSource)BOOTCFG_NAND;
                firmSource = (FirmwareSource)BOOTCFG_FIRM;
                needConfig = DONT_CONFIGURE;
            }
        }

        //Boot options aren't being forced
        if(needConfig != DONT_CONFIGURE)
        {
            bool pinExists = MULTICONFIG(PIN) != 0 && verifyPin();

            //If no configuration file exists or SELECT is held, load configuration menu
            bool shouldLoadConfigMenu = needConfig == CREATE_CONFIGURATION || ((pressed & BUTTON_SELECT) && !(pressed & BUTTON_L1));

            if(shouldLoadConfigMenu)
            {
                configMenu(pinExists);

                //Update pressed buttons
                pressed = HID_PAD;
            }

            if(isA9lh && !CFG_BOOTENV && pressed == SAFE_MODE)
            {
                nandType = FIRMWARE_SYSNAND;
                firmSource = FIRMWARE_SYSNAND;

                //Flag to tell loader to init SD
                configTemp |= 1 << 8;

                //If the PIN has been verified, wait to make it easier to press the SAFE_MODE combo
                if(pinExists && !shouldLoadConfigMenu)
                {
                    while(HID_PAD & PIN_BUTTONS);
                    chrono(2);
                }
            }
            else
            {
                if(CONFIG(PAYLOADSPLASH) && loadSplash()) pressed = HID_PAD;

                /* If L and R/A/Select or one of the single payload buttons are pressed,
                   chainload an external payload */
                bool shouldLoadPayload = ((pressed & SINGLE_PAYLOAD_BUTTONS) && !(pressed & (BUTTON_L1 | BUTTON_R1))) ||
                                         ((pressed & L_PAYLOAD_BUTTONS) && (pressed & BUTTON_L1));

                if(shouldLoadPayload) loadPayload(pressed);

                if(!CONFIG(PAYLOADSPLASH)) loadSplash();

                //Determine if the user chose to use the SysNAND FIRM as default for a R boot
                bool useSysAsDefault = isA9lh ? CONFIG(USESYSFIRM) : false;

                //If R is pressed, boot the non-updated NAND with the FIRM of the opposite one
                if(pressed & BUTTON_R1)
                {
                    nandType = useSysAsDefault ? FIRMWARE_EMUNAND : FIRMWARE_SYSNAND;
                    firmSource = useSysAsDefault ? FIRMWARE_SYSNAND : FIRMWARE_EMUNAND;
                }

                /* Else, boot the NAND the user set to autoboot or the opposite one, depending on L,
                   with their own FIRM */
                else
                {
                    nandType = (CONFIG(AUTOBOOTSYS) != !(pressed & BUTTON_L1)) ? FIRMWARE_EMUNAND : FIRMWARE_SYSNAND;
                    firmSource = nandType;
                }

                //If we're booting EmuNAND or using EmuNAND FIRM, determine which one from the directional pad buttons, or otherwise from the config
                if(nandType == FIRMWARE_EMUNAND || firmSource == FIRMWARE_EMUNAND)
                {
                    FirmwareSource temp;
                    switch(pressed & EMUNAND_BUTTONS)
                    {
                        case BUTTON_UP:
                            temp = FIRMWARE_EMUNAND;
                            break;
                        case BUTTON_RIGHT:
                            temp = FIRMWARE_EMUNAND2;
                            break;
                        case BUTTON_DOWN:
                            temp = FIRMWARE_EMUNAND3;
                            break;
                        case BUTTON_LEFT:
                            temp = FIRMWARE_EMUNAND4;
                            break;
                        default:
                            temp = (FirmwareSource)(1 + MULTICONFIG(DEFAULTEMU));
                            break;
                    }

                    if(nandType == FIRMWARE_EMUNAND) nandType = temp;
                    else firmSource = temp;
                }
            }
        }
    }

    //If we need to boot EmuNAND, make sure it exists
    if(nandType != FIRMWARE_SYSNAND)
    {
        locateEmuNand(&emuHeader, &nandType);
        if(nandType == FIRMWARE_SYSNAND) firmSource = FIRMWARE_SYSNAND;
    }

    //Same if we're using EmuNAND as the FIRM source
    else if(firmSource != FIRMWARE_SYSNAND)
        locateEmuNand(&emuHeader, &firmSource);

    if(!isFirmlaunch)
    {
        configTemp |= (u32)nandType | ((u32)firmSource << 3);
        writeConfig(needConfig, configTemp);
    }

    u32 firmVersion = loadFirm(&firmType, firmSource);

    switch(firmType)
    {
        case NATIVE_FIRM:
            patchNativeFirm(firmVersion, nandType, emuHeader, isA9lh);
            break;
        case SAFE_FIRM:
        case NATIVE_FIRM1X2X:
            if(isA9lh) patch1x2xNativeAndSafeFirm();
            break;
        default:
            //Skip patching on unsupported O3DS AGB/TWL FIRMs
            if(isN3DS || firmVersion >= (firmType == TWL_FIRM ? 0x16 : 0xB)) patchLegacyFirm(firmType);
            break;
    }

    launchFirm(firmType);
}

#ifdef DEV
static inline u32 loadFirm(FirmwareType *firmType, FirmwareSource firmSource)
{
    section = firm->section;

    const char *firmwareFiles[4] = {
        "/puma/firmware.bin",
        "/puma/firmware_twl.bin",
        "/puma/firmware_agb.bin",
        "/puma/firmware_safe.bin"
    };

    //Load FIRM from CTRNAND
    u32 firmVersion = firmRead(firm, (u32)*firmType);

    bool loadFromSd = false;

    if(!isN3DS && *firmType == NATIVE_FIRM)
    {
        if(firmVersion < 0x18)
        {
            //We can't boot < 3.x EmuNANDs
            if(firmSource != FIRMWARE_SYSNAND) 
                error("An old unsupported EmuNAND has been detected.\nPuma33DS is unable to boot it");

            if(BOOTCFG_SAFEMODE != 0) error("SAFE_MODE is not supported on 1.x/2.x FIRM");

            *firmType = NATIVE_FIRM1X2X;
        }

        //We can't boot a 3.x/4.x NATIVE_FIRM, load one from SD
        else if(firmVersion < 0x25) loadFromSd = true;
    }

    //Check that the SD FIRM is right for the console from the ARM9 section address
    if(fileRead(firm, *firmType == NATIVE_FIRM1X2X ? firmwareFiles[0] : firmwareFiles[(u32)*firmType], 0x400000) &&
       ((section[3].offset ? section[3].address : section[2].address) == (isN3DS ? (u8 *)0x8006000 : (u8 *)0x8006800)))
        firmVersion = 0xFFFFFFFF;
    else
    {
        if(loadFromSd) error("An old unsupported FIRM has been detected.\nCopy a valid firmware.bin in /puma to boot");
        decryptExeFs((u8 *)firm);
    }

    return firmVersion;
}
#else
static inline u32 loadFirm(FirmwareType *firmType, FirmwareSource firmSource)
{
    section = firm->section;

    //Load FIRM from CTRNAND
    u32 firmVersion = firmRead(firm, (u32)*firmType);

    if(!isN3DS && *firmType == NATIVE_FIRM)
    {
        if(firmVersion < 0x18)
        {
            //We can't boot < 3.x EmuNANDs
            if(firmSource != FIRMWARE_SYSNAND) 
                error("An old unsupported EmuNAND has been detected.\nPuma33DS is unable to boot it");

            if(BOOTCFG_SAFEMODE != 0) error("SAFE_MODE is not supported on 1.x/2.x FIRM");

            *firmType = NATIVE_FIRM1X2X;
        }

        //We can't boot a 3.x/4.x NATIVE_FIRM, load one from SD
        else if(firmVersion < 0x25)
        {
            if(!fileRead(firm, "/puma/firmware.bin", 0x400000) || section[2].address != (u8 *)0x8006800)
                error("An old unsupported FIRM has been detected.\nCopy a valid firmware.bin in /puma to boot");

            //No assumption regarding FIRM version
            firmVersion = 0xFFFFFFFF;
        }
    }

    if(firmVersion != 0xFFFFFFFF) decryptExeFs((u8 *)firm);

    return firmVersion;
}
#endif

static inline void patchNativeFirm(u32 firmVersion, FirmwareSource nandType, u32 emuHeader, bool isA9lh)
{
    u8 *arm9Section = (u8 *)firm + section[2].offset,
       *arm11Section1 = (u8 *)firm + section[1].offset;

    if(isN3DS)
    {
        //Decrypt ARM9Bin and patch ARM9 entrypoint to skip arm9loader
        arm9Loader(arm9Section);
        firm->arm9Entry = (u8 *)0x801B01C;
    }

    //Sets the 7.x NCCH KeyX and the 6.x gamecard save data KeyY on >= 6.0 O3DS FIRMs, if not using A9LH
    else if(!isA9lh && firmVersion >= 0x29) setRSAMod0DerivedKeys();

    //Find the Process9 .code location, size and memory address
    u32 process9Size,
        process9MemAddr;
    u8 *process9Offset = getProcess9(arm9Section + 0x15000, section[2].size - 0x15000, &process9Size, &process9MemAddr);

#ifdef DEV
    //Find Kernel11 SVC table and handler, exceptions page and free space locations
    u32 baseK11VA;
    u8 *freeK11Space;
    u32 *arm11SvcHandler, 
        *arm11ExceptionsPage,
        *arm11SvcTable = getKernel11Info(arm11Section1, section[1].size, &baseK11VA, &freeK11Space, &arm11SvcHandler, &arm11ExceptionsPage);
#else
    //Find Kernel11 SVC table and free space locations
    u32 baseK11VA;
    u8 *freeK11Space;
    u32 *arm11SvcTable = getKernel11Info(arm11Section1, section[1].size, &baseK11VA, &freeK11Space);
#endif

    //Apply signature patches
    patchSignatureChecks(process9Offset, process9Size);

    //Apply EmuNAND patches
    if(nandType != FIRMWARE_SYSNAND)
    {
        u32 branchAdditive = (u32)firm + section[2].offset - (u32)section[2].address;
        patchEmuNand(arm9Section, section[2].size, process9Offset, process9Size, emuHeader, branchAdditive);
    }

    //Apply FIRM0/1 writes patches on sysNAND to protect A9LH
    else if(isA9lh) patchFirmWrites(process9Offset, process9Size);

    //Apply firmlaunch patches
    patchFirmlaunches(process9Offset, process9Size, process9MemAddr);

    //11.0 FIRM patches
    if(firmVersion >= (isN3DS ? 0x21 : 0x52))
    {
        //Apply anti-anti-DG patches
        patchTitleInstallMinVersionCheck(process9Offset, process9Size);

        //Restore svcBackdoor
        reimplementSvcBackdoor(arm11Section1, arm11SvcTable, baseK11VA, &freeK11Space);
    }

    implementSvcGetCFWInfo(arm11Section1, arm11SvcTable, baseK11VA, &freeK11Space);

#ifdef DEV
    //Apply UNITINFO patch
    if(MULTICONFIG(DEVOPTIONS) == 1) patchUnitInfoValueSet(arm9Section, section[2].size);

    if(isA9lh && MULTICONFIG(DEVOPTIONS) != 2)
    {
        //Install ARM11 exception handlers
        u32 codeSetOffset;
        u32 stackAddress = getInfoForArm11ExceptionHandlers(arm11Section1, section[1].size, &codeSetOffset);
        installArm11Handlers(arm11ExceptionsPage, stackAddress, codeSetOffset);

        //Kernel9/Process9 debugging
        patchArm9ExceptionHandlersInstall(arm9Section, section[2].size);
        patchSvcBreak9(arm9Section, section[2].size, (u32)section[2].address);
        patchKernel9Panic(arm9Section, section[2].size);

        //Stub svcBreak11 with "bkpt 65535"
        patchSvcBreak11(arm11Section1, arm11SvcTable);

        //Stub kernel11Panic with "bkpt 65534"
        patchKernel11Panic(arm11Section1, section[1].size);
    }

    if(CONFIG(PATCHACCESS))
    {
        patchArm11SvcAccessChecks(arm11SvcHandler);
        patchK11ModuleChecks(arm11Section1, section[1].size, &freeK11Space);
        patchP9AccessChecks(process9Offset, process9Size);
    }
#endif
}

static inline void patchLegacyFirm(FirmwareType firmType)
{
    u8 *arm9Section = (u8 *)firm + section[3].offset;
    
    //On N3DS, decrypt ARM9Bin and patch ARM9 entrypoint to skip arm9loader
    if(isN3DS)
    {
        arm9Loader(arm9Section);
        firm->arm9Entry = (u8 *)0x801301C;
    }

    applyLegacyFirmPatches((u8 *)firm, firmType);

#ifdef DEV
    //Apply UNITINFO patch
    if(MULTICONFIG(DEVOPTIONS) == 1) patchUnitInfoValueSet(arm9Section, section[3].size);
#endif
}

static inline void patch1x2xNativeAndSafeFirm(void)
{
    u8 *arm9Section = (u8 *)firm + section[2].offset;

    if(isN3DS)
    {
        //Decrypt ARM9Bin and patch ARM9 entrypoint to skip arm9loader
        arm9Loader(arm9Section);
        firm->arm9Entry = (u8 *)0x801B01C;

        patchFirmWrites(arm9Section, section[2].size);
    }
    else patchOldFirmWrites(arm9Section, section[2].size);

#ifdef DEV
    if(MULTICONFIG(DEVOPTIONS) != 2)
    {
        //Kernel9/Process9 debugging
        patchArm9ExceptionHandlersInstall(arm9Section, section[2].size);
        patchSvcBreak9(arm9Section, section[2].size, (u32)section[2].address);
    }
#endif
}

#ifdef DEV
static inline void copySection0AndInjectSystemModules(FirmwareType firmType)
{
    u32 srcModuleSize,
        dstModuleSize;

    for(u8 *src = (u8 *)firm + section[0].offset, *srcEnd = src + section[0].size, *dst = section[0].address;
        src < srcEnd; src += srcModuleSize, dst += dstModuleSize)
    {
        srcModuleSize = *(u32 *)(src + 0x104) * 0x200;
        const char *moduleName = (char *)(src + 0x200);

        char fileName[30] = "/puma/sysmodules/";
        const char *ext = ".cxi";

        //Read modules from files if they exist
        concatenateStrings(fileName, moduleName);
        concatenateStrings(fileName, ext);

        u32 fileSize = fileRead(dst, fileName, 2 * srcModuleSize);
        if(fileSize) dstModuleSize = fileSize;
        else
        {
            const void *module;

            if(firmType == NATIVE_FIRM && memcmp(moduleName, "loader", 6) == 0)
            {
                module = injector;
                dstModuleSize = injector_size;
            }
            else
            {
                module = src;
                dstModuleSize = srcModuleSize;
            }

            memcpy(dst, module, dstModuleSize);
        }
    }
}
#else
static inline void copySection0AndInjectSystemModules(void)
{
    u32 srcModuleSize,
        dstModuleSize;

    for(u8 *src = (u8 *)firm + section[0].offset, *srcEnd = src + section[0].size, *dst = section[0].address;
        src < srcEnd; src += srcModuleSize, dst += dstModuleSize)
    {
        srcModuleSize = *(u32 *)(src + 0x104) * 0x200;
        const char *moduleName = (const char *)(src + 0x200);

        const void *module;

        if(memcmp(moduleName, "loader", 6) == 0)
        {
            module = injector;
            dstModuleSize = injector_size;
        }
        else
        {
            module = src;
            dstModuleSize = srcModuleSize;
        }

        memcpy(dst, module, dstModuleSize);
    }
}
#endif

static inline void launchFirm(FirmwareType firmType)
{
#ifdef DEV
    //Allow module injection and/or inject 3ds_injector on new NATIVE_FIRMs and LGY FIRMs
    u32 sectionNum;
    if(firmType != SAFE_FIRM && firmType != NATIVE_FIRM1X2X)
    {
        copySection0AndInjectSystemModules(firmType);
        sectionNum = 1;
    }
    else sectionNum = 0;
#else
    //If we're booting NATIVE_FIRM, section0 needs to be copied separately to inject 3ds_injector
    u32 sectionNum;
    if(firmType == NATIVE_FIRM)
    {
        copySection0AndInjectSystemModules();
        sectionNum = 1;
    }
    else sectionNum = 0;
#endif

    //Copy FIRM sections to respective memory locations
    for(; sectionNum < 4 && section[sectionNum].size; sectionNum++)
        memcpy(section[sectionNum].address, (u8 *)firm + section[sectionNum].offset, section[sectionNum].size);

    //Determine the ARM11 entry to use
    vu32 *arm11;
    if(isFirmlaunch) arm11 = (u32 *)0x1FFFFFFC;
    else
    {
        deinitScreens();
        arm11 = (u32 *)0x1FFFFFF8;
    }

    //Set ARM11 kernel entrypoint
    *arm11 = (u32)firm->arm11Entry;

    flushEntireDCache(); //Ensure that all memory transfers have completed and that the data cache has been flushed 
    flushEntireICache();

    //Final jump to ARM9 kernel
    ((void (*)())firm->arm9Entry)();
}
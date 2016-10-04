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

#include "config.h"
#include "memory.h"
#include "fs.h"
#include "utils.h"
#include "screen.h"
#include "draw.h"
#include "buttons.h"
#include "pin.h"

bool readConfig(void)
{
    if(fileRead(&configData, CONFIG_PATH, sizeof(CfgData)) != sizeof(CfgData) ||
       memcmp(configData.magic, "CONF", 4) != 0 ||
       configData.formatVersionMajor != CONFIG_VERSIONMAJOR ||
       configData.formatVersionMinor != CONFIG_VERSIONMINOR)
    {
        configData.config = 0;
        return false;
    }

    return true;
}

void writeConfig(ConfigurationStatus needConfig, u32 configTemp)
{
    /* If the configuration is different from previously, overwrite it.
       Just the no-forcing flag being set is not enough */
    if(needConfig == CREATE_CONFIGURATION || (configTemp & 0xFFFFFF7F) != configData.config)
    {
        if(needConfig == CREATE_CONFIGURATION)
        {
            memcpy(configData.magic, "CONF", 4);
            configData.formatVersionMajor = CONFIG_VERSIONMAJOR;
            configData.formatVersionMinor = CONFIG_VERSIONMINOR;
        }

        //Merge the new options and new boot configuration
        configData.config = (configData.config & 0xFFFFFE00) | (configTemp & 0x1FF);

        if(!fileWrite(&configData, CONFIG_PATH, sizeof(CfgData)))
            error("Error writing the configuration file");
    }
}

void configMenu(bool oldPinStatus, u32 oldPinMode)
{
    const char *multiOptionsText[]  = { "Default EmuNAND: 1( ) 2( ) 3( ) 4( )",
                                        "Screen brightness: 4( ) 3( ) 2( ) 1( )",
                                        "Splash: Off( ) Before( ) After( ) payloads",
                                        "PIN lock: Off( ) 4( ) 6( ) 8( ) digits",
                                        "New 3DS CPU: Off( ) Clock( ) L2( ) Clock+L2( )",
                                        "Dev. features: Off( ) ErrDisp( ) UNITINFO( )"
                                      };

    const char *singleOptionsText[] = { "( ) Autoboot SysNAND",
                                        "( ) Use SysNAND FIRM if booting with R (A9LH)",
                                        "( ) Enable FIRMs and modules loading from SD",
                                        "( ) Use custom path",
                                        "( ) Enable region/language emu. and ext. .code",
                                        "( ) Show NAND or user string in System Settings",
                                        "( ) Show GBA boot screen in patched AGB_FIRM",
                                        "( ) Patch SVC/service/archive/ARM9 access",
										"( ) Region free Home",
										"( ) Block auto/mandatory updates",
										"( ) SecureInfo_C and unsigned SI_A support",
										"( ) Force TestMenu instead of Home"
                                      };

    const char *optionsDescription[]  = { "Select the default EmuNAND.\n\n"
                                          "It will be booted when no\n"
                                          "directional pad buttons are pressed.",

                                          "Select the screen brightness.",

                                          "Enable splash screen support.\n\n"
                                          "\t* 'Before payloads' displays it\n"
                                          "before booting payloads\n"
                                          "(intended for splashes that display\n"
                                          "button hints).\n\n"
                                          "\t* 'After payloads' displays it\n"
                                          "afterwards.",

                                          "Activate a PIN lock.\n\n"
                                          "The PIN will be asked each time\n"
                                          "Puma33DS boots.\n"
                                          "4, 6 or 8 digits can be selected.\n"
                                          "The ABXY buttons and the directional\n"
                                          "pad buttons can be used as keys.\n\n"
                                          "A message can also be displayed\n"
                                          "by creating /puma/pinmessage.txt.",

                                          "Select the New 3DS CPU mode.\n\n"
                                          "It will be always enabled.\n\n"
                                          "'Clock+L2' can cause issues with some\n"
                                          "games.",

                                          "Select the developer features.\n\n"
                                          "\t* 'Off' disables exception handlers.\n"
                                          "\t* 'ErrDisp' displays debug info\n"
                                          "on the 'An error has occurred' screen.\n" //length of a line
                                          "\t* 'UNITINFO' makes the console be\n"
                                          "always detected as a development unit,\n"
                                          "which breaks online features and\n"
                                          "Amiibos but enhances some dev apps).",

                                          "If enabled SysNAND will be launched on\n"
                                          "boot. Otherwise, an EmuNAND will.\n\n"
                                          "Hold L on boot to override.\n\n"
                                          "To use a different EmuNAND from the\n"
                                          "default, hold a directional pad button\n"
                                          "(Up/Right/Down/Left equal EmuNANDs\n"
                                          "1/2/3/4).",

                                          "If enabled, when holding R on boot\n"
                                          "EmuNAND will be booted with the\n"
                                          "SysNAND FIRM.\n\n"
                                          "Otherwise, SysNAND will be booted\n"
                                          "with an EmuNAND FIRM.\n\n"
                                          "To use a different EmuNAND from the\n"
                                          "default, hold a directional pad button\n"
                                          "(Up/Right/Down/Left equal EmuNANDs\n"
                                          "1/2/3/4), also add A if you have\n"
                                          "a matching payload.",

                                          "Enables the loading of kernel modules\n"
                                          "from /puma/sysmodules/*.cxi.\n\n"
                                          "Kernels (and cetk, if encrypted)\n"
										  "will also be loaded from /puma/\n"
										  "firmware{|_twl|_agb|_safe}.bin and\n" 
										  "cetk{|_twl|_agb|_safe}",

                                          "Activate built-in pathchanger.\n\n"
                                          "To use this option, you must type\n"
										  "the new path of Puma33DS's .bin\n"
										  "into /puma/path.txt.",

                                          "Enable overriding the region and\n"
                                          "language configuration and the usage\n"
                                          "of patched code binaries for specific\n"
                                          "games.\n\n"
                                          "Also makes certain DLCs for\n"
                                          "out-of-region games work.\n\n"
                                          "Country spoofing will also be enabled.",

                                          "Show the currently booted NAND in\n"
                                          "System Settings (Sys = SysNAND,\n"
                                          "Emu = EmuNAND 1, EmuX = EmuNAND X,\n"
                                          "SysE = SysNAND with EmuNAND 1 FIRM,\n"
                                          "SyEX = SysNAND with EmuNAND X FIRM,\n"
                                          "EmXS = EmuNAND X with SysNAND FIRM)\n"
                                          "or an user-defined custom string\n"
                                          "(check the Readme for instructions)",

                                          "Show the GBA boot screen when\n"
                                          "launching GBA games.\n\n"
										  "(Breaks bad ROMs with an incorrect\n"
										  "Nintendo logo)",

                                          "Disable SVC, service, archive and ARM9\n"
                                          "exheader access checks.\n\n"
                                          "The service and archive patches\n"
                                          "don't work on New 3DS FIRMs between\n"
                                          "9.3 and 10.4.\n\n"
                                          "Only change this if you know what you\n"
                                          "are doing!",
										  
										  "Make foreign titles visible on HOME\n"//regionfree
                                          "(and block card updates).\n"
										  "\t* Some applications may also need\n"
										  "language/region spoofing to work.\n"
										  "\t* Causes black squares to appear\n"
										  "on <9.0 HOME versions.",
										  
										  "Blocks automatic update downloads\n"//antiupdate
                                          " (\"nag screen\").\n"
										  "Spoofs FRIENDS driver version to 7.\n"
										  "Blocks card updates, and attempts to\n"
										  "prevent eShop/NZone from checking\n"
										  "if the system needs updating.",
										  
										  "If /rw/sys/SecureInfo_C exists\n"//secureinfo
                                          "it will be used instead of _A.\n"
										  "Also enables booting with \n"
										  "an unsigned SecureInfo file.",
										  
										  "Forces TestMenu to be launched\n"//testmenu
                                          "instead of HOME.\n"
										  "Requires TestMenu to be installed,\n"
										  "and for best results, DevMenu too"
                                       };


    struct multiOption {
        u32 posXs[4];
        u32 posY;
        u32 enabled;
    } multiOptions[] = {
        { .posXs = {19, 24, 29, 34} },
        { .posXs = {21, 26, 31, 36} },
        { .posXs = {12, 22, 31, 0}  },
        { .posXs = {14, 19, 24, 29} },
        { .posXs = {17, 26, 32, 44} },
        { .posXs = {19, 30, 42, 0}  }
    };

    //Calculate the amount of the various kinds of options and pre-select the first single one
    u32 multiOptionsAmount = sizeof(multiOptions) / sizeof(struct multiOption),
        singleOptionsAmount = sizeof(singleOptionsText) / sizeof(char *),
        totalIndexes = multiOptionsAmount + singleOptionsAmount - 1,
        selectedOption = multiOptionsAmount;

    struct singleOption {
        u32 posY;
        bool enabled;
    } singleOptions[singleOptionsAmount];

    //Parse the existing options
    for(u32 i = 0; i < multiOptionsAmount; i++)
        multiOptions[i].enabled = MULTICONFIG(i);
    for(u32 i = 0; i < singleOptionsAmount; i++)
        singleOptions[i].enabled = CONFIG(i);

    initScreens();

    drawString(CONFIG_TITLE, true, 10, 10, COLOR_TITLE);
    drawString("Press A to select, START to save", true, 10, 30, COLOR_WHITE);

    //Character to display a selected option
    char selected = 'x';

    u32 endPos = 42;

    //Display all the multiple choice options in white
    for(u32 i = 0; i < multiOptionsAmount; i++)
    {
        if(!(i == NEWCPU && !isN3DS))
        {
            multiOptions[i].posY = endPos + SPACING_Y;
            endPos = drawString(multiOptionsText[i], true, 10, multiOptions[i].posY, COLOR_WHITE);
            drawCharacter(selected, true, 10 + multiOptions[i].posXs[multiOptions[i].enabled] * SPACING_X, multiOptions[i].posY, COLOR_WHITE);
        }
    }

    endPos += SPACING_Y / 2;
    u32 color = COLOR_RED;

    //Display all the normal options in white except for the first one
    for(u32 i = 0; i < singleOptionsAmount; i++)
    {
        singleOptions[i].posY = endPos + SPACING_Y;
        endPos = drawString(singleOptionsText[i], true, 10, singleOptions[i].posY, color);
        if(singleOptions[i].enabled) drawCharacter(selected, true, 10 + SPACING_X, singleOptions[i].posY, color);
        color = COLOR_WHITE;
    }

    drawString(optionsDescription[selectedOption], false, 10, 10, COLOR_WHITE);

    u32 pressed = 0;

    //Boring configuration menu
    while(pressed != BUTTON_START)
    {
        do
        {
            pressed = waitInput();
        }
        while(!(pressed & MENU_BUTTONS));

        if(pressed != BUTTON_A)
        {
            //Remember the previously selected option
            u32 oldSelectedOption = selectedOption;

            switch(pressed)
            {
                case BUTTON_UP:
                    if(!selectedOption) selectedOption = totalIndexes;
                    else selectedOption = (selectedOption == NEWCPU + 1 && !isN3DS) ? selectedOption - 2 : selectedOption - 1;
                    break;
                case BUTTON_DOWN:
                    if(selectedOption == totalIndexes) selectedOption = 0;
                    else selectedOption = (selectedOption == NEWCPU - 1 && !isN3DS) ? selectedOption + 2 : selectedOption + 1;
                    break;
                case BUTTON_LEFT:
                    selectedOption = 0;
                    break;
                case BUTTON_RIGHT:
                    selectedOption = totalIndexes;
                    break;
                default:
                    continue;
            }

            if(selectedOption == oldSelectedOption) continue;

            //The user moved to a different option, print the old option in white and the new one in red. Only print 'x's if necessary
            if(oldSelectedOption < multiOptionsAmount)
            {
                drawString(multiOptionsText[oldSelectedOption], true, 10, multiOptions[oldSelectedOption].posY, COLOR_WHITE);
                drawCharacter(selected, true, 10 + multiOptions[oldSelectedOption].posXs[multiOptions[oldSelectedOption].enabled] * SPACING_X, multiOptions[oldSelectedOption].posY, COLOR_WHITE);
            }
            else
            {
                u32 singleOldSelected = oldSelectedOption - multiOptionsAmount;
                drawString(singleOptionsText[singleOldSelected], true, 10, singleOptions[singleOldSelected].posY, COLOR_WHITE);
                if(singleOptions[singleOldSelected].enabled) drawCharacter(selected, true, 10 + SPACING_X, singleOptions[singleOldSelected].posY, COLOR_WHITE);
            }

            if(selectedOption < multiOptionsAmount)
                drawString(multiOptionsText[selectedOption], true, 10, multiOptions[selectedOption].posY, COLOR_RED);
            else
            {
                u32 singleSelected = selectedOption - multiOptionsAmount;
                drawString(singleOptionsText[singleSelected], true, 10, singleOptions[singleSelected].posY, COLOR_RED);
            }

            clearScreens(false, true, false);
            drawString(optionsDescription[selectedOption], false, 10, 10, COLOR_WHITE);
        }
        else
        {
            //The selected option's status changed, print the 'x's accordingly
            if(selectedOption < multiOptionsAmount)
            {
                u32 oldEnabled = multiOptions[selectedOption].enabled;
                drawCharacter(selected, true, 10 + multiOptions[selectedOption].posXs[oldEnabled] * SPACING_X, multiOptions[selectedOption].posY, COLOR_BLACK);
                multiOptions[selectedOption].enabled = (oldEnabled == 3 || !multiOptions[selectedOption].posXs[oldEnabled + 1]) ? 0 : oldEnabled + 1;

                if(selectedOption == BRIGHTNESS) updateBrightness(multiOptions[BRIGHTNESS].enabled);
            }
            else
            {
                bool oldEnabled = singleOptions[selectedOption - multiOptionsAmount].enabled;
                singleOptions[selectedOption - multiOptionsAmount].enabled = !oldEnabled;
                if(oldEnabled) drawCharacter(selected, true, 10 + SPACING_X, singleOptions[selectedOption - multiOptionsAmount].posY, COLOR_BLACK);
            }
        }

        //In any case, if the current option is enabled (or a multiple choice option is selected) we must display a red 'x'
        if(selectedOption < multiOptionsAmount)
            drawCharacter(selected, true, 10 + multiOptions[selectedOption].posXs[multiOptions[selectedOption].enabled] * SPACING_X, multiOptions[selectedOption].posY, COLOR_RED);
        else
        {
            u32 singleSelected = selectedOption - multiOptionsAmount;
            if(singleOptions[singleSelected].enabled) drawCharacter(selected, true, 10 + SPACING_X, singleOptions[singleSelected].posY, COLOR_RED);
        }
    }

    //Preserve the last-used boot options (first 9 bits)
    configData.config &= 0x1FF;

    //Parse and write the new configuration
    for(u32 i = 0; i < multiOptionsAmount; i++)
        configData.config |= multiOptions[i].enabled << (i * 2 + 9);
    for(u32 i = 0; i < singleOptionsAmount; i++)
        configData.config |= (singleOptions[i].enabled ? 1 : 0) << (i + 21);

    u32 newPinMode = MULTICONFIG(PIN);

    if(newPinMode != 0) newPin(oldPinStatus && newPinMode == oldPinMode, newPinMode);
    else if(oldPinStatus) fileDelete(PIN_PATH);

    //Wait for the pressed buttons to change
    while(HID_PAD & PIN_BUTTONS);

    chrono(2);
}
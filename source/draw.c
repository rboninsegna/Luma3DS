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
*   Code to print to the screen by mid-kid @CakesFW
*   https://github.com/mid-kid/CakesForeveryWan
*/

#include "draw.h"
#include "strings.h"
#include "screen.h"
#include "utils.h"
#include "fs.h"
#include "font.h"

bool loadSplash(void)
{

    const char *topSplashPath = "/puma/splash.bin",
               *bottomSplashPath = "/puma/splashbottom.bin";

    bool isTopSplashValid = getFileSize(topSplashPath) == SCREEN_TOP_FBSIZE,
         isBottomSplashValid = getFileSize(bottomSplashPath) == SCREEN_BOTTOM_FBSIZE;

    //Don't delay boot nor init the screens if no splash images or invalid splash images are on the SD
    if(!isTopSplashValid && !isBottomSplashValid)
        return false;

    initScreens();


    if(isTopSplashValid) fileRead(fb->top_left, topSplashPath, 0);
    if(isBottomSplashValid) fileRead(fb->bottom, bottomSplashPath, 0);

    chrono(3);

    return true;
}

void drawCharacter(char character, bool isTopScreen, u32 posX, u32 posY, u32 color)
{
    u8 *select = isTopScreen ? fb->top_left : fb->bottom;

    for(u32 y = 0; y < 8; y++)
    {
        char charPos = font[character * 8 + y];

        for(u32 x = 0; x < 8; x++)
            if((charPos >> (7 - x)) & 1)
            {
                u32 screenPos = (posX * SCREEN_HEIGHT * 3 + (SCREEN_HEIGHT - y - posY - 1) * 3) + x * 3 * SCREEN_HEIGHT;

                select[screenPos] = color >> 16;
                select[screenPos + 1] = color >> 8;
                select[screenPos + 2] = color;
            }
    }
}

u32 drawString(const char *string, bool isTopScreen, u32 posX, u32 posY, u32 color)
{
    for(u32 i = 0, line_i = 0; i < strlen(string); i++, line_i++)
    {
        if(string[i] == '\n')
        {
            posY += SPACING_Y;
            line_i = 0;
            i++;
        }
        else if(line_i >= ((isTopScreen ? SCREEN_TOP_WIDTH : SCREEN_BOTTOM_WIDTH) - posX) / SPACING_X)
        {
            //Make sure we never get out of the screen
            posY += SPACING_Y;
            line_i = 1; //Little offset so we know the same string continues
            if(string[i] == ' ') i++; //Spaces at the start look weird
        }

        drawCharacter(string[i], isTopScreen, posX + line_i * SPACING_X, posY, color);
    }

    return posY;
}
# Puma33DS
*(N?)3DS "Custom Firmware"*

I've made this CFW because I am dissatisfied with how Luma forces the user to apply certain patches despite the fact they're not clearly required for reasonable operation (and some are in fact counter-productive in specific scenarios), while yet being my favorite CFW on almost every other point.

If you couldn't tell, 99% of the code is made by upstream, by up-upstream, etc

I don't plan to mantain the dev branch, but key features may be merged.

It is not inherently A9LH exclusive but I don't plan on trying to maintain compatibility with the .dat build.

## The name
> ***Al3x _10m, [29.07.16 16:02]***
>>
>> nice :P
>>
>> why puma tho'?
>
>
> ***Wolfvak, [29.07.16 16:04]***
> [In reply to Al3x _10m]
>>
>> Because of peterpeter
>>
>> aka petermary17 aka a9lhpeterhax aka that guy who said lima3ds
>>
>> I think it's because of that, he made [the Lima33DS thing and the Puma3DS thing](https://imgur.com/a/DH62x)
>>
>> My guess is @Ryccardo mixed the memes up

Mainly that. If you really want to look into it, you could read it as "Pro Luma" given the main difference...

## Storage Folder
Since I only use one CFW at a time,
and partially out of respect for the original project,
and it's Well Known that nobody likes folders on root,
and I'm lazy,

I've decided to keep using /luma/. Note however that **the config.bin of Luma and Puma are incompatible, so be sure to delete it when switching**.

## Compiling

First you need to install DevKitARM.

As of 2016-8-1 you'll also need manually compile and install an updated ctrulib, as the bundled one is too old.
Just download its source, `make` it, then replace devKitPro/libctru/* with the project folder jou just downloaded and compiled.

You will also need [armips](https://github.com/Kingcom/armips), [bin2c](https://sourceforge.net/projects/bin2c/), and a recent build of [makerom](https://github.com/profi200/Project_CTR) added to your PATH (for example, in devkitARM/bin/).
For your convenience, here are [Windows](http://www91.zippyshare.com/v/ePGpjk9r/file.html) and [Linux](https://mega.nz/#!uQ1T1IAD!Q91O0e12LXKiaXh_YjXD3D5m8_W3FuMI-hEa6KVMRDQ) builds of armips (thanks to who compiled them!).  

Then clone the repository recursively with: `git clone --recursive https://github.com/rboninsegna/Puma33DS.git`

Finally just run `make a9lh` and everything should work!

You can then find arm9loaderhax.bin in the 'out' folder.

### Source files that access configurable options

Asterisk indicates options "exclusive" to Puma33DS, whose numbers will need to be adjusted if Luma3DS adds more options.

    * injector/source/patcher.c
    * source/config.c
    . source/firm.c
    . source/firm.h
    . source/patches.c
    . source/screen.c

## Credits
 
See https://github.com/AuroraWright/Luma3DS/wiki/Credits, plus thanks to Reisyukaku for their TestMenu NS patch.

## Licensing

This software, like Luma3DS, is "Free Software" licensed under the terms of the [GPLv3](http://www.gnu.de/documents/gpl-3.0.en.html).  

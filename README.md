# Puma33DS
*(N?)3DS "Custom Firmware"*

I've made this CFW because I am dissatisfied with how Luma forces the user to apply certain patches despite the fact they're not clearly required for reasonable operation (and some are in fact counter-productive in specific scenarios), while yet being my favorite CFW on almost every other point.

If you couldn't tell, 99% of the code is made by upstream, by up-upstream, etc

It is now A9LH exclusive.

Since v8, it is now based on Luma3DS-dev, since now that Aurora unified the codebases the dilemma of most frequent updates vs. more features is no more!

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

According to popular request by multibooters, I've changed "luma" to "puma".

The internal layout is still Luma-compatible.

### Config.bin version

My policy is to have the major version matching the release number in which features were actually added or removed.

## "Region/language/country emulation + ext. .code"?

### Region spoofing

1. Create a text file: 3 characters uppercase region, a space, 2 characters uppercase language. Any further characters, including line breaks, are ignored.
2. Save as /puma/locales/[u64 titleID in hex, uppercase].txt
3. Make sure the appropriate option is enabled, and that you're working on a regular app (title ID 00040000-*)

Possible regions: JPN, USA, EUR, AUS (unused), CHN, KOR, TWN

Possible languages: JP, EN, FR, DE, IT, ES, ZH, KO, NL, PT, RU, TW

Note, not all possible region-language pairs are supported. This means:
* EUR goes with EN, FR, DE, IT, ES, PT, NL, or RU;
* USA goes with EN, FR, ES, or PT;
* JPN, KOR, CHN, TWN only go with their single language.


### Code replacement

1. Create custom code.bin
2. Save as /puma/code_sections/[u64 titleID in hex, uppercase].bin
3. Make sure the appropriate option is enabled, and that you're working on a regular app (title ID 00040000-*)


### eShop country spoofing

1. Create a text file: 2 characters uppercase country name. Any further characters, including line breaks, are ignored.
2. Save as /puma/locales/country.txt
3. Make sure the region spoofing option is enabled too. 

## Custom version string

1. Create a text file containing a printf-compatible format string of up to 19 characters. The default is `Ver. %d.%d.%d-%d%ls`.
2. Save as /puma/customversion.txt
3. Make sure the System Settings version string option is enabled too.

## Compiling

First you need to install DevKitARM.

As of 2016-8-1 you'll also need manually compile and install an updated ctrulib, as the bundled one is too old.
Just download its source, `make` it, then replace devKitPro/libctru/* with the project folder you just downloaded and compiled.

You will also need [armips](https://github.com/Kingcom/armips), [bin2c](https://sourceforge.net/projects/bin2c/), and a recent build of [makerom](https://github.com/profi200/Project_CTR) added to your PATH (for example, in devkitARM/bin/).
For your convenience, here are [Windows](http://www91.zippyshare.com/v/ePGpjk9r/file.html) and [Linux](https://mega.nz/#!uQ1T1IAD!Q91O0e12LXKiaXh_YjXD3D5m8_W3FuMI-hEa6KVMRDQ) builds of armips (thanks to who compiled them!).  

Then clone the repository recursively with: `git clone --recursive https://github.com/rboninsegna/Puma33DS.git`

Finally just run `make a9lh` and everything should work!

You can then find arm9loaderhax.bin in the 'out' folder.

### Source files that access configurable options

Thanks to Luma3DS switching to symbolic option names instead of hardcoded numbers, adding or removing options is no big deal anymore.
Just make sure the list in source/config.c is in sync with:
* injector/source/patcher.h
* source/config.h
	
### No git in PATH?

Just change this block of the Makefile:
    
	name := Puma33DS
    revision := $(shell git describe --tags --match v[0-9]* --abbrev=8 | sed 's/-[0-9]*-g/-/i')
    commit := $(shell git rev-parse --short=8 HEAD)

to have `revision` and `commit` to be static strings like `name` already is!

## Credits
 
See https://github.com/AuroraWright/Luma3DS/wiki/Credits, plus thanks to @Reisyukaku for their TestMenu NS patch, and to @yifanlu for the country response spoofing.

## Licensing

This software, like Luma3DS, is "Free Software" licensed under the terms of the [GPLv3](http://www.gnu.de/documents/gpl-3.0.en.html).  

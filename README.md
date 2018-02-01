# mame2003-plus-libretro
MAME as it was in 2003, using the libretro API. Suitable for lower-end devices that would struggle to run current versions of MAME (later versions of MAME are increasingly accurate, thus can perform worse).

## ROMset compatibility with MAME 2003-plus
**The MAME 2003-plus core accepts MAME 0.78 ROMsets.** Each version of an arcade emulator must be used with ROMs that have the same exact version number. For example, MAME 0.37b5 ROMsets are required by the MAME 2000 emulator, but MAME 0.37b5 sets will not work correctly with the MAME 2003-plus or MAME 2010 emulator cores. Those cores require MAME 0.78 and MAME 0.139 ROM sets, respectively.

File-not-found errors are the result of a ROMset that is wrong or incomplete, including if you're trying to run a "Split" clone .zip without the parent .zip present. Non-Merged MAME0.78 ROMsets do not require parent .zips to be present.

## Multiple lightgun, steering wheel, and "spinner/dial" devices
MAME 2003-plus has support for multiple mice or touch devices in games that support trackballs, etc.

MAME 2003-plus also supports one or two spinners/dials via the "Share 2 player dial controls across one X/Y device" core option.

## Compiling for the Wii
* To run on Wii's memory constraints some drivers in src/driver.c must be removed.

## Directories
* Some MAME games require data from an internal hard drive, CD-ROM, laserdisk, or other media in order to be emulated -- those forms of media are packaged as CHD files. CHD files should be copied to subfolders within the folder where the MAME ROM zips have been installed. e.g.:
```
/libretro content dir/blitz/blitz.chd
```
* Some games require an additional zip file with recorded sounds or music in order for audio to work correctly. Audio 'sample' files should be placed in subdirectories within `/libretro system dir/mame2003-plus/` e.g.:
```
/libretro system dir/mame2003-plus/samples/
```
* High score, cheat, and history metadata files should be moved from github's [`/libretro/mame2003-plus-libretro/tree/master/metadata`](https://github.com/libretro/mame2003-plus-libretro/tree/master/metadata) and placed within `/libretro system dir/mame2003-plus/` e.g.:
```
/libretro system dir/mame2003-plus/hiscore.dat
/libretro system dir/mame2003-plus/cheat.dat
/libretro system dir/mame2003-plus/history.dat
```
* User-generated content is placed in sub-directories within `/libretro savefile dir/mame2003-plus/` e.g.:
```
/libretro savefile dir/mame2003-plus/diff/
/libretro savefile dir/mame2003-plus/nvram/
/libretro savefile dir/mame2003-plus/hi/
/libretro savefile dir/mame2003-plus/cfg/
/libretro savefile dir/mame2003-plus/inp/
/libretro savefile dir/mame2003-plus/memcard/
/libretro savefile dir/mame2003-plus/snap/
```

### Core options
* **Frameskip** (0-5)
* **DCS Speedhack** (enabled/disabled)
  Speedhack for the Midway sound hardware used in Mortal Kombat 2, 3 and others. Improves performance in these games.
* **Skip Disclaimer** (enabled/disabled)
  Skips the 'nag-screen'.
* **Skip Warnings** (disabled/enabled)
  Skips the warning screen shown before games with incomplete emulation.
* **Samples** (enabled/disabled)
  Requires valid sample zips.
* **Sample Rate (KHz)** (11025-48000)
  Lowering may improve performance on weaker devices
* **Cheats** (disabled/enabled)
  Requires a valid cheat.dat file.
* **Share 2 player dial controls across one X/Y device** (disabled/enabled)
  Some dial/spinner hardware are actually one device with one axis for each player. This supports that setup, by breaking down the normal mouse x/y into two seperate inputs.
* **Mouse Device** (mouse/pointer/disabled)
  Switch between mouse (e.g. hardware mouse, trackball, etc), pointer (touchpad, touchscreen, lightgun, etc), or disabled.
* **TATE Mode** (disabled/enabled)
  Enable if rotating display for vertically oriented games (Pac-Man, Galaga, etc). Requires `video_allow_rotate = "false"` cfg setting in RetroArch.

## Additional configuration information:
 * Official RetroArch documentation for the mame2003-plus-libretro core are published at [buildbot.libretro.com/docs](https://buildbot.libretro.com/docs/). They can be [edited and improved by users via github.com](https://github.com/libretro/docs/blob/master/docs/library/mame2003-plus.md).
 * The RetroArch documentation owes much to RetroPie's [detailed MAME 2003-plus configuration wiki](https://github.com/RetroPie/RetroPie-Setup/wiki/lr-mame2003-plus) which can be adapted for use in other environments.
 
### Development reference links:
 * [MAME: Benchmarks, Useful Code, Bug Fixes, Known Issues](http://www.anthrofox.org/code/mame/index.html) at anthrofox.org
 * [diff file which records efforts taken to address the unaligned memory issue](https://code.oregonstate.edu/svn/dsp_bd/uclinux-dist/trunk/user/games/xmame/xmame-0.106/src/unix/contrib/patches/word-align-patch)
 * [Directory of xmame diffs](http://web.archive.org/web/20090718202532/http://www.filewatcher.com/b/ftp/ftp.zenez.com/pub/mame/xmame.0.0.html) - Offline as of March 2017

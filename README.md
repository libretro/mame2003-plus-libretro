# Building romsets for mame2003-plus

## What Arcade romsets work?

**mame2003-plus is built on the same codebase as MAME 0.78, meaning that 95% or more of MAME 0.78 romsets will work as-is in mame2003-plus, where they immediately benefit from its bugfixes and other improvements.** In order to play the new games and games which received ROM updates in mame2003-plus, you will need to find or build the correct romsets.

mame2003-plus has the ability to generate an XML "DAT" file directly from the MAME Menu. When `mame_keyboard` input is enabled, you can enter the MAME menu by pressing the `Tab` key. With any input mode, you can also access the MAME menu by turning it on as a core option.

> **What is a romset?**
> Arcade games are packaged as zip files, most of which are composed of more than one individual 'ROM' files. That is why some resources refer to an individual arcade game as a ROM (like people use to describe a zipped game cartridge ROM) while other resources refer to an individual game as a ROM set, ROMset, or romset.

## Step 1: Obtaining an XML DAT

DAT files describe the exact ROM contents that the emultaor needs including filenames, file sizes, and checksums to verify contents are not incorrect or corrupt. mame2003-plus has the ability to generate an XML "DAT" file from the MAME Menu. When `mame_keyboard` input is enabled, you can enter the MAME menu by pressing the `Tab` key. With any input mode, you can also access the MAME menu by turning it on as a core option.

## Step 2: Finding a source for ROMs
A complete MAME 0.78 romset collection and a complete MAME 0.139 romset collection together include nearly all ROMs needed to rebuild a complete collection of mame2003-plus romsets. It is also be possible to build a complete mame2003-plus collection from the most recent MAME collection plus the MAME "Rollback" collection.

## Step 3: Building mame2003-plus romsets
Refer to [Validating, Rebuilding, and Filtering ROM Collections](https://github.com/RetroPie/RetroPie-Setup/wiki/Validating,-Rebuilding,-and-Filtering-ROM-Collections) for details on how to configure ClrMamePro to use your sources as "rebuild" folders.

We recommend the "Full Non-Merged" format, where each romset zip files includes all the files needed to run each game, including any ROMs from 'parent' ROM sets and BIOS sets. To configure ClrMamePro to validate or rebuild a Full Non-Merged collection, use "Non-Merged" mode and disable "Separate BIOS Sets" from the "Advanced" menu in both ClrMamePro's Rebuild and Scanner menus.

#### WORD TO THE WISE ABOUT CLRMAMEPRO SETTINGS
ClrMamePro remains the most popular tool for rebuilding MAME romsets, at least for now. That said, ClrMamePro is focused on supporting more recent MAME versions so there are at least two things to know if you are using ClrMamePro to generate a mame2003-plus set:

1. If you are scanning CHDs, go to `Settings` -> `Compressor` -> `CHDMan` tab and change `Req. CHD Version` to `3`.
2. If you are using the suggested setting of `Disable Separate BIOS Sets` then ClrMamePro will report the BIOS romset files as missing even though you told the program you don't want them. mame2003-plus incorporates 15 different kinds of BIOS romsets, so it is normal to see a ClrMamePro message like this after a clean and complete scan: `You are missing 15 of 4831 known mame2003-plus.xml sets (+ BIOS sets)`

### Sourcing CHDs

mame2003-plus uses exactly the same MAME 0.78 CHDs as mame2003. 

### Sourcing Samples

Generally mame2003-plus uses the same samples as mame2003 however there are some exceptions. It should also be mentioned that the new CD soundtrack samples are not circulated as part of any common packs.

Sample sourcing docs are a work in progress.

# Input System

## RetroPad Input Modes

### Modern

The `modern` layout sets mame2003-plus to the same input configration as Final Burn Alpha (FBA). This layout is the fight stick & pad layout popularized by Street Fighter IV and assumes an 8+ button controller.

### SNES

The `SNES` layout strives to map arcade games to match their Super Nintendo / Super Famicom ports when possible. When there is no SNES port as a reference, the SNES layout aims to set sensible defaults with that controller layout in mind. The `SNES` layout only needs a 6+ button controller by default and is not intended for 8+ button fight sticks.

### MAME Classic

The `MAME Classic` input mode uses mainline MAME's default Xbox 360 controller layout. This layout is not a good match for 6-button fighters, but may suit other games.

## Keyboard Input

`mame_keyboard` sets the core to process keyboard input directly through the legacy "MAME" keyboard interface. Use this input mode only if your input device is seen as a keyboard, including some arcade control panel hardware.

## MAME Menu

In mame2003-plus, the MAME menu can be accessed via "retropad" users by activating it in the core options. If your input mode is set to allow input to the `mame_keyboard` interface, you can also enter the menu by pressing the `Tab` key.

# Directories

* Some MAME games require data from an internal hard drive, CD-ROM, laserdisk, or other media in order to be emulated -- those forms of media are packaged as CHD files. CHD files should be copied to subfolders within the folder where the MAME ROM zips have been installed. e.g.:
```
/libretro content dir/blitz/blitz.chd
```
* Some games require an additional zip file with recorded sounds or music in order for audio to work correctly. Audio 'sample' files should be placed in subdirectories within `/libretro system dir/mame2003-plus/` e.g.:
```
/libretro system dir/mame2003-plus/samples/
```
* Cheat and history metadata files should be moved from github's [`/libretro/mame2003-plus-libretro/tree/master/metadata`](https://github.com/libretro/mame2003-plus-libretro/tree/master/metadata) and placed within `/libretro system dir/mame2003-plus/` e.g.:
```
/libretro system dir/mame2003-plus/cheat.dat
/libretro system dir/mame2003-plus/history.dat
```
* Backdrop artwork zip files should be placed in a sub-directory within the system folder:
```
/libretro system dir/mame2003-plus/artwork
```

* User-generated content is placed in subdirectories within `/libretro savefile dir/mame2003-plus/`:
```
/libretro savefile dir/mame2003-plus/cfg/
/libretro savefile dir/mame2003-plus/diff/
/libretro savefile dir/mame2003-plus/hi/
/libretro savefile dir/mame2003-plus/memcard/
/libretro savefile dir/mame2003-plus/nvram/
```

# Core options

The first value listed for the core option represents the default. "Restart" indicates that the core must be restarted in order for changes to that option to take effect.

* **Frameskip**: `0|1|2|3|4|5`
* **Input interface**: `retropad|mame_keyboard|simultaneous`
* **RetroPad Layout**: `modern|SNES|MAME classic`
* **Mouse Device**: `mouse|pointer|disabled` - Switch between mouse (e.g. hardware mouse, trackball, etc), pointer (touchpad, touchscreen, lightgun, etc), or disabled. Defaults to `pointer` in iOS.
* **Show Lightgun crosshair**: `enabled|disabled`
* **Display MAME menu** `disabled|enabled`
* **Brightness**: `1.0|0.2|0.3|0.4|0.5|0.6|0.7|0.8|0.9|1.1|1.2|1.3|1.4|1.5|1.6|1.7|1.8|1.9|2.0`
* **Gamma correction**: `1.2|0.5|0.6|0.7|0.8|0.9|1.1|1.2|1.3|1.4|1.5|1.6|1.7|1.8|1.9|2.0`
* **Use Backdrop artwork** (Restart): `disabled|enabled`
* **Specify BIOS region** (Restart): `default|asia|asia-aes|debug|europe|europe_a|japan|japan_a|japan_b|taiwan|us|us_a|uni-bios.10|uni-bios.11|uni-bios.13|uni-bios.20`
* **Share 2 player dial controls across one X/Y device**: `disabled|enabled` - Some dial/spinner hardware are actually one device with one axis for each player. This supports that setup, by invisibly breaking down the normal mouse x/y into two separate inputs.
* **Dual Joystick Mode (Players 1 & 2)**: `disabled|enabled` - Player 1 uses Joysticks 1 & 2, Player 2 uses Joysticks 3 & 4
* **Right Stick to Buttons**: `enabled|disabled` - Invisibly remap the retropad's right analog stick to serve as buttons
* **TATE Mode**: `disabled|enabled` - Enable if rotating display for vertically oriented games (Pac-Man, Galaga, etc). Requires `video_allow_rotate = "false"` cfg setting in RetroArch.
* **EXPERIMENTAL: Vector resolution multiplier**: (Restart) `1|2|3|4|5|6`
* **EXPERIMENTAL: Vector antialias**: `disabled|enabled`
* **Vector translucency**: `enabled|disabled`
* **EXPERIMENTAL: Vector beam width**: `1|2|3|4|5`
* **Vector flicker**: `20|0|10|20|30|40|50|60|70|80|90|100`,
* **Vector intensity**: `1.5|0.5|1|2|2.5|3`
* **EXPERIMENTAL: Skip ROM verification**: (Restart) `disabled|enabled`
* **Sample Rate (KHz)**: `48000|8000|11025|22050|44100` - Change this manually only for specific reasons. The audio sample rate has far-reaching consequences.
* **MK2/MK3 DCS Speedhack**: `enabled|disabled` - Speedhack for the Midway sound hardware used in Mortal Kombat 2, 3 and others. Improves performance in these games.
* **Skip Warnings**: `disabled|enabled`


# Troubleshooting

## Logging

### What are logs? Why are they so important?
libretro cores like mame2003-plus are designed to run on many different combinations of hardware, operating system, and content. It is not possible for a volunteer-based open source project to test all possible combinations.

The answer to this dilemma involves "logs", which open source software use to record essential information about your system and its function that other users and volunteers need in order to help troubleshoot problems and improve compatibility.

### Posting Logs in forums and Github
Often log files are lengthy which make them difficult to read when they're pasted directly into a post on the forums or github. If your log file is more than six or seven lines long, you will be asked to post a link to it instead.

One free and straightforward system for posting and sharing logs is [Github Gist](https://gist.github.com/). You can paste the contents of a log file, or the log file itself, into the Gist website. After you log has been added to the Gist, press the `Create Public Gist` button to create a shareable link.

### RetroArch Logs
Please refer to the doc [Generating RetroArch Logs](https://buildbot.libretro.com/.docs/guides/generating-retroarch-logs/) for details on using RetroArch GUI and commandline options to generate logs.

### RetroPie Logs

[Use RetroPie's Runcommand system](https://github.com/RetroPie/RetroPie-Setup/wiki/runcommand) to instruct RetroArch to direct the mame2003-plus log to the file `/dev/shm/runcommand.log`. You will then need to consult the RetroPie docs in order to access that file to paste into a Gist.

## Disable Rewind

Placeholder

# Development

#programming channel of the libretro discord chat server: https://discordapp.com/invite/C4amCeV


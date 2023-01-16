/***************************************************************************

Bosconian  (c) 1981 Namco
Galaga     (c) 1981 Namco
Xevious    (c) 1982 Namco
Dig Dug    (c) 1982 Namco

driver by Nicola Salmoria
based on previous work by Martin Scragg, Mirko Buffoni, Aaron Giles


All these games are based on the same 3xZ80, shared memory, CPU design.
Bosconian and Galaga use the same CPU board, with minor differences
(Galaga has one missing RAM and no 50XX custom)
Xevious is physically different, but logically identical.
Dig Dug is the only one a bit different, because it reads the dip switches
through a custom chip instead of having them mapped in memory.

The video board, on the other hand, is completely different for all the games,
that's why they use separate vidhrdw/ source files.


Custom ICs:
----------
Bosconian:
---------
CPU board:
06XX     interface to custom 5xXX
07XX     clock divider
08XX(x3) bus controller
50XX     player score control (protection)
51XX     I/O
54XX     explosion sound generator

Video board:
03XX(x2) ?
05XX     starfield generator
06XX     interface to custom 5xXX
07XX     clock divider
50XX     player score control (only used as protection check)
52XX     sample player

Galaga:
------
CPU board:
06XX     interface to custom 5xXX
07XX     clock divider
08XX(x3) bus controller
51XX     I/O
54XX     explosion sound generator

Video board:
00XX     tilemap address generator with scrolling capability (only Super Pacman)
02XX     gfx data shifter and mixer (16-bit in, 4-bit out)
04XX     sprite address generator
05XX     starfield generator
07XX     clock divider

Xevious:
-------
CPU board:
06XX     interface to custom 5xXX
07XX     clock divider
08XX(x3) bus controller
50XX     player score control (only used for a protection check on startup)
51XX     I/O
54XX     explosion sound generator

Video board:
03XX(x2) ?
04XX     sprite address generator
07XX     clock divider
11XX(x2) gfx data shifter and mixer (16-bit in, 4-bit out)
12XX     sprite generator
13XX     dual scrolling tilemap address generator

Dig Dug:
-------
CPU board:
06XX     interface to custom 5xXX
07XX     clock divider
08XX(x3) bus controller
51XX     I/O
53XX     I/O

Video board:
00XX     tilemap address generator
02XX     gfx data shifter and mixer (16-bit in, 4-bit out)
04XX     sprite address generator
07XX     clock divider


Memory maps:
-----------
Bosconian:
---------
MAIN CPU:

Address          Dir Data     Name      Description
---------------- --- -------- --------- -----------------------
0000xxxxxxxxxxxx R   xxxxxxxx ROM 3N    program ROM
0001xxxxxxxxxxxx R   xxxxxxxx ROM 3M    program ROM
0010xxxxxxxxxxxx R   xxxxxxxx ROM 3L    program ROM
0011xxxxxxxxxxxx R   xxxxxxxx ROM 3K    program ROM
the rest of the memory map is common to the other CPUs

SUB CPU:

Address          Dir Data     Name      Description
---------------- --- -------- --------- -----------------------
0000xxxxxxxxxxxx R   xxxxxxxx ROM 3J    program ROM
0001xxxxxxxxxxxx R   xxxxxxxx ROM 3H    program ROM
0010------------              n.c.
0011------------              n.c.
the rest of the memory map is common to the other CPUs

SOUND CPU:

Address          Dir Data     Name      Description
---------------- --- -------- --------- -----------------------
0000xxxxxxxxxxxx R   xxxxxxxx ROM 3E    program ROM
0001------------              n.c.
0010------------              n.c.
0011------------              n.c.
the rest of the memory map is common to the other CPUs

COMMON:

Address          Dir Data     Name      Description
---------------- --- -------- --------- -----------------------
01000-----------              n.c.
01001-----------              n.c.
01010-----------              n.c.
01011-----------              n.c.
01100-----------              n.c.
01101-----00xxxx   W ----xxxx RAM 2A    \ sound control registers
01101-----01xxxx   W ----xxxx RAM 2B    /
01101-----10-000   W -------x IRQ1      main CPU irq enable/acknowledge
01101-----10-001   W -------x IRQ2      motion CPU irq enable/acknowledge
01101-----10-010   W -------x NMION     sound CPU nmi enable
01101-----10-011   W -------x RESET     reset sub and sound CPU, and 5xXX chips on CPU board
01101-----10-100   W -------x n.c.
01101-----10-101   W -------x MOD 0     unused?
01101-----10-110   W -------x MOD 1     unused?
01101-----10-111   W -------x MOD 2     unused?
01101-----11----   W -------- WDR       watchdog reset
01101-----00-xxx R   -------x DIP SW    dip switch B
01101-----00-xxx R   ------x- DIP SW    dip switch A
01101-----01---- R            n.c.
01101-----10---- R            n.c.
01101-----11---- R            n.c.
01110--0-------- R/W xxxxxxxx I/O       custom 06XX data
01110--1-------- R/W xxxxxxxx I/O       custom 06XX control
01111xxxxxxxxxxx R/W xxxxxxxx RAM 2N    work RAM (not present in Galaga)
10000xxxxxxxxxxx R/W xxxxxxxx DHRAM     tilemap RAM (tile code) [1]
10001xxxxxxxxxxx R/W xxxxxxxx VCRAM     tilemap RAM (tile attr) [1]
10010--0-------- R/W xxxxxxxx EXCS      custom 06XX #2 data
10010--1-------- R/W xxxxxxxx EXCS      custom 06XX #2 control
10011----000xxxx   W ----xxxx SOWR      bullets shape and X pos msb [2]
10011----001----   W xxxxxxxx POSI X    playfield X scroll
10011----010----   W xxxxxxxx POSI Y    playfield Y scroll
10011----011----   W -----xxx STAR      to 05XX: starfield X scroll speed
10011----011----   W --xxx--- STAR      to 05XX: starfield Y scroll speed
10011----100----   W -------- STARCLR   to 05XX: unknown
10011----101----   W          n.c.
10011----110----   W          n.c.
10011----111-000   W -------x FLIP      flip screen
10011----111-001   W -------x n.c.
10011----111-010   W -------x n.c.
10011----111-011   W -------x n.c.
10011----111-100   W -------x BLK 0     \ to 05XX: starfield blink
10011----111-101   W -------x BLK 1     /          (select active subset)
10011----111-110   W -------x n.c.
10011----111-111   W -------x RESET     reset 5xXX chips on video board
10100-----------              n.c.
10101-----------              n.c.
10110-----------              n.c.
10111-----------              n.c.

[1] 1st half is radar + sprite registers, 2nd half is scrolling playfield
[2] SO = Small Objects? Only locations 4-F are used.


Galaga:
------
MAIN CPU:

Address          Dir Data     Name      Description
---------------- --- -------- --------- -----------------------
0000xxxxxxxxxxxx R   xxxxxxxx ROM 3N    program ROM
0001xxxxxxxxxxxx R   xxxxxxxx ROM 3M    program ROM
0010xxxxxxxxxxxx R   xxxxxxxx ROM 3L    program ROM
0011xxxxxxxxxxxx R   xxxxxxxx ROM 3K    program ROM
the rest of the memory map is common to the other CPUs

SUB CPU:

Address          Dir Data     Name      Description
---------------- --- -------- --------- -----------------------
0000xxxxxxxxxxxx R   xxxxxxxx ROM 3J    program ROM
0001------------              n.c.
0010------------              n.c.
0011------------              n.c.
the rest of the memory map is common to the other CPUs

SOUND CPU:

Address          Dir Data     Name      Description
---------------- --- -------- --------- -----------------------
0000xxxxxxxxxxxx R   xxxxxxxx ROM 3E    program ROM
0001------------              n.c.
0010------------              n.c.
0011------------              n.c.
the rest of the memory map is common to the other CPUs

COMMON:

Address          Dir Data     Name      Description
---------------- --- -------- --------- -----------------------
01000-----------              n.c.
01001-----------              n.c.
01010-----------              n.c.
01011-----------              n.c.
01100-----------              n.c.
01101-----00----   W ----xxxx RAM 2A    \ sound control registers
01101-----01----   W ----xxxx RAM 2B    /
01101-----10-000   W -------x IRQ1      main CPU irq enable/acknowledge
01101-----10-001   W -------x IRQ2      motion CPU irq enable/acknowledge
01101-----10-010   W -------x NMION     sound CPU nmi enable
01101-----10-011   W -------x RESET     reset sub and sound CPU, and 5xXX chips on CPU board
01101-----10-100   W -------x n.c.
01101-----10-101   W -------x MOD 0     unused?
01101-----10-110   W -------x MOD 1     unused?
01101-----10-111   W -------x MOD 2     unused?
01101-----11----   W -------- WDR       watchdog reset
01101-----00-xxx R   -------x DIP SW    dip switch B
01101-----00-xxx R   ------x- DIP SW    dip switch A
01101-----01---- R            n.c.
01101-----10---- R            n.c.
01101-----11---- R            n.c.
01110--0-------- R/W xxxxxxxx I/O       custom 06XX data
01110--1-------- R/W xxxxxxxx I/O       custom 06XX control
10000xxxxxxxxxxx R/W xxxxxxxx RAM 1K    tilemap RAM
10001-xxxxxxxxxx R/W xxxxxxxx RAM 3E/3F work RAM
10001-111xxxxxxx R/W xxxxxxxx           portion holding sprite registers
10010-xxxxxxxxxx R/W xxxxxxxx RAM 3K/3L work RAM
10010-111xxxxxxx R/W xxxxxxxx           portion holding sprite registers
10011-xxxxxxxxxx R/W xxxxxxxx RAM 3H/3J work RAM
10011-111xxxxxxx R/W xxxxxxxx           portion holding sprite registers
10100--------000   W -------x           \
10100--------001   W -------x            > to 05XX: starfield X scroll speed
10100--------010   W -------x           /
10100--------011   W -------x           \ to 05XX: starfield blink
10100--------100   W -------x           /          (select active subset)
10100--------101   W -------x           to 05XX: unknown. It is the same as STARCLR in Bosconian
10100--------110   W -------x n.c.
10100--------111   W -------x FLIP      flip screen
10101-----------              n.c.
10110-----------              n.c.
10111-----------              n.c.


Namco vs Midway ROM names and locations
---------------------------------------
Location  ID         Location  ID
--------  ----       --------  -----
CPU 3P    GG1-1      CPU 3N    3200A
CPU 3M    GG1-2      CPU 3M    3300B
CPU 2M    GG1-3      CPU 3L    3400C
CPU 2L    GG1-4      CPU 3K    3500D
CPU 3F    GG1-5      CPU 3J    3600E
CPU 2C    GG1-7      CPU 3E    3700G
CPU 1D    GG1-1[bpr] CPU 1D
CPU 5C    GG1-2[bpr] CPU 5C

VID 4L    GG1-9      VID 4L    2600J
VID 4F    GG1-10     VID 4F    2700K
VID 4D    GG1-11     VID 4D    2800L
VID 1C    GG1-3[bpr] VID 1C
VID 2N    GG1-4[bpr] VID 2N
VID 5N    GG1-5[bpr] VID 5N


Xevious:
-------
MAIN CPU:

Address          Dir Data     Name      Description
---------------- --- -------- --------- -----------------------
000xxxxxxxxxxxxx R   xxxxxxxx ROM 1     program ROM
001xxxxxxxxxxxxx R   xxxxxxxx ROM 2     program ROM
the rest of the memory map is common to the other CPUs

MOTION CPU:

Address          Dir Data     Name      Description
---------------- --- -------- --------- -----------------------
000xxxxxxxxxxxxx R   xxxxxxxx ROM 3     program ROM
the rest of the memory map is common to the other CPUs

SOUND CPU:

Address          Dir Data     Name      Description
---------------- --- -------- --------- -----------------------
00-xxxxxxxxxxxxx R   xxxxxxxx ROM 4     program ROM
the rest of the memory map is common to the other CPUs

COMMON:
a small part of the decoding for the video board is done by a PAL so it is inferred by program behaviour

Address          Dir Data     Name      Description
---------------- --- -------- --------- -----------------------
01000-----------              n.c.
01001-----------              n.c.
01010-----------              n.c.
01011-----------              n.c.
01100-----------              n.c.
01101-----00----   W ----xxxx SRAM 0    \ sound control registers
01101-----01----   W ----xxxx SRAM 1    /
01101-----10-000   W -------x IRQ1      main CPU irq enable/acknowledge
01101-----10-001   W -------x IRQ2      motion CPU irq enable/acknowledge
01101-----10-010   W -------x NMION     sound CPU nmi enable
01101-----10-011   W -------x RESET     reset sub and sound CPU, and 5xXX chips on CPU board
01101-----10-100   W -------x n.c.
01101-----10-101   W -------x n.c.
01101-----10-110   W -------x n.c.
01101-----10-111   W -------x n.c.
01101-----11----   W -------- WDR       watchdog reset
01101-----00-xxx R   -------x DIP SW    dip switch B
01101-----00-xxx R   ------x- DIP SW    dip switch A
01101-----01---- R            n.c.
01101-----10---- R            n.c.
01101-----11---- R            n.c.
01110--0-------- R/W xxxxxxxx I/O       custom 06XX data
01110--1-------- R/W xxxxxxxx I/O       custom 06XX control
01111xxxxxxxxxxx R/W xxxxxxxx           work RAM
1000-xxxxxxxxxxx R/W xxxxxxxx           work RAM
1000-1111xxxxxxx R/W xxxxxxxx           portion holding sprite registers (x, y)
1001-xxxxxxxxxxx R/W xxxxxxxx           work RAM
1001-1111xxxxxxx R/W xxxxxxxx           portion holding sprite registers (flip, size)
1010-xxxxxxxxxxx R/W xxxxxxxx           work RAM
1010-1111xxxxxxx R/W xxxxxxxx           portion holding sprite registers (sprite number & color)
10110xxxxxxxxxxx R/W xxxxxxxx PF0       fg tilemap RAM (tile attributes)
10111xxxxxxxxxxx R/W xxxxxxxx PF1       bg tilemap RAM (tile attributes)
11000xxxxxxxxxxx R/W xxxxxxxx PF2       fg tilemap RAM (tile code)
11001xxxxxxxxxxx R/W xxxxxxxx PF3       bg tilemap RAM (tile code)
1101-----000---x   W xxxxxxxx           bg X scroll (9-bit data: A0 is the msb)
1101-----001---x   W xxxxxxxx           fg X scroll (9-bit data: A0 is the msb)
1101-----010---x   W xxxxxxxx           bg Y scroll (9-bit data: A0 is the msb)
1101-----011---x   W xxxxxxxx           fg Y scroll (9-bit data: A0 is the msb)
1101-----111----   W -------x FLIP      flip screen
1110------------              n.c.
1111-----------0   W xxxxxxxx BS0       \ address to read from background data ROMs
1111-----------1   W xxxxxxxx BS1       / (see xevious_bb_r)
1111-----------0 R   xxxxxxxx BB0       \ read from background data ROMs
1111-----------1 R   xxxxxxxx BB1       /


Namco vs Atari ROM names and locations
--------------------------------------
Location  ID          Location  ID
--------  ----        --------  ----------
CPU 3P    XVI-1       CPU 1M    136018-118
CPU 3M    XVI-2        "   "      "     "
CPU 2M    XVI-3       CPU 1L    136018-119
CPU 2L    XVI-4        "   "      "     "
CPU 3F    XVI-5       CPU 4C    136018-120
CPU 3J    XVI-6        "   "      "     "
CPU 2C    XVI-7       CPU 2C    136018-127
CPU 5N    XVI-1[bpr]  CPU 6M    136018-028
CPU 7N    XVI-2[bpr]  CPU 8M    136018-029

VID 2A    XVI-9       VID 2A    136018-101
VID 2B    XVI-10      VID 2B    136018-102
VID 2C    XVI-11      VID 2C    136018-103
VID 3B    XVI-12      VID 3B    136018-104
VID 3C    XVI-13      VID 3C    136018-105
VID 3D    XVI-14      VID 3D    136018-106
VID 4M    XVI-15      VID 4M    136018-107
VID 4N    XVI-16      VID 4N    136018-108
VID 4P    XVI-17      VID 4P    136018-109
VID 4R    XVI-18      VID 4R    136018-110
VID 3L    XVI-4[bpr]  VID 3L    136018-011
VID 3M    XVI-5[bpr]  VID 3M    136018-012
VID 4F    XVI-6[bpr]  VID 4F    136018-013
VID 4H    XVI-7[bpr]  VID 4H    136018-014
VID 6A    XVI-8[bpr]  VID 6A    136018-015
VID 6D    XVI-9[bpr]  VID 6D    136018-016
VID 6E    XVI-10[bpr] VID 6E    136018-017


Dig Dug:
-------
MAIN CPU:

Address          Dir Data     Name      Description
---------------- --- -------- --------- -----------------------
0000xxxxxxxxxxxx R   xxxxxxxx ROM 0     program ROM
0001xxxxxxxxxxxx R   xxxxxxxx ROM 1     program ROM
0010xxxxxxxxxxxx R   xxxxxxxx ROM 2     program ROM
0011xxxxxxxxxxxx R   xxxxxxxx ROM 3     program ROM
the rest of the memory map is common to the other CPUs

SUB CPU:

Address          Dir Data     Name      Description
---------------- --- -------- --------- -----------------------
0000xxxxxxxxxxxx R   xxxxxxxx ROM 4     program ROM
0001xxxxxxxxxxxx R   xxxxxxxx ROM 5     program ROM
0010------------              n.c.
0011------------              n.c.
the rest of the memory map is common to the other CPUs

SOUND CPU:

Address          Dir Data     Name      Description
---------------- --- -------- --------- -----------------------
0000xxxxxxxxxxxx R   xxxxxxxx ROM 6     program ROM
0001xxxxxxxxxxxx R   xxxxxxxx ROM 7     program ROM (optional, not used)
0010------------              n.c.
0011------------              n.c.
the rest of the memory map is common to the other CPUs

COMMON:

Address          Dir Data     Name      Description
---------------- --- -------- --------- -----------------------
01000-----------              n.c.
01001-----------              n.c.
01010-----------              n.c.
01011-----------              n.c.
01100-----------              n.c.
01101-----00----   W ----xxxx AUDIO 0   \ sound control registers
01101-----01----   W ----xxxx AUDIO 1   /
01101-----10-000   W -------x IRQ1      main CPU irq enable/acknowledge
01101-----10-001   W -------x IRQ2      sub CPU irq enable/acknowledge
01101-----10-010   W -------x NMION     sound CPU nmi enable
01101-----10-011   W -------x RESET     reset sub and sound CPU, and 5xXX chips on CPU board
01101-----10-100   W -------x n.c.
01101-----10-101   W -------x MOD 0     \
01101-----10-110   W -------x MOD 1     | to custom 53XX
01101-----10-111   W -------x MOD 2     /
01101-----11----   W -------- WDDIS     watchdog reset
01110--0-------- R/W xxxxxxxx I/O       custom 06XX data
01110--1-------- R/W xxxxxxxx I/O       custom 06XX control
01111-----------              n.c.
10000xxxxxxxxxxx R/W xxxxxxxx RAM 0     tilemap RAM + work RAM
10001-xxxxxxxxxx R/W xxxxxxxx OBJRAM    work RAM
10001-111xxxxxxx R/W xxxxxxxx           portion holding sprite registers (sprite number and color)
10010-xxxxxxxxxx R/W xxxxxxxx POSRAM    work RAM
10010-111xxxxxxx R/W xxxxxxxx           portion holding sprite registers (x and y)
10011-xxxxxxxxxx R/W xxxxxxxx FLPRAM    work RAM
10011-111xxxxxxx R/W xxxxxxxx           portion holding sprite registers (flip)
10100--------000   W -------x           \ background ROM (114) bank select
10100--------001   W -------x           /
10100--------010   W -------x           tilemap color select (low or high 4 bits of tilemap RAM)
10100--------011   W -------x           background enable
10100--------100   W -------x           \ background color lookup PROM (112) bank select
10100--------101   W -------x           /
10100--------110   W -------x n.c.
10100--------111   W -------x FLIP      flip screen
10101-----------              n.c.
10110-----------              n.c.
10111----0xxxxxx   W xxxxxxxx EAROM     non volatile memory address latch and data write
10111----0------ R   xxxxxxxx EAROM     non volatile memory read
10111----1------   W ----xxxx EAROM     non volatile memory control



Namco vs Atari ROM names and locations
--------------------------------------
The Namco version is composed of two boards, while the Atari version is
single board. There are two revisions of the Atari version.

Location  ID        Location  Location  ID
                    (type 1)  (type 2)
--------  ----      --------  --------  ----------
CPU 3P    DD1-1     6L        2C/D      136007-101
CPU 3M    DD1-2     6M        2E        136007-102
CPU 2M    DD1-3     6N/P      2B/C      136007-103
CPU 2L    DD1-4     6R        2A        136007-104
CPU 3F    DD1-5     6C        2P        136007-105
CPU 3J    DD1-6     6D        2N        136007-106
CPU 2C    DD1-7     5L        2K/L      136007-107
CPU 5N    [bpr]     2K/L      10A       136007-109
CPU 7N    [bpr]     2P        11A       136007-110

VID 2C    DD1-9     8R        5K        136007-108
VID 1C    [bpr]     4G        8F        136007-111
VID 2N    [bpr]     10K/L     4N        136007-112
VID 5N    [bpr]     1R        8L        136007-113
VID 2D    DD1-10    9N        4J        136007-114
VID 5C    DD1-11    10C/D     4F        136007-115
VID 5F    DD1-12    7A/B      5B        136007-119
VID 5H    DD1-13    8A/B      5A        136007-118
VID 5J    DD1-14    7C        5C        136007-117
VID 5K    DD1-15    8C        5D        136007-116



Gatsbee (Galaga mod/bootleg)
----------------------------
This game runs on modified bootleg Galaga hardware (blue board with PCB numbers DG-09-02 and DG-07-02)

ROM8: is a 2764. pins 1, 26, 27, 28 tied together.
      pin2 out of socket, has wire that is tied to pin 4 of a LS259 that sits on top of the main Z80
      CPU located at 5B/6B

Z80: There are 2 logic chips sitting on top of it which are wired up to the Z80 and to each other.
     Looks like this....
     |-------------------|
     |  LS32   LS259     <
     |-------------------|

Bend all the legs outwards.
Line up the LS259 so pin 16 is in line with Z80 pin 11
Line up the LS32 so pin 7 is in line with Z80 pin 29
Atach the 2 chips to the top of the Z80 with some glue
Connect like this....

LS32 pin 1 tied to Z80 pin 22
LS32 pin 2 tied to Z80 pin 19
LS32 pin 3,4 tied together
LS32 pin 5 tied to Z80 pin 4
LS32 pin 6 tied to pin 10 LS32
LS32 pin 7 tied to Z80 pin 29 (GND)
LS32 pin 8 tied to LS259 pin 14
LS32 pin 9 tied to Z80 pin 5
LS32 pins 11, 12, 13 have NC
LS32 pin 14 tied to Z80 pin 11 (+5V)

LS259 pin 1 tied to Z80 pin 30
LS259 pin 2 tied to Z80 pin 31
LS259 pin 3 tied to Z80 pin 32
LS259 pin 4 to ROM 8 (as above)
LS259 pins 5, 6, 7 have NC
LS259 pin 8 tied to Z80 pin 29 (GND)
LS259 pins 9, 10, 11, 12 have NC
LS259 pin 13 tied to Z80 pin 14
LS259 pin 15 tied to Z80 pin 26
LS259 pin 16 tied to Z80 pin 11



Easter eggs:
-----------
- Bosconian:
  - enter service mode
  - keep B1 pressed and enter the following sequence:
    5xU 6xR 1xD 4xL
  (c) 1981 NAMCO LTD. will be added at the bottom of the screen.

- Galaga:
  - enter service mode
  - keep B1 pressed and enter the following sequence:
    5xR 6xL 3xR 7xL
  (c) 1981 NAMCO LTD. will appear on the screen.

- Xevious:
  - start a game
  - go to the bottom right of the screen and keep B2 pressed
  NAMCO ORIGINAL
  program by EVEZOO
  will be written at the bottom of the screen
  In Super Xevious this is changed to
  special thanks for you
  by game designer EVEZOO

- Dig Dug:
  - enter service mode
  - keep B1 pressed and enter the following sequence:
    6xU 3xR 4xD 8xL
  (c) 1982 NAMCO LTD. will appear on the screen.


Notes:
-----
- The Cabinet Type "dip switch" actually comes from the edge connector, but is mapped
  in memory in place of dip switch #8. dip switch #8 selects single/dual coin counters
  and is entirely handled by hardware.

- galaga: there is a bug in the sound CPU program. During initialization, it enables
  NMI before clearing RAM, but the NMI handler doesn't save the registers, so it cannot
  interrupt program execution. If the NMI happens before the LDIR that clears RAM has
  finished, the program will crash.
  To prevent this, I had to use a custom interrupt_gen, timing NMI generation
  appropriately.

- galaga: there were "fast shoot" hacks available, which are not supported.
  Their effects can be replicated with this line in cheat.dat:
  galaga:1:070D:0D:100:Fast Shoot

- bosco: we have two dumps of the sound shape ROM, "prom.1d" and "bosco.spr". Music
  changes a lot from one version to the other.
  I'm using the former because it is more similar to the other Namco games. The latter,
  after masking off the unused top 4 bits and inverting bit 3, matches the Galaga one,
  so it might have come from a (bootleg?) conversion.

- bosco & galaga: the Midway arcade cabinet had an optional rapid fire board, using
  a 556 to generate autofire while the button was held. That really makes little
  sense in Galaga! For Bosconian, I guess it was for the boscomdo set I, because the
  other sets have autofire built-in.

- the bosconian video system is (apart from the starfield) almost identical functionally
  to Rally X, but the hardware is quite different: Rally X has no custom ICs.

- digdug: if you enter service mode and press press service coin something like
  the following is written at the bottom of the screen:
  99.9999.9999.9999.9999.
  This is explained in the manual: it is the number of games played, of points, etc.
  The counters start from 999 and count backwards.

- gallag is identical to galagao, apart from the title changed to "GALLAG" and the
  copyright notice changed from "(c) 1981 NAMCO LTD" to "1 9 8 2" (and the Namco logo
  removed from the gfx). The only interesting thing about it is the 4th Z80, used to
  simulate the custom 5xXX chips of the original.
  It also has different explosion and starfield circuitries, to do without the Namco
  custom chips.

- differences between versions of digdug:
  - the background graphics are slightly different in the Atari version, the earth is
    less regular.

  - "digdugb" and "digduga1" are identical, apart from the gfx and copyright notices
    changed from "NAMCO LTD." to "ATARI INC.".

  - "digdug" is almost identical to "digdugat" (apart from the above changes), but
    there are three more instructions in the latter that change the code alignment.

  - "dzigzag" and "digdugb" are identical, apart from the hacked gfx and the copyright
    notices changed from "NAMCO LTD." to "1 9 8 2". It's a bottleg of "digdugb", and
    not of "digduga1", because the hidden "NAMCO" string at offset 0x1eea of CPU2 is
    still present, while it is replaced by "ATARI" in digduga1.
    The only interesting thing about the bootleg is the 4th Z80, used to simulate
    the custom 5xXX chips of the original.


TODO:
----
- bosco & galaga:
  - the starfield is wrong.
  - The function of STARCLR is unknown. It is not latched and there are no data bits
    used...

- bosco: is the scrolling tilemap placement correct? It is currently aligned so that
  the test grid shown on startup is correct, but this way an unerased grey strip
  remains on the left of the screen during the title sequence.

- xevious: the watchdog should fire if not reset for 8 frames, I think. MAME is
  currently hardwired to wait 3 seconds. This causes credit sounds to play for a
  while when the test mode switch is toggled because the machine doesn't reset soon
  enough.

- Should Xevios have a 4th Z80 like the other bootlegs? We don't have a dump for
  that ROM.

- xevious: I haven't found any Easter egg in service mode. The main loop is very
  simple so there might just not be one, though this would be the only Namco game
  of that era to not have a service mode Easter egg. On the other hand, the service
  mode in this game is VERY spartan when compared to the other Namco games.

- dzigzag: emulate the 4th CPU (should be similar to battles)


***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "machine/atari_vg.h"
#include "machine/namcoio.h"
#include "sound/namco.h"
#include "sound/namco52.h"
#include "sound/namco54.h"
#include "includes/galaga.h"
#include "vidhrdw/generic.h"


static INTERRUPT_GEN( galaga_cpu3_nmi )
{
	/* see notes at the top of the driver */
	if (cpu_getiloops() & 1)
		nmi_line_pulse();
}

static READ_HANDLER( bosco_dsw_r )
{
	int bit0,bit1;

	bit0 = (readinputport(3) >> offset) & 1;
	bit1 = (readinputport(2) >> offset) & 1;

	return bit0 | (bit1 << 1);
}

static WRITE_HANDLER( galaga_flip_screen_w )
{
	flip_screen_set(data & 1);
}

static WRITE_HANDLER( bosco_flip_screen_w )
{
	flip_screen_set(~data & 1);
}


static WRITE_HANDLER( bosco_latch_w )
{
	int bit = data & 1;

	switch (offset)
	{
		case 0x00:	/* IRQ1 */
			cpu_interrupt_enable(0,bit);
			if (!bit)
				cpu_set_irq_line(0, 0, CLEAR_LINE);
			break;

		case 0x01:	/* IRQ2 */
			cpu_interrupt_enable(1,bit);
			if (!bit)
				cpu_set_irq_line(1, 0, CLEAR_LINE);
			break;

		case 0x02:	/* NMION */
			cpu_interrupt_enable(2,!bit);
			break;

		case 0x03:	/* RESET */
			cpu_set_reset_line(1,bit ? CLEAR_LINE : ASSERT_LINE);
			cpu_set_reset_line(2,bit ? CLEAR_LINE : ASSERT_LINE);
			break;

		case 0x04:	/* n.c. */
			break;

		case 0x05:	/* MOD 0 (xevious: n.c.) */
		case 0x06:	/* MOD 1 (xevious: n.c.) */
		case 0x07:	/* MOD 2 (xevious: n.c.) */
			break;
	}
}


static READ_HANDLER( in0_l )	{ return readinputport(0); }		/* fire and start buttons */
static READ_HANDLER( in0_h )	{ return readinputport(0) >> 4; }	/* coins */
static READ_HANDLER( in1_l )	{ return readinputport(1); }		/* P1 joystick */
static READ_HANDLER( in1_h )	{ return readinputport(1) >> 4; }	/* P2 joystick */
static READ_HANDLER( dipA_l )	{ return readinputport(2); }		/* dips A */
static READ_HANDLER( dipA_h )	{ return readinputport(2) >> 4; }	/* dips A */
static READ_HANDLER( dipB_l )	{ return readinputport(3); }		/* dips B */
static READ_HANDLER( dipB_h )	{ return readinputport(3) >> 4; }	/* dips B */
static WRITE_HANDLER( out_0 )
{
	set_led_status(1,data & 1);
	set_led_status(0,data & 2);
	coin_counter_w(1,~data & 4);
	coin_counter_w(0,~data & 8);
}
static WRITE_HANDLER( out_1 )
{
	coin_lockout_global_w(data & 1);
}

static struct namcoio_interface intf0 =
{
	{ in0_l, in0_h, in1_l, in1_h },	/* port read handlers */
	{ out_0, out_1 }				/* port write handlers */
};
static struct namcoio_interface intf1 =
{
	{ dipA_l, dipA_h, dipB_l, dipB_h },	/* port read handlers */
	{ NULL, NULL }						/* port write handlers */
};


static MACHINE_INIT( bosco )
{
	int i;

	/* Reset all latches */
	for (i = 0;i < 8;i++)
		bosco_latch_w(i,0);

	namco_06xx_init(0, 0,
		NAMCOIO_51XX, &intf0,
		NAMCOIO_NONE, NULL,
		NAMCOIO_50XX, NULL,
		NAMCOIO_54XX, NULL);

	namco_06xx_init(1, 1,
		NAMCOIO_50XX, NULL,
		NAMCOIO_52XX, NULL,
		NAMCOIO_NONE, NULL,
		NAMCOIO_NONE, NULL);
}

static MACHINE_INIT( galaga )
{
	int i;

	/* Reset all latches */
	for (i = 0;i < 8;i++)
		bosco_latch_w(i,0);

	namco_06xx_init(0, 0,
		NAMCOIO_51XX, &intf0,
		NAMCOIO_NONE, NULL,
		NAMCOIO_NONE, NULL,
		NAMCOIO_54XX, NULL);
}

static MACHINE_INIT( xevious )
{
	int i;

	/* Reset all latches */
	for (i = 0;i < 8;i++)
		bosco_latch_w(i,0);

	namco_06xx_init(0, 0,
		NAMCOIO_51XX, &intf0,
		NAMCOIO_NONE, NULL,
		NAMCOIO_50XX, NULL,
		NAMCOIO_54XX, NULL);
}

static MACHINE_INIT( battles )
{
	int i;

	/* Reset all latches */
	for (i = 0;i < 8;i++)
		bosco_latch_w(i,0);

	battles_customio_init();
}

static MACHINE_INIT( digdug )
{
	int i;

	/* Reset all latches */
	for (i = 0;i < 8;i++)
		bosco_latch_w(i,0);

	namco_06xx_init(0, 0,
		NAMCOIO_51XX, &intf0,
		NAMCOIO_53XX_DIGDUG, &intf1,
		NAMCOIO_NONE, NULL,
		NAMCOIO_NONE, NULL);
}

/* the same memory map is used by all three CPUs; all RAM areas are shared */
static MEMORY_READ_START( bosco_readmem )
    { 0x0000, 0x3fff, MRA_ROM },			/* the only area different for each CPU */
	{ 0x6800, 0x6807, bosco_dsw_r },
	{ 0x7000, 0x70ff, namco_06xx_0_data_r },
	{ 0x7100, 0x7100, namco_06xx_0_ctrl_r },
	{ 0x7800, 0x7fff, bosco_sharedram1_r },
	{ 0x8000, 0x8fff, bosco_videoram_r },	/* + sprite registers */
	{ 0x9800, 0x980f, bosco_sharedram2_r },
	{ 0x9000, 0x90ff, namco_06xx_1_data_r },
	{ 0x9100, 0x9100, namco_06xx_1_ctrl_r },
	{ 0x9800, 0x980f, MRA_RAM },
MEMORY_END

/* the same memory map is used by all three CPUs; all RAM areas are shared */
static MEMORY_WRITE_START( bosco_writemem )
    { 0x0000, 0x3fff, MWA_NOP },			/* the only area different for each CPU */
	{ 0x6800, 0x681f, pengo_sound_w, &namco_soundregs },
	{ 0x6820, 0x6827, bosco_latch_w },						/* misc latches */
	{ 0x6830, 0x6830, watchdog_reset_w },
	{ 0x7000, 0x70ff, namco_06xx_0_data_w },
	{ 0x7100, 0x7100, namco_06xx_0_ctrl_w },
	{ 0x7800, 0x7fff, bosco_sharedram1_w, &bosco_sharedram },
	{ 0x8000, 0x8fff, bosco_videoram_w, &bosco_videoram },	/* + sprite registers */
	{ 0x9000, 0x90ff, namco_06xx_1_data_w },
	{ 0x9100, 0x9100, namco_06xx_1_ctrl_w },
	{ 0x9800, 0x980f, bosco_sharedram2_w, &bosco_radarattr },
	{ 0x9810, 0x9810, bosco_scrollx_w },
	{ 0x9820, 0x9820, bosco_scrolly_w },
	{ 0x9830, 0x9830, bosco_starcontrol_w },
	{ 0x9840, 0x9840, bosco_starclr_w },
	{ 0x9870, 0x9870, bosco_flip_screen_w },
	{ 0x9874, 0x9875, bosco_starblink_w },
MEMORY_END


static MEMORY_READ_START( galaga_readmem )
    { 0x0000, 0x3fff, MRA_ROM },			/* the only area different for each CPU */
	{ 0x6800, 0x6807, bosco_dsw_r },
	{ 0x7000, 0x70ff, namco_06xx_0_data_r },
	{ 0x7100, 0x7100, namco_06xx_0_ctrl_r },
	{ 0x8000, 0x87ff, galaga_videoram_r },
	{ 0x8800, 0x8bff, galaga_sharedram1_r },
	{ 0x9000, 0x93ff, galaga_sharedram2_r },
	{ 0x9800, 0x9bff, galaga_sharedram3_r },
MEMORY_END

static MEMORY_WRITE_START( galaga_writemem )
    { 0x0000, 0x3fff, MWA_NOP },			/* the only area different for each CPU */
	{ 0x6800, 0x681f, pengo_sound_w, &namco_soundregs },
	{ 0x6820, 0x6827, bosco_latch_w },						/* misc latches */
	{ 0x6830, 0x6830, watchdog_reset_w },
	{ 0x7000, 0x70ff, namco_06xx_0_data_w },
	{ 0x7100, 0x7100, namco_06xx_0_ctrl_w },
	{ 0x8000, 0x87ff, galaga_videoram_w, &galaga_videoram },
    { 0x8800, 0x8bff, galaga_sharedram1_w, &galaga_ram1 },
    { 0x9000, 0x93ff, galaga_sharedram2_w, &galaga_ram2 },
    { 0x9800, 0x9bff, galaga_sharedram3_w, &galaga_ram3 },
	{ 0xa000, 0xa005, galaga_starcontrol_w },
	{ 0xa007, 0xa007, galaga_flip_screen_w },
MEMORY_END

static MEMORY_READ_START( xevious_readmem )
    { 0x0000, 0x3fff, MRA_ROM },			/* the only area different for each CPU */
	{ 0x6800, 0x6807, bosco_dsw_r },
	{ 0x7000, 0x70ff, namco_06xx_0_data_r },
	{ 0x7100, 0x7100, namco_06xx_0_ctrl_r },
	{ 0x7800, 0x7fff, xevious_sharedram0_r },	/* work RAM */
	{ 0x8000, 0x87ff, xevious_sharedram1_r },    /* work RAM + sprite registers */
	{ 0x9000, 0x97ff, xevious_sharedram2_r },    /* work RAM + sprite registers */
	{ 0xa000, 0xa7ff, xevious_sharedram3_r },	/* work RAM + sprite registers */
	{ 0xb000, 0xb7ff, xevious_fg_colorram_r },
	{ 0xb800, 0xbfff, xevious_bg_colorram_r },
	{ 0xc000, 0xc7ff, xevious_fg_videoram_r },
	{ 0xc800, 0xcfff, xevious_bg_videoram_r },
	{ 0xf000, 0xffff, xevious_bb_r },
MEMORY_END

static MEMORY_WRITE_START( xevious_writemem )
    { 0x0000, 0x3fff, MWA_NOP },	/* the only area different for each CPU */
	{ 0x6800, 0x681f, pengo_sound_w, &namco_soundregs },
	{ 0x6820, 0x6827, bosco_latch_w },	/* misc latches */
	{ 0x6830, 0x6830, watchdog_reset_w },
	{ 0x7000, 0x70ff, namco_06xx_0_data_w },
	{ 0x7100, 0x7100, namco_06xx_0_ctrl_w },
	{ 0x7800, 0x7fff, xevious_sharedram0_w, &xevious_sharedram },				/* work RAM */
	{ 0x8000, 0x87ff, xevious_sharedram1_w, &xevious_sr1 },	/* work RAM + sprite registers */
	{ 0x9000, 0x97ff, xevious_sharedram2_w, &xevious_sr2 },	/* work RAM + sprite registers */
	{ 0xa000, 0xa7ff, xevious_sharedram3_w, &xevious_sr3 },	/* work RAM + sprite registers */
	{ 0xb000, 0xb7ff, xevious_fg_colorram_w, &xevious_fg_colorram },
	{ 0xb800, 0xbfff, xevious_bg_colorram_w, &xevious_bg_colorram },
	{ 0xc000, 0xc7ff, xevious_fg_videoram_w, &xevious_fg_videoram },
	{ 0xc800, 0xcfff, xevious_bg_videoram_w, &xevious_bg_videoram },
	{ 0xd000, 0xd07f, xevious_vh_latch_w },
	{ 0xf000, 0xffff, xevious_bs_w },
MEMORY_END

static MEMORY_READ_START( digdug_readmem )
    { 0x0000, 0x3fff, MRA_ROM },			/* the only area different for each CPU */
	{ 0x7000, 0x70ff, namco_06xx_0_data_r },
	{ 0x7100, 0x7100, namco_06xx_0_ctrl_r },
	{ 0x8000, 0x83ff, digdug_videoram_r },	/* tilemap RAM (bottom half of RAM 0 */
	{ 0x8400, 0x87ff, digdug_sharedram0_r },	/* work RAM (top half for RAM 0 */
	{ 0x8800, 0x8bff, digdug_sharedram1_r },	/* work RAM + sprite registers */
	{ 0x9000, 0x93ff, digdug_sharedram2_r },	/* work RAM + sprite registers */
	{ 0x9800, 0x9bff, digdug_sharedram3_r },	/* work RAM + sprite registers */
	{ 0xa000, 0xa007, MRA_NOP },		/* video latches (spurious reads when setting latch bits) */
	{ 0xb800, 0xb83f, atari_vg_earom_r },	/* non volatile memory data */
MEMORY_END

static MEMORY_WRITE_START( digdug_writemem )
    { 0x0000, 0x3fff, MWA_NOP },			/* the only area different for each CPU */
	{ 0x6800, 0x681f, pengo_sound_w, &namco_soundregs },
	{ 0x6820, 0x6827, bosco_latch_w },						/* misc latches */
	{ 0x6830, 0x6830, watchdog_reset_w },
	{ 0x7000, 0x70ff, namco_06xx_0_data_w },
	{ 0x7100, 0x7100, namco_06xx_0_ctrl_w },
	{ 0x8000, 0x83ff, digdug_videoram_w, &digdug_videoram },	/* tilemap RAM (bottom half of RAM 0 */
	{ 0x8400, 0x87ff, digdug_sharedram0_w, &digdug_sharedram},					/* work RAM (top half for RAM 0 */
	{ 0x8800, 0x8bff, digdug_sharedram1_w, &digdug_objram },	/* work RAM + sprite registers */
	{ 0x9000, 0x93ff, digdug_sharedram2_w, &digdug_posram },	/* work RAM + sprite registers */
	{ 0x9800, 0x9bff, digdug_sharedram3_w, &digdug_flpram },	/* work RAM + sprite registers */
	{ 0xa000, 0xa007, digdug_PORT_w },		/* video latches (spurious reads when setting latch bits) */
	{ 0xb800, 0xb83f, atari_vg_earom_w },	                    /* non volatile memory data */
	{ 0xb840, 0xb840, atari_vg_earom_ctrl_w },					/* non volatile memory control */
MEMORY_END



/* bootleg 4th CPU replacing the 5xXX chips */
static MEMORY_READ_START( readmem4_galaga )
    { 0x0000, 0x0fff, MRA_ROM },
	{ 0x1000, 0x107f, MRA_RAM },
MEMORY_END

static MEMORY_WRITE_START( writemem4_galaga )
    { 0x0000, 0x0fff, MWA_ROM },
	{ 0x1000, 0x107f, MWA_RAM },
MEMORY_END


static MEMORY_READ_START( readmem4_battles )
    { 0x0000, 0x0fff, MRA_ROM },
	{ 0x4000, 0x4003, battles_input_port_r },
	{ 0x6000, 0x6000, battles_customio3_r },
	{ 0x7000, 0x7000, battles_customio_data3_r },
	{ 0x8000, 0x80ff, MRA_RAM },
MEMORY_END

static MEMORY_WRITE_START( writemem4_battles )
    { 0x0000, 0x0fff, MWA_ROM },
	{ 0x4001, 0x4001, battles_CPU4_coin_w },
	{ 0x5000, 0x5000, battles_noise_sound_w },
	{ 0x6000, 0x6000, battles_customio3_w },
	{ 0x7000, 0x7000, battles_customio_data3_w },
	{ 0x8000, 0x80ff, MWA_RAM },
MEMORY_END


static MEMORY_READ_START( readmem4_dzigzag )
    { 0x0000, 0x0fff, MRA_ROM },
	{ 0x1000, 0x107f, MRA_RAM },
	{ 0x4000, 0x4007, MRA_RAM },	/* dip switches? bits 0 & 1 used */
MEMORY_END

static MEMORY_WRITE_START( writemem4_dzigzag )
    { 0x0000, 0x0fff, MWA_ROM },
	{ 0x1000, 0x107f, MWA_RAM },
MEMORY_END


INPUT_PORTS_START( bosco )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )

	PORT_START	/* DSW A */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x01, "Easy" )
	PORT_DIPSETTING(    0x03, "Medium" )
	PORT_DIPSETTING(    0x02, "Hardest" )
	PORT_DIPSETTING(    0x00, "Auto" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "Freeze" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )

	PORT_START	/* DSW B */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	/* TODO: bonus scores are different for 5 lives */
	PORT_DIPNAME( 0x38, 0x08, "Bonus Fighter" )
	PORT_DIPSETTING(    0x30, "15K 50K" )
	PORT_DIPSETTING(    0x38, "20K 70K" )
	PORT_DIPSETTING(    0x08, "10K 50K 50K" )
	PORT_DIPSETTING(    0x10, "15K 50K 50K" )
	PORT_DIPSETTING(    0x18, "15K 70K 70K" )
	PORT_DIPSETTING(    0x20, "20K 70K 70K" )
	PORT_DIPSETTING(    0x28, "30K 100K 100K" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPNAME( 0xc0, 0x80, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x40, "2" )
	PORT_DIPSETTING(    0x80, "3" )
	PORT_DIPSETTING(    0xc0, "5" )
INPUT_PORTS_END

INPUT_PORTS_START( boscomd )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )

	PORT_START	/* DSW A */
	PORT_DIPNAME( 0x01, 0x01, "2 Credits Game" )
	PORT_DIPSETTING(    0x00, "1 Player" )
	PORT_DIPSETTING(    0x01, "2 Players" )
	PORT_DIPNAME( 0x06, 0x06, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x02, "Easy" )
	PORT_DIPSETTING(    0x06, "Medium" )
	PORT_DIPSETTING(    0x04, "Hardest" )
	PORT_DIPSETTING(    0x00, "Auto" )
	PORT_DIPNAME( 0x08, 0x08, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Freeze" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )

	PORT_START	/* DSW B */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	/* TODO: bonus scores are different for 5 lives */
	PORT_DIPNAME( 0x38, 0x08, "Bonus Fighter" )
	PORT_DIPSETTING(    0x30, "15K 50K" )
	PORT_DIPSETTING(    0x38, "20K 70K" )
	PORT_DIPSETTING(    0x08, "10K 50K 50K" )
	PORT_DIPSETTING(    0x10, "15K 50K 50K" )
	PORT_DIPSETTING(    0x18, "15K 70K 70K" )
	PORT_DIPSETTING(    0x20, "20K 70K 70K" )
	PORT_DIPSETTING(    0x28, "30K 100K 100K" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPNAME( 0xc0, 0x80, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x40, "2" )
	PORT_DIPSETTING(    0x80, "3" )
	PORT_DIPSETTING(    0xc0, "5" )
INPUT_PORTS_END


INPUT_PORTS_START( galaga )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_2WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_2WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_2WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_2WAY | IPF_COCKTAIL )

	PORT_START      /* DSW A */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x03, "Easy" )
	PORT_DIPSETTING(    0x00, "Medium" )
	PORT_DIPSETTING(    0x01, "Hard" )
	PORT_DIPSETTING(    0x02, "Hardest" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "Freeze" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Rack Test" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )

	PORT_START      /* DSW B */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	/* TODO: bonus scores are different for 5 lives */
	PORT_DIPNAME( 0x38, 0x10, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x20, "20K 60K 60K" )
	PORT_DIPSETTING(    0x18, "20K 60K" )
	PORT_DIPSETTING(    0x10, "20K 70K 70K" )
	PORT_DIPSETTING(    0x30, "20K 80K 80K" )
	PORT_DIPSETTING(    0x38, "30K 80K" )
	PORT_DIPSETTING(    0x08, "30K 100K 100K" )
	PORT_DIPSETTING(    0x28, "30K 120K 120K" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPNAME( 0xc0, 0x80, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x80, "3" )
	PORT_DIPSETTING(    0x40, "4" )
	PORT_DIPSETTING(    0xc0, "5" )
INPUT_PORTS_END

/* dip switches are slightly different */
INPUT_PORTS_START( galagamw )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_2WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_2WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_2WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_2WAY | IPF_COCKTAIL )

	PORT_START      /* DSW A */
	PORT_DIPNAME( 0x01, 0x01, "2 Credits Game" )
	PORT_DIPSETTING(    0x00, "1 Player" )
	PORT_DIPSETTING(    0x01, "2 Players" )
	PORT_DIPNAME( 0x06, 0x06, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x06, "Easy" )
	PORT_DIPSETTING(    0x00, "Medium" )
	PORT_DIPSETTING(    0x02, "Hard" )
	PORT_DIPSETTING(    0x04, "Hardest" )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "Freeze" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Rack Test" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )

	PORT_START      /* DSW B */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	/* TODO: bonus scores are different for 5 lives */
	PORT_DIPNAME( 0x38, 0x10, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x20, "20K 60K 60K" )
	PORT_DIPSETTING(    0x18, "20K 60K" )
	PORT_DIPSETTING(    0x10, "20K 70K 70K" )
	PORT_DIPSETTING(    0x30, "20K 80K 80K" )
	PORT_DIPSETTING(    0x38, "30K 80K" )
	PORT_DIPSETTING(    0x08, "30K 100K 100K" )
	PORT_DIPSETTING(    0x28, "30K 120K 120K" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPNAME( 0xc0, 0x80, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x80, "3" )
	PORT_DIPSETTING(    0x40, "4" )
	PORT_DIPSETTING(    0xc0, "5" )
INPUT_PORTS_END


INPUT_PORTS_START( xevious )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )

	PORT_START	/* DSW A */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	/* TODO: bonus scores are different for 5 lives */
	PORT_DIPNAME( 0x1c, 0x1c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x18, "10K 40K 40K" )
	PORT_DIPSETTING(    0x14, "10K 50K 50K" )
	PORT_DIPSETTING(    0x10, "20K 50K 50K" )
	PORT_DIPSETTING(    0x1c, "20K 60K 60K" )
	PORT_DIPSETTING(    0x0c, "20K 70K 70K" )
	PORT_DIPSETTING(    0x08, "20K 80K 80K" )
	PORT_DIPSETTING(    0x04, "20K 60K" )
	PORT_DIPSETTING(    0x00, "None" )
	/* Bonus scores for 5 lives
	PORT_DIPNAME( 0x1c, 0x1c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x18, "10K 50K 50K" )
	PORT_DIPSETTING(    0x14, "20K 50K 50K" )
	PORT_DIPSETTING(    0x10, "20K 60K 60K" )
	PORT_DIPSETTING(    0x1c, "20K 70K 70K" )
	PORT_DIPSETTING(    0x0c, "20K 80K 80K" )
	PORT_DIPSETTING(    0x08, "30K 100K 100K" )
	PORT_DIPSETTING(    0x04, "20K 80K" )
	PORT_DIPSETTING(    0x00, "None" )
	*/
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x40, "1" )
	PORT_DIPSETTING(    0x20, "2" )
	PORT_DIPSETTING(    0x60, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )

	PORT_START	/* DSW B */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_DIPNAME( 0x02, 0x02, "Flags Award Bonus Life" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x40, "Easy" )
	PORT_DIPSETTING(    0x60, "Normal" )
	PORT_DIPSETTING(    0x20, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x80, 0x80, "Freeze" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

/* same as xevious but different "Coin B" Dip Switch and "Copyright" Dip Switch instead of "Freeze" */
INPUT_PORTS_START( xeviousa )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )

	PORT_START	/* DSW A */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	/* TODO: bonus scores are different for 5 lives */
	PORT_DIPNAME( 0x1c, 0x1c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x18, "10K 40K 40K" )
	PORT_DIPSETTING(    0x14, "10K 50K 50K" )
	PORT_DIPSETTING(    0x10, "20K 50K 50K" )
	PORT_DIPSETTING(    0x1c, "20K 60K 60K" )
	PORT_DIPSETTING(    0x0c, "20K 70K 70K" )
	PORT_DIPSETTING(    0x08, "20K 80K 80K" )
	PORT_DIPSETTING(    0x04, "20K 60K" )
	PORT_DIPSETTING(    0x00, "None" )
	/* Bonus scores for 5 lives
	PORT_DIPNAME( 0x1c, 0x1c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x18, "10K 50K 50K" )
	PORT_DIPSETTING(    0x14, "20K 50K 50K" )
	PORT_DIPSETTING(    0x10, "20K 60K 60K" )
	PORT_DIPSETTING(    0x1c, "20K 70K 70K" )
	PORT_DIPSETTING(    0x0c, "20K 80K 80K" )
	PORT_DIPSETTING(    0x08, "30K 100K 100K" )
	PORT_DIPSETTING(    0x04, "20K 80K" )
	PORT_DIPSETTING(    0x00, "None" )
	*/
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x40, "1" )
	PORT_DIPSETTING(    0x20, "2" )
	PORT_DIPSETTING(    0x60, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )

	PORT_START	/* DSW B */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_DIPNAME( 0x02, 0x02, "Flags Award Bonus Life" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_6C ) )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x40, "Easy" )
	PORT_DIPSETTING(    0x60, "Normal" )
	PORT_DIPSETTING(    0x20, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	/* when switch is on Namco, high score names are 10 letters long */
	PORT_DIPNAME( 0x80, 0x80, "Copyright" )
	PORT_DIPSETTING(    0x00, "Namco" )
	PORT_DIPSETTING(    0x80, "Atari/Namco" )
INPUT_PORTS_END

/* same as xevious but "Copyright" Dip Switch instead of "Freeze" */
INPUT_PORTS_START( xeviousb )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )

	PORT_START	/* DSW A */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	/* TODO: bonus scores are different for 5 lives */
	PORT_DIPNAME( 0x1c, 0x1c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x18, "10K 40K 40K" )
	PORT_DIPSETTING(    0x14, "10K 50K 50K" )
	PORT_DIPSETTING(    0x10, "20K 50K 50K" )
	PORT_DIPSETTING(    0x1c, "20K 60K 60K" )
	PORT_DIPSETTING(    0x0c, "20K 70K 70K" )
	PORT_DIPSETTING(    0x08, "20K 80K 80K" )
	PORT_DIPSETTING(    0x04, "20K 60K" )
	PORT_DIPSETTING(    0x00, "None" )
	/* Bonus scores for 5 lives
	PORT_DIPNAME( 0x1c, 0x1c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x18, "10K 50K 50K" )
	PORT_DIPSETTING(    0x14, "20K 50K 50K" )
	PORT_DIPSETTING(    0x10, "20K 60K 60K" )
	PORT_DIPSETTING(    0x1c, "20K 70K 70K" )
	PORT_DIPSETTING(    0x0c, "20K 80K 80K" )
	PORT_DIPSETTING(    0x08, "30K 100K 100K" )
	PORT_DIPSETTING(    0x04, "20K 80K" )
	PORT_DIPSETTING(    0x00, "None" )
	*/
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x40, "1" )
	PORT_DIPSETTING(    0x20, "2" )
	PORT_DIPSETTING(    0x60, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )

	PORT_START	/* DSW B */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_DIPNAME( 0x02, 0x02, "Flags Award Bonus Life" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x40, "Easy" )
	PORT_DIPSETTING(    0x60, "Normal" )
	PORT_DIPSETTING(    0x20, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	/* when switch is on Namco, high score names are 10 letters long */
	PORT_DIPNAME( 0x80, 0x80, "Copyright" )
	PORT_DIPSETTING(    0x00, "Namco" )
	PORT_DIPSETTING(    0x80, "Atari/Namco" )
INPUT_PORTS_END

/* same as xevious but different "Coin B" Dip Switch and inverted "Freeze" Dip Switch */
INPUT_PORTS_START( sxevious )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )

	PORT_START	/* DSW A */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	/* TODO: bonus scores are different for 5 lives */
	PORT_DIPNAME( 0x1c, 0x1c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x18, "10K 40K 40K" )
	PORT_DIPSETTING(    0x14, "10K 50K 50K" )
	PORT_DIPSETTING(    0x10, "20K 50K 50K" )
	PORT_DIPSETTING(    0x1c, "20K 60K 60K" )
	PORT_DIPSETTING(    0x0c, "20K 70K 70K" )
	PORT_DIPSETTING(    0x08, "20K 80K 80K" )
	PORT_DIPSETTING(    0x04, "20K 60K" )
	PORT_DIPSETTING(    0x00, "None" )
	/* Bonus scores for 5 lives
	PORT_DIPNAME( 0x1c, 0x1c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x18, "10K 50K 50K" )
	PORT_DIPSETTING(    0x14, "20K 50K 50K" )
	PORT_DIPSETTING(    0x10, "20K 60K 60K" )
	PORT_DIPSETTING(    0x1c, "20K 70K 70K" )
	PORT_DIPSETTING(    0x0c, "20K 80K 80K" )
	PORT_DIPSETTING(    0x08, "30K 100K 100K" )
	PORT_DIPSETTING(    0x04, "20K 80K" )
	PORT_DIPSETTING(    0x00, "None" )
	*/
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x40, "1" )
	PORT_DIPSETTING(    0x20, "2" )
	PORT_DIPSETTING(    0x60, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )

	PORT_START	/* DSW B */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_DIPNAME( 0x02, 0x02, "Flags Award Bonus Life" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_6C ) )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x40, "Easy" )
	PORT_DIPSETTING(    0x60, "Normal" )
	PORT_DIPSETTING(    0x20, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x80, 0x00, "Freeze" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )
INPUT_PORTS_END


INPUT_PORTS_START( digdug )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_COCKTAIL )

	PORT_START	/* DSW A */
	PORT_DIPNAME( 0x07, 0x01, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_7C ) )
	/* TODO: bonus scores are different for 5 lives */
	PORT_DIPNAME( 0x38, 0x18, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x20, "10k 40k 40k" )
	PORT_DIPSETTING(    0x10, "10k 50k 50k" )
	PORT_DIPSETTING(    0x30, "20k 60k 60k" )
	PORT_DIPSETTING(    0x08, "20k 70k 70k" )
	PORT_DIPSETTING(    0x28, "10k 40k" )
	PORT_DIPSETTING(    0x18, "20k 60k" )
	PORT_DIPSETTING(    0x38, "10k" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPNAME( 0xc0, 0x80, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x40, "2" )
	PORT_DIPSETTING(    0x80, "3" )
	PORT_DIPSETTING(    0xc0, "5" )

	PORT_START	/* DSW B */
	PORT_DIPNAME( 0xc0, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x20, 0x20, "Freeze" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0x08, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Easy" )
	PORT_DIPSETTING(    0x02, "Medium" )
	PORT_DIPSETTING(    0x01, "Hard" )
	PORT_DIPSETTING(    0x03, "Hardest" )
INPUT_PORTS_END



static struct GfxLayout charlayout_2bpp =
{
	8,8,
	RGN_FRAC(1,1),
	2,
	{ 0, 4 },
	{ 8*8+0, 8*8+1, 8*8+2, 8*8+3, 0, 1, 2, 3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	16*8
};

static struct GfxLayout charlayout_xevious =
{
	8,8,
	RGN_FRAC(1,1),
	1,
	{ 0 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8
};

static struct GfxLayout charlayout_digdug =
{
	8,8,
	RGN_FRAC(1,1),
	1,
	{ 0 },
	{ 7, 6, 5, 4, 3, 2, 1, 0 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8
};

static struct GfxLayout bgcharlayout =
{
	8,8,
	RGN_FRAC(1,2),
	2,
	{ 0, RGN_FRAC(1,2) },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8
};

static struct GfxLayout spritelayout_bosco =
{
	16,16,
	RGN_FRAC(1,1),
	2,
	{ 0, 4 },
	{ 8*8, 8*8+1, 8*8+2, 8*8+3, 16*8+0, 16*8+1, 16*8+2, 16*8+3,
			24*8+0, 24*8+1, 24*8+2, 24*8+3, 0, 1, 2, 3  },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			32*8, 33*8, 34*8, 35*8, 36*8, 37*8, 38*8, 39*8 },
	64*8
};

static struct GfxLayout spritelayout_galaga =
{
	16,16,
	RGN_FRAC(1,1),
	2,
	{ 0, 4 },
	{ 0, 1, 2, 3, 8*8, 8*8+1, 8*8+2, 8*8+3, 16*8+0, 16*8+1, 16*8+2, 16*8+3,
			24*8+0, 24*8+1, 24*8+2, 24*8+3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			32*8, 33*8, 34*8, 35*8, 36*8, 37*8, 38*8, 39*8 },
	64*8
};

static struct GfxLayout spritelayout_xevious =
{
	16,16,
	RGN_FRAC(1,2),
	3,
	{ RGN_FRAC(1,2)+4, 0, 4 },
	{ 0, 1, 2, 3, 8*8+0, 8*8+1, 8*8+2, 8*8+3,
			16*8+0, 16*8+1, 16*8+2, 16*8+3, 24*8+0, 24*8+1, 24*8+2, 24*8+3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			32*8, 33*8, 34*8, 35*8, 36*8, 37*8, 38*8, 39*8 },
	64*8
};

static struct GfxLayout dotlayout =
{
	4,4,
	8,
	3,	/* 2 bits color + 1 bit transparency */
	{ 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8 },
	{ 0*32, 1*32, 2*32, 3*32 },
	16*8
};

static struct GfxDecodeInfo gfxdecodeinfo_bosco[] =
{
	{ REGION_GFX1, 0, &charlayout_2bpp,       0, 64 },
	{ REGION_GFX2, 0, &spritelayout_bosco, 64*4, 64 },
	{ REGION_GFX3, 0, &dotlayout,     64*4+64*4,  1 },
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo gfxdecodeinfo_galaga[] =
{
	{ REGION_GFX1, 0, &charlayout_2bpp,        0, 64 },
	{ REGION_GFX2, 0, &spritelayout_galaga, 64*4, 64 },
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo gfxdecodeinfo_xevious[] =
{
	{ REGION_GFX1, 0, &charlayout_xevious, 128*4+64*8,  64 },
	{ REGION_GFX2, 0, &bgcharlayout,                0, 128 },
	{ REGION_GFX3, 0, &spritelayout_xevious,    128*4,  64 },
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo gfxdecodeinfo_digdug[] =
{
	{ REGION_GFX1, 0, &charlayout_digdug,         0, 16 },
	{ REGION_GFX2, 0, &spritelayout_galaga,    16*2, 64 },
	{ REGION_GFX3, 0, &charlayout_2bpp, 64*4 + 16*2, 64 },
	{ -1 } /* end of array */
};



static struct namco_interface namco_interface =
{
	18432000/6/32,	/* 96 kHz sample rate */
	3,				/* number of voices */
	100,			/* playback volume */
	REGION_SOUND1	/* memory region */
};

static struct namco_52xx_interface namco_52xx_interface =
{
	18432000/12,	/* 1.536 MHz */
	50,				/* volume */
	REGION_SOUND2	/* memory region */
};

static struct namco_54xx_interface namco_54xx_interface =
{
	18432000/12,		/* 1.536 MHz */
	{ 100, 100, 100 }	/* volume of the three outputs */
};

static const char *bosco_sample_names[] =
{
	"*bosco",
	"bigbang.wav",
	"midbang.wav",
	"shot.wav",
	0	/* end of array */
};

static struct Samplesinterface samples_interface_bosco =
{
	3,	/* 3 channels */
	100,	/* volume */
	bosco_sample_names
};

static const char *galaga_sample_names[] =
{
	"*galaga",
	"bang.wav",
	"bang.wav",
/*	"init.wav", */
	0       /* end of array */
};

static struct Samplesinterface samples_interface_galaga =
{
	3,	/* 3 channels */
	80,	/* volume */
	galaga_sample_names
};

static const char *xevious_sample_names[] =
{
	"*xevious",
	"explo2.wav",	/* Solvalou explosion */
	"explo3.wav",	/* credit */
	"explo4.wav",	/* Garu Zakato explosion */
	"explo1.wav",	/* ground target explosion */
	0	/* end of array */
};

struct Samplesinterface samples_interface_xevious =
{
	3,	/* 3 channels */
	80,	/* volume */
	xevious_sample_names
};

static const char *battles_sample_names[] =
{
	"*battles",
	"explo1.wav",	/* ground target explosion */
	"explo2.wav",	/* Solvalou explosion */
	0	/* end of array */
};

struct Samplesinterface samples_interface_battles =
{
	1,	/* one channel */
	80,	/* volume */
	battles_sample_names
};



static MACHINE_DRIVER_START( bosco )

	/* basic machine hardware */
	MDRV_CPU_ADD(Z80, 18432000/6)	/* 3.072 MHz */
	MDRV_CPU_MEMORY(bosco_readmem,bosco_writemem)
	MDRV_CPU_VBLANK_INT(irq0_line_assert,1)

	MDRV_CPU_ADD(Z80, 18432000/6)	/* 3.072 MHz */
	MDRV_CPU_MEMORY(bosco_readmem,bosco_writemem)
	MDRV_CPU_VBLANK_INT(irq0_line_assert,1)

	MDRV_CPU_ADD(Z80, 18432000/6)	/* 3.072 MHz */
	MDRV_CPU_MEMORY(bosco_readmem,bosco_writemem)
	MDRV_CPU_VBLANK_INT(nmi_line_pulse,2)	/* 64V */

	MDRV_FRAMES_PER_SECOND(60.606060)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)
	MDRV_INTERLEAVE(100)	/* 100 CPU slices per frame - an high value to ensure proper */
							/* synchronization of the CPUs */
	MDRV_MACHINE_INIT(bosco)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(36*8, 32*8)
	MDRV_VISIBLE_AREA(0*8, 36*8-1, 2*8, 30*8-1)
	MDRV_GFXDECODE(gfxdecodeinfo_bosco)
	MDRV_PALETTE_LENGTH(32+64)
	MDRV_COLORTABLE_LENGTH(64*4+64*4+8)

	MDRV_PALETTE_INIT(bosco)
	MDRV_VIDEO_START(bosco)
	MDRV_VIDEO_UPDATE(bosco)
	MDRV_VIDEO_EOF(bosco)

	/* sound hardware */
	MDRV_SOUND_ADD(NAMCO_15XX, namco_interface)
	MDRV_SOUND_ADD(NAMCO_52XX, namco_52xx_interface)
	MDRV_SOUND_ADD(NAMCO_54XX, namco_54xx_interface)
	MDRV_SOUND_ADD(SAMPLES, samples_interface_bosco)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( galaga )

	/* basic machine hardware */
	MDRV_CPU_ADD(Z80, 18432000/6)	/* 3.072 MHz */
	MDRV_CPU_MEMORY(galaga_readmem,galaga_writemem)
	MDRV_CPU_VBLANK_INT(irq0_line_assert,1)

	MDRV_CPU_ADD(Z80, 18432000/6)	/* 3.072 MHz */
	MDRV_CPU_MEMORY(galaga_readmem,galaga_writemem)
	MDRV_CPU_VBLANK_INT(irq0_line_assert,1)

	MDRV_CPU_ADD(Z80, 18432000/6)	/* 3.072 MHz */
	MDRV_CPU_MEMORY(galaga_readmem,galaga_writemem)
	MDRV_CPU_VBLANK_INT(galaga_cpu3_nmi,4)	/* 64V (see notes at the top of the driver) */

	MDRV_FRAMES_PER_SECOND(60.606060)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)
	MDRV_INTERLEAVE(100)	/* 100 CPU slices per frame - an high value to ensure proper */
							/* synchronization of the CPUs */
	MDRV_MACHINE_INIT(galaga)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(36*8, 28*8)
	MDRV_VISIBLE_AREA(0*8, 36*8-1, 0*8, 28*8-1)
	MDRV_GFXDECODE(gfxdecodeinfo_galaga)
	MDRV_PALETTE_LENGTH(32+64)
	MDRV_COLORTABLE_LENGTH(64*4+64*4)

	MDRV_PALETTE_INIT(galaga)
	MDRV_VIDEO_START(galaga)
	MDRV_VIDEO_UPDATE(galaga)
	MDRV_VIDEO_EOF(galaga)

	/* sound hardware */
	MDRV_SOUND_ADD(NAMCO_15XX, namco_interface)
	MDRV_SOUND_ADD(SAMPLES, samples_interface_galaga)
	MDRV_SOUND_ADD(NAMCO_54XX, namco_54xx_interface)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( galagab )

	/* basic machine hardware */
	MDRV_IMPORT_FROM(galaga)

	MDRV_CPU_ADD(Z80, 18432000/6)	/* 3.072 MHz */
	MDRV_CPU_MEMORY(readmem4_galaga,writemem4_galaga)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( xevious )

	/* basic machine hardware */
	MDRV_CPU_ADD(Z80, 18432000/6)	/* 3.072 MHz */
	MDRV_CPU_MEMORY(xevious_readmem,xevious_writemem)
	MDRV_CPU_VBLANK_INT(irq0_line_pulse,1)

	MDRV_CPU_ADD(Z80, 18432000/6)	/* 3.072 MHz */
	MDRV_CPU_MEMORY(xevious_readmem,xevious_writemem)
	MDRV_CPU_VBLANK_INT(irq0_line_pulse,1)

	MDRV_CPU_ADD(Z80, 18432000/6)	/* 3.072 MHz */
	MDRV_CPU_MEMORY(xevious_readmem,xevious_writemem)
	MDRV_CPU_VBLANK_INT(nmi_line_pulse,2)	/* 64V */

	MDRV_FRAMES_PER_SECOND(60.606060)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)
	MDRV_INTERLEAVE(100)	/* 100 CPU slices per frame - an high value to ensure proper */
							/* synchronization of the CPUs */
	MDRV_MACHINE_INIT(xevious)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(36*8, 28*8)
	MDRV_VISIBLE_AREA(0*8, 36*8-1, 0*8, 28*8-1)
	MDRV_GFXDECODE(gfxdecodeinfo_xevious)
	MDRV_PALETTE_LENGTH(128+1)
	MDRV_COLORTABLE_LENGTH(128*4+64*8+64*2)

	MDRV_PALETTE_INIT(xevious)
	MDRV_VIDEO_START(xevious)
	MDRV_VIDEO_UPDATE(xevious)

	/* sound hardware */
	MDRV_SOUND_ADD(NAMCO_15XX, namco_interface)
	MDRV_SOUND_ADD_TAG("samples", SAMPLES, samples_interface_xevious)
	MDRV_SOUND_ADD(NAMCO_54XX, namco_54xx_interface)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( battles )

	/* basic machine hardware */
	MDRV_IMPORT_FROM( xevious )

	MDRV_CPU_ADD(Z80, 18432000/6)	/* 3.072 MHz */
	MDRV_CPU_MEMORY(readmem4_battles,writemem4_battles)
	MDRV_CPU_VBLANK_INT(battles_interrupt_4,1)

	MDRV_MACHINE_INIT(battles)

	/* video hardware */
	MDRV_PALETTE_INIT(battles)

	/* sound hardware */
	MDRV_SOUND_REPLACE("samples", SAMPLES, samples_interface_battles)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( digdug )

	/* basic machine hardware */
	MDRV_CPU_ADD(Z80, 18432000/6)	/* 3.072 MHz */
	MDRV_CPU_MEMORY(digdug_readmem,digdug_writemem)
	MDRV_CPU_VBLANK_INT(irq0_line_assert,1)

	MDRV_CPU_ADD(Z80, 18432000/6)	/* 3.072 MHz */
	MDRV_CPU_MEMORY(digdug_readmem,digdug_writemem)
	MDRV_CPU_VBLANK_INT(irq0_line_assert,1)

	MDRV_CPU_ADD(Z80, 18432000/6)	/* 3.072 MHz */
	MDRV_CPU_MEMORY(digdug_readmem,digdug_writemem)
	MDRV_CPU_VBLANK_INT(nmi_line_pulse,2)	/* 64V */

	MDRV_FRAMES_PER_SECOND(60.606060)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)
	MDRV_INTERLEAVE(100)	/* 100 CPU slices per frame - an high value to ensure proper */
							/* synchronization of the CPUs */
	MDRV_MACHINE_INIT(digdug)
	MDRV_NVRAM_HANDLER(atari_vg)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(36*8, 28*8)
	MDRV_VISIBLE_AREA(0*8, 36*8-1, 0*8, 28*8-1)
	MDRV_GFXDECODE(gfxdecodeinfo_digdug)
	MDRV_PALETTE_LENGTH(32)
	MDRV_COLORTABLE_LENGTH(16*2+64*4+64*4)

	MDRV_PALETTE_INIT(digdug)
	MDRV_VIDEO_START(digdug)
	MDRV_VIDEO_UPDATE(digdug)

	/* sound hardware */
	MDRV_SOUND_ADD(NAMCO_15XX, namco_interface)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( dzigzag )

	/* basic machine hardware */
	MDRV_IMPORT_FROM(digdug)

	MDRV_CPU_ADD(Z80, 18432000/6)	/* 3.072 MHz */
	MDRV_CPU_MEMORY(readmem4_dzigzag,writemem4_dzigzag)
MACHINE_DRIVER_END



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( bosco )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )	/* 64k for code for the first CPU  */
	ROM_LOAD( "bos3_1.bin",   0x0000, 0x1000, CRC(96021267) SHA1(bd49b0caabcccf9df45a272d767456a4fc8a7c07) )
	ROM_LOAD( "bos1_2.bin",   0x1000, 0x1000, CRC(2d8f3ebe) SHA1(75de1cba7531ae4bf7fbbef7b8e37b9fec4ed0d0) )
	ROM_LOAD( "bos1_3.bin",   0x2000, 0x1000, CRC(c80ccfa5) SHA1(f2bbec2ea9846d4601f06c0b4242744447a88fda) )
	ROM_LOAD( "bos1_4b.bin",  0x3000, 0x1000, CRC(a3f7f4ab) SHA1(eb26184311bae0767c7a5593926e6eadcbcb680e) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* 64k for the second CPU */
	ROM_LOAD( "bos1_5c.bin",  0x0000, 0x1000, CRC(a7c8e432) SHA1(3607be75daa10f1f98dbfd9e600c5ba513130d44) )
	ROM_LOAD( "bos3_6.bin",   0x1000, 0x1000, CRC(4543cf82) SHA1(50ad7d1ab6694eb8fab88d0fa79ee04f6984f3ca) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )	/* 64k for the third CPU  */
	ROM_LOAD( "2900.3e",      0x0000, 0x1000, CRC(d45a4911) SHA1(547236adca9174f5cc0ec05b9649618bb92ba630) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "5300.5d",      0x0000, 0x1000, CRC(a956d3c5) SHA1(c5a9d7b1f9b4acda8fb9762414e085cb5fb80c9e) )

	ROM_REGION( 0x1000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "5200.5e",      0x0000, 0x1000, CRC(e869219c) SHA1(425614cd0642743a82ef9c1aada29774a92203ea) )

	ROM_REGION( 0x0100, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "prom.2d",      0x0000, 0x0100, CRC(9b69b543) SHA1(47af3f67e50794e839b74fe61197af2228084efd) )	/* dots */

	ROM_REGION( 0x0160, REGION_PROMS, 0 )
	ROM_LOAD( "bosco.6b",     0x0000, 0x0020, CRC(d2b96fb0) SHA1(54c100ec9d173d7dd48a453ebed5f625053cb6e0) )	/* palette */
	ROM_LOAD( "bosco.4m",     0x0020, 0x0100, CRC(4e15d59c) SHA1(3542ead6421d169c3569e121ec2be304e108787c) )	/* lookup table */
	ROM_LOAD( "prom.2r",      0x0120, 0x0020, CRC(b88d5ba9) SHA1(7b97a38a540b7ca4b7d9ae338ec38b9b1a337846) )	/* video layout (not used) */
	ROM_LOAD( "prom.7h",      0x0140, 0x0020, CRC(87d61353) SHA1(c7493e52662c921625676a4a4e8cf4371bd938b7) )	/* video timing (not used) */

	ROM_REGION( 0x0200, REGION_SOUND1, 0 )
	ROM_LOAD( "prom.1d",      0x0000, 0x0100, CRC(de2316c6) SHA1(0e55c56046331888d1d3f0d9823d2ceb203e7d3f) )
	ROM_LOAD( "prom.5c",      0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )	/* timing - not used */

	ROM_REGION( 0x3000, REGION_SOUND2, 0 )	/* ROMs for digitised speech */
	ROM_LOAD( "4900.5n",      0x0000, 0x1000, CRC(09acc978) SHA1(2b264aaeb6eba70ad91593413dca733990e5467b) )
	ROM_LOAD( "5000.5m",      0x1000, 0x1000, CRC(e571e959) SHA1(9c81d7bec73bc605f7dd9a089171b0f34c4bb09a) )
	ROM_LOAD( "5100.5l",      0x2000, 0x1000, CRC(17ac9511) SHA1(266f3fae90d2fe38d109096d352863a52b379899) )
ROM_END

ROM_START( boscoo )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )	/* 64k for code for the first CPU  */
	ROM_LOAD( "bos1_1.bin",   0x0000, 0x1000, CRC(0d9920e7) SHA1(e7633233f603ccb5b7a970ed5b58ef361ef2c94e) )
	ROM_LOAD( "bos1_2.bin",   0x1000, 0x1000, CRC(2d8f3ebe) SHA1(75de1cba7531ae4bf7fbbef7b8e37b9fec4ed0d0) )
	ROM_LOAD( "bos1_3.bin",   0x2000, 0x1000, CRC(c80ccfa5) SHA1(f2bbec2ea9846d4601f06c0b4242744447a88fda) )
	ROM_LOAD( "bos1_4b.bin",  0x3000, 0x1000, CRC(a3f7f4ab) SHA1(eb26184311bae0767c7a5593926e6eadcbcb680e) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* 64k for the second CPU */
	ROM_LOAD( "bos1_5c.bin",  0x0000, 0x1000, CRC(a7c8e432) SHA1(3607be75daa10f1f98dbfd9e600c5ba513130d44) )
	ROM_LOAD( "2800.3h",      0x1000, 0x1000, CRC(31b8c648) SHA1(de0db24d385d2361ec989bf32388df8202ad535c) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )	/* 64k for the third CPU  */
	ROM_LOAD( "2900.3e",      0x0000, 0x1000, CRC(d45a4911) SHA1(547236adca9174f5cc0ec05b9649618bb92ba630) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "5300.5d",      0x0000, 0x1000, CRC(a956d3c5) SHA1(c5a9d7b1f9b4acda8fb9762414e085cb5fb80c9e) )

	ROM_REGION( 0x1000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "5200.5e",      0x0000, 0x1000, CRC(e869219c) SHA1(425614cd0642743a82ef9c1aada29774a92203ea) )

	ROM_REGION( 0x0100, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "prom.2d",      0x0000, 0x0100, CRC(9b69b543) SHA1(47af3f67e50794e839b74fe61197af2228084efd) )	/* dots */

	ROM_REGION( 0x0260, REGION_PROMS, 0 )
	ROM_LOAD( "bosco.6b",     0x0000, 0x0020, CRC(d2b96fb0) SHA1(54c100ec9d173d7dd48a453ebed5f625053cb6e0) )	/* palette */
	ROM_LOAD( "bosco.4m",     0x0020, 0x0100, CRC(4e15d59c) SHA1(3542ead6421d169c3569e121ec2be304e108787c) )	/* lookup table */
	ROM_LOAD( "prom.2r",      0x0120, 0x0020, CRC(b88d5ba9) SHA1(7b97a38a540b7ca4b7d9ae338ec38b9b1a337846) )	/* video layout (not used) */
	ROM_LOAD( "prom.7h",      0x0140, 0x0020, CRC(87d61353) SHA1(c7493e52662c921625676a4a4e8cf4371bd938b7) )	/* video timing (not used) */

	ROM_REGION( 0x0200, REGION_SOUND1, 0 )
	ROM_LOAD( "prom.1d",      0x0000, 0x0100, CRC(de2316c6) SHA1(0e55c56046331888d1d3f0d9823d2ceb203e7d3f) )
	ROM_LOAD( "prom.5c",      0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )	/* timing - not used */

	ROM_REGION( 0x3000, REGION_SOUND2, 0 )	/* ROMs for digitised speech */
	ROM_LOAD( "4900.5n",      0x0000, 0x1000, CRC(09acc978) SHA1(2b264aaeb6eba70ad91593413dca733990e5467b) )
	ROM_LOAD( "5000.5m",      0x1000, 0x1000, CRC(e571e959) SHA1(9c81d7bec73bc605f7dd9a089171b0f34c4bb09a) )
	ROM_LOAD( "5100.5l",      0x2000, 0x1000, CRC(17ac9511) SHA1(266f3fae90d2fe38d109096d352863a52b379899) )
ROM_END

ROM_START( boscoo2 )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )	/* 64k for code for the first CPU  */
	ROM_LOAD( "bos1_1.bin",   0x0000, 0x1000, CRC(0d9920e7) SHA1(e7633233f603ccb5b7a970ed5b58ef361ef2c94e) )
	ROM_LOAD( "bos1_2.bin",   0x1000, 0x1000, CRC(2d8f3ebe) SHA1(75de1cba7531ae4bf7fbbef7b8e37b9fec4ed0d0) )
	ROM_LOAD( "bos1_3.bin",   0x2000, 0x1000, CRC(c80ccfa5) SHA1(f2bbec2ea9846d4601f06c0b4242744447a88fda) )
	ROM_LOAD( "bos1_4.3k",    0x3000, 0x1000, CRC(7ebea2b8) SHA1(92fc66526ed77f3efd947b7d321b255aba4a0140) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* 64k for the second CPU */
	ROM_LOAD( "bos1_5b.3j",   0x0000, 0x1000, CRC(3d6955a8) SHA1(f89860d74865da5ced2f5b2196bdaa8eeb5e2322) )
	ROM_LOAD( "2800.3h",      0x1000, 0x1000, CRC(31b8c648) SHA1(de0db24d385d2361ec989bf32388df8202ad535c) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )	/* 64k for the third CPU  */
	ROM_LOAD( "2900.3e",      0x0000, 0x1000, CRC(d45a4911) SHA1(547236adca9174f5cc0ec05b9649618bb92ba630) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "5300.5d",      0x0000, 0x1000, CRC(a956d3c5) SHA1(c5a9d7b1f9b4acda8fb9762414e085cb5fb80c9e) )

	ROM_REGION( 0x1000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "5200.5e",      0x0000, 0x1000, CRC(e869219c) SHA1(425614cd0642743a82ef9c1aada29774a92203ea) )

	ROM_REGION( 0x0100, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "prom.2d",      0x0000, 0x0100, CRC(9b69b543) SHA1(47af3f67e50794e839b74fe61197af2228084efd) )	/* dots */

	ROM_REGION( 0x0260, REGION_PROMS, 0 )
	ROM_LOAD( "bosco.6b",     0x0000, 0x0020, CRC(d2b96fb0) SHA1(54c100ec9d173d7dd48a453ebed5f625053cb6e0) )	/* palette */
	ROM_LOAD( "bosco.4m",     0x0020, 0x0100, CRC(4e15d59c) SHA1(3542ead6421d169c3569e121ec2be304e108787c) )	/* lookup table */
	ROM_LOAD( "prom.2r",      0x0120, 0x0020, CRC(b88d5ba9) SHA1(7b97a38a540b7ca4b7d9ae338ec38b9b1a337846) )	/* video layout (not used) */
	ROM_LOAD( "prom.7h",      0x0140, 0x0020, CRC(87d61353) SHA1(c7493e52662c921625676a4a4e8cf4371bd938b7) )	/* video timing (not used) */

	ROM_REGION( 0x0200, REGION_SOUND1, 0 )
	ROM_LOAD( "prom.1d",      0x0000, 0x0100, CRC(de2316c6) SHA1(0e55c56046331888d1d3f0d9823d2ceb203e7d3f) )
	ROM_LOAD( "prom.5c",      0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )	/* timing - not used */

	ROM_REGION( 0x3000, REGION_SOUND2, 0 )	/* ROMs for digitised speech */
	ROM_LOAD( "4900.5n",      0x0000, 0x1000, CRC(09acc978) SHA1(2b264aaeb6eba70ad91593413dca733990e5467b) )
	ROM_LOAD( "5000.5m",      0x1000, 0x1000, CRC(e571e959) SHA1(9c81d7bec73bc605f7dd9a089171b0f34c4bb09a) )
	ROM_LOAD( "5100.5l",      0x2000, 0x1000, CRC(17ac9511) SHA1(266f3fae90d2fe38d109096d352863a52b379899) )
ROM_END

ROM_START( boscomd )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )	/* 64k for code for the first CPU  */
	ROM_LOAD( "3n",       0x0000, 0x1000, CRC(441b501a) SHA1(7b4921ff40b3c56950fd32aa0ec5563b02a00929) )
	ROM_LOAD( "3m",       0x1000, 0x1000, CRC(a3c5c7ef) SHA1(70a095a8dbca857245a70404f803916f519e0cbc) )
	ROM_LOAD( "3l",       0x2000, 0x1000, CRC(6ca9a0cf) SHA1(8f70e29beae921e63cd65689a618ca678dd14614) )
	ROM_LOAD( "3k",       0x3000, 0x1000, CRC(d83bacc5) SHA1(cf2fbfa81dabb9b6bcf436d61992e705723776fb) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* 64k for the second CPU */
	ROM_LOAD( "3j",       0x0000, 0x1000, CRC(4374e39a) SHA1(7571fd5961f49a0e9ba4301ddd0aca52e94e2f8b) )
	ROM_LOAD( "3h",       0x1000, 0x1000, CRC(04e9fcef) SHA1(2115a9718d511854848704e2693f9efa1c80a307) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )	/* 64k for the third CPU  */
	ROM_LOAD( "2900.3e",      0x0000, 0x1000, CRC(d45a4911) SHA1(547236adca9174f5cc0ec05b9649618bb92ba630) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "5300.5d",      0x0000, 0x1000, CRC(a956d3c5) SHA1(c5a9d7b1f9b4acda8fb9762414e085cb5fb80c9e) )

	ROM_REGION( 0x1000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "5200.5e",      0x0000, 0x1000, CRC(e869219c) SHA1(425614cd0642743a82ef9c1aada29774a92203ea) )

	ROM_REGION( 0x0100, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "prom.2d",      0x0000, 0x0100, CRC(9b69b543) SHA1(47af3f67e50794e839b74fe61197af2228084efd) )	/* dots */

	ROM_REGION( 0x0260, REGION_PROMS, 0 )
	ROM_LOAD( "bosco.6b",     0x0000, 0x0020, CRC(d2b96fb0) SHA1(54c100ec9d173d7dd48a453ebed5f625053cb6e0) )	/* palette */
	ROM_LOAD( "bosco.4m",     0x0020, 0x0100, CRC(4e15d59c) SHA1(3542ead6421d169c3569e121ec2be304e108787c) )	/* lookup table */
	ROM_LOAD( "prom.2r",      0x0120, 0x0020, CRC(b88d5ba9) SHA1(7b97a38a540b7ca4b7d9ae338ec38b9b1a337846) )	/* video layout (not used) */
	ROM_LOAD( "prom.7h",      0x0140, 0x0020, CRC(87d61353) SHA1(c7493e52662c921625676a4a4e8cf4371bd938b7) )	/* video timing (not used) */

	ROM_REGION( 0x0200, REGION_SOUND1, 0 )
	ROM_LOAD( "prom.1d",      0x0000, 0x0100, CRC(de2316c6) SHA1(0e55c56046331888d1d3f0d9823d2ceb203e7d3f) )
	ROM_LOAD( "prom.5c",      0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )	/* timing - not used */

	ROM_REGION( 0x3000, REGION_SOUND2, 0 )	/* ROMs for digitised speech */
	ROM_LOAD( "4900.5n",      0x0000, 0x1000, CRC(09acc978) SHA1(2b264aaeb6eba70ad91593413dca733990e5467b) )
	ROM_LOAD( "5000.5m",      0x1000, 0x1000, CRC(e571e959) SHA1(9c81d7bec73bc605f7dd9a089171b0f34c4bb09a) )
	ROM_LOAD( "5100.5l",      0x2000, 0x1000, CRC(17ac9511) SHA1(266f3fae90d2fe38d109096d352863a52b379899) )
ROM_END

ROM_START( boscomdo )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )	/* 64k for code for the first CPU  */
	ROM_LOAD( "2300.3n",      0x0000, 0x1000, CRC(db6128b0) SHA1(ddd285f7e00d5e58ab9b15838528e0020d47fcd2) )
	ROM_LOAD( "2400.3m",      0x1000, 0x1000, CRC(86907614) SHA1(3295ab6c5171a069875c2239b3325296c1df6031) )
	ROM_LOAD( "2500.3l",      0x2000, 0x1000, CRC(a21fae11) SHA1(dff38d90ee30558274d2d399edc3281c2ef5cb69) )
	ROM_LOAD( "2600.3k",      0x3000, 0x1000, CRC(11d6ae23) SHA1(f2f72f5c777b684f7ffd53b9c034560211113499) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* 64k for the second CPU */
	ROM_LOAD( "2700.3j",      0x0000, 0x1000, CRC(7254e65e) SHA1(c2ee29fcb5173e8d46a80a8a1b931a53dbdeae66) )
	ROM_LOAD( "2800.3h",      0x1000, 0x1000, CRC(31b8c648) SHA1(de0db24d385d2361ec989bf32388df8202ad535c) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )	/* 64k for the third CPU  */
	ROM_LOAD( "2900.3e",      0x0000, 0x1000, CRC(d45a4911) SHA1(547236adca9174f5cc0ec05b9649618bb92ba630) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "5300.5d",      0x0000, 0x1000, CRC(a956d3c5) SHA1(c5a9d7b1f9b4acda8fb9762414e085cb5fb80c9e) )

	ROM_REGION( 0x1000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "5200.5e",      0x0000, 0x1000, CRC(e869219c) SHA1(425614cd0642743a82ef9c1aada29774a92203ea) )

	ROM_REGION( 0x0100, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "prom.2d",      0x0000, 0x0100, CRC(9b69b543) SHA1(47af3f67e50794e839b74fe61197af2228084efd) )	/* dots */

	ROM_REGION( 0x0260, REGION_PROMS, 0 )
	ROM_LOAD( "bosco.6b",     0x0000, 0x0020, CRC(d2b96fb0) SHA1(54c100ec9d173d7dd48a453ebed5f625053cb6e0) )	/* palette */
	ROM_LOAD( "bosco.4m",     0x0020, 0x0100, CRC(4e15d59c) SHA1(3542ead6421d169c3569e121ec2be304e108787c) )	/* lookup table */
	ROM_LOAD( "prom.2r",      0x0120, 0x0020, CRC(b88d5ba9) SHA1(7b97a38a540b7ca4b7d9ae338ec38b9b1a337846) )	/* video layout (not used) */
	ROM_LOAD( "prom.7h",      0x0140, 0x0020, CRC(87d61353) SHA1(c7493e52662c921625676a4a4e8cf4371bd938b7) )	/* video timing (not used) */

	ROM_REGION( 0x0200, REGION_SOUND1, 0 )
	ROM_LOAD( "prom.1d",      0x0000, 0x0100, CRC(de2316c6) SHA1(0e55c56046331888d1d3f0d9823d2ceb203e7d3f) )
	ROM_LOAD( "prom.5c",      0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )	/* timing - not used */

	ROM_REGION( 0x3000, REGION_SOUND2, 0 )	/* ROMs for digitised speech */
	ROM_LOAD( "4900.5n",      0x0000, 0x1000, CRC(09acc978) SHA1(2b264aaeb6eba70ad91593413dca733990e5467b) )
	ROM_LOAD( "5000.5m",      0x1000, 0x1000, CRC(e571e959) SHA1(9c81d7bec73bc605f7dd9a089171b0f34c4bb09a) )
	ROM_LOAD( "5100.5l",      0x2000, 0x1000, CRC(17ac9511) SHA1(266f3fae90d2fe38d109096d352863a52b379899) )
ROM_END



ROM_START( galaga )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )     /* 64k for code for the first CPU  */
	ROM_LOAD( "gg1-1b.3p",    0x0000, 0x1000, CRC(ab036c9f) SHA1(ca7f5da42d4e76fd89bb0b35198a23c01462fbfe) )
	ROM_LOAD( "gg1-2b.3m",    0x1000, 0x1000, CRC(d9232240) SHA1(ab202aa259c3d332ef13dfb8fc8580ce2a5a253d) )
	ROM_LOAD( "gg1-3.2m",     0x2000, 0x1000, CRC(753ce503) SHA1(481f443aea3ed3504ec2f3a6bfcf3cd47e2f8f81) )
	ROM_LOAD( "gg1-4b.2l",    0x3000, 0x1000, CRC(499fcc76) SHA1(ddb8b121903646c320939c7d13f4aa4ebb130378) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )     /* 64k for the second CPU */
	ROM_LOAD( "gg1-5b.3f",    0x0000, 0x1000, CRC(bb5caae3) SHA1(e957a581463caac27bc37ca2e2a90f27e4f62b6f) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )     /* 64k for the third CPU  */
	ROM_LOAD( "gg1-7b.2c",    0x0000, 0x1000, CRC(d016686b) SHA1(44c1a04fba3c7c826ff484185cb881b4b22e6657) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "gg1-9.4l",     0x0000, 0x1000, CRC(58b2f47c) SHA1(62f1279a784ab2f8218c4137c7accda00e6a3490) )

	ROM_REGION( 0x2000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "gg1-11.4d",    0x0000, 0x1000, CRC(ad447c80) SHA1(e697c180178cabd1d32483c5d8889a40633f7857) )
	ROM_LOAD( "gg1-10.4f",    0x1000, 0x1000, CRC(dd6f1afc) SHA1(c340ed8c25e0979629a9a1730edc762bd72d0cff) )

	ROM_REGION( 0x0220, REGION_PROMS, 0 )
	ROM_LOAD( "prom-5.5n",    0x0000, 0x0020, CRC(54603c6b) SHA1(1a6dea13b4af155d9cb5b999a75d4f1eb9c71346) )	/* palette */
	ROM_LOAD( "prom-4.2n",    0x0020, 0x0100, CRC(59b6edab) SHA1(0281de86c236c88739297ff712e0a4f5c8bf8ab9) )	/* char lookup table */
	ROM_LOAD( "prom-3.1c",    0x0120, 0x0100, CRC(4a04bb6b) SHA1(cdd4bc1013f5c11984fdc4fd10e2d2e27120c1e5) )	/* sprite lookup table */

	ROM_REGION( 0x0200, REGION_SOUND1, 0 )
	ROM_LOAD( "prom-1.1d",    0x0000, 0x0100, CRC(7a2815b4) SHA1(085ada18c498fdb18ecedef0ea8fe9217edb7b46) )
	ROM_LOAD( "prom-2.5c",    0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )	/* timing - not used */
ROM_END


ROM_START( galagamf )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )     /* 64k for code for the first CPU  */
	ROM_LOAD( "3200a.bin",    0x0000, 0x1000, CRC(3ef0b053) SHA1(0c04a362b737998c0952a753fb3fd8c8a17e9b46) )
	ROM_LOAD( "3300b.bin",    0x1000, 0x1000, CRC(1b280831) SHA1(f7ea12e61929717ebe43a4198a97f109845a2c62) )
	ROM_LOAD( "3400c.bin",    0x2000, 0x1000, CRC(16233d33) SHA1(a7eb799be5e23058754a92b15e6527bfbb47a354) )
	ROM_LOAD( "3500d.bin",    0x3000, 0x1000, CRC(0aaf5c23) SHA1(3f4b0bb960bf002261e9c1278c88f594c6aa8ab6) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )     /* 64k for the second CPU */
	ROM_LOAD( "3600fast.bin", 0x0000, 0x1000, CRC(23d586e5) SHA1(43346c69385e9091e64cff6c027ac2689cafcbb9) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )     /* 64k for the third CPU  */
	ROM_LOAD( "3700g.bin",    0x0000, 0x1000, CRC(b07f0aa4) SHA1(7528644a8480d0be2d0d37069515ed319e94778f) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "gg1-9.4l",     0x0000, 0x1000, CRC(58b2f47c) SHA1(62f1279a784ab2f8218c4137c7accda00e6a3490) )

	ROM_REGION( 0x2000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "gg1-11.4d",    0x0000, 0x1000, CRC(ad447c80) SHA1(e697c180178cabd1d32483c5d8889a40633f7857) )
	ROM_LOAD( "gg1-10.4f",    0x1000, 0x1000, CRC(dd6f1afc) SHA1(c340ed8c25e0979629a9a1730edc762bd72d0cff) )

	ROM_REGION( 0x0220, REGION_PROMS, 0 )
	ROM_LOAD( "prom-5.5n",    0x0000, 0x0020, CRC(54603c6b) SHA1(1a6dea13b4af155d9cb5b999a75d4f1eb9c71346) )	/* palette */
	ROM_LOAD( "prom-4.2n",    0x0020, 0x0100, CRC(59b6edab) SHA1(0281de86c236c88739297ff712e0a4f5c8bf8ab9) )	/* char lookup table */
	ROM_LOAD( "prom-3.1c",    0x0120, 0x0100, CRC(4a04bb6b) SHA1(cdd4bc1013f5c11984fdc4fd10e2d2e27120c1e5) )	/* sprite lookup table */

	ROM_REGION( 0x0200, REGION_SOUND1, 0 )
	ROM_LOAD( "prom-1.1d",    0x0000, 0x0100, CRC(7a2815b4) SHA1(085ada18c498fdb18ecedef0ea8fe9217edb7b46) )
	ROM_LOAD( "prom-2.5c",    0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )	/* timing - not used */
ROM_END


ROM_START( galagao )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )     /* 64k for code for the first CPU  */
	ROM_LOAD( "gg1-1",        0x0000, 0x1000, CRC(a3a0f743) SHA1(6907773db7c002ecde5e41853603d53387c5c7cd) )
	ROM_LOAD( "gg1-2",        0x1000, 0x1000, CRC(43bb0d5c) SHA1(666975aed5ce84f09794c54b550d64d95ab311f0) )
	ROM_LOAD( "gg1-3.2m",     0x2000, 0x1000, CRC(753ce503) SHA1(481f443aea3ed3504ec2f3a6bfcf3cd47e2f8f81) )
	ROM_LOAD( "gg1-4",        0x3000, 0x1000, CRC(83874442) SHA1(366cb0dbd31b787e64f88d182108b670d03b393e) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )     /* 64k for the second CPU */
	ROM_LOAD( "gg1-5",        0x0000, 0x1000, CRC(3102fccd) SHA1(d29b68d6aab3217fa2106b3507b9273ff3f927bf) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )     /* 64k for the third CPU  */
	ROM_LOAD( "gg1-7",        0x0000, 0x1000, CRC(8995088d) SHA1(d6cb439de0718826d1a0363c9d77de8740b18ecf) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "gg1-9.4l",     0x0000, 0x1000, CRC(58b2f47c) SHA1(62f1279a784ab2f8218c4137c7accda00e6a3490) )

	ROM_REGION( 0x2000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "gg1-11.4d",    0x0000, 0x1000, CRC(ad447c80) SHA1(e697c180178cabd1d32483c5d8889a40633f7857) )
	ROM_LOAD( "gg1-10.4f",    0x1000, 0x1000, CRC(dd6f1afc) SHA1(c340ed8c25e0979629a9a1730edc762bd72d0cff) )

	ROM_REGION( 0x0220, REGION_PROMS, 0 )
	ROM_LOAD( "prom-5.5n",    0x0000, 0x0020, CRC(54603c6b) SHA1(1a6dea13b4af155d9cb5b999a75d4f1eb9c71346) )	/* palette */
	ROM_LOAD( "prom-4.2n",    0x0020, 0x0100, CRC(59b6edab) SHA1(0281de86c236c88739297ff712e0a4f5c8bf8ab9) )	/* char lookup table */
	ROM_LOAD( "prom-3.1c",    0x0120, 0x0100, CRC(4a04bb6b) SHA1(cdd4bc1013f5c11984fdc4fd10e2d2e27120c1e5) )	/* sprite lookup table */

	ROM_REGION( 0x0200, REGION_SOUND1, 0 )
	ROM_LOAD( "prom-1.1d",    0x0000, 0x0100, CRC(7a2815b4) SHA1(085ada18c498fdb18ecedef0ea8fe9217edb7b46) )
	ROM_LOAD( "prom-2.5c",    0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )	/* timing - not used */
ROM_END

ROM_START( galagamw )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )     /* 64k for code for the first CPU  */
	ROM_LOAD( "3200a.bin",    0x0000, 0x1000, CRC(3ef0b053) SHA1(0c04a362b737998c0952a753fb3fd8c8a17e9b46) )
	ROM_LOAD( "3300b.bin",    0x1000, 0x1000, CRC(1b280831) SHA1(f7ea12e61929717ebe43a4198a97f109845a2c62) )
	ROM_LOAD( "3400c.bin",    0x2000, 0x1000, CRC(16233d33) SHA1(a7eb799be5e23058754a92b15e6527bfbb47a354) )
	ROM_LOAD( "3500d.bin",    0x3000, 0x1000, CRC(0aaf5c23) SHA1(3f4b0bb960bf002261e9c1278c88f594c6aa8ab6) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )     /* 64k for the second CPU */
	ROM_LOAD( "3600e.bin",    0x0000, 0x1000, CRC(bc556e76) SHA1(0d3d68243c4571d985b4d8f7e0ea9f6fcffa2116) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )     /* 64k for the third CPU  */
	ROM_LOAD( "3700g.bin",    0x0000, 0x1000, CRC(b07f0aa4) SHA1(7528644a8480d0be2d0d37069515ed319e94778f) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "gg1-9.4l",     0x0000, 0x1000, CRC(58b2f47c) SHA1(62f1279a784ab2f8218c4137c7accda00e6a3490) )

	ROM_REGION( 0x2000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "gg1-11.4d",    0x0000, 0x1000, CRC(ad447c80) SHA1(e697c180178cabd1d32483c5d8889a40633f7857) )
	ROM_LOAD( "gg1-10.4f",    0x1000, 0x1000, CRC(dd6f1afc) SHA1(c340ed8c25e0979629a9a1730edc762bd72d0cff) )

	ROM_REGION( 0x0220, REGION_PROMS, 0 )
	ROM_LOAD( "prom-5.5n",    0x0000, 0x0020, CRC(54603c6b) SHA1(1a6dea13b4af155d9cb5b999a75d4f1eb9c71346) )	/* palette */
	ROM_LOAD( "prom-4.2n",    0x0020, 0x0100, CRC(59b6edab) SHA1(0281de86c236c88739297ff712e0a4f5c8bf8ab9) )	/* char lookup table */
	ROM_LOAD( "prom-3.1c",    0x0120, 0x0100, CRC(4a04bb6b) SHA1(cdd4bc1013f5c11984fdc4fd10e2d2e27120c1e5) )	/* sprite lookup table */

	ROM_REGION( 0x0200, REGION_SOUND1, 0 )
	ROM_LOAD( "prom-1.1d",    0x0000, 0x0100, CRC(7a2815b4) SHA1(085ada18c498fdb18ecedef0ea8fe9217edb7b46) )
	ROM_LOAD( "prom-2.5c",    0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )	/* timing - not used */
ROM_END

ROM_START( galagamk )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )     /* 64k for code for the first CPU  */
	ROM_LOAD( "mk2-1",        0x0000, 0x1000, CRC(23cea1e2) SHA1(18db33ade0ca6e47cc48aa151d2ccbb4646e3ae3) )
	ROM_LOAD( "mk2-2",        0x1000, 0x1000, CRC(89695b1a) SHA1(fda5557018884e903f855bf3b69a25d75ed8a767) )
	ROM_LOAD( "3400c.bin",    0x2000, 0x1000, CRC(16233d33) SHA1(a7eb799be5e23058754a92b15e6527bfbb47a354) )
	ROM_LOAD( "mk2-4",        0x3000, 0x1000, CRC(24b767f5) SHA1(d4c03e2ed582cfa7f8168ac352f790ef7af54cb8) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )     /* 64k for the second CPU */
	ROM_LOAD( "gg1-5",        0x0000, 0x1000, CRC(3102fccd) SHA1(d29b68d6aab3217fa2106b3507b9273ff3f927bf) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )     /* 64k for the third CPU  */
	ROM_LOAD( "gg1-7b.2c",    0x0000, 0x1000, CRC(d016686b) SHA1(44c1a04fba3c7c826ff484185cb881b4b22e6657) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "gg1-9.4l",     0x0000, 0x1000, CRC(58b2f47c) SHA1(62f1279a784ab2f8218c4137c7accda00e6a3490) )

	ROM_REGION( 0x2000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "gg1-11.4d",    0x0000, 0x1000, CRC(ad447c80) SHA1(e697c180178cabd1d32483c5d8889a40633f7857) )
	ROM_LOAD( "gg1-10.4f",    0x1000, 0x1000, CRC(dd6f1afc) SHA1(c340ed8c25e0979629a9a1730edc762bd72d0cff) )

	ROM_REGION( 0x0220, REGION_PROMS, 0 )
	ROM_LOAD( "prom-5.5n",    0x0000, 0x0020, CRC(54603c6b) SHA1(1a6dea13b4af155d9cb5b999a75d4f1eb9c71346) )	/* palette */
	ROM_LOAD( "prom-4.2n",    0x0020, 0x0100, CRC(59b6edab) SHA1(0281de86c236c88739297ff712e0a4f5c8bf8ab9) )	/* char lookup table */
	ROM_LOAD( "prom-3.1c",    0x0120, 0x0100, CRC(4a04bb6b) SHA1(cdd4bc1013f5c11984fdc4fd10e2d2e27120c1e5) )	/* sprite lookup table */

	ROM_REGION( 0x0200, REGION_SOUND1, 0 )
	ROM_LOAD( "prom-1.1d",    0x0000, 0x0100, CRC(7a2815b4) SHA1(085ada18c498fdb18ecedef0ea8fe9217edb7b46) )
	ROM_LOAD( "prom-2.5c",    0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )	/* timing - not used */
ROM_END

ROM_START( gallag )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )     /* 64k for code for the first CPU  */
	ROM_LOAD( "gg1-1",        0x0000, 0x1000, CRC(a3a0f743) SHA1(6907773db7c002ecde5e41853603d53387c5c7cd) )
	ROM_LOAD( "gallag.2",     0x1000, 0x1000, CRC(5eda60a7) SHA1(853d7b974dd04abd7af3a8ba2681dfabce4dce18) )
	ROM_LOAD( "gg1-3.2m",     0x2000, 0x1000, CRC(753ce503) SHA1(481f443aea3ed3504ec2f3a6bfcf3cd47e2f8f81) )
	ROM_LOAD( "gg1-4",        0x3000, 0x1000, CRC(83874442) SHA1(366cb0dbd31b787e64f88d182108b670d03b393e) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )     /* 64k for the second CPU */
	ROM_LOAD( "gg1-5",        0x0000, 0x1000, CRC(3102fccd) SHA1(d29b68d6aab3217fa2106b3507b9273ff3f927bf) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )     /* 64k for the third CPU  */
	ROM_LOAD( "gg1-7",        0x0000, 0x1000, CRC(8995088d) SHA1(d6cb439de0718826d1a0363c9d77de8740b18ecf) )

	ROM_REGION( 0x10000, REGION_CPU4, 0 )	/* 64k for a Z80 which emulates the custom I/O chip (not used) */
	ROM_LOAD( "gallag.6",     0x0000, 0x1000, CRC(001b70bc) SHA1(b465eee91e75257b7b049d49c0064ab5fd66c576) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "gallag.8",     0x0000, 0x1000, CRC(169a98a4) SHA1(edbeb11076061e744ea88d9899dbdfe0964c7e78) )

	ROM_REGION( 0x2000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "gg1-11.4d",    0x0000, 0x1000, CRC(ad447c80) SHA1(e697c180178cabd1d32483c5d8889a40633f7857) )
	ROM_LOAD( "gg1-10.4f",    0x1000, 0x1000, CRC(dd6f1afc) SHA1(c340ed8c25e0979629a9a1730edc762bd72d0cff) )

	ROM_REGION( 0x0220, REGION_PROMS, 0 )
	ROM_LOAD( "prom-5.5n",    0x0000, 0x0020, CRC(54603c6b) SHA1(1a6dea13b4af155d9cb5b999a75d4f1eb9c71346) )	/* palette */
	ROM_LOAD( "prom-4.2n",    0x0020, 0x0100, CRC(59b6edab) SHA1(0281de86c236c88739297ff712e0a4f5c8bf8ab9) )	/* char lookup table */
	ROM_LOAD( "prom-3.1c",    0x0120, 0x0100, CRC(4a04bb6b) SHA1(cdd4bc1013f5c11984fdc4fd10e2d2e27120c1e5) )	/* sprite lookup table */

	ROM_REGION( 0x0200, REGION_SOUND1, 0 )
	ROM_LOAD( "prom-1.1d",    0x0000, 0x0100, CRC(7a2815b4) SHA1(085ada18c498fdb18ecedef0ea8fe9217edb7b46) )
	ROM_LOAD( "prom-2.5c",    0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )	/* timing - not used */
ROM_END

ROM_START( gatsbee )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )     /* 64k for code for the first CPU  */
	ROM_LOAD( "1.4b",	      0x0000, 0x1000, CRC(9fb8e28b) SHA1(7171e3fb37b0d6cc8f7a023c1775080d5986de99) )
	ROM_LOAD( "2.4c",	      0x1000, 0x1000, CRC(bf6cb840) SHA1(5763140d32d35a38cdcb49e6de1fd5b07a9e8cc2) )
	ROM_LOAD( "3.4d",	      0x2000, 0x1000, CRC(3604e2dd) SHA1(1736cf8497f7ac28e92ca94fa137c144353dc192) )
	ROM_LOAD( "4.4e",	      0x3000, 0x1000, CRC(bf9f613b) SHA1(41c852fc77f0f35bf48a5b81a19234ed99871c89) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )     /* 64k for the second CPU */
	ROM_LOAD( "gg1-5",        0x0000, 0x1000, CRC(3102fccd) SHA1(d29b68d6aab3217fa2106b3507b9273ff3f927bf) )	/* 5.4j */

	ROM_REGION( 0x10000, REGION_CPU3, 0 )     /* 64k for the third CPU  */
	ROM_LOAD( "gg1-7",        0x0000, 0x1000, CRC(8995088d) SHA1(d6cb439de0718826d1a0363c9d77de8740b18ecf) )	/* 7.4k */

	ROM_REGION( 0x10000, REGION_CPU4, 0 )	/* 64k for a Z80 which emulates the custom I/O chip (not used) */
	ROM_LOAD( "gallag.6",     0x0000, 0x1000, CRC(001b70bc) SHA1(b465eee91e75257b7b049d49c0064ab5fd66c576) )

	ROM_REGION( 0x2000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "8.5r",  0x0000, 0x2000, CRC(b324f650) SHA1(7bcb254f7cf03bd84291b9fdc27b8962b3e12aa4) )

	ROM_REGION( 0x2000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "9.6a",         0x0000, 0x1000, CRC(22e339d5) SHA1(9ac2887ede802d28daa4ad0a0a54bcf7b1155a2e) )
	ROM_LOAD( "10.7a",        0x1000, 0x1000, CRC(60dcf940) SHA1(6530aa5b4afef4a8422ece76a93d0c5b1d93355e) )

	ROM_REGION( 0x0220, REGION_PROMS, 0 )
	ROM_LOAD( "prom-5.5n",    0x0000, 0x0020, CRC(54603c6b) SHA1(1a6dea13b4af155d9cb5b999a75d4f1eb9c71346) )	/* palette */
	ROM_LOAD( "prom-4.2n",    0x0020, 0x0100, CRC(59b6edab) SHA1(0281de86c236c88739297ff712e0a4f5c8bf8ab9) )	/* char lookup table */
	ROM_LOAD( "prom-3.1c",    0x0120, 0x0100, CRC(4a04bb6b) SHA1(cdd4bc1013f5c11984fdc4fd10e2d2e27120c1e5) )	/* sprite lookup table */

	ROM_REGION( 0x0200, REGION_SOUND1, 0 )
	ROM_LOAD( "prom-1.1d",    0x0000, 0x0100, CRC(7a2815b4) SHA1(085ada18c498fdb18ecedef0ea8fe9217edb7b46) )
	ROM_LOAD( "prom-2.5c",    0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )	/* timing - not used */
ROM_END


ROM_START( nebulbee )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )     /* 64k for code for the first CPU  */
	ROM_LOAD( "nebulbee.01",  0x0000, 0x1000, CRC(f405f2c4) SHA1(9249afeffd8df0f24539ea9b4f88c23a6ad58d8c) )
	ROM_LOAD( "nebulbee.02",  0x1000, 0x1000, CRC(31022b60) SHA1(90e64afb4128c6dfeeee89635ea9f97a34f70f5f) )
	ROM_LOAD( "04j_g03.bin",  0x2000, 0x1000, CRC(753ce503) SHA1(481f443aea3ed3504ec2f3a6bfcf3cd47e2f8f81) )
	ROM_LOAD( "nebulbee.04",  0x3000, 0x1000, CRC(d76788a5) SHA1(adcb83cf64951d86c701a99b410e9230912f8a48) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )     /* 64k for the second CPU */
	ROM_LOAD( "04e_g05.bin",  0x0000, 0x1000, CRC(3102fccd) SHA1(d29b68d6aab3217fa2106b3507b9273ff3f927bf) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )     /* 64k for the third CPU  */
	ROM_LOAD( "04d_g06.bin",  0x0000, 0x1000, CRC(8995088d) SHA1(d6cb439de0718826d1a0363c9d77de8740b18ecf) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "07m_g08.bin",  0x0000, 0x1000, CRC(58b2f47c) SHA1(62f1279a784ab2f8218c4137c7accda00e6a3490) )

	ROM_REGION( 0x2000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "07e_g10.bin",  0x0000, 0x1000, CRC(ad447c80) SHA1(e697c180178cabd1d32483c5d8889a40633f7857) )
	ROM_LOAD( "07h_g09.bin",  0x1000, 0x1000, CRC(dd6f1afc) SHA1(c340ed8c25e0979629a9a1730edc762bd72d0cff) )

	ROM_REGION( 0x0320, REGION_PROMS, 0 )
	ROM_LOAD( "5n.bin",       0x0000, 0x0020, CRC(54603c6b) SHA1(1a6dea13b4af155d9cb5b999a75d4f1eb9c71346) )	/* palette */
	ROM_LOAD( "2n.bin",       0x0020, 0x0100, CRC(a547d33b) SHA1(7323084320bb61ae1530d916f5edd8835d4d2461) )	/* char lookup table */
	ROM_LOAD( "1c.bin",       0x0120, 0x0100, CRC(b6f585fb) SHA1(dd10147c4f05fede7ae6e7a760681700a660e87e) )	/* sprite lookup table */
	ROM_LOAD( "5c.bin",       0x0220, 0x0100, CRC(8bd565f6) SHA1(bedba65816abfc2ebeacac6ee335ca6f136e3e3d) )	/* unknown */

	ROM_REGION( 0x0100, REGION_SOUND1, 0 )	/* sound prom */
	ROM_LOAD( "1d.bin",       0x0000, 0x0100, CRC(86d92b24) SHA1(6bef9102b97c83025a2cf84e89d95f2d44c3d2ed) )
ROM_END


ROM_START( xevious )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )	/* 64k for the first CPU */
	ROM_LOAD( "xvi_1.3p",     0x0000, 0x1000, CRC(09964dda) SHA1(4882b25b0938a903f3a367455ba788a30759b5b0) )
	ROM_LOAD( "xvi_2.3m",     0x1000, 0x1000, CRC(60ecce84) SHA1(8adc60a5fcbca74092518dbc570ffff0f04c5b17) )
	ROM_LOAD( "xvi_3.2m",     0x2000, 0x1000, CRC(79754b7d) SHA1(c6a154858716e1f073b476824b183de20e06d093) )
	ROM_LOAD( "xvi_4.2l",     0x3000, 0x1000, CRC(c7d4bbf0) SHA1(4b846de204d08651253d3a141677c8a31626af07) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* 64k for the second CPU */
	ROM_LOAD( "xvi_5.3f",     0x0000, 0x1000, CRC(c85b703f) SHA1(15f1c005b9d806a384ab1f2240b9c580bfe83893) )
	ROM_LOAD( "xvi_6.3j",     0x1000, 0x1000, CRC(e18cdaad) SHA1(6b79efee1a9642edb9f752101737132401248aed) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )	/* 64k for the audio CPU */
	ROM_LOAD( "xvi_7.2c",     0x0000, 0x1000, CRC(dd35cf1c) SHA1(f8d1f8e019d8198308443c2e7e815d0d04b23d14) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "xvi_12.3b",    0x0000, 0x1000, CRC(088c8b26) SHA1(9c3b61dfca2f84673a78f7f66e363777a8f47a59) )	/* foreground characters */

	ROM_REGION( 0x2000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "xvi_13.3c",    0x0000, 0x1000, CRC(de60ba25) SHA1(32bc09be5ff8b52ee3a26e0ac3ebc2d4107badb7) )	/* bg pattern B0 */
	ROM_LOAD( "xvi_14.3d",    0x1000, 0x1000, CRC(535cdbbc) SHA1(fb9ffe5fc43e0213231267e98d605d43c15f61e8) )	/* bg pattern B1 */

	ROM_REGION( 0xa000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "xvi_15.4m",    0x0000, 0x2000, CRC(dc2c0ecb) SHA1(19ddbd9805f77f38c9a9a1bb30dba6c720b8609f) )	/* sprite set #1, planes 0/1 */
	ROM_LOAD( "xvi_17.4p",    0x2000, 0x2000, CRC(dfb587ce) SHA1(acff2bf5cde85a16cdc98a52cdea11f77fadf25a) )	/* sprite set #2, planes 0/1 */
	ROM_LOAD( "xvi_16.4n",    0x4000, 0x1000, CRC(605ca889) SHA1(3bf380ef76c03822a042ecc73b5edd4543c268ce) )	/* sprite set #3, planes 0/1 */
	ROM_LOAD( "xvi_18.4r",    0x5000, 0x2000, CRC(02417d19) SHA1(b5f830dd2cf25cf154308d2e640f0ecdcda5d8cd) )	/* sprite set #1, plane 2, set #2, plane 2 */
	/* 0x7000-0x8fff  will be unpacked from 0x5000-0x6fff */
	ROM_FILL(                 0x9000, 0x1000, 0x00 )	/* empty space to decode sprite set #3 as 3 bits per pixel */

	ROM_REGION( 0x4000, REGION_GFX4, 0 )	/* background tilemaps */
	ROM_LOAD( "xvi_9.2a",     0x0000, 0x1000, CRC(57ed9879) SHA1(3106d1aacff06cf78371bd19967141072b32b7d7) )
	ROM_LOAD( "xvi_10.2b",    0x1000, 0x2000, CRC(ae3ba9e5) SHA1(49064b25667ffcd81137cd5e800df4b78b182a46) )
	ROM_LOAD( "xvi_11.2c",    0x3000, 0x1000, CRC(31e244dd) SHA1(3f7eac12863697a98e1122111801606759e44b2a) )

	ROM_REGION( 0x0b00, REGION_PROMS, 0 )
	ROM_LOAD( "xvi_8bpr.6a",  0x0000, 0x0100, CRC(5cc2727f) SHA1(0dc1e63a47a4cb0ba75f6f1e0c15e408bb0ee2a1) ) /* palette red component */
	ROM_LOAD( "xvi_9bpr.6d",  0x0100, 0x0100, CRC(5c8796cc) SHA1(63015e3c0874afc6b1ca032f1ffb8f90562c77c8) ) /* palette green component */
	ROM_LOAD( "xvi10bpr.6e",  0x0200, 0x0100, CRC(3cb60975) SHA1(c94d5a5dd4d8a08d6d39c051a4a722581b903f45) ) /* palette blue component */
	ROM_LOAD( "xvi_7bpr.4h",  0x0300, 0x0200, CRC(22d98032) SHA1(ec6626828c79350417d08b98e9631ad35edd4a41) ) /* bg tiles lookup table low bits */
	ROM_LOAD( "xvi_6bpr.4f",  0x0500, 0x0200, CRC(3a7599f0) SHA1(a4bdf58c190ca16fc7b976c97f41087a61fdb8b8) ) /* bg tiles lookup table high bits */
	ROM_LOAD( "xvi_4bpr.3l",  0x0700, 0x0200, CRC(fd8b9d91) SHA1(87ddf0b9d723aabb422d6d416aa9ec6bc246bf34) ) /* sprite lookup table low bits */
	ROM_LOAD( "xvi_5bpr.3m",  0x0900, 0x0200, CRC(bf906d82) SHA1(776168a73d3b9f0ce05610acc8a623deae0a572b) ) /* sprite lookup table high bits */

	ROM_REGION( 0x0200, REGION_SOUND1, 0 )	/* sound PROMs */
	ROM_LOAD( "xvi_2bpr.7n",  0x0000, 0x0100, CRC(550f06bc) SHA1(816a0fafa0b084ac11ae1af70a5186539376fc2a) )
	ROM_LOAD( "xvi_1bpr.5n",  0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )	/* timing - not used */
ROM_END

ROM_START( xeviousa )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )	/* 64k for the first CPU */
	ROM_LOAD( "xea-1m-a.bin", 0x0000, 0x2000, CRC(8c2b50ec) SHA1(f770873b711d838556dde67a8aac8a7f572fcc5b) )
	ROM_LOAD( "xea-1l-a.bin", 0x2000, 0x2000, CRC(0821642b) SHA1(c6c322c61d0985a2ac59f5e92d4e351107afb9eb) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* 64k for the second CPU */
	ROM_LOAD( "xea-4c-a.bin", 0x0000, 0x2000, CRC(14d8fa03) SHA1(e8114141394adda86184b146f2497cfeef7fc2eb) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )	/* 64k for the audio CPU */
	ROM_LOAD( "xvi_7.2c",     0x0000, 0x1000, CRC(dd35cf1c) SHA1(f8d1f8e019d8198308443c2e7e815d0d04b23d14) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "xvi_12.3b",    0x0000, 0x1000, CRC(088c8b26) SHA1(9c3b61dfca2f84673a78f7f66e363777a8f47a59) )	/* foreground characters */

	ROM_REGION( 0x2000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "xvi_13.3c",    0x0000, 0x1000, CRC(de60ba25) SHA1(32bc09be5ff8b52ee3a26e0ac3ebc2d4107badb7) )	/* bg pattern B0 */
	ROM_LOAD( "xvi_14.3d",    0x1000, 0x1000, CRC(535cdbbc) SHA1(fb9ffe5fc43e0213231267e98d605d43c15f61e8) )	/* bg pattern B1 */

	ROM_REGION( 0xa000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "xvi_15.4m",    0x0000, 0x2000, CRC(dc2c0ecb) SHA1(19ddbd9805f77f38c9a9a1bb30dba6c720b8609f) )	/* sprite set #1, planes 0/1 */
	ROM_LOAD( "xvi_17.4p",    0x2000, 0x2000, CRC(dfb587ce) SHA1(acff2bf5cde85a16cdc98a52cdea11f77fadf25a) )	/* sprite set #2, planes 0/1 */
	ROM_LOAD( "xvi_16.4n",    0x4000, 0x1000, CRC(605ca889) SHA1(3bf380ef76c03822a042ecc73b5edd4543c268ce) )	/* sprite set #3, planes 0/1 */
	ROM_LOAD( "xvi_18.4r",    0x5000, 0x2000, CRC(02417d19) SHA1(b5f830dd2cf25cf154308d2e640f0ecdcda5d8cd) )	/* sprite set #1, plane 2, set #2, plane 2 */
	/* 0x7000-0x8fff  will be unpacked from 0x5000-0x6fff */
	ROM_FILL(                 0x9000, 0x1000, 0x00 )	/* empty space to decode sprite set #3 as 3 bits per pixel */

	ROM_REGION( 0x4000, REGION_GFX4, 0 )	/* background tilemaps */
	ROM_LOAD( "xvi_9.2a",     0x0000, 0x1000, CRC(57ed9879) SHA1(3106d1aacff06cf78371bd19967141072b32b7d7) )
	ROM_LOAD( "xvi_10.2b",    0x1000, 0x2000, CRC(ae3ba9e5) SHA1(49064b25667ffcd81137cd5e800df4b78b182a46) )
	ROM_LOAD( "xvi_11.2c",    0x3000, 0x1000, CRC(31e244dd) SHA1(3f7eac12863697a98e1122111801606759e44b2a) )

	ROM_REGION( 0x0b00, REGION_PROMS, 0 )
	ROM_LOAD( "xvi_8bpr.6a",  0x0000, 0x0100, CRC(5cc2727f) SHA1(0dc1e63a47a4cb0ba75f6f1e0c15e408bb0ee2a1) ) /* palette red component */
	ROM_LOAD( "xvi_9bpr.6d",  0x0100, 0x0100, CRC(5c8796cc) SHA1(63015e3c0874afc6b1ca032f1ffb8f90562c77c8) ) /* palette green component */
	ROM_LOAD( "xvi10bpr.6e",  0x0200, 0x0100, CRC(3cb60975) SHA1(c94d5a5dd4d8a08d6d39c051a4a722581b903f45) ) /* palette blue component */
	ROM_LOAD( "xvi_7bpr.4h",  0x0300, 0x0200, CRC(22d98032) SHA1(ec6626828c79350417d08b98e9631ad35edd4a41) ) /* bg tiles lookup table low bits */
	ROM_LOAD( "xvi_6bpr.4f",  0x0500, 0x0200, CRC(3a7599f0) SHA1(a4bdf58c190ca16fc7b976c97f41087a61fdb8b8) ) /* bg tiles lookup table high bits */
	ROM_LOAD( "xvi_4bpr.3l",  0x0700, 0x0200, CRC(fd8b9d91) SHA1(87ddf0b9d723aabb422d6d416aa9ec6bc246bf34) ) /* sprite lookup table low bits */
	ROM_LOAD( "xvi_5bpr.3m",  0x0900, 0x0200, CRC(bf906d82) SHA1(776168a73d3b9f0ce05610acc8a623deae0a572b) ) /* sprite lookup table high bits */

	ROM_REGION( 0x0200, REGION_SOUND1, 0 )	/* sound PROMs */
	ROM_LOAD( "xvi_2bpr.7n",  0x0000, 0x0100, CRC(550f06bc) SHA1(816a0fafa0b084ac11ae1af70a5186539376fc2a) )
	ROM_LOAD( "xvi_1bpr.5n",  0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )	/* timing - not used */
ROM_END

ROM_START( xeviousb )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )	/* 64k for the first CPU */
	ROM_LOAD( "1m.bin",       0x0000, 0x2000, CRC(e82a22f6) SHA1(6fd09a7fb263cda3d5268cc6d7bfe71a57ac4b47) )
	ROM_LOAD( "1l.bin",       0x2000, 0x2000, CRC(13831df9) SHA1(a7892d1d98868a83a5d1092976873b82577e9e94) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* 64k for the second CPU */
	ROM_LOAD( "4c.bin",       0x0000, 0x2000, CRC(827e7747) SHA1(d22645d71b164613834336e26e6942506a0e7eaa) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )	/* 64k for the audio CPU */
	ROM_LOAD( "xvi_7.2c",     0x0000, 0x1000, CRC(dd35cf1c) SHA1(f8d1f8e019d8198308443c2e7e815d0d04b23d14) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "xvi_12.3b",    0x0000, 0x1000, CRC(088c8b26) SHA1(9c3b61dfca2f84673a78f7f66e363777a8f47a59) )	/* foreground characters */

	ROM_REGION( 0x2000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "xvi_13.3c",    0x0000, 0x1000, CRC(de60ba25) SHA1(32bc09be5ff8b52ee3a26e0ac3ebc2d4107badb7) )	/* bg pattern B0 */
	ROM_LOAD( "xvi_14.3d",    0x1000, 0x1000, CRC(535cdbbc) SHA1(fb9ffe5fc43e0213231267e98d605d43c15f61e8) )	/* bg pattern B1 */

	ROM_REGION( 0xa000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "xvi_15.4m",    0x0000, 0x2000, CRC(dc2c0ecb) SHA1(19ddbd9805f77f38c9a9a1bb30dba6c720b8609f) )	/* sprite set #1, planes 0/1 */
	ROM_LOAD( "xvi_17.4p",    0x2000, 0x2000, CRC(dfb587ce) SHA1(acff2bf5cde85a16cdc98a52cdea11f77fadf25a) )	/* sprite set #2, planes 0/1 */
	ROM_LOAD( "xvi_16.4n",    0x4000, 0x1000, CRC(605ca889) SHA1(3bf380ef76c03822a042ecc73b5edd4543c268ce) )	/* sprite set #3, planes 0/1 */
	ROM_LOAD( "xvi_18.4r",    0x5000, 0x2000, CRC(02417d19) SHA1(b5f830dd2cf25cf154308d2e640f0ecdcda5d8cd) )	/* sprite set #1, plane 2, set #2, plane 2 */
	/* 0x7000-0x8fff  will be unpacked from 0x5000-0x6fff */
	ROM_FILL(                 0x9000, 0x1000, 0x00 )	/* empty space to decode sprite set #3 as 3 bits per pixel */

	ROM_REGION( 0x4000, REGION_GFX4, 0 )	/* background tilemaps */
	ROM_LOAD( "xvi_9.2a",     0x0000, 0x1000, CRC(57ed9879) SHA1(3106d1aacff06cf78371bd19967141072b32b7d7) )
	ROM_LOAD( "xvi_10.2b",    0x1000, 0x2000, CRC(ae3ba9e5) SHA1(49064b25667ffcd81137cd5e800df4b78b182a46) )
	ROM_LOAD( "xvi_11.2c",    0x3000, 0x1000, CRC(31e244dd) SHA1(3f7eac12863697a98e1122111801606759e44b2a) )

	ROM_REGION( 0x0b00, REGION_PROMS, 0 )
	ROM_LOAD( "xvi_8bpr.6a",  0x0000, 0x0100, CRC(5cc2727f) SHA1(0dc1e63a47a4cb0ba75f6f1e0c15e408bb0ee2a1) ) /* palette red component */
	ROM_LOAD( "xvi_9bpr.6d",  0x0100, 0x0100, CRC(5c8796cc) SHA1(63015e3c0874afc6b1ca032f1ffb8f90562c77c8) ) /* palette green component */
	ROM_LOAD( "xvi10bpr.6e",  0x0200, 0x0100, CRC(3cb60975) SHA1(c94d5a5dd4d8a08d6d39c051a4a722581b903f45) ) /* palette blue component */
	ROM_LOAD( "xvi_7bpr.4h",  0x0300, 0x0200, CRC(22d98032) SHA1(ec6626828c79350417d08b98e9631ad35edd4a41) ) /* bg tiles lookup table low bits */
	ROM_LOAD( "xvi_6bpr.4f",  0x0500, 0x0200, CRC(3a7599f0) SHA1(a4bdf58c190ca16fc7b976c97f41087a61fdb8b8) ) /* bg tiles lookup table high bits */
	ROM_LOAD( "xvi_4bpr.3l",  0x0700, 0x0200, CRC(fd8b9d91) SHA1(87ddf0b9d723aabb422d6d416aa9ec6bc246bf34) ) /* sprite lookup table low bits */
	ROM_LOAD( "xvi_5bpr.3m",  0x0900, 0x0200, CRC(bf906d82) SHA1(776168a73d3b9f0ce05610acc8a623deae0a572b) ) /* sprite lookup table high bits */

	ROM_REGION( 0x0200, REGION_SOUND1, 0 )	/* sound PROMs */
	ROM_LOAD( "xvi_2bpr.7n",  0x0000, 0x0100, CRC(550f06bc) SHA1(816a0fafa0b084ac11ae1af70a5186539376fc2a) )
	ROM_LOAD( "xvi_1bpr.5n",  0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )	/* timing - not used */
ROM_END

ROM_START( xeviousc )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )	/* 64k for the first CPU */
	ROM_LOAD( "xvi_u_.3p",    0x0000, 0x1000, CRC(7b203868) SHA1(3bafaa42bccddfaf8d9197e93416a731b7f8fb94) )
	ROM_LOAD( "xv_2-2.3m",    0x1000, 0x1000, CRC(b6fe738e) SHA1(23cdf1f2c2642f9bc3f843b5c338372027032380) )
	ROM_LOAD( "xv_2-3.2m",    0x2000, 0x1000, CRC(dbd52ff5) SHA1(eb42393720fc1fd4a1f6cdba87ac4177fd5827fe) )
	ROM_LOAD( "xvi_u_.2l",    0x3000, 0x1000, CRC(ad12af53) SHA1(ff3a96d6f7357fb2d33cd9d77d53477b9071ffc9) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* 64k for the second CPU */
	ROM_LOAD( "xv2_5.3f",     0x0000, 0x1000, CRC(f8cc2861) SHA1(9b02c00cff6c771d46776416295f9e12a2166cc5) )
	ROM_LOAD( "xvi_6.3j",     0x1000, 0x1000, CRC(e18cdaad) SHA1(6b79efee1a9642edb9f752101737132401248aed) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )	/* 64k for the audio CPU */
	ROM_LOAD( "xvi_7.2c",     0x0000, 0x1000, CRC(dd35cf1c) SHA1(f8d1f8e019d8198308443c2e7e815d0d04b23d14) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "xvi_12.3b",    0x0000, 0x1000, CRC(088c8b26) SHA1(9c3b61dfca2f84673a78f7f66e363777a8f47a59) )	/* foreground characters */

	ROM_REGION( 0x2000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "xvi_13.3c",    0x0000, 0x1000, CRC(de60ba25) SHA1(32bc09be5ff8b52ee3a26e0ac3ebc2d4107badb7) )	/* bg pattern B0 */
	ROM_LOAD( "xvi_14.3d",    0x1000, 0x1000, CRC(535cdbbc) SHA1(fb9ffe5fc43e0213231267e98d605d43c15f61e8) )	/* bg pattern B1 */

	ROM_REGION( 0xa000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "xvi_15.4m",    0x0000, 0x2000, CRC(dc2c0ecb) SHA1(19ddbd9805f77f38c9a9a1bb30dba6c720b8609f) )	/* sprite set #1, planes 0/1 */
	ROM_LOAD( "xvi_17.4p",    0x2000, 0x2000, CRC(dfb587ce) SHA1(acff2bf5cde85a16cdc98a52cdea11f77fadf25a) )	/* sprite set #2, planes 0/1 */
	ROM_LOAD( "xvi_16.4n",    0x4000, 0x1000, CRC(605ca889) SHA1(3bf380ef76c03822a042ecc73b5edd4543c268ce) )	/* sprite set #3, planes 0/1 */
	ROM_LOAD( "xvi_18.4r",    0x5000, 0x2000, CRC(02417d19) SHA1(b5f830dd2cf25cf154308d2e640f0ecdcda5d8cd) )	/* sprite set #1, plane 2, set #2, plane 2 */
	/* 0x7000-0x8fff  will be unpacked from 0x5000-0x6fff */
	ROM_FILL(                 0x9000, 0x1000, 0x00 )	/* empty space to decode sprite set #3 as 3 bits per pixel */

	ROM_REGION( 0x4000, REGION_GFX4, 0 )	/* background tilemaps */
	ROM_LOAD( "xvi_9.2a",     0x0000, 0x1000, CRC(57ed9879) SHA1(3106d1aacff06cf78371bd19967141072b32b7d7) )
	ROM_LOAD( "xvi_10.2b",    0x1000, 0x2000, CRC(ae3ba9e5) SHA1(49064b25667ffcd81137cd5e800df4b78b182a46) )
	ROM_LOAD( "xvi_11.2c",    0x3000, 0x1000, CRC(31e244dd) SHA1(3f7eac12863697a98e1122111801606759e44b2a) )

	ROM_REGION( 0x0b00, REGION_PROMS, 0 )
	ROM_LOAD( "xvi_8bpr.6a",  0x0000, 0x0100, CRC(5cc2727f) SHA1(0dc1e63a47a4cb0ba75f6f1e0c15e408bb0ee2a1) ) /* palette red component */
	ROM_LOAD( "xvi_9bpr.6d",  0x0100, 0x0100, CRC(5c8796cc) SHA1(63015e3c0874afc6b1ca032f1ffb8f90562c77c8) ) /* palette green component */
	ROM_LOAD( "xvi10bpr.6e",  0x0200, 0x0100, CRC(3cb60975) SHA1(c94d5a5dd4d8a08d6d39c051a4a722581b903f45) ) /* palette blue component */
	ROM_LOAD( "xvi_7bpr.4h",  0x0300, 0x0200, CRC(22d98032) SHA1(ec6626828c79350417d08b98e9631ad35edd4a41) ) /* bg tiles lookup table low bits */
	ROM_LOAD( "xvi_6bpr.4f",  0x0500, 0x0200, CRC(3a7599f0) SHA1(a4bdf58c190ca16fc7b976c97f41087a61fdb8b8) ) /* bg tiles lookup table high bits */
	ROM_LOAD( "xvi_4bpr.3l",  0x0700, 0x0200, CRC(fd8b9d91) SHA1(87ddf0b9d723aabb422d6d416aa9ec6bc246bf34) ) /* sprite lookup table low bits */
	ROM_LOAD( "xvi_5bpr.3m",  0x0900, 0x0200, CRC(bf906d82) SHA1(776168a73d3b9f0ce05610acc8a623deae0a572b) ) /* sprite lookup table high bits */

	ROM_REGION( 0x0200, REGION_SOUND1, 0 )	/* sound PROMs */
	ROM_LOAD( "xvi_2bpr.7n",  0x0000, 0x0100, CRC(550f06bc) SHA1(816a0fafa0b084ac11ae1af70a5186539376fc2a) )
	ROM_LOAD( "xvi_1bpr.5n",  0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )	/* timing - not used */
ROM_END

ROM_START( xevios )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )	/* 64k for the first CPU */
	ROM_LOAD( "4.7h",         0x0000, 0x1000, CRC(1f8ca4c0) SHA1(9fdaa2e0016c07e274544f8334778fe81b8344a5) )
	ROM_LOAD( "5.6h",         0x1000, 0x1000, CRC(2e47ce8f) SHA1(fb35dd086e98279a5f17036f624ef5294c777d84) )
	ROM_LOAD( "xvi_3.2m",     0x2000, 0x1000, CRC(79754b7d) SHA1(c6a154858716e1f073b476824b183de20e06d093) )
	ROM_LOAD( "w7.4h",        0x3000, 0x1000, CRC(17f48277) SHA1(ffe590acf07985355ef91fbe0fc3dcf6e8fd62fd) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* 64k for the second CPU */
	ROM_LOAD( "xvi_5.3f",     0x0000, 0x1000, CRC(c85b703f) SHA1(15f1c005b9d806a384ab1f2240b9c580bfe83893) )
	ROM_LOAD( "xvi_6.3j",     0x1000, 0x1000, CRC(e18cdaad) SHA1(6b79efee1a9642edb9f752101737132401248aed) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )	/* 64k for the audio CPU */
	ROM_LOAD( "xvi_7.2c",     0x0000, 0x1000, CRC(dd35cf1c) SHA1(f8d1f8e019d8198308443c2e7e815d0d04b23d14) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "xvi_12.3b",    0x0000, 0x1000, CRC(088c8b26) SHA1(9c3b61dfca2f84673a78f7f66e363777a8f47a59) )	/* foreground characters */

	ROM_REGION( 0x2000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "xvi_13.3c",    0x0000, 0x1000, CRC(de60ba25) SHA1(32bc09be5ff8b52ee3a26e0ac3ebc2d4107badb7) )	/* bg pattern B0 */
	ROM_LOAD( "xvi_14.3d",    0x1000, 0x1000, CRC(535cdbbc) SHA1(fb9ffe5fc43e0213231267e98d605d43c15f61e8) )	/* bg pattern B1 */

	ROM_REGION( 0xa000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "xvi_15.4m",    0x0000, 0x2000, CRC(dc2c0ecb) SHA1(19ddbd9805f77f38c9a9a1bb30dba6c720b8609f) )	/* sprite set #1, planes 0/1 */
	ROM_LOAD( "xvi_17.4p",    0x2000, 0x2000, CRC(dfb587ce) SHA1(acff2bf5cde85a16cdc98a52cdea11f77fadf25a) )	/* sprite set #2, planes 0/1 */
	ROM_LOAD( "xvi_16.4n",    0x4000, 0x1000, CRC(605ca889) SHA1(3bf380ef76c03822a042ecc73b5edd4543c268ce) )	/* sprite set #3, planes 0/1 */
	ROM_LOAD( "16.8d",        0x5000, 0x2000, CRC(44262c04) SHA1(4291f83193d11064c2ba6a9af27951b93bb945c3) )	/* sprite set #1, plane 2, set #2, plane 2 */
	/* 0x7000-0x8fff  will be unpacked from 0x5000-0x6fff */
	ROM_FILL(                 0x9000, 0x1000, 0x00 )	/* empty space to decode sprite set #3 as 3 bits per pixel */

	ROM_REGION( 0x4000, REGION_GFX4, 0 )	/* background tilemaps */
	ROM_LOAD( "10.1d",        0x0000, 0x1000, CRC(10baeebb) SHA1(c544c9e0bb7a1ef93b3f2c2c1397f659d5334373) )
	ROM_LOAD( "xvi_10.2b",    0x1000, 0x2000, CRC(ae3ba9e5) SHA1(49064b25667ffcd81137cd5e800df4b78b182a46) )
	ROM_LOAD( "12.3d",        0x3000, 0x1000, CRC(51a4e83b) SHA1(fbf3b1e47b75c5e0b297ee2cd6597b1dfd80bc6f) )

	ROM_REGION( 0x0b00, REGION_PROMS, 0 )
	ROM_LOAD( "xvi_8bpr.6a",  0x0000, 0x0100, CRC(5cc2727f) SHA1(0dc1e63a47a4cb0ba75f6f1e0c15e408bb0ee2a1) ) /* palette red component */
	ROM_LOAD( "xvi_9bpr.6d",  0x0100, 0x0100, CRC(5c8796cc) SHA1(63015e3c0874afc6b1ca032f1ffb8f90562c77c8) ) /* palette green component */
	ROM_LOAD( "xvi10bpr.6e",  0x0200, 0x0100, CRC(3cb60975) SHA1(c94d5a5dd4d8a08d6d39c051a4a722581b903f45) ) /* palette blue component */
	ROM_LOAD( "xvi_7bpr.4h",  0x0300, 0x0200, CRC(22d98032) SHA1(ec6626828c79350417d08b98e9631ad35edd4a41) ) /* bg tiles lookup table low bits */
	ROM_LOAD( "xvi_6bpr.4f",  0x0500, 0x0200, CRC(3a7599f0) SHA1(a4bdf58c190ca16fc7b976c97f41087a61fdb8b8) ) /* bg tiles lookup table high bits */
	ROM_LOAD( "xvi_4bpr.3l",  0x0700, 0x0200, CRC(fd8b9d91) SHA1(87ddf0b9d723aabb422d6d416aa9ec6bc246bf34) ) /* sprite lookup table low bits */
	ROM_LOAD( "xvi_5bpr.3m",  0x0900, 0x0200, CRC(bf906d82) SHA1(776168a73d3b9f0ce05610acc8a623deae0a572b) ) /* sprite lookup table high bits */

	ROM_REGION( 0x0200, REGION_SOUND1, 0 )	/* sound PROMs */
	ROM_LOAD( "xvi_2bpr.7n",  0x0000, 0x0100, CRC(550f06bc) SHA1(816a0fafa0b084ac11ae1af70a5186539376fc2a) )
	ROM_LOAD( "xvi_1bpr.5n",  0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )	/* timing - not used */

	ROM_REGION( 0x3000, REGION_USER1, 0 )
	/* extra ROMs (function unknown, could be emulation of the custom I/O */
	/* chip with a Z80): */
	ROM_LOAD( "1.16j",        0x0000, 0x1000, CRC(2618f0ce) SHA1(54e8644b5609d6f6ec717a7469c76901eb79f26e) )
	ROM_LOAD( "2.17b",        0x1000, 0x2000, CRC(de359fac) SHA1(a55df9984bfffafeadae8a5a63b07f1fa9c5eebf) )
ROM_END

ROM_START( battles )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )	/* 64k for the first CPU */
	ROM_LOAD( "b_1.bin",      0x0000, 0x2000, CRC(b6e4f4f3) SHA1(ceaaa63b50e75dcb05aeb68574336dfe56a8434a) )
	ROM_LOAD( "b_2.bin",      0x2000, 0x2000, CRC(47017bc8) SHA1(0da73ae079fb6a64eed56197e2c88609ef34166c) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* 64k for the second CPU */
	ROM_LOAD( "b_3.bin",      0x0000, 0x2000, CRC(0ede5706) SHA1(65b235c5abe487612e11d0235410f1ca59b06e95) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )	/* 64k for the audio CPU */
	ROM_LOAD( "xvi_7.2c",     0x0000, 0x1000, CRC(dd35cf1c) SHA1(f8d1f8e019d8198308443c2e7e815d0d04b23d14) )

	ROM_REGION( 0x10000, REGION_CPU4, 0 )	/* 64k for the CUSTOM I/O Emulation CPU */
	ROM_LOAD( "b_5.bin",      0x0000, 0x1000, CRC(23107dfb) SHA1(74c49a5648faab632ae5ed8dd18a1d8b39837e2d) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "b_9.bin",      0x0000, 0x1000, CRC(5bd6e9ae) SHA1(f16c7eec39fce856c775b2b81ab55fb42376850e) )	/* foreground characters */

	ROM_REGION( 0x2000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "b_10.bin",     0x0000, 0x1000, CRC(b43ea55d) SHA1(06f4c4e7fc71b9e173c3bdf91c40f47750051b5e) )	/* bg pattern B0 */
	ROM_LOAD( "b_11.bin",     0x1000, 0x1000, CRC(73603931) SHA1(1f7824b107a5a3d5c3434f02f17173a1f85fd29c) )	/* bg pattern B1 */

	ROM_REGION( 0xa000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "xvi_15.4m",    0x0000, 0x2000, CRC(dc2c0ecb) SHA1(19ddbd9805f77f38c9a9a1bb30dba6c720b8609f) )	/* sprite set #1, planes 0/1 */
	ROM_LOAD( "xvi_17.4p",    0x2000, 0x2000, CRC(dfb587ce) SHA1(acff2bf5cde85a16cdc98a52cdea11f77fadf25a) )	/* sprite set #2, planes 0/1 */
	ROM_LOAD( "xvi_16.4n",    0x4000, 0x1000, CRC(605ca889) SHA1(3bf380ef76c03822a042ecc73b5edd4543c268ce) )	/* sprite set #3, planes 0/1 */
	ROM_LOAD( "xvi_18.4r",    0x5000, 0x2000, CRC(02417d19) SHA1(b5f830dd2cf25cf154308d2e640f0ecdcda5d8cd) )	/* sprite set #1, plane 2, set #2, plane 2 */
	/* 0x7000-0x8fff  will be unpacked from 0x5000-0x6fff */
	ROM_FILL(                 0x9000, 0x1000, 0x00 )	/* empty space to decode sprite set #3 as 3 bits per pixel */

	ROM_REGION( 0x4000, REGION_GFX4, 0 )	/* background tilemaps */
	ROM_LOAD( "xvi_9.2a",     0x0000, 0x1000, CRC(57ed9879) SHA1(3106d1aacff06cf78371bd19967141072b32b7d7) )
	ROM_LOAD( "xvi_10.2b",    0x1000, 0x2000, CRC(ae3ba9e5) SHA1(49064b25667ffcd81137cd5e800df4b78b182a46) )
	ROM_LOAD( "xvi_11.2c",    0x3000, 0x1000, CRC(31e244dd) SHA1(3f7eac12863697a98e1122111801606759e44b2a) )

	ROM_REGION( 0x1400, REGION_PROMS, 0 )
	ROM_LOAD( "xvi_8bpr.6a",  0x0000, 0x0100, CRC(5cc2727f) SHA1(0dc1e63a47a4cb0ba75f6f1e0c15e408bb0ee2a1) ) /* palette red component */
	ROM_LOAD( "xvi_9bpr.6d",  0x0100, 0x0100, CRC(5c8796cc) SHA1(63015e3c0874afc6b1ca032f1ffb8f90562c77c8) ) /* palette green component */
	ROM_LOAD( "xvi10bpr.6e",  0x0200, 0x0100, CRC(3cb60975) SHA1(c94d5a5dd4d8a08d6d39c051a4a722581b903f45) ) /* palette blue component */
	ROM_LOAD( "b_-bpr.bin",   0x0300, 0x0400, CRC(d2d208b1) SHA1(6c8d29912c03ee93759e24085bc66ab738768bcc) ) /* bg tiles lookup table low bits */
	ROM_LOAD( "b_6bpr.bin",   0x0700, 0x0400, CRC(0260c041) SHA1(1a7516e8b18ffdd9789eec8b834c17b3ba312afe) ) /* bg tiles lookup table high bits */
	ROM_LOAD( "b_4bpr.bin",   0x0b00, 0x0400, CRC(33764974) SHA1(567b048b8a93e30090ccee4f6aadc0353524d8d1) ) /* sprite lookup table low bits */
	ROM_LOAD( "b_5bpr.bin",   0x0f00, 0x0400, CRC(43674c7e) SHA1(94c19a9da81839cb1dfde3f11b2fd82ffe45efb9) ) /* sprite lookup table high bits */

	ROM_REGION( 0x0200, REGION_SOUND1, 0 )	/* sound PROMs */
	ROM_LOAD( "xvi_2bpr.7n",  0x0000, 0x0100, CRC(550f06bc) SHA1(816a0fafa0b084ac11ae1af70a5186539376fc2a) )
	ROM_LOAD( "xvi_1bpr.5n",  0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )	/* timing - not used */
ROM_END

ROM_START( sxevious )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )	/* 64k for the first CPU */
	ROM_LOAD( "cpu_3p.rom",   0x0000, 0x1000, CRC(1c8d27d5) SHA1(2c41303d8c74acb5840295a4b460a39a9a8e21bb) )
	ROM_LOAD( "cpu_3m.rom",   0x1000, 0x1000, CRC(fd04e615) SHA1(7169e7f3bd1e9cfae9671b89f2a45f56b968e1ff) )
	ROM_LOAD( "cpu_2m.rom",   0x2000, 0x1000, CRC(294d5404) SHA1(ecc39fb2c0065a36f20541747089b4e30dfb99b1) )
	ROM_LOAD( "cpu_2l.rom",   0x3000, 0x1000, CRC(6a44bf92) SHA1(0ca726f7f9528789f2a718df55e59406a283cdfa) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* 64k for the second CPU */
	ROM_LOAD( "cpu_3f.rom",   0x0000, 0x1000, CRC(d4bd3d81) SHA1(5831bb306bd650779207936bfd00f25864733abb) )
	ROM_LOAD( "cpu_3j.rom",   0x1000, 0x1000, CRC(af06be5f) SHA1(5a020822387ab8c69214db961180760fa9853e6e) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )	/* 64k for the audio CPU */
	ROM_LOAD( "xvi_7.2c",     0x0000, 0x1000, CRC(dd35cf1c) SHA1(f8d1f8e019d8198308443c2e7e815d0d04b23d14) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "xvi_12.3b",    0x0000, 0x1000, CRC(088c8b26) SHA1(9c3b61dfca2f84673a78f7f66e363777a8f47a59) )	/* foreground characters */

	ROM_REGION( 0x2000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "xvi_13.3c",    0x0000, 0x1000, CRC(de60ba25) SHA1(32bc09be5ff8b52ee3a26e0ac3ebc2d4107badb7) )	/* bg pattern B0 */
	ROM_LOAD( "xvi_14.3d",    0x1000, 0x1000, CRC(535cdbbc) SHA1(fb9ffe5fc43e0213231267e98d605d43c15f61e8) )	/* bg pattern B1 */

	ROM_REGION( 0xa000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "xvi_15.4m",    0x0000, 0x2000, CRC(dc2c0ecb) SHA1(19ddbd9805f77f38c9a9a1bb30dba6c720b8609f) )	/* sprite set #1, planes 0/1 */
	ROM_LOAD( "xvi_17.4p",    0x2000, 0x2000, CRC(dfb587ce) SHA1(acff2bf5cde85a16cdc98a52cdea11f77fadf25a) )	/* sprite set #2, planes 0/1 */
	ROM_LOAD( "xvi_16.4n",    0x4000, 0x1000, CRC(605ca889) SHA1(3bf380ef76c03822a042ecc73b5edd4543c268ce) )	/* sprite set #3, planes 0/1 */
	ROM_LOAD( "xvi_18.4r",    0x5000, 0x2000, CRC(02417d19) SHA1(b5f830dd2cf25cf154308d2e640f0ecdcda5d8cd) )	/* sprite set #1, plane 2, set #2, plane 2 */
	/* 0x7000-0x8fff  will be unpacked from 0x5000-0x6fff */
	ROM_FILL(                 0x9000, 0x1000, 0x00 )	/* empty space to decode sprite set #3 as 3 bits per pixel */

	ROM_REGION( 0x4000, REGION_GFX4, 0 )	/* background tilemaps */
	ROM_LOAD( "xvi_9.2a",     0x0000, 0x1000, CRC(57ed9879) SHA1(3106d1aacff06cf78371bd19967141072b32b7d7) )
	ROM_LOAD( "xvi_10.2b",    0x1000, 0x2000, CRC(ae3ba9e5) SHA1(49064b25667ffcd81137cd5e800df4b78b182a46) )
	ROM_LOAD( "xvi_11.2c",    0x3000, 0x1000, CRC(31e244dd) SHA1(3f7eac12863697a98e1122111801606759e44b2a) )

	ROM_REGION( 0x0b00, REGION_PROMS, 0 )
	ROM_LOAD( "xvi_8bpr.6a",  0x0000, 0x0100, CRC(5cc2727f) SHA1(0dc1e63a47a4cb0ba75f6f1e0c15e408bb0ee2a1) ) /* palette red component */
	ROM_LOAD( "xvi_9bpr.6d",  0x0100, 0x0100, CRC(5c8796cc) SHA1(63015e3c0874afc6b1ca032f1ffb8f90562c77c8) ) /* palette green component */
	ROM_LOAD( "xvi10bpr.6e",  0x0200, 0x0100, CRC(3cb60975) SHA1(c94d5a5dd4d8a08d6d39c051a4a722581b903f45) ) /* palette blue component */
	ROM_LOAD( "xvi_7bpr.4h",  0x0300, 0x0200, CRC(22d98032) SHA1(ec6626828c79350417d08b98e9631ad35edd4a41) ) /* bg tiles lookup table low bits */
	ROM_LOAD( "xvi_6bpr.4f",  0x0500, 0x0200, CRC(3a7599f0) SHA1(a4bdf58c190ca16fc7b976c97f41087a61fdb8b8) ) /* bg tiles lookup table high bits */
	ROM_LOAD( "xvi_4bpr.3l",  0x0700, 0x0200, CRC(fd8b9d91) SHA1(87ddf0b9d723aabb422d6d416aa9ec6bc246bf34) ) /* sprite lookup table low bits */
	ROM_LOAD( "xvi_5bpr.3m",  0x0900, 0x0200, CRC(bf906d82) SHA1(776168a73d3b9f0ce05610acc8a623deae0a572b) ) /* sprite lookup table high bits */

	ROM_REGION( 0x0200, REGION_SOUND1, 0 )	/* sound PROMs */
	ROM_LOAD( "xvi_2bpr.7n",  0x0000, 0x0100, CRC(550f06bc) SHA1(816a0fafa0b084ac11ae1af70a5186539376fc2a) )
	ROM_LOAD( "xvi_1bpr.5n",  0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )	/* timing - not used */
ROM_END



ROM_START( digdug )
	ROM_REGION( 0x10000, REGION_CPU1, 0 ) /* 64k for code for the first CPU  */
	ROM_LOAD( "dd1a.1",       0x0000, 0x1000, CRC(a80ec984) SHA1(86689980410b9429cd7582c7a76342721c87d030) )
	ROM_LOAD( "dd1a.2",       0x1000, 0x1000, CRC(559f00bd) SHA1(fde17785df21956d6fd06bcfe675c392dadb1524) )
	ROM_LOAD( "dd1a.3",       0x2000, 0x1000, CRC(8cbc6fe1) SHA1(57b8a5777f8bb9773caf0cafe5408c8b9768cb25) )
	ROM_LOAD( "dd1a.4",       0x3000, 0x1000, CRC(d066f830) SHA1(b0a615fe4a5c8742c1e4ef234ef34c369d2723b9) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for the second CPU */
	ROM_LOAD( "dd1a.5",       0x0000, 0x1000, CRC(6687933b) SHA1(c16144de7633595ddc1450ddce379f48e7b2195a) )
	ROM_LOAD( "dd1a.6",       0x1000, 0x1000, CRC(843d857f) SHA1(89b2ead7e478e119d33bfd67376cdf28f83de67a) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 ) /* 64k for the third CPU  */
	ROM_LOAD( "136007.107",   0x0000, 0x1000, CRC(a41bce72) SHA1(2b9b74f56aa7939d9d47cf29497ae11f10d78598) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "dd1.9",        0x0000, 0x0800, CRC(f14a6fe1) SHA1(0aa63300c2cb887196de590aceb98f3cf06fead4) )

	ROM_REGION( 0x4000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "136007.116",   0x0000, 0x1000, CRC(e22957c8) SHA1(4700c63f4f680cb8ab8c44e6f3e1712aabd5daa4) )
	ROM_LOAD( "dd1.14",       0x1000, 0x1000, CRC(2829ec99) SHA1(3e435c1afb2e44487cd7ba28a93ada2e5ccbb86d) )
	ROM_LOAD( "136007.118",   0x2000, 0x1000, CRC(458499e9) SHA1(578bd839f9218c3cf4feee1223a461144e455df8) )
	ROM_LOAD( "136007.119",   0x3000, 0x1000, CRC(c58252a0) SHA1(bd79e39e8a572d2b5c205e6de27ca23e43ec9f51) )

	ROM_REGION( 0x1000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "dd1.11",       0x0000, 0x1000, CRC(7b383983) SHA1(57f1e8f5171d13f9f76bd091d81b4423b59f6b42) )

	ROM_REGION( 0x1000, REGION_GFX4, 0 ) /* 4k for the playfield graphics */
	ROM_LOAD( "dd1.10b",      0x0000, 0x1000, CRC(2cf399c2) SHA1(317c48818992f757b1bd0e3997fa99937f81b52c) )

	ROM_REGION( 0x0220, REGION_PROMS, 0 )
	ROM_LOAD( "136007.113",   0x0000, 0x0020, CRC(4cb9da99) SHA1(91a5852a15d4672c29fdcbae75921794651f960c) )
	ROM_LOAD( "136007.111",   0x0020, 0x0100, CRC(00c7c419) SHA1(7ea149e8eb36920c3b84984b5ce623729d492fd3) )
	ROM_LOAD( "136007.112",   0x0120, 0x0100, CRC(e9b3e08e) SHA1(a294cc4da846eb702d61678396bfcbc87d30ea95) )

	ROM_REGION( 0x0200, REGION_SOUND1, 0 )	/* sound prom */
	ROM_LOAD( "136007.110",   0x0000, 0x0100, CRC(7a2815b4) SHA1(085ada18c498fdb18ecedef0ea8fe9217edb7b46) )
	ROM_LOAD( "136007.109",   0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )	/* timing - not used */
ROM_END

ROM_START( digdugb )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )	/* 64k for code for the first CPU  */
	ROM_LOAD( "136007.101",   0x0000, 0x1000, CRC(b9198079) SHA1(1d3fe04020f584ed250e32fdc6f6a3b769342884) )
	ROM_LOAD( "136007.102",   0x1000, 0x1000, CRC(b2acbe49) SHA1(c8f713e8cfa70d3bc64d3002ff7bffc65ee138e2) )
	ROM_LOAD( "136007.103",   0x2000, 0x1000, CRC(d6407b49) SHA1(0e71a8f02778286488865e20439776dbb2a8ec78) )
	ROM_LOAD( "dd1.4b",       0x3000, 0x1000, CRC(f4cebc16) SHA1(19b568f92069a1cfe1c07287408efe3b0e253375) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* 64k for the second CPU */
	ROM_LOAD( "dd1.5b",       0x0000, 0x1000, CRC(370ef9b4) SHA1(746b1fa15f5f2cfd69d8b5a7d6fb8c770abc3b4d) )
	ROM_LOAD( "dd1.6b",       0x1000, 0x1000, CRC(361eeb71) SHA1(372c97c666411c3590d790213ae6fa1ccb5ffa1c) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )	/* 64k for the third CPU  */
	ROM_LOAD( "136007.107",   0x0000, 0x1000, CRC(a41bce72) SHA1(2b9b74f56aa7939d9d47cf29497ae11f10d78598) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "dd1.9",        0x0000, 0x0800, CRC(f14a6fe1) SHA1(0aa63300c2cb887196de590aceb98f3cf06fead4) )

	ROM_REGION( 0x4000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "136007.116",   0x0000, 0x1000, CRC(e22957c8) SHA1(4700c63f4f680cb8ab8c44e6f3e1712aabd5daa4) )
	ROM_LOAD( "dd1.14",       0x1000, 0x1000, CRC(2829ec99) SHA1(3e435c1afb2e44487cd7ba28a93ada2e5ccbb86d) )
	ROM_LOAD( "136007.118",   0x2000, 0x1000, CRC(458499e9) SHA1(578bd839f9218c3cf4feee1223a461144e455df8) )
	ROM_LOAD( "136007.119",   0x3000, 0x1000, CRC(c58252a0) SHA1(bd79e39e8a572d2b5c205e6de27ca23e43ec9f51) )

	ROM_REGION( 0x1000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "dd1.11",       0x0000, 0x1000, CRC(7b383983) SHA1(57f1e8f5171d13f9f76bd091d81b4423b59f6b42) )

	ROM_REGION( 0x1000, REGION_GFX4, 0 ) /* 4k for the playfield graphics */
	ROM_LOAD( "dd1.10b",      0x0000, 0x1000, CRC(2cf399c2) SHA1(317c48818992f757b1bd0e3997fa99937f81b52c) )

	ROM_REGION( 0x0220, REGION_PROMS, 0 )
	ROM_LOAD( "136007.113",   0x0000, 0x0020, CRC(4cb9da99) SHA1(91a5852a15d4672c29fdcbae75921794651f960c) )
	ROM_LOAD( "136007.111",   0x0020, 0x0100, CRC(00c7c419) SHA1(7ea149e8eb36920c3b84984b5ce623729d492fd3) )
	ROM_LOAD( "136007.112",   0x0120, 0x0100, CRC(e9b3e08e) SHA1(a294cc4da846eb702d61678396bfcbc87d30ea95) )

	ROM_REGION( 0x0200, REGION_SOUND1, 0 )	/* sound prom */
	ROM_LOAD( "136007.110",   0x0000, 0x0100, CRC(7a2815b4) SHA1(085ada18c498fdb18ecedef0ea8fe9217edb7b46) )
	ROM_LOAD( "136007.109",   0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )	/* timing - not used */
ROM_END

ROM_START( digdugat )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )	/* 64k for code for the first CPU  */
	ROM_LOAD( "136007.201",   0x0000, 0x1000, CRC(23d0b1a4) SHA1(a118d55e03a9ccf069f37c7bac2c9044dccd1f5e) )
	ROM_LOAD( "136007.202",   0x1000, 0x1000, CRC(5453dc1f) SHA1(8be091dd53e9b44e80e1ac9b1751efbe832db78d) )
	ROM_LOAD( "136007.203",   0x2000, 0x1000, CRC(c9077dfa) SHA1(611b3e1b575a51639530917366557773534c80aa) )
	ROM_LOAD( "136007.204",   0x3000, 0x1000, CRC(a8fc8eac) SHA1(7a24197f4ec5989bc4d635b27b6578f4d62cb5f4) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* 64k for the second CPU */
	ROM_LOAD( "136007.205",   0x0000, 0x1000, CRC(5ba385c5) SHA1(f4577bddff74a14b13b212f5553fa13fe9ae4bcc) )
	ROM_LOAD( "136007.206",   0x1000, 0x1000, CRC(382b4011) SHA1(2b79ddcf48177c99b5fa1f957374f4baa2bec143) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )	/* 64k for the third CPU  */
	ROM_LOAD( "136007.107",   0x0000, 0x1000, CRC(a41bce72) SHA1(2b9b74f56aa7939d9d47cf29497ae11f10d78598) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "136007.108",   0x0000, 0x0800, CRC(3d24a3af) SHA1(857ae93e2a41258a129dcecbaed2df359540b735) )

	ROM_REGION( 0x4000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "136007.116",   0x0000, 0x1000, CRC(e22957c8) SHA1(4700c63f4f680cb8ab8c44e6f3e1712aabd5daa4) )
	ROM_LOAD( "136007.117",   0x1000, 0x1000, CRC(a3bbfd85) SHA1(2105455762e0de120f2d943f9010a7d06c6b6448) )
	ROM_LOAD( "136007.118",   0x2000, 0x1000, CRC(458499e9) SHA1(578bd839f9218c3cf4feee1223a461144e455df8) )
	ROM_LOAD( "136007.119",   0x3000, 0x1000, CRC(c58252a0) SHA1(bd79e39e8a572d2b5c205e6de27ca23e43ec9f51) )

	ROM_REGION( 0x1000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "136007.115",   0x0000, 0x1000, CRC(754539be) SHA1(466ae754eb4721df8814d4d33a31d867507d45b3) )

	ROM_REGION( 0x1000, REGION_GFX4, 0 )	/* 4k for the playfield graphics */
	ROM_LOAD( "136007.114",   0x0000, 0x1000, CRC(d6822397) SHA1(055ca6514141323f1e6dfcf91451507c04114d41) )

	ROM_REGION( 0x0220, REGION_PROMS, 0 )
	ROM_LOAD( "136007.113",   0x0000, 0x0020, CRC(4cb9da99) SHA1(91a5852a15d4672c29fdcbae75921794651f960c) )
	ROM_LOAD( "136007.111",   0x0020, 0x0100, CRC(00c7c419) SHA1(7ea149e8eb36920c3b84984b5ce623729d492fd3) )
	ROM_LOAD( "136007.112",   0x0120, 0x0100, CRC(e9b3e08e) SHA1(a294cc4da846eb702d61678396bfcbc87d30ea95) )

	ROM_REGION( 0x0200, REGION_SOUND1, 0 )	/* sound prom */
	ROM_LOAD( "136007.110",   0x0000, 0x0100, CRC(7a2815b4) SHA1(085ada18c498fdb18ecedef0ea8fe9217edb7b46) )
	ROM_LOAD( "136007.109",   0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )	/* timing - not used */
ROM_END

ROM_START( digduga1 )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )	/* 64k for code for the first CPU  */
	ROM_LOAD( "136007.101",   0x0000, 0x1000, CRC(b9198079) SHA1(1d3fe04020f584ed250e32fdc6f6a3b769342884) )
	ROM_LOAD( "136007.102",   0x1000, 0x1000, CRC(b2acbe49) SHA1(c8f713e8cfa70d3bc64d3002ff7bffc65ee138e2) )
	ROM_LOAD( "136007.103",   0x2000, 0x1000, CRC(d6407b49) SHA1(0e71a8f02778286488865e20439776dbb2a8ec78) )
	ROM_LOAD( "136007.104",   0x3000, 0x1000, CRC(b3ad42c3) SHA1(83ea80f0dd42ec1cb62e6ed45d5dda43ed21f567) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* 64k for the second CPU */
	ROM_LOAD( "136007.105",   0x0000, 0x1000, CRC(0a2aef4a) SHA1(ef40974fde8e8c305059e1dd03ea811a6aaca737) )
	ROM_LOAD( "136007.106",   0x1000, 0x1000, CRC(a2876d6e) SHA1(08e8ac50918ae32dd6fb34e65534652beb0395b2) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )	/* 64k for the third CPU  */
	ROM_LOAD( "136007.107",   0x0000, 0x1000, CRC(a41bce72) SHA1(2b9b74f56aa7939d9d47cf29497ae11f10d78598) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "136007.108",   0x0000, 0x0800, CRC(3d24a3af) SHA1(857ae93e2a41258a129dcecbaed2df359540b735) )

	ROM_REGION( 0x4000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "136007.116",   0x0000, 0x1000, CRC(e22957c8) SHA1(4700c63f4f680cb8ab8c44e6f3e1712aabd5daa4) )
	ROM_LOAD( "136007.117",   0x1000, 0x1000, CRC(a3bbfd85) SHA1(2105455762e0de120f2d943f9010a7d06c6b6448) )
	ROM_LOAD( "136007.118",   0x2000, 0x1000, CRC(458499e9) SHA1(578bd839f9218c3cf4feee1223a461144e455df8) )
	ROM_LOAD( "136007.119",   0x3000, 0x1000, CRC(c58252a0) SHA1(bd79e39e8a572d2b5c205e6de27ca23e43ec9f51) )

	ROM_REGION( 0x1000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "136007.115",   0x0000, 0x1000, CRC(754539be) SHA1(466ae754eb4721df8814d4d33a31d867507d45b3) )

	ROM_REGION( 0x1000, REGION_GFX4, 0 )	/* 4k for the playfield graphics */
	ROM_LOAD( "136007.114",   0x0000, 0x1000, CRC(d6822397) SHA1(055ca6514141323f1e6dfcf91451507c04114d41) )

	ROM_REGION( 0x0220, REGION_PROMS, 0 )
	ROM_LOAD( "136007.113",   0x0000, 0x0020, CRC(4cb9da99) SHA1(91a5852a15d4672c29fdcbae75921794651f960c) )
	ROM_LOAD( "136007.111",   0x0020, 0x0100, CRC(00c7c419) SHA1(7ea149e8eb36920c3b84984b5ce623729d492fd3) )
	ROM_LOAD( "136007.112",   0x0120, 0x0100, CRC(e9b3e08e) SHA1(a294cc4da846eb702d61678396bfcbc87d30ea95) )

	ROM_REGION( 0x0200, REGION_SOUND1, 0 )	/* sound prom */
	ROM_LOAD( "136007.110",   0x0000, 0x0100, CRC(7a2815b4) SHA1(085ada18c498fdb18ecedef0ea8fe9217edb7b46) )
	ROM_LOAD( "136007.109",   0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )	/* timing - not used */
ROM_END

ROM_START( dzigzag )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )	/* 64k for code for the first CPU  */
	ROM_LOAD( "136007.101",   0x0000, 0x1000, CRC(b9198079) SHA1(1d3fe04020f584ed250e32fdc6f6a3b769342884) )
	ROM_LOAD( "136007.102",   0x1000, 0x1000, CRC(b2acbe49) SHA1(c8f713e8cfa70d3bc64d3002ff7bffc65ee138e2) )
	ROM_LOAD( "136007.103",   0x2000, 0x1000, CRC(d6407b49) SHA1(0e71a8f02778286488865e20439776dbb2a8ec78) )
	ROM_LOAD( "zigzag4",      0x3000, 0x1000, CRC(da20d2f6) SHA1(4eafe5ee917060d01d9df92d678c455edbbf27a6) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* 64k for the second CPU */
	ROM_LOAD( "zigzag5",      0x0000, 0x2000, CRC(f803c748) SHA1(a4c7dde0b794366cbfd03f339de980a6575a42fc) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )	/* 64k for the third CPU  */
	ROM_LOAD( "136007.107",   0x0000, 0x1000, CRC(a41bce72) SHA1(2b9b74f56aa7939d9d47cf29497ae11f10d78598) )

	ROM_REGION( 0x10000, REGION_CPU4, 0 )	/* 64k for a Z80 which emulates the custom I/O chip (not used) */
	ROM_LOAD( "zigzag7",      0x0000, 0x1000, CRC(24c3510c) SHA1(3214a16f697f88d23f3441e58c56110930d7c341) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "zigzag8",      0x0000, 0x0800, CRC(86120541) SHA1(c974441ee0421a38c25bc7c3edbc6b510b7df473) )

	ROM_REGION( 0x4000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "136007.116",   0x0000, 0x1000, CRC(e22957c8) SHA1(4700c63f4f680cb8ab8c44e6f3e1712aabd5daa4) )
	ROM_LOAD( "zigzag12",     0x1000, 0x1000, CRC(386a0956) SHA1(79f5d6af1fdc467a503216a588cb03535c823a40) )
	ROM_LOAD( "zigzag13",     0x2000, 0x1000, CRC(69f6e395) SHA1(10a7518e963f2cecb494d77137e01a068116e20b) )
	ROM_LOAD( "136007.119",   0x3000, 0x1000, CRC(c58252a0) SHA1(bd79e39e8a572d2b5c205e6de27ca23e43ec9f51) )

	ROM_REGION( 0x1000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "dd1.11",       0x0000, 0x1000, CRC(7b383983) SHA1(57f1e8f5171d13f9f76bd091d81b4423b59f6b42) )

	ROM_REGION( 0x1000, REGION_GFX4, 0 ) /* 4k for the playfield graphics */
	ROM_LOAD( "dd1.10b",      0x0000, 0x1000, CRC(2cf399c2) SHA1(317c48818992f757b1bd0e3997fa99937f81b52c) )

	ROM_REGION( 0x0220, REGION_PROMS, 0 )
	ROM_LOAD( "136007.113",   0x0000, 0x0020, CRC(4cb9da99) SHA1(91a5852a15d4672c29fdcbae75921794651f960c) )
	ROM_LOAD( "136007.111",   0x0020, 0x0100, CRC(00c7c419) SHA1(7ea149e8eb36920c3b84984b5ce623729d492fd3) )
	ROM_LOAD( "136007.112",   0x0120, 0x0100, CRC(e9b3e08e) SHA1(a294cc4da846eb702d61678396bfcbc87d30ea95) )

	ROM_REGION( 0x0200, REGION_SOUND1, 0 )	/* sound prom */
	ROM_LOAD( "136007.110",   0x0000, 0x0100, CRC(7a2815b4) SHA1(085ada18c498fdb18ecedef0ea8fe9217edb7b46) )
	ROM_LOAD( "136007.109",   0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )	/* timing - not used */
ROM_END



static DRIVER_INIT (galaga)
{
	/* swap bytes for flipped character so we can decode them together with normal characters */
	UINT8 *rom = memory_region(REGION_GFX1);
	int i;

	for (i = 0;i < memory_region_length(REGION_GFX1);i++)
	{
		if ((i & 0x0808) == 0x0800)
		{
			int t = rom[i];
			rom[i] = rom[i+8];
			rom[i+8] = t;
		}
	}
}

static DRIVER_INIT (gatsbee)
{
	init_galaga();

	/* Gatsbee has a larger character ROM, we need a handler for banking */
	install_mem_write_handler(0, 0x1000, 0x1000, gatsbee_bank_w);
}


static DRIVER_INIT( xevious )
{
	data8_t *rom;
	int i;

	rom = memory_region(REGION_GFX3) + 0x5000;
	for (i = 0;i < 0x2000;i++)
		rom[i + 0x2000] = rom[i] >> 4;
}

static DRIVER_INIT( xevios )
{
	int A;


	/* convert one of the sprite ROMs to the format used by Xevious */
	for (A = 0x5000;A < 0x7000;A++)
	{
		UINT8 *rom = memory_region(REGION_GFX3);

		rom[A] = BITSWAP8(rom[A],1,3,5,7,0,2,4,6);
	}

	/* convert one of tile map ROMs to the format used by Xevious */
	for (A = 0x0000;A < 0x1000;A++)
	{
		UINT8 *rom = memory_region(REGION_GFX4);

		rom[A] = BITSWAP8(rom[A],3,7,5,1,2,6,4,0);
	}

	init_xevious();
}


static DRIVER_INIT( battles )
{
	/* replace the Namco I/O handlers with interface to the 4th CPU */
	install_mem_read_handler (0, 0x7000, 0x700f, battles_customio_data0_r );
	install_mem_read_handler (0, 0x7100, 0x7100, battles_customio0_r );
	install_mem_write_handler(0, 0x7000, 0x700f, battles_customio_data0_w );
	install_mem_write_handler(0, 0x7100, 0x7100, battles_customio0_w );

	init_xevious();
}



GAMECX(1981, bosco,    0,       bosco,   bosco,    0,       ROT0,  "Namco", "Bosconian (new version)", GAME_IMPERFECT_GRAPHICS, &bosco_ctrl, NULL )
GAMECX(1981, boscoo,   bosco,   bosco,   bosco,    0,       ROT0,  "Namco", "Bosconian (old version)", GAME_IMPERFECT_GRAPHICS, &bosco_ctrl, NULL )
GAMECX(1981, boscoo2,  bosco,   bosco,   bosco,    0,       ROT0,  "Namco", "Bosconian (older version)", GAME_IMPERFECT_GRAPHICS, &bosco_ctrl, NULL )
GAMECX(1981, boscomd,  bosco,   bosco,   boscomd,  0,       ROT0,  "Namco (Midway license)", "Bosconian (Midway, new version)", GAME_IMPERFECT_GRAPHICS, &bosco_ctrl, NULL )
GAMECX(1981, boscomdo, bosco,   bosco,   boscomd,  0,       ROT0,  "Namco (Midway license)", "Bosconian (Midway, old version)", GAME_IMPERFECT_GRAPHICS, &bosco_ctrl, NULL )

GAMECX(1981, galaga,   0,       galaga,  galaga,   galaga,  ROT90, "Namco", "Galaga (Namco rev. B)", GAME_IMPERFECT_GRAPHICS, &galaga_ctrl, NULL )
GAMECX(1981, galagao,  galaga,  galaga,  galaga,   galaga,  ROT90, "Namco", "Galaga (Namco)", GAME_IMPERFECT_GRAPHICS, &galaga_ctrl, NULL )
GAMECX(1981, galagamw, galaga,  galaga,  galagamw, galaga,  ROT90, "Namco (Midway license)", "Galaga (Midway set 1)", GAME_IMPERFECT_GRAPHICS, &galaga_ctrl, NULL )
GAMECX(1981, galagamk, galaga,  galaga,  galaga,   galaga,  ROT90, "Namco (Midway license)", "Galaga (Midway set 2)", GAME_IMPERFECT_GRAPHICS, &galaga_ctrl, NULL )
GAMECX(1981, galagamf, galaga,  galaga,  galaga,   galaga,  ROT90, "Namco (Midway license)", "Galaga (Midway set 1 with fast shoot hack)", GAME_IMPERFECT_GRAPHICS, &galaga_ctrl, NULL )

GAMECX(1982, gallag,   galaga,  galagab, galaga,   galaga,  ROT90, "bootleg",       "Gallag",        GAME_IMPERFECT_GRAPHICS, &galaga_ctrl, NULL )
GAMECX(1984, gatsbee,  galaga,  galagab, galaga,   gatsbee, ROT90, "hack (Uchida)", "Gatsbee",       GAME_IMPERFECT_GRAPHICS, &galaga_ctrl, NULL )
GAMECX(1981, nebulbee, galaga,  galaga,  galaga,   galaga,  ROT90, "bootleg",       "Nebulous Bee",  GAME_IMPERFECT_GRAPHICS, &galaga_ctrl, NULL )

GAMEC( 1982, xevious,  0,       xevious, xevious,  xevious, ROT90, "Namco", "Xevious (Namco)", &xevious_ctrl, NULL )
GAMEC( 1982, xeviousa, xevious, xevious, xeviousa, xevious, ROT90, "Namco (Atari license)", "Xevious (Atari set 1)", &xevious_ctrl, NULL )
GAMEC( 1982, xeviousb, xevious, xevious, xeviousb, xevious, ROT90, "Namco (Atari license)", "Xevious (Atari set 2)", &xevious_ctrl, NULL )
GAMEC( 1982, xeviousc, xevious, xevious, xeviousa, xevious, ROT90, "Namco (Atari license)", "Xevious (Atari set 3)", &xevious_ctrl, NULL )
GAMEC( 1982, xevios,   xevious, xevious, xevious,  xevios,  ROT90, "bootleg", "Xevios", &xevious_ctrl, NULL )
GAMEC( 1982, battles,  xevious, battles, xevious,  battles, ROT90, "bootleg", "Battles", &xevious_ctrl, NULL )
GAMEC( 1984, sxevious, xevious, xevious, sxevious, xevious, ROT90, "Namco", "Super Xevious", &xevious_ctrl, NULL )

GAMEC( 1982, digdug,   0,       digdug,  digdug,   0,       ROT90, "Namco", "Dig Dug (rev 2)", &digdug_ctrl, NULL )
GAMEC( 1982, digdugb,  digdug,  digdug,  digdug,   0,       ROT90, "Namco", "Dig Dug (rev 1)", &digdug_ctrl, NULL )
GAMEC( 1982, digdugat, digdug,  digdug,  digdug,   0,       ROT90, "Namco (Atari license)", "Dig Dug (Atari, rev 2)", &digdug_ctrl, NULL )
GAMEC( 1982, digduga1, digdug,  digdug,  digdug,   0,       ROT90, "Namco (Atari license)", "Dig Dug (Atari, rev 1)", &digdug_ctrl, NULL )
GAMEC( 1982, dzigzag,  digdug,  dzigzag, digdug,   0,       ROT90, "bootleg", "Zig Zag (Dig Dug hardware)", &digdug_ctrl, NULL )

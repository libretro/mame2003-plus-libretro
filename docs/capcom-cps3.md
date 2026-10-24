# Capcom CPS-3: how the driver works and where it differs from the hardware

Sets: `redearth`, `warzard`, `sfiii`, `sfiii2`, `jojo`, `sfiii3`, `jojoba`
and their clones (53 in all, MAME's list without the multi-game bootlegs),
plus `sfiii4n`, the "4rd Arrange Edition 2013" hack of 3rd Strike that
FBNeo has and MAME does not.

Files: `src/drivers/cps3.c` (memory map, flash SIMMs, video, DMA, sets),
`src/sndhrdw/cps3_sndhrdw.c` (the 16 voice sample chip). A C89 port of
current MAME's `cps3.cpp` and `cps3_a.cpp`.

## The board

- A custom Hitachi SH-2 at 25 MHz that decrypts what it reads from the BIOS,
  the program SIMMs and its cache RAM, with a per-game key stored in the
  cartridge.
- The game in flash SIMMs: two SIMMs of 4 Fujitsu 29F016A chips (2 MB each)
  for the program, up to four SIMMs of 8 chips for graphics and samples.
- 2D video: sprites with zoom, tilemaps drawn as sprites, a text layer,
  palette DMA with fades, character DMA with RLE decompression.
- 16 voices of 8-bit samples read from the graphics SIMMs.
- A SCSI CD-ROM drive: the BIOS writes the game from the CD into the SIMMs.

## No CD drive

The CD drive is not emulated. All sets run from SIMMs already written:

- The "NO CD" sets come with their SIMMs and a BIOS that does not look for a
  drive, as in MAME.
- The CD sets (Europe, USA, Japan, Asia, Hispanic versions) get the SIMM data
  of their version, the same as the NO CD set of that date (the SIMM
  contents do not depend on the region, the BIOS does). As in FBNeo, which
  plays them the same way. MAME needs the CD image instead.
- The CD BIOSes would stop on the missing drive. The BIOS has a flag the
  no-CD BIOSes have set (byte 0x1fecc, bit 0); the driver sets it, and the
  game runs from the SIMMs.
- The Red Earth / Warzard CD BIOSes have no such flag: the driver starts the
  game from the SIMMs as the BIOS would (PC and SP from the program's vector
  table, VBR on it), as FBNeo does.

The flash chips answer the Fujitsu command set (ID, program, erase), but
what is written to them is not saved: no set writes them while playing.

## Region

The region is not a switch on the board: it is a byte in the cart's flash
(the low nibble of the BIOS byte 0x1fecb, 0x1fedb on Red Earth / Warzard),
which the BIOS and the games read. A fake DIP switch, "Region", patches it at
reset, as in FBNeo: Japan, Asia, Europe, USA, Hispanic, Brazil, Oceania or
the second Asia code, or "As BIOS" to keep the dumped value. The default is
Europe for every set, whatever region its BIOS has; 2nd Impact, which had no
European release, defaults to USA. It changes the boot screen colour, the
warnings, the coin settings, the language and the title (Warzard in Japan,
Red Earth elsewhere). Change it, then reset.

The game options (coins, difficulty, monitor flip, language...) are in the
games' own test menu (F2) and kept in the cart's EEPROM, saved as NVRAM.

## Encryption

The BIOS is decrypted at load. The program SIMMs are kept as dumped and also
decrypted into a copy the CPU fetches opcodes from; data reads come out
decrypted, as the custom CPU reads them. Two hooks in the SH-2 core (both
optional, unused by the other SH-2 drivers) do this:

- `opcode_read`: the opcode fetch, from the BIOS, the decrypted program copy
  or the decrypted copy of the cache RAM.
- `dma_read_hook`: the SH-2 DMA bypasses the decryption on the real board;
  the hook re-encrypts what the handlers decrypted.

`sfiii2` runs off a "dead" cartridge: the key is gone and its program is
stored with code encrypted and data not, as in MAME.

## Video

The renderer is MAME's: sprites and tilemaps go to a render buffer of
palette indexes (with the shadow/highlight blend bits), which is copied to the
screen through the full screen zoom; the text layer is drawn on top. Tiles
are read straight from the character RAM (no gfx elements to keep dirty).
Colours are 15-bit with no extension of the low bits, as on the board (its
video DAC has them grounded). The screen width follows the CRTC registers, as
in MAME (384 pixels in the games checked).

## Idle loop

Each game waits for the vblank interrupt by reading a word of the main RAM
in a loop. When the loop reads it, the CPU skips to the next event (the
addresses are FBNeo's, one per game and version). Speed goes from about
4.5x to 12-16x real time on an x86 core. The games run the same; only the
attract mode can pick other fighters, since the games draw random numbers
from the time spent in that loop.

## Sound

MAME's sound device: 16 voices, signed 8-bit samples, the board clock / 3 /
384 (37.3 kHz), loop and end addresses, a volume per side. Levels checked
against MAME's WAV output of the same attract mode.

## ROM loading of merged sets

Merged sets keep the files of the clones in subfolders of the parent's zip,
with the same names (`sfiii3n/sfiii3-simm1.0`, `sfiii3nr1/sfiii3-simm1.0`).
The ROM loader of this core matched by name only and took the first one.
It now looks for the expected CRC first, then for the name, so merged,
split and non-merged sets all load.

## Checked

- Attract mode of `sfiii3n`, `jojoba`, `sfiii2n`, `jojon`, `sfiiin`,
  `redearthn` against MAME 0.264 at the same frames: the same screens.
- A game of 3rd Strike (coin, character select, fight).
- CD sets: the regional warning screens, then the game (`sfiii3`, `sfiii3u`,
  `sfiiih`, `sfiii2`, `jojor2`, `jojobaner2`, `warzard`).
- Save states: loaded in a new process, the game goes on frame for frame.
- Big-endian: the PowerPC build renders as x86.

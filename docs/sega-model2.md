# Sega Model 2: choices that differ from the hardware

The Model 2 driver (`src/drivers/model2.c`) emulates the boards as the games
see them. In a few places the hardware is not available or not emulated, and
the driver does something simpler. Each of those choices is listed here, with
the reason and its consequences.

## Communication board (network link)

The communication board links cabinets for multi-player games over a network.
It is not emulated: there is no network.

For most games the board reads as **absent** (the CN register reads 0xff).
The games that look for it then run as single machines.

### Virtual-On: board present, link always stand-alone

Sets: `von`, `vonu`, `vonj`.

**Why.** Virtual-On does not start a game without the board, even on a
machine set to stand-alone. At boot (addresses from `von`):

1. `0x185d0` sets the defaults: role stand-alone, link mode 0.
2. `0x18920` reads the link ID from the backup RAM (byte `0x1d00028`:
   0 slave, 1 master, other values stand-alone; the factory setting is 2).
3. `0xc6480` initialises the link. It reads the board's CN register first.
   Without a board it prints "THERE IS NO COMMUNICATION BOARD!", sets the
   link mode to 0xff and returns, without looking at the link ID.

With link mode 0xff the game never leaves the attract mode: after a coin the
screen stays black and silent (game mode 4, sub-state 0 waits on a status
word that stays 0xff).

With a board present, step 3 enables it (CN=1) and waits for the board's
firmware to report ready (ZFG=1, read as bit 7 of the FG register going to
0). With the link ID on stand-alone it then returns at once and the game never
uses the link. A board that is present but never ready stops the game at boot;
this is also what the current MAME device does without its network
simulation, and MAME lists these sets as not working.

**What the driver does.**

- The board reads as present and reports ready as soon as the game enables
  it (`m2comm_ctrl_w`, CN=1 sets ZFG=1). Its shared RAM is plain RAM.
- The link ID always reads as stand-alone (`von_linkid_r`, installed over the
  backup RAM dword at `0x1d00028`). Writes still go to the backup RAM.

**Consequences.**

- Master and slave are not available. The test menu always shows
  stand-alone, whatever is chosen there.
- The game computes the backup RAM checksum by reading through the same
  handler, so the checksum stays valid and the other settings are kept.
- An NVRAM from another emulator with the link set to master or slave fails
  the checksum once: the game then restores its factory settings.

**How it was checked.** A valid NVRAM with the link ID on slave (checksum
recomputed with the game's own CRC, table at `0x2f20` in `von`) stops at boot
on "THIS IS SLAVE SITE" without the override. With the override, the same
NVRAM boots, plays a game, and keeps its other settings.

**Not added: `vonr` (Virtual-On Relay).** That version always sets its role
to relay (`0xf1cf0` writes 0) before the link is initialised, and then waits
for the network. It cannot run as a single machine.

### Daytona USA: factory link ID forced to single at start-up

Sets: `daytona`, `daytonase`, `daytona93`, `daytonas`.

**Why.** Daytona USA runs without the board, but its factory settings in the
program ROM set the link ID to master. On a new NVRAM (or after a factory
reset) the game uses those settings and keeps looking for the other cabinets
("NETWORK CHECKING"), so a game never starts.

**What the driver does.** When the game is loaded, `DRIVER_INIT( daytona )`
finds the factory settings table in the program ROM (it starts with
`SEGA@\x82`) and changes the link ID at offset 0x0b from 1 (master) to
0 (single). The board reads as absent.

| Set | Table | Factory link ID | Patched |
|---|---|---|---|
| `daytona` | `0x39d60` | 1 (master) | yes |
| `daytonase` | `0x3ab20` | 1 (master) | yes |
| `daytonas` | `0x3c570` | 1 (master) | yes |
| `daytona93` | `0x3c150` | 0 (single) | no, already single |

**Consequences.**

- Only the factory value changes. A link ID already saved in the NVRAM is
  kept, and the setting can still be changed in the test menu. Set to master
  or slave, the game waits for the network as on the real hardware; set it
  back to single to play.
- Restoring the factory settings from the test menu gives single, not
  master.

### Motor Raid

Sets: `motoraid`.

The game has link code ("Network Check"), but with the board absent it
starts and plays with its factory settings. Nothing is changed.

## Model 1 I/O board (original Model 2)

Sets: `daytona` and clones, `vcop`, `vcopa`.

The original Model 2 reads its inputs through a Model 1 I/O board: a Z80
with its own BIOS, behind a dual-port RAM at `0x1c00000`.

- With the BIOS (`model1io.zip` for Daytona USA, `model1io2.zip` for Virtua
  Cop), the board runs its own firmware on an emulated Z80.
- Without it, the board's firmware is simulated: the driver writes the
  inputs, the analog channels and the light guns where the games read them,
  and handles the backup commands. Both BIOS files are optional ROMs.

## INDY 500 and Over Rev (Model 2B, `indy500` machine)

Sets: `indy500`, `indy500d`, `indy500to`, `overrevb`, `overrevba`.

- **Drive board (force feedback).** The games write their commands to it
  through port E of the 315-5649 and never wait for an answer. It is not
  emulated: the writes are ignored, as in MAME when no drive board program
  is loaded.
- **Communication board program.** The Over Rev sets list the communication
  board program (`epr-18643.7`) as in MAME. Its CPU is not run, so the ROM
  is optional: the sets load with or without it.
- **Over Rev parent.** As in MAME, the parent `overrev` is the Model 2C
  version (see the Model 2C sections), so `overrevb` and `overrevba` load
  from `overrev.zip`. The I/O board BIOS entries (`model1io`, `model1io2`),
  like the other BIOS sets of the core, are not in the game list: loading
  their zip reports that there is no such game instead of starting a
  machine without a program.
- **Inverted pedals.** Over Rev reads its pedals inverted (0xff released), as
  `PORT_REVERSE` in MAME. The driver inverts those channels itself.

Super GT 24h (`sgt24h`) runs on the same machine. Its pedals are read
inverted too, and it uses the same driver init as Over Rev.

## Sound link (MIDI input of the SCSP)

On Model 2A and later the i960 sends its sound commands through a UART to the
MIDI input of the SCSP. The driver sends a byte only when the SCSP input
queue has room, so no command is lost; while the queue is full the UART
reports itself busy and the game waits.

The SCSP input queue holds 32 bytes, as in current MAME (the original MAME
0.78 code had 8). With 8 bytes Super GT 24h hung at the end of its boot: the
sound program does not read the queue for a while, and the game sends sound
commands from its vertical blank interrupt, so it waited there for several
frames. Its frame timer (same interrupt priority) expired meanwhile; the
i960 records such an interrupt in its interrupt table, which this game keeps
in ROM, so the interrupt was lost and the main loop waited for the timer
forever (black screen after "STAND ALONE"). With 32 bytes the commands fit
and the game plays.

## Rail Chase 2: mirrored sample ROMs

Sets: `rchase2`.

The sound board takes either four 16 Mbit sample ROMs (IC32, IC33, IC34,
IC35) or two 32 Mbit ones (IC32, IC34). Rail Chase 2 has two 16 Mbit ROMs in
the 32 Mbit positions IC32 and IC34. A 16 Mbit ROM there ignores the top
address line, so it shows up twice: the region at 0x200000 mirrors IC32 and
the one at 0x600000 mirrors IC34.

The sound program streams some effects through the 68000 into ring buffers
in sound RAM, reading them from those mirrors (the title screen animation
reads its stream from 0xa1e97c, inside the mirror of IC32). MAME loads the
ROMs at 0x000000 and 0x400000 and leaves the mirrors empty, so those effects
play silence: the title animation is silent and some in-game effects are
missing, as reported for MAME (mametesters 09393).

The ROM definition loads each ROM a second time into its mirror (`ROM_RELOAD`), as the same fix proposed to MAME.

**How it was checked.** Current MAME (built from source, October 2026), with
no input, plays nothing during the title animation (22-30 s, 56-62 s and
90-96 s of the attract mode), as this core did before the change. With the
same mirroring added to MAME's `init_rchase2`, MAME plays the title
animation sound at the same points as this core. The billboard ROM
(`segabill`), not dumped here, was an empty placeholder in that MAME test; it
drives the score display only. With it the title
animation has its sound and the in-game sound is about a third louder (the
effects that were missing). The video is unchanged. The other Model 2 sets
fill every ROM position they use, or (`dynabb`) use the four-ROM layout, where
an empty socket is not a mirror.

## Sound: the SCSP effect DSP

Sets: every SCSP game that loads a DSP program (Dead or Alive among them).

The SCSP has a small DSP for effects (reverb, echo). The slots send their
output to it (ISEL, IMXL), it runs its microprogram once per sample with its
delay lines in the sound RAM (RBP, RBL), and its 16 outputs are mixed with
the EFSDL and EFPAN of slots 0-15. The SCSP of this core did not emulate it,
so those effects were missing, and the sounds a game sends only to the DSP
were silent. The DSP is now the one of current MAME (`src/sound/scspdsp.c`),
compared with MAME's on the Dead or Alive program and on random programs:
same outputs, registers and delay RAM, sample by sample.

**Dead or Alive and the MADRS registers.** The DSP reads its delay line
addresses from the MADRS registers (`0x780-0x7bf`). MAME also writes the
range `0x7c0-0x7ff` to MADRS, as a mirror. Dead or Alive loads MADRS and then
clears `0x7c0-0x7ff`, so in MAME every delay line address becomes 0: the
reverb feeds back into one word, its output drifts to a DC level and
saturates, and the sound breaks up and then goes silent (listed in MAME's
known issues for `doa`). The mirror is not from the hardware: it replaced an
out-of-bounds write in 2014, and Mednafen (checked on Saturn hardware) maps
MADRS at `0x780-0x7bf` only. Here `0x7c0-0x7ff` is ignored and reads as 0.

**Master volume.** The SCSP master volume (MVOL) now scales the output once
the game has written it, as in MAME. Dead or Alive keeps it at 0 for the
first seconds while its DSP starts on an uninitialised delay RAM; without
MVOL that start was audible as a burst of noise. The games that use 15 (the
maximum) sound exactly as before.

**How it was checked.** MAME with the mirror removed plays the Dead or Alive
attract mode for over 11 minutes without the DC drift (it saturated within
the first minute before). In this core the games that do not start the DSP
produce exactly the same audio as before.

## Video: tile layers and polygons

The tile layers (System 24 tilemaps A and B) are mixed among themselves
first, A over B whatever their priority bits; the priority bit of the
pixel that wins then puts it behind or in front of the polygons. MAME draws
each priority on its own (the low tiles of A and B, the polygons, the high
tiles of A and B), so a high tile of B covers a low tile of A: on the
Virtua Cop 2 stage select the city map (tilemap B, priority set) hid
"SHOOT TO SELECT" (tilemap A), as listed in MAME's known issues for
`vcop2`. The other Model 2 games checked look the same as before.

## Video: black polygon pixels

The polygons are drawn into their own buffer, then laid over the tile
layers. MAME's buffer holds `rgb_t` pixels, whose alpha byte makes a drawn
black pixel (0xff000000) different from an empty one (0). This core stored
plain RGB and took every black pixel as empty, so wherever a polygon was
pure black the layer behind showed through: blue car shadows and missing
dust in Sega Rally, blue tyres and shadows in INDY 500 and Super GT, the
sky behind black clothes in Dead or Alive, the attract backgrounds of Motor
Raid. Drawn pixels now carry the same opaque marker in the top byte. All the
Model 2 games were compared before and after; the changed pixels are the
black ones, and they now match MAME.

## Sega Rally Championship (Model 2A)

Sets: `srallyc`, `srallycb`, `srallycc`, `srallycdx`, `srallycdxa`.

- **Gear lever.** The H-pattern lever is read on port C bits 4-6. As for
  Daytona USA it is built from two buttons, shift up and shift down.
- **Drive board.** The force feedback board is written to through port E and
  never answers; it is not emulated. Its program (`epr-17891`) and the
  communication board program (`epr-16726`) are not listed.
- **Start grid.** MAME lists the initial placement of a few opponent cars as
  slightly wrong, depending on the i960 clock. Here, with the i960 at half
  and at double its clock, only one of the 8 start grid objects moves, by
  about 0.3 units (it is still settling when the race starts). Compared with
  a video of a real Model 2A-CRX, aligned on the race clock (chase view,
  full throttle): same speed (220 km/h at 0'04"0), same course, and the
  first opponent appears at the end of the village at 0'05"0 in both, at the
  same distance up to 0'06"0. Nothing is changed.

## Manx TT Superbike (Model 2A)

Sets: `manxtt` (DX/Twin, Revision D), `manxttc` (DX/Twin, Revision C),
`manxttdx` (DX).

- **Cabinet type.** The MAME sets include an EEPROM image that sets the
  cabinet type: DX for `manxtt`, Twin for `manxttc`. A new NVRAM starts from
  it (`REGION_PROMS`, 16-bit little-endian as in MAME). The type can still be
  changed in the test menu.
- **Motion slider (DX).** The DX bike moves; its drive board takes commands
  on port E of the 315-5649 and answers on port D. At boot the game shows
  "ACTIVE MOTION SLIDER / NOW CHECKING SYSTEM" (code at `0x5f580` in
  `manxtt`): after command `0x3d` it waits up to 846 frames for status
  `0xce`, sends the bike center value, then waits for `0xcd`. Without an
  answer it shows "Error! ... Not Respond" and never starts; MAME stops
  there (its known issue "no escape from active motion slider tutorial").
  The board is simulated with those two answers: `0xce` while the command is
  `0x3d`, `0xcd` otherwise. The board's error codes (`0xb0`-`0xb7`: first
  setting, encoder, overcurrent, overheat, bank volume, ROM, RAM, centering)
  are never sent. The bike does not move, of course.
- **Model 1 sound board (DX).** The DX cabinet has a Model 1 sound board
  (68000, YM3438, two MultiPCM) next to the SCSP, for extra sounds. It runs as
  a fourth CPU (`REGION_CPU4`, samples in `REGION_SOUND2`/`REGION_SOUND3`)
  and gets the same sound commands from the i960 UART, as in MAME. Its replies
  are not passed back: the i960 reads the SCSP's.
- **Communication board.** Its program (`epr-18643a`) is not listed; the
  board is not emulated and the game runs as a single machine.

## Sky Target (Model 2A)

Sets: `skytargt`.

The stick is on analog channels 0 (Y) and 2 (X, reversed), as in MAME.
Machine gun, missile and view change are buttons 1-3.

## Air Walkers (Model 2A)

Sets: `airwlkrs`.

Up to four players on two input ports: port F bit 7 selects players 1-2 or
3-4 on ports C and D, and the matching start buttons on IN0 bits 4-5, as in
MAME.

## Hanguk Pro Yagu 98 (Model 2A)

Sets: `hpyagu98`.

The game stops with "Error #1" unless the NVRAM holds what its boot code
checks: EEPROM words 4-5, the string "98KOREA PRO B.B." at the start of the
backup RAM, a magic dword at `0x398` (`v * 3 + 0x3e82 == 0xf0c891a2`), and the
settings block at `0x10` with its CRC (a copy at `0x1d4` restores it when
only one is valid). MAME lists two hand-made images, EEPROM and backup RAM,
with the settings of Dynamite Baseball 97. A new NVRAM starts from them, as in
MAME (`REGION_PROMS` and `REGION_GFX1`); older sets without these two files
stop on "Error #1".

## Desert Tank (original Model 2)

Sets: `desert`.

Model 1 I/O board as Daytona USA (its BIOS runs when `model1io.zip` is
present). Steering, accelerator and brake on the I/O board analog inputs,
the shift lever as a toggle, as in MAME.

## Model 2C: the TGPx4 (MB86235)

The Model 2C boards replace the TGP with a Fujitsu MB86235 "TGPx4". Its
interpreter comes from current MAME (`src/cpu/mb86235`, in C89), with the
program memory, the external bus (buffer RAM at `0x400000`, the copro data
ROM at `0x800000`) and the two FIFOs as driver callbacks. As in MAME, the
CPU waits in halt until the i960 has sent its program, and a read from the
empty input FIFO repeats the instruction once the i960 pushes a value. Two
places differ from MAME, where its code is plainly a slip:

- **FEA/FES** (exponent add/subtract) keep the sign and the mantissa and
  change only the exponent field; MAME keeps only the exponent field.
- **EBL** (the low 14 bits of the external base register) is written as the
  low bits; MAME shifts the value up as for EBU.

None of the games checked showed a difference from either change.

The 2C games run faster than real time here, where MAME runs them at about
70%.

## Video: polygon clipping

A polygon is clipped against four planes, and a convex one gains at most one
vertex for each, so MAME's buffers hold 8. A bad polygon (self-crossing, or
with NaN coordinates) can double its vertices at each plane: Top Skater's
models drawn with the standard object format overflowed the buffers and
crashed the core. The buffers now hold 64 vertices and only polygons that
still fit in 8 are kept.

## Top Skater (Model 2C)

Sets: `topskatr`, `topskatru`, `topskatruo`, `topskatrj`.

The game uploads its own geometrizer program (MAME's notes: "a slightly
different geometry code"), and MAME shows no 3D. Its object data command
(`0x11`, the second bank of command numbers) reads a compact polygon list:
the attribute word, then only the points, three words for a triangle and
six for a quad. The standard format (no normals) also has three unused
normal words and, for triangles, an unused point. Read with the standard
format, every polygon after the first was garbage. The driver reads the
compact format for command `0x11` in this game only; the skaters, the
board, the ramps and the crowd are all drawn. The command word also carries the
center select in bits 6-7, as the rasterizer command does: the speed meter
is drawn around the window's second vanishing point, in the bottom left
corner. Compared with a video of the arcade game: the course, the HUD and
the speed meter are in the same places.

The program's command `0x1b` loads a second matrix and `0x1c` copies the
first matrix's translation into it; a vertex whose x has the lowest bit set
goes through the second matrix (the skinned joints of the skaters). MAME
pull request #16444 (jkind73, open) found the compact format and the second
matrix too.

The board has two foot sensors (front and tail), on while a foot stands on
it. MAME reads them active low, so with no input both feet are off the
board: the skater always jumps, floats 16 units above the ground with the
camera close behind, and its shadow (which the game puts on the ground
under the light) is far below, off screen. Here they are on at rest and a
button lifts a foot. Compared with a video of the arcade game at the same
race time, the skater now rides crouched, the camera is at the same
distance and the shadow is under the board. The attract demos, played by
the game, matched the video before already (shadows included).

The analog inputs are curving (channel 0, stick X) and slide (channel 1);
MAME puts both on the same axis, here slide is on stick Y so a pad can
reach both.

The music is on the DSB2 MPEG board (see the music board section).

## Sega Ski Super G (Model 2C)

Sets: `skisuprg`.

The drive board answers on port D to the commands on port E. At boot the
game shows "CHECK DRIVE BOARD" (code at `0x88b10`): it clears port E and
waits for `0xce` (ready), sends the inclining center and waits for `0xff`
(taken), within 479 frames; without an answer it stops on "DRIVE BOARD
TROUBLE CODE: FF", as in MAME. In game, `0xb0`-`0xb7` are board errors
(init, encoder, overload, overheat, bank volume, ROM...). The board is
simulated: `0xce` while port E is 0, `0xff` otherwise. The game then runs
through the World Series and the races.

Swing is on analog channel 0 (stick Y), inclining on channel 1 (stick X),
the foot sensors on port D are not read (port D is the drive board).

## Sega Touring Car Championship and Top Skater: music board (DSB)

Sets: `stcc`, `stccb`, `stcca`, `stcco` (DSB, Z80) and the Top Skater sets
(DSB2, 68000).

Their music is MPEG audio (layer II, 32 kHz stereo) on a Digital Sound
Board, from current MAME (`dsbz80.cpp`, `dsb2.cpp` and the decoder
`mpeg_audio.cpp`, in C89: `src/sndhrdw/segadsb.c`, `src/sound/mpeg_audio.c`).
The board's CPU gets the game's sound commands on its i8251, from the same
line as the SCSP (each byte the i960 sends goes to both), and drives the
decoder: start and end addresses, play, loop, stop, volume and stereo mode
for the DSB; a command FIFO and a 1 kHz timer interrupt for the DSB2. The
decoded audio is its own stream, mixed 9 dB under the SCSP's full level
(MAME mixes both at full level). The level was measured on two recordings
of Top Skater cabinets: with the music and the sound effects of the core as
separate tracks of the same run, aligned on the attract mode, the
recordings came out with the music 9 dB lower against the effects (within
1 dB, both recordings); at full level the music also clipped. The decoded
music itself matches the recordings (spectral correlation 0.9-0.97). The
recordings play the music about 0.45% faster than the core's 32 kHz
(emulated time); not changed, it may be the recordings. Touring Car's DSB
uses the same level (no recording checked).

The decoder never reads past the MPEG ROM (MAME guards this on the DSB2
only). The drive board and the communication board programs are not
listed.

## The House of the Dead (Model 2C)

Sets: `hotd`, `hotdo`.

The guns work as in Virtua Cop 2 (the 315-5649 serial channel), with the
ranges of MAME's `hotd` ports. MAME's prototype `hotdp` is not listed: one
of its texture ROMs was never dumped.

## Other Model 2C sets

`waverunr`, `bel`, `segawski`, `dynamcopc` and `overrev` (now the Model 2C
parent of the Model 2B sets) run with the inputs of MAME.

## Motor Raid - Twin/DX (Model 2A)

Sets: `motoraiddx`.

The DX cabinet has a moving bike, run by a drive board on the 315-5649
ports: the game writes commands to port E (inverted) and reads the board's
status on port D. The drive board's program is not dumped and MAME lists the
set as not working: the game stops at "DRIVEBOARD NOT RESPONSE ERROR".

The board is simulated, as for Manx TT DX. From the cabinet check (code
copied to RAM at `0x2158d0`, ROM `0xc58d0`):

- port D reads `0xcd` when the board is ready; the check then skips the
  initialisation and centring steps (which wait for `0xcf` and `0xce`);
- the wire test sends commands `0xe0`-`0xef` and expects bit `n & 7`
  cleared (table at ROM `0xc5720`); the simulation answers the same.

The bike does not move, the game runs as on a working cabinet.

## Rail Chase 2 (`rchase2a`)

A second program for Rail Chase 2 that reads the guns the other way round
(maybe a factory conversion). The guns are centred at `0x80` over the full
range, as MAME's `rchase2a` ports.

## Daytona USA hacks

Sets: `daytonat`, `daytonata` (Turbo hack), `daytonam` (To The MAXX),
`daytonagtx` (GTX 2004 Edition). Hacks by Kyle Hodgetts, listed by MAME as
not working; here they play.

- **To The MAXX**: the kit has a PIC 16F84 in socket IC15. It answers at
  `0x240000`, where the third program ROM (`maxx.14`) is seen. As MAME:
  16-bit reads of the first 32 bytes step a 4-bit counter and return the
  PIC's answer.
- **GTX 2004**: a banked ROM over the data ROMs at `0x2c00000`. Reads of
  some bytes switch the bank, as MAME (`0xff00c` and `0xf0003` bank 0,
  `0xff000` bank 1, `0xffffc` the empty bank 2).

## Not added

- **Royal Ascot II** (`rascot2`): a horse racing terminal made for the
  SegaNet network. It waits for the network ("SegaNet Wait Status" in MAME)
  and never starts a race as a single machine.

- **Power Sled** (`powsled`, `powsledr`, `powsledm`): a network of
  cabinets run by a Main unit, whose program was never dumped (MAME's
  `powsledm` is a hack of the Relay program). Every version waits on the
  network ("NETWORK: NG") in MAME, and would need the whole network to be
  simulated.

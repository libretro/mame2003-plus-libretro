# Sega Model 1: how the driver works and where it differs from the hardware

Sets: `vr`, `vformula`, `vf`, `swa`, `swaj`, `wingwar`, `wingwaru`,
`wingwarj`, `wingwar360`, `netmerc`.

Files: `src/drivers/model1.c` (boards, maps, ROMs), `src/machine/model1_machine.c`
(the TGP), `src/vidhrdw/model1_vidhrdw.c` (the renderer, ported from current
MAME), `src/sndhrdw/segadsb.c` (the Digital Sound Board of Star Wars Arcade).

## The TGP (geometry coprocessor)

The TGP is a Fujitsu MB86233 DSP running a program from a 315-xxxx ROM:
315-5573 (Virtua Racing, Virtua Formula), 315-5724 (Virtua Fighter), 315-5711
(Star Wars Arcade, Wing War, NetMerc). The V60 sends it commands (a function
number and its arguments) through a FIFO and reads the results back.

The driver runs the TGP functions in C (high level emulation, HLE), started
from MAME 0.215, the last MAME with an HLE TGP. In MAME 0.215 Virtua Racing had
no HLE at all and Star Wars Arcade and Wing War did not work.

### Checked against the real program

The core also contains the MB86233 (from the Model 2 work), so the real TGP
program can run next to the HLE. A development build (`-DMODEL1_TGP_DEV`)
reads `MODEL1_TGP` from the environment:

- `lle`: the game runs on the real TGP program only.
- `oracle`: the game runs on the real TGP program, the HLE gets the same
  input, and every call is compared: function, arguments, the results of
  both. The pairing follows the input stream (a call ends when the DSP takes
  the next function word), so a function giving a wrong number of results
  does not shift the others. The TGP RAM both sides keep is compared every
  frame. Every 600 frames it prints, for each function, the calls that were
  exact, near (rounding), wrong, or gave a wrong number of results.

With this, every function the games call was rewritten from the DSP program
until the results match bit for bit: same table lookups (sine, 1/x, 1/sqrt,
atan come from the TGP's table ROM through its I/O ports, with one
multiply-subtract step), same order of the sums, same comparisons (the ALU's
`-0` is neither negative nor positive). Checked on attract and play of
`vr` (three tracks), `vformula`, `vf`, `swa`, `wingwar`, `wingwar360` and
`netmerc`: no call differs and the TGP RAM stays identical.

Virtua Fighter's program (315-5724) is marked as a bad dump in MAME, but it
runs the game on the MB86233 core, so it is checked the same way. About half
of its functions differ from 315-5573 (matrix stack and saved matrices at
other addresses, the camera, collision bitmaps, ground box test, list walks
in the copro RAM), so it has its own table. MAME 0.215 used the Virtua Racing
table for it, with guessed camera functions (one returned random numbers),
which put the camera far from the fighters. For Virtua Fighter the oracle
also compares the TGP's internal RAM after every call. Over 9000 frames of
attract and fights, 55 functions and 3.8 million calls give the same results
and the same RAM.

The table lookups, the copro RAM (0x2000 words; with address bit 0x40000 set
the address steps by 4) and the list functions follow current MAME.

### Program bugs reproduced

The HLE reproduces what the real programs do, bugs included, because the
games run with them on the real boards:

- 315-5711 `roty` starts its first sum from the DSP's `d` register, which
  still holds the dispatch address (a denormal): the first matrix element
  gets `0x6c` added to it.
- 315-5711 `normalize` never loads y; it uses whatever the scratch word at
  `$71` holds from an earlier function (tracked by the HLE).
- 315-5711 `f47` starts its sum of squares from the z difference.
- 315-5711 `fsqrt` skips the mantissa step and does not return a square root.
- The matrix stack keeps 7 levels; past them the depth only counts (315-5711
  pushes again past 8, while its pops past 7 only count). The matrix slots
  and the 315-5711 accumulator stack sit in the same TGP RAM, as on the
  board.
- 315-5711 `f52` (0x34, matrix inverse; Star Wars Arcade calls it) works
  by cofactors, never reads the first element, ORs two elements where a
  subtract is expected, and adds in the second element the scratch word
  `$11` an earlier function left (push, multiply and read leave a matrix
  element or argument there; tracked by the HLE).
- 315-5711 `f56` (0x38, segment against a polygon list) stops at the first
  polygon hit, not the nearest one.
- The 315-5711 games use the current matrix before setting it: the HLE starts
  from what the TGP RAM holds at power on (zeros in the emulated TGP).

### What is not emulated

- Table entries a program leaves empty (it returns at once) take no
  arguments and give no results; 315-5724 functions no game was seen calling
  are not emulated, except the simple ones (normalize, divide, set the
  translation).
- 315-5711 functions no game was seen calling: self tests 0x04, 0x3d, 0x3e
  (results 0), 0x42 (matrix stack pop and multiply, with bugs overwriting the
  stack), 0x21 (the program jumps into the middle of another function). They
  take and give the right number of words so the command stream stays in step.

### ROMs

The TGP program ROM stays in the sets as in current MAME. For Star Wars
Arcade, Wing War and NetMerc the HLE follows 315-5711 with CRC 6a21f304, as
current MAME. An older dump (CRC c5ddb8fc) differs in two words: the entry
of `f52` (which then reads the wrong RAM and gives NaNs) and the end test
of `f56`. It loads with a checksum warning; the HLE does not use the ROM.

The V60 reaches the TGP in its I/O space as well as in memory, with the
same mirrors as current MAME: Wing War sends its boot command (8, clear the
matrix stack) with an OUT to a mirror of the FIFO, and the real program
waits for it before taking any other command.

## I/O board

The Model 1 I/O board (model1io, model1io2: a Z80 with its own BIOS) is
simulated: inputs, DIP switches, analog channels and the backup are put where
the games read them in the 2 KB dual port RAM:

    00-07  analog channels
    08-0a  IN0 - IN2 (active low)
    0b-0d  DSW1 - DSW3 (active low, all off)
    0e     board switches
    0f     outputs (lamps, coin counters)
    11     drive board / motion base output
    20     command (2 store the backup 0x100-0x1ff, 3 load it), 21 status

The DIP switches must read as off: 0 turned on the debug displays of Virtua
Racing and NetMerc. The backup is saved in the NVRAM file.

## Sound

Model 1 sound board (68000, two MultiPCM, YM3438) behind an i8251 UART at
31250 baud, as current MAME. Star Wars Arcade also has the Digital Sound
Board (Z80 + MPEG audio), which listens to the sound board's UART.

The simulated I/O board answers at once, so the V60 sends its first sound
commands before the 68000 has finished its init. The real UART holds one byte
and would lose the others; Virtua Fighter then stayed silent. The bytes for
the 68000 are kept in a queue.

Virtua Racing and Virtua Fighter play no sound in attract mode with their
default settings, as on the board.

## Save states

Besides the RAMs and registers, a save state keeps what MAME 0.78 does not
save by itself and what a game sets only once: the TGP FIFOs (a state can
fall between the words of a command, or before the V60 read the results),
the data ROM bank, the time left of the interval and UART timers, and the
renderer's view (viewport, light, light parameters: Virtua Fighter sends
them only when a stage starts, and without them a loaded state drew the
fighters black). Checked by loading in a new process: Virtua Fighter and
Virtua Racing's attract mode go on frame for frame as in a straight run
(analog controls: see Open questions).

## Other boards

- Link board (Virtua Racing, Virtua Formula, Wing War): not emulated; the
  games see it with its CPU in reset and run stand-alone.
- R360 motion base (`wingwar360`): the game states it sends are answered as
  the real base does, so a game starts. No motion.
- Polhemus tracker (`netmerc`): the board (a 386 with its own program) that
  tracks the visor is not emulated. As in current MAME, the visor pose the
  game reads at dual port RAM 0x80-0x8b (x, y, z, then three angles in
  radians * 8192) is set once at reset, looking ahead. The gun on its mount
  is not tracked: it uses the stick (analog channels 0 and 2), and moving
  goes where the gun points.
- NetMerc gun mount calibration: the ends of both axes are kept in the
  backup RAM (0x38 / 0x3c horizontal, 0x40 / 0x44 vertical). At boot the
  game sets its factory calibration (0x46-0xc4, 0x21-0x69) only when one of
  them is zero, so a new NetMerc backup RAM is 0-filled (the other games
  get 1s, which Virtua Fighter needs): with 1s it kept them, divided by
  zero, spun the gun, failed its checksum and stayed on one attract
  screen. The stick covers the same range as the factory calibration.
- NetMerc sound: `mpr-18134` (the first sample ROM of the first MultiPCM)
  is a bad dump, all 0xff, in the only set known (MAME has the same one).
  That MultiPCM has no sample table, so the music it plays is silent; the
  effects, on the second MultiPCM, play.
- NetMerc defaults (test menu, GAME ASSIGNMENTS): country Japan, attract
  sound off, so attract mode is silent and starts with the Japan warning.

## Video

`model1_vidhrdw.c` is a C port of current MAME's `model1_v.cpp`. Two choices
keep the output the same on every CPU: float to int conversions follow x86
(out of range gives `INT_MIN`, other CPUs saturate), and the files of the TGP
and the renderer are compiled without fused multiply-add (GCC and clang fuse
`a*b+c` on PowerPC and ARM64). Big-endian PowerPC renders pixel for pixel as
x86.

Polygons with the moire flag (Virtua Fighter's shadows) are drawn as a
checkerboard, every other pixel, as in current MAME; a 720p capture of a
real Virtua Fighter board shows the same pattern.

Compared with captures of real boards: Virtua Fighter (the stages up to
Akira's: backgrounds, floors, colours, lighting, camera, shadows), Virtua
Racing (Bay Bridge: start grid, bridge, HUD), Star Wars Arcade (attract:
Star Destroyer, cockpit battle, hangar), Wing War and NetMerc.

## Open questions

- Virtua Fighter: the eight stages up to Akira's were compared with a
  capture of a real board (backgrounds, floors, colours, camera); Dural's
  stage and the bonus stage were not.
- After loading a state during play, an analog control (Virtua Racing's
  accelerator) starts again from its rest position: the core's input
  system does not save it. Digital inputs and attract mode go on as in a
  straight run.

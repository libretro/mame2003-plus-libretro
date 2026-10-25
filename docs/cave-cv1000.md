# Cave CV1000: how the driver works and where it differs from MAME

Sets (MAME's list without the touch screen mahjong game `mmmbanc`):

| Board | Sets |
|---|---|
| CV1000-B (8 MB RAM) | `mushisam`, `mushisama`, `mushisamb`, `ibara`, `ibarao`, `ibarablk`, `ibarablka`, `espgal2`, `espgal2a`, `espgal2b`, `mushitam`, `mushitama`, `pinkswts`, `pinkswtsa`, `pinkswtsb`, `pinkswtsx`, `pinkswtssc`, `futari15`, `futari15a`, `futari10`, `futaribl`, `futariblj`, `mmpork`, `deathsml` |
| CV1000-D (16 MB RAM) | `dsmbl`, `ddpdfk`, `ddpdfk10`, `dfkbl` |

`futaribl` is the World / English set; the others are Japanese.

Files:

- `src/cpu/sh3/sh3.c`: the SH-3 (SH7709S) CPU and its peripherals.
- `src/cpu/sh3/sh3jit.c`: the SH-3 recompiler, included by `sh3.c`.
- `src/drivers/cv1k.c`: memory maps, NAND flash, RTC9701, inputs, sets.
- `src/vidhrdw/cv1k_vidhrdw.c`: the blitter (Cave's EPIC12 FPGA).
- `src/sound/ymz770.c`: the Yamaha YMZ770 ("AMM"), on `src/sound/mpeg_audio.c`.

All are C89 ports of current MAME (`sh.cpp`, `sh3comn.cpp`, `sh4*.cpp`,
`cv1k.cpp`, `epic12*.cpp`, `ymz770.cpp`).

## The board

- Hitachi SH7709S, an SH-3 at 102.4 MHz, big-endian, no FPU. The boot code
  copies the game from the NAND flash to work RAM and runs it from there.
- The blitter: copies sprites from an 8192 x 4096 VRAM to the screen with
  flips, tint, transparency and 8 source and 8 destination blend modes, and
  uploads graphics to VRAM. The games wait on it.
- The YMZ770: 16 channels of compressed audio from two 4 MB flash chips,
  and a sequencer.
- 128 MB NAND flash with the graphics and data, a NOR flash with the boot
  code, the RTC9701 with its EEPROM (settings and high scores).

## CPU

The interpreter follows current MAME's SH-3 instruction by instruction,
with the cycle counts of MAME's recompiler (MAME only runs the SH-3 with its
recompiler) and MAME's SH7709S cache and bus timing model: every data access
goes through a model of the 16 KB cache, the write-back, the burst fills and
the wait states of each bus area. The games' speed (and their slowdowns)
depends on it.

Peripherals: the interrupt controller, the three timers (TMU), the DMA
controller (the transfer at once, the end interrupt after the transfer
time, as MAME), the I/O ports (inputs, NAND control, RTC serial line), the
cache address array. The timers run on the machine time, as in MAME.

## Recompiler

Core option "SH-3 Dynarec" (enabled by default; offered for these games
only, on the hosts where it runs: x86, x86-64, ARMv7, AArch64, except the
systems that forbid run-time code generation). Big-endian hosts use the
interpreter.

It gives the same results as the interpreter, cycle for cycle: a saved
state is the same with and without it, so netplay, save states and RunAhead
don't depend on it.

- Straight runs of instructions, up to an unconditional branch and its
  delay slot, become native code (sljit, as the MIPS and ADSP recompilers
  of this core). Conditional branches leave the block or loop inside it.
- The timeslice is checked before every instruction, as the interpreter's
  loop does; the cycles are taken in the interpreter's order around the
  memory accesses and the bus timing.
- Loads and stores compute their address natively and call the
  interpreter's accessors (memory handlers, cache and bus timing). The
  instructions without native code call the interpreter for that
  instruction.
- The block leaves when an interrupt becomes pending or a write touches
  translated code. Writes over translated code (CPU stores, DMA) and state
  loads flush the translations.

## Speedups

As in MAME, each game has an idle loop speedup: the read of the variable the
game polls in its wait loop skips 10 us of CPU time. The loop reads it in
the delay slot of its branch; MAME's recompiler reports there the PC it
stored two bytes after the loop start, so it skips on every pass; this
driver does the same with the delay slot's own PC.

MAME suspends the CPU for those 10 us. This core's scheduler doesn't give
the time back to a CPU resumed in the timeslice that suspended it, so the
CPU would fall further behind on every pass: the 10 us are eaten from the
timeslice instead (1024 cycles), with the same effect.

## Video

The VRAM is kept as 16-bit pixels (the 5-5-5 colour and the transparency
bit), 64 MB instead of MAME's 128 MB of 32-bit pixels; the blits work on
the same values as MAME's. The blitter runs at once when started, and is
busy for the time MAME computes for that blit (the games read the busy
flag). MAME can run it in a separate thread; this core doesn't.

## Sound

The YMZ770 decodes MAME's AMM variant of MPEG audio, added to
`mpeg_audio.c` next to layers I and II (used by the Sega DSB), which are
unchanged.

## NAND flash, RTC, NVRAM

- The NAND is read with its spare area and bad block table through the
  flash interface and port E (busy for 25 us after a page read).
- The RTC9701 is set from the host clock at start, as MAME does.
- NVRAM: the RTC EEPROM, and the NAND blocks the game erased or programmed
  (only those, not the 128 MB).

## Save states

Every part has its state saved (CPU, peripherals, cache model, blitter
and VRAM, YMZ770, NAND, RTC). A state is about 77 MB with the VRAM (85 MB on
CV1000-D). Tested: a save and load on every frame (RunAhead-like) gives the
same frames, audio and state as a run without them.

## Not done

- `mmmbanc` (Medal Mahjong Moukari Bancho), a touch screen game.
- Akai Katana and DoDonPachi SaiDaiOuJou: commented out in MAME too.

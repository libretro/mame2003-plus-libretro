# set this to mame, mess or the destination you want to build
# TARGET = mame
# TARGET = mess
# TARGET = mmsnd
# example for a tiny compile
# TARGET = tiny
ifeq ($(TARGET),)
TARGET = mame
endif

# uncomment next line to include the debugger
# DEBUG = 1

# uncomment next line to include the symbols for symify
# SYMBOLS = 1

# uncomment next line to use Assembler 68000 engine
# X86_ASM_68000 = 1

# uncomment next line to use Assembler 68020 engine
# X86_ASM_68020 = 1

# uncomment next line to use DRC MIPS3 engine
# X86_MIPS3_DRC = 1

MAMEOS = libretro

# CPU core include paths
VPATH=src $(wildcard src/cpu/*)

# compiler, linker and utilities
AR = @ar
NATIVECC = @gcc
CC = @gcc
LD = @gcc
ASM = @nasm
ASMFLAGS = -f coff
MD = -mkdir
RM = @rm -f


ifeq ($(MAMEOS),msdos)
PREFIX = d
else
PREFIX =
endif

# build the targets in different object dirs, since mess changes
# some structures and thus they can't be linked against each other.
OBJ = obj/mame
DEFS = -DINLINE="static __inline__" -Dasm=__asm__

CFLAGS = -Isrc -Isrc/includes -Isrc/$(MAMEOS) -I$(OBJ)/cpu/m68000 -Isrc/cpu/m68000

RETRO_PROFILE = 0
CFLAGS += -DRETRO_PROFILE=$(RETRO_PROFILE)

ifneq ($(platform), sncps3)
CFLAGS += -Wall -Wno-sign-compare -Wunused \
	-Wpointer-arith -Wbad-function-cast -Wcast-align -Waggregate-return \
	-Wshadow -Wstrict-prototypes -Wundef \
	-Wformat-security -Wwrite-strings \
	-Wdisabled-optimization
endif

ifdef SYMBOLS
CFLAGS += -O0 -Wall -Wno-unused -g
else
CFLAGS += -DNDEBUG $(ARCH) -O3 -fomit-frame-pointer -fstrict-aliasing
endif

# extra options needed *only* for the osd files
CFLAGSOSDEPEND = $(CFLAGS)

# the windows osd code at least cannot be compiled with -pedantic
CFLAGSPEDANTIC = $(CFLAGS) -pedantic

ZLIBOBJS :=

OBJDIRS = obj $(OBJ) $(OBJ)/cpu $(OBJ)/sound $(OBJ)/$(MAMEOS) \
	$(OBJ)/drivers $(OBJ)/machine $(OBJ)/vidhrdw $(OBJ)/sndhrdw
ifdef MESS
OBJDIRS += $(OBJ)/mess $(OBJ)/mess/systems $(OBJ)/mess/machine \
	$(OBJ)/mess/vidhrdw $(OBJ)/mess/sndhrdw $(OBJ)/mess/tools
endif

include src/$(MAMEOS)/$(MAMEOS).mak
all:	maketree $(EMULATOR)

# include the various .mak files
include Makefile.common

ifdef DEBUG
DBGDEFS = -DMAME_DEBUG
else
DBGDEFS =
DBGOBJS =
endif

# platform .mak files will want to add to this
ifeq ($(STATIC_LINKING),1)
CFLAGS += -Isrc/libretro/includes/zlib
else
ZLIBOBJS := deps/zlib/adler32.c \
	deps/zlib/compress.c \
	deps/zlib/crc32.c \
	deps/zlib/deflate.c \
	deps/zlib/gzclose.c \
	deps/zlib/gzlib.c \
	deps/zlib/gzread.c \
	deps/zlib/gzwrite.c \
	deps/zlib/inffast.c \
	deps/zlib/inflate.c \
	deps/zlib/inftrees.c \
	deps/zlib/trees.c \
	deps/zlib/uncompr.c \
	deps/zlib/zutil.c \
	deps/zlib/ioapi.c \
	deps/zlib/unzip.c
endif

# combine the various definitions to one
CDEFS = $(DEFS) $(COREDEFS) $(CPUDEFS) $(SOUNDDEFS) $(ASMDEFS) $(DBGDEFS)

# primary target
$(EMULATOR): $(OBJS) $(COREOBJS) $(OSOBJS) $(ZLIBOBJS) $(DRVLIBS)
ifeq ($(STATIC_LINKING),1)
	@echo Archiving $@...
	$(AR) rcs $@ $(OBJS) $(COREOBJS) $(OSOBJS) $(ZLIBOBJS) $(DRVLIBS)
else
	@echo Linking $@...
	$(LD) $(LDFLAGS) $(OBJS) $(COREOBJS) $(OSOBJS) $(ZLIBOBJS) $(LIBS) $(DRVLIBS) -o $@
endif

$(OBJ)/$(MAMEOS)/%.o: src/$(MAMEOS)/%.c
	$(CC) $(CDEFS) $(CFLAGSOSDEPEND) $(PLATCFLAGS) -c $< -o $@

$(OBJ)/%.o: src/%.c
	$(CC) $(CDEFS) $(CFLAGS) $(PLATCFLAGS) -c $< -o $@

# generated asm files for the 68000 emulator
$(OBJ)/cpu/m68000/68000.o:  $(OBJ)/cpu/m68000/68000.asm
	@echo Assembling $<...
	$(ASM) -o $@ $(ASMFLAGS) $(subst -D,-d,$(ASMDEFS)) $<

$(OBJ)/cpu/m68000/68020.o:  $(OBJ)/cpu/m68000/68020.asm
	@echo Assembling $<...
	$(ASM) -o $@ $(ASMFLAGS) $(subst -D,-d,$(ASMDEFS)) $<

$(OBJ)/%.a:
	@echo Archiving $@...
	$(RM) $@
	$(AR) cr $@ $^

makedir:
	@echo make makedir is no longer necessary, just type make

$(sort $(OBJDIRS)):
	$(MD) $@

maketree: $(sort $(OBJDIRS))

clean:
	@echo Deleting object tree $(OBJ)...
	$(RM) -r $(OBJ)
	@echo Deleting $(EMULATOR)...
	$(RM) $(EMULATOR)

clean68k:
	@echo Deleting 68k files...
	$(RM) -r $(OBJ)/cpuintrf.o
	$(RM) -r $(OBJ)/drivers/cps2.o
	$(RM) -r $(OBJ)/cpu/m68000



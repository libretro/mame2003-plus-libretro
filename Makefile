# compiler, linker and utilities
AR = @ar
CC = @gcc
LD = @gcc
RM = @rm -f

CORE_DIR := src
TARGET_NAME := mame2003

ifeq ($(platform),)
platform = unix
ifeq ($(shell uname -a),)
   platform = win
else ifneq ($(findstring MINGW,$(shell uname -a)),)
   platform = win
else ifneq ($(findstring Darwin,$(shell uname -a)),)
   platform = osx
else ifneq ($(findstring win,$(shell uname -a)),)
	platform = win
endif
endif

# system platform
system_platform = unix
ifeq ($(shell uname -a),)
	EXE_EXT = .exe
	system_platform = win
else ifneq ($(findstring Darwin,$(shell uname -a)),)
	system_platform = osx
ifeq ($(shell uname -p),powerpc)
	arch = ppc
else
	arch = intel
endif
else ifneq ($(findstring MINGW,$(shell uname -a)),)
	system_platform = win
endif


LIBM := -lm

ifeq ($(platform), unix)
   TARGET = $(TARGET_NAME)_libretro.so
   fpic = -fPIC

   CFLAGS += $(fpic)
   PLATCFLAGS += -Dstricmp=strcasecmp
   LDFLAGS += $(fpic) -shared -Wl,--version-script=link.T

else ifeq ($(platform), linux-portable)
   TARGET = $(TARGET_NAME)_libretro.so
   fpic = -fPIC -nostdlib

   CFLAGS += $(fpic)
   PLATCFLAGS += -Dstricmp=strcasecmp
	LIBM :=
   LDFLAGS += $(fpic) -shared -Wl,--version-script=link.T

else ifeq ($(platform), osx)
   TARGET = $(TARGET_NAME)_libretro.dylib
   fpic = -fPIC
ifeq ($(arch),ppc)
   BIGENDIAN = 1
   PLATCFLAGS += -D__ppc__ -D__POWERPC__
endif
   CFLAGS += $(fpic) -Dstricmp=strcasecmp
   LDFLAGS += $(fpic) -dynamiclib
OSXVER = `sw_vers -productVersion | cut -c 4`
	fpic += -mmacosx-version-min=10.1

# iOS
else ifneq (,$(findstring ios,$(platform)))

   TARGET = $(TARGET_NAME)_libretro_ios.dylib
   fpic = -fPIC
   CFLAGS += $(fpic) -Dstricmp=strcasecmp
   LDFLAGS += $(fpic) -dynamiclib

ifeq ($(IOSSDK),)
   IOSSDK := $(shell xcodebuild -version -sdk iphoneos Path)
endif

   CC = cc -arch armv7 -isysroot $(IOSSDK)
   LD = cc -arch armv7 -isysroot $(IOSSDK)
ifeq ($(platform),ios9)
   fpic += -miphoneos-version-min=8.0
   CC += -miphoneos-version-min=8.0
   LD += -miphoneos-version-min=8.0
else
   fpic += -miphoneos-version-min=5.0
   CC += -miphoneos-version-min=5.0
   LD += -miphoneos-version-min=5.0
endif

else ifeq ($(platform), ctr)
   TARGET = $(TARGET_NAME)_libretro_ctr.a
   CC = $(DEVKITARM)/bin/arm-none-eabi-gcc$(EXE_EXT)
   CXX = $(DEVKITARM)/bin/arm-none-eabi-g++$(EXE_EXT)
   AR = $(DEVKITARM)/bin/arm-none-eabi-ar$(EXE_EXT)
   PLATCFLAGS += -DARM11 -D_3DS
   PLATCFLAGS += -march=armv6k -mtune=mpcore -mfloat-abi=hard -mfpu=vfp
   PLATCFLAGS += -Wall -mword-relocations
   PLATCFLAGS += -fomit-frame-pointer -ffast-math
   ENDIANNESS_DEFINES:=-DLSB_FIRST
   CXXFLAGS = $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++11
   CPU_ARCH := arm
   STATIC_LINKING = 1
else ifeq ($(platform), rpi2)
   TARGET = $(TARGET_NAME)_libretro.so
   fpic = -fPIC
   CFLAGS += $(fpic)
   LDFLAGS += $(fpic) -shared -Wl,--version-script=link.T
   PLATCFLAGS += -Dstricmp=strcasecmp
   PLATCFLAGS += -marm -mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -funsafe-math-optimizations
   PLATCFLAGS += -fomit-frame-pointer -ffast-math
   ENDIANNESS_DEFINES:=-DLSB_FIRST
   CXXFLAGS = $(CFLAGS) -fno-rtti -fno-exceptions
   CPU_ARCH := arm
else ifeq ($(platform), android-armv7)
   TARGET = $(TARGET_NAME)_libretro_android.so

   CFLAGS += -fPIC 
   PLATCFLAGS += -march=armv7-a -mfloat-abi=softfp -Dstricmp=strcasecmp
   LDFLAGS += -fPIC -shared -Wl,--version-script=link.T

   CC = arm-linux-androideabi-gcc
   AR = arm-linux-androideabi-ar
   LD = arm-linux-androideabi-gcc
else ifeq ($(platform), qnx)
   TARGET = $(TARGET_NAME)_libretro_qnx.so

   CFLAGS += -fPIC 
   PLATCFLAGS += -march=armv7-a -Dstricmp=strcasecmp
   LDFLAGS += -fPIC -shared -Wl,--version-script=link.T

   CC = qcc -Vgcc_ntoarmv7le
   AR = qcc -Vgcc_ntoarmv7le
   LD = QCC -Vgcc_ntoarmv7le
else ifeq ($(platform), wii)
   TARGET = $(TARGET_NAME)_libretro_wii.a
   BIGENDIAN = 1
    
   CC = $(DEVKITPPC)/bin/powerpc-eabi-gcc$(EXE_EXT)
   AR = $(DEVKITPPC)/bin/powerpc-eabi-ar$(EXE_EXT)
   PLATCFLAGS += -DGEKKO -mrvl -mcpu=750 -meabi -mhard-float -D__ppc__ -D__POWERPC__ -Dstricmp=strcasecmp
   STATIC_LINKING = 1
else ifeq ($(platform), ps3)
   TARGET = $(TARGET_NAME)_libretro_ps3.a
   BIGENDIAN = 1
    
   CC = $(CELL_SDK)/host-win32/ppu/bin/ppu-lv2-gcc.exe
   AR = $(CELL_SDK)/host-win32/ppu/bin/ppu-lv2-ar.exe
   PLATCFLAGS += -D__CELLOS_LV2__ -D__ppc__ -D__POWERPC__ -Dstricmp=strcasecmp
   STATIC_LINKING = 1
else ifeq ($(platform), sncps3)
   TARGET = $(TARGET_NAME)_libretro_ps3.a
   BIGENDIAN = 1
    
   CC = $(CELL_SDK)/host-win32/sn/bin/ps3ppusnc.exe
   AR = $(CELL_SDK)/host-win32/sn/bin/ps3snarl.exe
   PLATCFLAGS += -D__CELLOS_LV2__ -D__ppc__ -D__POWERPC__ -Dstricmp=strcasecmp
   STATIC_LINKING = 1
else ifeq ($(platform), psp1)
	TARGET = $(TARGET_NAME)_libretro_psp1.a

	CC = psp-gcc$(EXE_EXT)
	AR = psp-ar$(EXE_EXT)
	PLATCFLAGS += -DPSP -Dstricmp=strcasecmp
	CFLAGS += -G0
   STATIC_LINKING = 1

else ifeq ($(platform), vita)
	TARGET = $(TARGET_NAME)_libretro_vita.a

	CC = arm-vita-eabi-gcc$(EXE_EXT)
	AR = arm-vita-eabi-ar$(EXE_EXT)
	PLATCFLAGS += -DVITA -Dstricmp=strcasecmp
   STATIC_LINKING = 1
else ifneq (,$(findstring armv,$(platform)))
   TARGET = $(TARGET_NAME)_libretro.so

   CFLAGS += -fPIC
   PLATCFLAGS += -Dstricmp=strcasecmp
   LDFLAGS += -fPIC -shared -Wl,--version-script=link.T
else
   EXE = .exe
   
   LDFLAGS += -shared -static-libgcc -static-libstdc++ -s -Wl,--version-script=link.T
endif

ifeq ($(BIGENDIAN), 1)
	PLATCFLAGS += -DMSB_FIRST
endif

PLATCFLAGS += $(fpic)

CFLAGS += -D__LIBRETRO__ -DPI=3.1415927
LDFLAGS += $(LIBM)

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



# build the targets in different object dirs, since mess changes
# some structures and thus they can't be linked against each other.
DEFS = -DINLINE="static __inline__" -Dasm=__asm__


RETRO_PROFILE = 0
CFLAGS += -DRETRO_PROFILE=$(RETRO_PROFILE)

ifneq ($(platform), sncps3)
CFLAGS += -Wall -Wno-sign-compare -Wunused \
	-Wpointer-arith -Wbad-function-cast -Wcast-align -Waggregate-return \
	-Wshadow -Wstrict-prototypes \
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

# include the various .mak files
include Makefile.common

CFLAGS += $(INCFLAGS)

# combine the various definitions to one
CDEFS = $(DEFS) $(COREDEFS) $(CPUDEFS) $(SOUNDDEFS) $(ASMDEFS) $(DBGDEFS)

OBJECTS := $(SOURCES_C:.c=.o)

all:	$(TARGET)
$(TARGET): $(OBJECTS)
ifeq ($(STATIC_LINKING),1)
	@echo Archiving $@...
	$(AR) rcs $@ $(OBJECTS)
else
	@echo Linking $@...
	$(CC) $(CDEFS) $(CFLAGSOSDEPEND) $(PLATCFLAGS) $(LDFLAGS) -o $@ $(OBJECTS) $(LIBS)
endif

%.o: %.c
	$(CC) $(CDEFS) $(CFLAGS) $(PLATCFLAGS) -c -o $@ $<

$(OBJ)/%.a:
	@echo Archiving $@...
	$(RM) $@
	$(AR) cr $@ $^

clean:
	rm -f $(OBJECTS) $(TARGET)

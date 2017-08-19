DEBUG=0
DEBUGGER=0
CORE_DIR := src
TARGET_NAME := mame2003

GIT_VERSION ?= " $(shell git rev-parse --short HEAD || echo unknown)"
ifneq ($(GIT_VERSION)," unknown")
	CFLAGS += -DGIT_VERSION=\"$(GIT_VERSION)\"
endif

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


X86_ASM_68000 = # don't use x86 Assembler 68000 engine by default; set to 1 to enable
X86_ASM_68020 = # don't use x86 Assembler 68020 engine by default; set to 1 to enable
X86_MIPS3_DRC = # don't use x86 DRC MIPS3 engine by default;       set to 1 to enable

ifeq ($(ARCH),)
	# no architecture value passed make; try to determine host platform
	UNAME_P = $(shell uname -p)
	ifneq ($(findstring powerpc,$(UNAME_P)),)
		ARCH = ppc
	else ifneq ($(findstring x86_64,$(UNAME_P)),)
		# catch "x86_64" first, but no commands for x86_x64 only at this point
	else ifneq ($(findstring 86,$(UNAME_P)),)
		ARCH = x86 # if "86" is found now it must be i386 or i686
	endif
endif

ifeq ($(ARCH), x86)
	X86_MIPS3_DRC = 1
endif

LIBS := -lm

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
	LIBS =
   LDFLAGS += $(fpic) -shared -Wl,--version-script=link.T

else ifeq ($(platform), osx)
   TARGET = $(TARGET_NAME)_libretro.dylib
   fpic = -fPIC
ifeq ($(ARCH),ppc)
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
   PLATCFLAGS += -D__IOS__

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
   TARGET = $(TARGET_NAME)_libretro_$(platform).a
   CC = $(DEVKITARM)/bin/arm-none-eabi-gcc$(EXE_EXT)
   CXX = $(DEVKITARM)/bin/arm-none-eabi-g++$(EXE_EXT)
   AR = $(DEVKITARM)/bin/arm-none-eabi-ar$(EXE_EXT)
   PLATCFLAGS += -DARM11 -D_3DS
   PLATCFLAGS += -march=armv6k -mtune=mpcore -mfloat-abi=hard -mfpu=vfp
   PLATCFLAGS += -Wall -mword-relocations
   PLATCFLAGS += -fomit-frame-pointer -ffast-math
   CXXFLAGS = $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++11
   CPU_ARCH := arm
   STATIC_LINKING = 1
else ifeq ($(platform), rpi2)
   TARGET = $(TARGET_NAME)_libretro.so
   fpic = -fPIC
   CFLAGS += $(fpic)
   LDFLAGS += $(fpic) -shared -Wl,--version-script=link.T
   PLATCFLAGS += -Dstricmp=strcasecmp
   PLATCFLAGS += -marm -mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard
   PLATCFLAGS += -fomit-frame-pointer -ffast-math
   CXXFLAGS = $(CFLAGS) -fno-rtti -fno-exceptions
   CPU_ARCH := arm
   ARM = 1
else ifeq ($(platform), rpi3)
   TARGET = $(TARGET_NAME)_libretro.so
   fpic = -fPIC
   CFLAGS += $(fpic)
   LDFLAGS += $(fpic) -shared -Wl,--version-script=link.T
   PLATCFLAGS += -Dstricmp=strcasecmp
   PLATCFLAGS += -marm -mcpu=cortex-a53 -mfpu=neon-fp-armv8 -mfloat-abi=hard
   PLATCFLAGS += -fomit-frame-pointer -ffast-math
   CXXFLAGS = $(CFLAGS) -fno-rtti -fno-exceptions
   CPU_ARCH := arm
   ARM = 1
else ifeq ($(platform), android-armv7)
   TARGET = $(TARGET_NAME)_libretro_android.so

   CFLAGS += -fPIC 
   PLATCFLAGS += -march=armv7-a -mfloat-abi=softfp -Dstricmp=strcasecmp
   LDFLAGS += -fPIC -shared -Wl,--version-script=link.T

   CC = arm-linux-androideabi-gcc
   AR = arm-linux-androideabi-ar
   LD = arm-linux-androideabi-gcc
else ifeq ($(platform), qnx)
   TARGET = $(TARGET_NAME)_libretro_$(platform).so

   CFLAGS += -fPIC 
   PLATCFLAGS += -march=armv7-a -Dstricmp=strcasecmp
   LDFLAGS += -fPIC -shared -Wl,--version-script=link.T

   CC = qcc -Vgcc_ntoarmv7le
   AR = qcc -Vgcc_ntoarmv7le
   LD = QCC -Vgcc_ntoarmv7le

else ifeq ($(platform), wii)
   TARGET = $(TARGET_NAME)_libretro_$(platform).a
   BIGENDIAN = 1
    
   CC = $(DEVKITPPC)/bin/powerpc-eabi-gcc$(EXE_EXT)
   AR = $(DEVKITPPC)/bin/powerpc-eabi-ar$(EXE_EXT)
   PLATCFLAGS += -DGEKKO -mrvl -mcpu=750 -meabi -mhard-float -D__ppc__ -D__POWERPC__ -Dstricmp=strcasecmp
   PLATCFLAGS += -U__INT32_TYPE__ -U __UINT32_TYPE__ -D__INT32_TYPE__=int
   STATIC_LINKING = 1

else ifeq ($(platform), wiiu)
   TARGET = $(TARGET_NAME)_libretro_$(platform).a
   BIGENDIAN = 1
    
   CC = $(DEVKITPPC)/bin/powerpc-eabi-gcc$(EXE_EXT)
   AR = $(DEVKITPPC)/bin/powerpc-eabi-ar$(EXE_EXT)
   PLATCFLAGS += -DGEKKO -DWIIU -mwup -mcpu=750 -meabi -mhard-float -D__ppc__ -D__POWERPC__ -Dstricmp=strcasecmp
   PLATCFLAGS += -U__INT32_TYPE__ -U __UINT32_TYPE__ -D__INT32_TYPE__=int
   STATIC_LINKING = 1

else ifeq ($(platform), ps3)
   TARGET = $(TARGET_NAME)_libretro_$(platform).a
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
else ifeq ($(platform), psl1ght)
   TARGET = $(TARGET_NAME)_libretro_$(platform).a
   BIGENDIAN = 1
    
   CC = $(PS3DEV)/ppu/bin/ppu-gcc$
   AR = $(PS3DEV)/ppu/bin/ppu-ar$
   PLATCFLAGS += -D__CELLOS_LV2__ -D__ppc__ -D__POWERPC__ -Dstricmp=strcasecmp
   STATIC_LINKING = 1
else ifeq ($(platform), psp1)
	TARGET = $(TARGET_NAME)_libretro_$(platform).a

	CC = psp-gcc$(EXE_EXT)
	AR = psp-ar$(EXE_EXT)
	PLATCFLAGS += -DPSP -Dstricmp=strcasecmp
	CFLAGS += -G0
   STATIC_LINKING = 1

else ifeq ($(platform), vita)
	TARGET = $(TARGET_NAME)_libretro_$(platform).a

	CC = arm-vita-eabi-gcc$(EXE_EXT)
	AR = arm-vita-eabi-ar$(EXE_EXT)
	PLATCFLAGS += -DVITA -Dstricmp=strcasecmp
	CFLAGS += -mthumb -mfloat-abi=hard -fsingle-precision-constant
	CFLAGS += -Wall -mword-relocations
	CFLAGS += -fomit-frame-pointer -ffast-math
	CFLAGS += -fno-unwind-tables -fno-asynchronous-unwind-tables 
	CFLAGS +=  -fno-optimize-sibling-calls
	CFLAGS += -ftree-vectorize -funroll-loops -fno-short-enums
	CXXFLAGS = $(CFLAGS) -fno-rtti -fno-exceptions
	HAVE_RZLIB := 1
	ARM = 1
	STATIC_LINKING := 1
	DEBUG = 0
		
else ifneq (,$(findstring armv,$(platform)))
   TARGET = $(TARGET_NAME)_libretro.so

   CFLAGS += -fPIC
   PLATCFLAGS += -Dstricmp=strcasecmp
   LDFLAGS += -fPIC -shared -Wl,--version-script=link.T

# GCW0
else ifeq ($(platform), gcw0)
	TARGET := $(TARGET_NAME)_libretro.so
	CC = /opt/gcw0-toolchain/usr/bin/mipsel-linux-gcc-4.9.1
	CXX = /opt/gcw0-toolchain/usr/bin/mipsel-linux-g++
	AR = /opt/gcw0-toolchain/usr/bin/mipsel-linux-ar
	LDFLAGS += -shared -Wl,--version-script=link.T -Wl,-no-undefined
	PLATCFLAGS += -Dstricmp=strcasecmp
	LIBS := -lc -lgcc
	fpic := -fPIC -nostdlib
	LIBS =
	CFLAGS += -lm -march=mips32 -mtune=mips32r2 -mhard-float

else ifeq ($(platform), emscripten)
	TARGET := $(TARGET_NAME)_libretro_$(platform).bc
	HAVE_RZLIB := 1
	STATIC_LINKING := 1
   PLATCFLAGS += -Dstricmp=strcasecmp

# Windows MSVC 2010 x64
else ifeq ($(platform), windows_msvc2010_x64)
	CC  = cl.exe
	CXX = cl.exe

PATH := $(shell IFS=$$'\n'; cygpath "$(VS100COMNTOOLS)../../VC/bin/amd64"):$(PATH)
PATH := $(PATH):$(shell IFS=$$'\n'; cygpath "$(VS100COMNTOOLS)../IDE")
LIB := $(shell IFS=$$'\n'; cygpath "$(VS100COMNTOOLS)../../VC/lib/amd64")
INCLUDE := $(shell IFS=$$'\n'; cygpath "$(VS100COMNTOOLS)../../VC/include")

WindowsSdkDir := $(shell reg query "HKLM\SOFTWARE\Microsoft\Microsoft SDKs\Windows\v7.0A" -v "InstallationFolder" | grep -o '[A-Z]:\\.*')lib/x64
WindowsSdkDir ?= $(shell reg query "HKLM\SOFTWARE\Microsoft\Microsoft SDKs\Windows\v7.1A" -v "InstallationFolder" | grep -o '[A-Z]:\\.*')lib/x64

WindowsSdkDirInc := $(shell reg query "HKLM\SOFTWARE\Microsoft\Microsoft SDKs\Windows\v7.0A" -v "InstallationFolder" | grep -o '[A-Z]:\\.*')Include
WindowsSdkDirInc ?= $(shell reg query "HKLM\SOFTWARE\Microsoft\Microsoft SDKs\Windows\v7.1A" -v "InstallationFolder" | grep -o '[A-Z]:\\.*')Include


INCFLAGS_PLATFORM = -I"$(WindowsSdkDirInc)"
export INCLUDE := $(INCLUDE)
export LIB := $(LIB);$(WindowsSdkDir)
TARGET := $(TARGET_NAME)_libretro.dll
PSS_STYLE :=2
LDFLAGS += -DLL
LIBS =
# Windows MSVC 2010 x86
else ifeq ($(platform), windows_msvc2010_x86)
	CC  = cl.exe
	CXX = cl.exe

PATH := $(shell IFS=$$'\n'; cygpath "$(VS100COMNTOOLS)../../VC/bin"):$(PATH)
PATH := $(PATH):$(shell IFS=$$'\n'; cygpath "$(VS100COMNTOOLS)../IDE")
LIB := $(shell IFS=$$'\n'; cygpath -w "$(VS100COMNTOOLS)../../VC/lib")
INCLUDE := $(shell IFS=$$'\n'; cygpath "$(VS100COMNTOOLS)../../VC/include")

WindowsSdkDir := $(shell reg query "HKLM\SOFTWARE\Microsoft\Microsoft SDKs\Windows\v7.0A" -v "InstallationFolder" | grep -o '[A-Z]:\\.*')lib
WindowsSdkDir ?= $(shell reg query "HKLM\SOFTWARE\Microsoft\Microsoft SDKs\Windows\v7.1A" -v "InstallationFolder" | grep -o '[A-Z]:\\.*')lib

WindowsSdkDirInc := $(shell reg query "HKLM\SOFTWARE\Microsoft\Microsoft SDKs\Windows\v7.0A" -v "InstallationFolder" | grep -o '[A-Z]:\\.*')Include
WindowsSdkDirInc ?= $(shell reg query "HKLM\SOFTWARE\Microsoft\Microsoft SDKs\Windows\v7.1A" -v "InstallationFolder" | grep -o '[A-Z]:\\.*')Include


INCFLAGS_PLATFORM = -I"$(WindowsSdkDirInc)"
export INCLUDE := $(INCLUDE)
export LIB := $(LIB);$(WindowsSdkDir)
TARGET := $(TARGET_NAME)_libretro.dll
PSS_STYLE :=2
LDFLAGS += -DLL
LIBS =

# Windows MSVC 2005 x86
else ifeq ($(platform), windows_msvc2005_x86)
	CC  = cl.exe
	CXX = cl.exe

PATH := $(shell IFS=$$'\n'; cygpath "$(VS80COMNTOOLS)../../VC/bin"):$(PATH)
PATH := $(PATH):$(shell IFS=$$'\n'; cygpath "$(VS80COMNTOOLS)../IDE")
INCLUDE := $(shell IFS=$$'\n'; cygpath "$(VS80COMNTOOLS)../../VC/include")
LIB := $(shell IFS=$$'\n'; cygpath -w "$(VS80COMNTOOLS)../../VC/lib")
BIN := $(shell IFS=$$'\n'; cygpath "$(VS80COMNTOOLS)../../VC/bin")

WindowsSdkDir := $(INETSDK)

export INCLUDE := $(INCLUDE);$(INETSDK)/Include;src/libretro/libretro-common/include/compat/msvc
export LIB := $(LIB);$(WindowsSdkDir);$(INETSDK)/Lib
TARGET := $(TARGET_NAME)_libretro.dll
PSS_STYLE :=2
LDFLAGS += -DLL
CFLAGS += -D_CRT_SECURE_NO_DEPRECATE
LIBS =
# Windows
else
   TARGET := $(TARGET_NAME)_libretro.dll
   CC = gcc
   LDFLAGS += -shared -static-libgcc -static-libstdc++ -s -Wl,--version-script=link.T
   CFLAGS += -D__WIN32__ -D__WIN32_LIBRETRO__ -Wno-missing-field-initializers
endif

ifeq ($(BIGENDIAN), 1)
	PLATCFLAGS += -DMSB_FIRST
endif

# use -fsigned-char on ARM to solve potential problems with code written/tested on x86
# eg on mame2003 audio on rtype leo is wrong without it.
ifeq ($(ARM), 1)
   PLATCFLAGS += -fsigned-char
endif

PLATCFLAGS += $(fpic)

RETRO_PROFILE = 0
CFLAGS += -DRETRO_PROFILE=$(RETRO_PROFILE)

ifneq ($(platform), sncps3)
ifeq (,$(findstring msvc,$(platform)))
CFLAGS += -Wall -Wno-sign-compare -Wunused \
	-Wpointer-arith -Wbad-function-cast -Wcast-align -Waggregate-return \
	-Wshadow -Wstrict-prototypes \
	-Wformat-security -Wwrite-strings \
	-Wdisabled-optimization
endif
endif

ifeq ($(DEBUG), 1)
   CFLAGS += -O0 -Wall -Wno-unused -g
else
   CFLAGS += -O2 -DNDEBUG
endif

ifeq (,$(findstring msvc,$(platform)))
   CFLAGS += -fomit-frame-pointer -fstrict-aliasing
endif

# extra options needed *only* for the osd files
CFLAGSOSDEPEND = $(CFLAGS)

# the windows osd code at least cannot be compiled with -pedantic
CFLAGSPEDANTIC = $(CFLAGS) -pedantic

# include the various .mak files
include Makefile.common

# build the targets in different object dirs, since mess changes
# some structures and thus they can't be linked against each other.
DEFS = $(COREDEFINES) -Dasm=__asm__

CFLAGS += $(INCFLAGS) $(INCFLAGS_PLATFORM)

# combine the various definitions to one
CDEFS = $(DEFS) $(COREDEFS) $(CPUDEFS) $(SOUNDDEFS) $(ASMDEFS) $(DBGDEFS)

OBJECTS := $(SOURCES_C:.c=.o)

OBJOUT   = -o
LINKOUT  = -o 

ifneq (,$(findstring msvc,$(platform)))
	OBJOUT = -Fo
	LINKOUT = -out:
	LD = link.exe
else
	LD = $(CC)
endif

define NEWLINE


endef

all:	$(TARGET)
$(TARGET): $(OBJECTS)
ifeq ($(STATIC_LINKING),1)
	@echo Archiving $@...
ifeq ($(platform), ps3)
	$(AR) rcs $@ $(foreach OBJECTS,$(OBJECTS),$(NEWLINE) $(AR) q $@ $(OBJECTS))
else
	$(AR) rcs $@ $(OBJECTS)
endif
else
	@echo Linking $@...
ifeq ($(platform), win)
	# Use a temporary file to hold the list of objects, as it can exceed windows shell command limits
	$(file >$@.in,$(OBJECTS))
	$(LD) $(LDFLAGS) $(LINKOUT)$@ @$@.in $(LIBS)
	@rm $@.in
else
	$(LD) $(LDFLAGS) $(LINKOUT)$@ $(OBJECTS) $(LIBS)
endif
endif

%.o: %.c
	$(CC) $(CDEFS) $(CFLAGS) $(PLATCFLAGS) -c $(OBJOUT)$@ $<

$(OBJ)/%.a:
	@echo Archiving $@...
	$(RM) $@
	$(AR) cr $@ $^

clean:
ifeq ($(platform), win)
	# Use a temporary file to hold the list of objects, as it can exceed windows shell command limits
	$(file >$@.in,$(OBJECTS))
	rm -f @$@.in $(TARGET)
	@rm $@.in
else ifeq ($(platform), ps3)
	find . -name "*.o" -type f -delete
	rm -f *.a
else
	rm -f $(OBJECTS) $(TARGET)
endif

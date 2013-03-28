OSOBJS = $(OBJ)/libretro/libretro.o $(OBJ)/libretro/osd.o $(OBJ)/libretro/keyboard.o $(OBJ)/libretro/joystick.o \
         $(OBJ)/libretro/video.o $(OBJ)/libretro/performance.o

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

ifeq ($(platform), unix)
   EMULATOR = mame078_libretro.so

   CFLAGS += -fPIC
   PLATCFLAGS += -Dstricmp=strcasecmp
   LDFLAGS += -fPIC -shared -Wl,--version-script=src/libretro/link.T
else ifeq ($(platform), osx)
   EMULATOR = mame078_libretro.dylib

   CFLAGS += -fPIC -Dstricmp=strcasecmp -m32
   LDFLAGS += -fPIC -dynamiclib -m32
else ifeq ($(platform), ios)
   EMULATOR = mame078_libretro.dylib

   CFLAGS += -fPIC -Dstricmp=strcasecmp
   LDFLAGS += -fPIC -dynamiclib

   CC = clang -arch armv7 -isysroot $(IOSSDK)
   LD = clang -arch armv7 -isysroot $(IOSSDK)
else ifeq ($(platform), android)
   EMULATOR = mame078_libretro.so

   CFLAGS += -fPIC 
   PLATCFLAGS += -march=armv7-a -Dstricmp=strcasecmp
   LDFLAGS += -fPIC -shared -Wl,--version-script=src/libretro/link.T

   CC = arm-linux-androideabi-gcc
   AR = arm-linux-androideabi-ar
   LD = arm-linux-androideabi-gcc
else ifeq ($(platform), wii)
   EMULATOR = mame078_libretro_wii.a
   BIGENDIAN = 1
    
   CC = $(DEVKITPPC)/bin/powerpc-eabi-gcc$(EXE_EXT)
   AR = $(DEVKITPPC)/bin/powerpc-eabi-ar$(EXE_EXT)
   PLATCFLAGS += -DGEKKO -mrvl -mcpu=750 -meabi -mhard-float -D__ppc__ -D__POWERPC__ -Dstricmp=strcasecmp
else ifeq ($(platform), ps3)
   EMULATOR = mame078_libretro_ps3.a
   BIGENDIAN = 1
    
   CC = $(CELL_SDK)/host-win32/ppu/bin/ppu-lv2-gcc.exe
   AR = $(CELL_SDK)/host-win32/ppu/bin/ppu-lv2-ar.exe
   PLATCFLAGS += -D__CELLOS_LV2__ -D__ppc__ -D__POWERPC__ -Dstricmp=strcasecmp
OSOBJS += utils/zlib/compress.o utils/zlib/deflate.o utils/zlib/trees.o
else ifeq ($(platform), psp1)
	EMULATOR = libretro_psp1.a

	CC = psp-gcc$(EXE_EXT)
	AR = psp-ar$(EXE_EXT)
	PLATCFLAGS += -DPSP -Dstricmp=strcasecmp
	CFLAGS += -G0
else
   EMULATOR = mame078_retro.dll
   EXE = .exe
   
   LDFLAGS += -shared -static-libgcc -static-libstdc++ -s -Wl,--version-script=libretro/link.T
endif

CFLAGS += -D__LIBRETRO__ -DPI=3.1415927
LDFLAGS += -lm

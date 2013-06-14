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

TARGET_NAME := mame078

ifeq ($(platform), unix)
   EMULATOR = $(TARGET_NAME)_libretro.so

   CFLAGS += -fPIC
   PLATCFLAGS += -Dstricmp=strcasecmp
   LDFLAGS += -fPIC -shared -Wl,--version-script=src/libretro/link.T
else ifeq ($(platform), osx)
   EMULATOR = $(TARGET_NAME)_libretro.dylib

   CFLAGS += -fPIC -Dstricmp=strcasecmp -m32
   LDFLAGS += -fPIC -dynamiclib -m32
else ifeq ($(platform), ios)
   EMULATOR = $(TARGET_NAME)_libretro_ios.dylib

   CFLAGS += -fPIC -Dstricmp=strcasecmp
   LDFLAGS += -fPIC -dynamiclib

   CC = clang -arch armv7 -isysroot $(IOSSDK)
   LD = clang -arch armv7 -isysroot $(IOSSDK)
else ifeq ($(platform), android-armv7)
   EMULATOR = libretro_$(TARGET_NAME).so

   CFLAGS += -fPIC 
   PLATCFLAGS += -march=armv7-a -mfloat-abi=softfp -Dstricmp=strcasecmp
   LDFLAGS += -fPIC -shared -Wl,--version-script=src/libretro/link.T

   CC = arm-linux-androideabi-gcc
   AR = arm-linux-androideabi-ar
   LD = arm-linux-androideabi-gcc
else ifeq ($(platform), qnx)
   EMULATOR = $(TARGET_NAME)_libretro_qnx.so

   CFLAGS += -fPIC 
   PLATCFLAGS += -march=armv7-a -Dstricmp=strcasecmp
   LDFLAGS += -fPIC -shared -Wl,--version-script=src/libretro/link.T

   CC = qcc -Vgcc_ntoarmv7le
   AR = qcc -Vgcc_ntoarmv7le
   LD = QCC -Vgcc_ntoarmv7le
else ifeq ($(platform), wii)
   EMULATOR = $(TARGET_NAME)_libretro_wii.a
   BIGENDIAN = 1
    
   CC = $(DEVKITPPC)/bin/powerpc-eabi-gcc$(EXE_EXT)
   AR = $(DEVKITPPC)/bin/powerpc-eabi-ar$(EXE_EXT)
   PLATCFLAGS += -DGEKKO -mrvl -mcpu=750 -meabi -mhard-float -D__ppc__ -D__POWERPC__ -Dstricmp=strcasecmp
else ifeq ($(platform), ps3)
   EMULATOR = $(TARGET_NAME)_libretro_ps3.a
   BIGENDIAN = 1
    
   CC = $(CELL_SDK)/host-win32/ppu/bin/ppu-lv2-gcc.exe
   AR = $(CELL_SDK)/host-win32/ppu/bin/ppu-lv2-ar.exe
   PLATCFLAGS += -D__CELLOS_LV2__ -D__ppc__ -D__POWERPC__ -Dstricmp=strcasecmp
else ifeq ($(platform), sncps3)
   EMULATOR = $(TARGET_NAME)_libretro_ps3.a
   BIGENDIAN = 1
    
   CC = $(CELL_SDK)/host-win32/sn/bin/ps3ppusnc.exe
   AR = $(CELL_SDK)/host-win32/sn/bin/ps3snarl.exe
   PLATCFLAGS += -D__CELLOS_LV2__ -D__ppc__ -D__POWERPC__ -Dstricmp=strcasecmp
else ifeq ($(platform), psp1)
	EMULATOR = libretro_psp1.a

	CC = psp-gcc$(EXE_EXT)
	AR = psp-ar$(EXE_EXT)
	PLATCFLAGS += -DPSP -Dstricmp=strcasecmp
	CFLAGS += -G0
else
   EMULATOR = $(TARGET_NAME)_retro.dll
   EXE = .exe
   
   LDFLAGS += -shared -static-libgcc -static-libstdc++ -s -Wl,--version-script=libretro/link.T
endif

CFLAGS += -D__LIBRETRO__ -DPI=3.1415927
LDFLAGS += -lm

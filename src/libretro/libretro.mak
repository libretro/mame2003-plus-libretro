OSOBJS = $(OBJ)/libretro/libretro.o $(OBJ)/libretro/osd.o $(OBJ)/libretro/keyboard.o $(OBJ)/libretro/joystick.o \
         $(OBJ)/libretro/video.o

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

TARGET_NAME := mame078

LIBM := -lm

ifeq ($(platform), unix)
   EMULATOR = $(TARGET_NAME)_libretro.so
   fpic = -fPIC

   CFLAGS += $(fpic)
   PLATCFLAGS += -Dstricmp=strcasecmp
   LDFLAGS += $(fpic) -shared -Wl,--version-script=src/libretro/link.T
else ifeq ($(platform), linux-portable)
   EMULATOR = $(TARGET_NAME)_libretro.so
   fpic = -fPIC -nostdlib

   CFLAGS += $(fpic)
   PLATCFLAGS += -Dstricmp=strcasecmp
	LIBM :=
   LDFLAGS += $(fpic) -shared -Wl,--version-script=src/libretro/link.T
else ifeq ($(platform), osx)
   EMULATOR = $(TARGET_NAME)_libretro.dylib
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

   EMULATOR = $(TARGET_NAME)_libretro_ios.dylib
   fpic = -fPIC
   CFLAGS += $(fpic) -Dstricmp=strcasecmp
   LDFLAGS += $(fpic) -dynamiclib

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
   EMULATOR = $(TARGET_NAME)_libretro_ctr.a
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
else ifeq ($(platform), android-armv7)
   EMULATOR = $(TARGET_NAME)_libretro_android.so

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

else ifeq ($(platform), vita)
	EMULATOR = libretro_vita.a

	CC = arm-vita-eabi-gcc$(EXE_EXT)
	AR = arm-vita-eabi-ar$(EXE_EXT)
	PLATCFLAGS += -DVITA -Dstricmp=strcasecmp
else ifneq (,$(findstring armv,$(platform)))
   EMULATOR = $(TARGET_NAME)_libretro.so

   CFLAGS += -fPIC
   PLATCFLAGS += -Dstricmp=strcasecmp
   LDFLAGS += -fPIC -shared -Wl,--version-script=src/libretro/link.T
else
   EMULATOR = $(TARGET_NAME)_libretro.dll
   EXE = .exe
   
   LDFLAGS += -shared -static-libgcc -static-libstdc++ -s -Wl,--version-script=src/libretro/link.T
endif

ifeq ($(BIGENDIAN), 1)
	PLATCFLAGS += -DMSB_FIRST
endif

CFLAGS += -D__LIBRETRO__ -DPI=3.1415927
LDFLAGS += $(LIBM)

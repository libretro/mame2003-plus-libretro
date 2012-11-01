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

ifeq ($(platform), unix)
   EMULATOR = libretro.so

   CFLAGS += -fPIC
   LDFLAGS += -fPIC -shared -Wl,--version-script=src/libretro/link.T
else ifeq ($(platform), osx)
   EMULATOR = libretro.dylib

   CFLAGS += -fPIC -Dstricmp=strcasecmp -m32
   LDFLAGS += -fPIC -dynamiclib -m32
else ifeq ($(platform), android)
   EMULATOR = libretro_mame.so

   CFLAGS += -fPIC 
   PLATCFLAGS += -march=armv7-a -Dstricmp=strcasecmp
   LDFLAGS += -fPIC -shared -Wl,--version-script=src/libretro/link.T

   CC = arm-linux-androideabi-gcc
   AR = arm-linux-androideabi-ar
   LD = arm-linux-androideabi-gcc
else
   EMULATOR = retro.dll
   EXE = .exe
   
   LDFLAGS += -shared -static-libgcc -static-libstdc++ -s -Wl,--version-script=libretro/link.T
endif

CFLAGS += -D__LIBRETRO__ -DPI=3.1415927
LDFLAGS += -lm

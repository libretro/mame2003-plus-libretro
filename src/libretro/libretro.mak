# only Windows specific output files and rules
# the first two targets generate the prefix.h header
# note this requires that OSOBJS be the first target
#
OSOBJS = $(OBJ)/libretro/libretro.o $(OBJ)/libretro/osd.o $(OBJ)/libretro/keyboard.o

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
   CFLAGS += -fPIC
   LDFLAGS += -fPIC -shared -Wl,--version-script=src/libretro/link.T
else ifeq ($(platform), osx)
   CC = clang
   CFLAGS += -fPIC -m32 -Dstricmp=strcasecmp
   LDFLAGS += -fPIC -dynamiclib -m32
else ifeq ($(platform), android)
   CC = arm-linux-androideabi-gcc
   AR = arm-linux-androideabi-ar
   LD = arm-linux-androideabi-gcc
   CFLAGS += -fPIC
   LDFLAGS += -fPIC -shared -Wl,--version-script=src/libretro/link.T
else
   CC = gcc
   CFLAGS += -fPIC
   LDFLAGS += -shared -static-libgcc -static-libstdc++ -s -Wl,--version-script=libretro/link.T
endif

CFLAGS += -D__LIBRETRO__ -DPI=3.1415927
LDFLAGS += -lm
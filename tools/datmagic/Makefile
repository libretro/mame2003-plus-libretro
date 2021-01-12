CFLAGS := -O2 -Wall -Wextra

all: datmagic

clean:
	rm -f datmagic.exe

datmagic: datmagic.c
	$(CC) $(CFLAGS) -o $@ $<

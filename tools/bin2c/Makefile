CFLAGS := -O2 -Wall -Wextra

all: bin2c

clean:
	rm -f bin2c test/test test/test_header.h

bin2c: bin2c.c
	$(CC) $(CFLAGS) -o $@ $<
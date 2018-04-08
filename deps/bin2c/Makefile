CFLAGS := -O2 -Wall -Wextra

.PHONY: all
all: bin2c

.PHONY: clean
clean:
	rm -f bin2c test/test test/test_header.h

bin2c: bin2c.c
	$(CC) $(CFLAGS) -o $@ $<

test/test_header.h: test/test.bin bin2c
	./bin2c $< $@ test_array

test/test: test/test.c test/test_header.h
	$(CC) $(CFLAGS) -o $@ $<

.PHONY: test
test: test/test test/test.bin
	test/test test/test.bin

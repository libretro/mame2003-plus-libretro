#include <assert.h>
#include <stdio.h>

#include "test_header.h"

int
main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Expected 1 argument, got %i\n", argc - 1);
        return 1;
    }

    FILE *f = fopen(argv[1], "rb");

    int length = 0;
    // We know the file has size 100 bytes so don't need to worry about
    // half full buffers. Also, we need to handle endianness of the
    // machine so we can't just read one byte at a time, but must instead
    // read 2 bytes at a time...
    char buff[2];

    while ((fread(buff, 1, 2, f)) == 2) {
        assert((buff[0] & 0xff) == (test_array[length] & 0xff));
        assert((buff[1] & 0xff) == (test_array[length + 1] & 0xff));

        length += 2;
    }

    assert(length == test_array_length);
    fclose(f);

    printf("All tests successful\n");
    return 0;
}

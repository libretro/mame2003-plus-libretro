/*
 * Converts binary data into a C source file.  The C source file
 * defines a string with the file name of the source of data, and an
 * unsigned character array containing the binary data.
 *
 * For example, if the source file is dl.lua, the generated file
 * contains:
 *
 * static const char dl_lua_source[] = "dl.lua";
 *
 * static const unsigned char dl_lua_bytes[] = {
 * ...
 * };
 *
 * A useful GNUMakefile rule follows.
 *
 * %.h:    %.lua
 *         luac -o $*.luo $*.lua
 *         bin2c -o $@ -n $*.lua $*.luo
 *         rm $*.luo
 *
 * John D. Ramsdell
 * Copyright (C) 2006 The MITRE Corporation
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>

#define COLUMNS 18

#ifdef PACKAGE_NAME
static const char package[] = PACKAGE_NAME;
#else
static const char *package = NULL;
#endif

#ifdef VERSION
static const char version[] = VERSION;
#else
static const char version[] = "version unknown";
#endif

static void
print_version(const char *program)
{
  if (package != NULL)
    program = package;
  fprintf(stderr, "%s %s\n", program, version);
}

static void
usage(const char *prog)
{
  fprintf(stderr,
	  "Usage: %s [options] file\n"
	  "Options:\n"
	  "  -n name -- generated C identifier source (default is file)\n"
	  "  -o file -- output to file (default is standard output)\n"
	  "  -v      -- print version information\n"
	  "  -h      -- print this message\n",
	  prog);
}

static void
emit_name(const char *name)
{
  int ch = *name;
  if (!ch) {
    putchar('_');
    return;
  }
  if (isalpha(ch))		/* Print underscore */
    putchar(ch);		/* when first char is */
  else				/* is not a letter. */
    putchar('_');
  for (;;) {
    ch = *++name;
    if (!ch)
      return;
    if (isalnum(ch))		/* Print underscore when */
      putchar(ch);		/* part of identifier is */
    else			/* not a letter or a digit. */
      putchar('_');
  }
}

static int
emit(const char *name)
{
  int col = COLUMNS;

  printf("static const char ");
  emit_name(name);
  printf("_source[] = \"%s\";\n\n", name);
  printf("static const unsigned char ");
  emit_name(name);
  printf("_bytes[] = {");
  for (;;) {
    int ch = getchar();
    if (ch == EOF) {
      printf("\n};\n");
      return 0;
    }
    if (col >= COLUMNS) {
      printf("\n  ");
      col = 0;
    }
    printf("%3d,", ch);
    col++;
  }
}

int
main(int argc, char *argv[])
{
  extern char *optarg;
  extern int optind;

  char *input = NULL;
  char *output = NULL;
  char *name = NULL;

  for (;;) {
    int c = getopt(argc, argv, "n:o:vh");
    if (c == -1)
      break;
    switch (c) {
    case 'n':
      name = optarg;
      break;
    case 'o':
      output = optarg;
      break;
    case 'v':
      print_version(argv[0]);
      return 0;
    case 'h':
      usage(argv[0]);
      return 0;
    default:
      usage(argv[0]);
      return 1;
    }
  }

  if (argc != optind + 1) {
    fprintf(stderr, "Bad arg count\n");
    usage(argv[0]);
    return 1;
  }

  input = argv[optind];
  if (!freopen(input, "rb", stdin)) {
    perror(input);
    return 1;
  }

  if (output && !freopen(output, "w", stdout)) {
    perror(output);
    return 1;
  }

  return emit(name ? name : input);
}

/* comm.c - select or reject lines common to two files
 *
 * Copyright 2012 Ilya Kuzmich <ikv@safe-mail.net>
 *
 * See http://opengroup.org/onlinepubs/9699919799/utilities/comm.html

// <# and ># take single digit, so 321 define flags
USE_COMM(NEWTOY(comm, "<2>2321", TOYFLAG_USR|TOYFLAG_BIN))

config COMM
  bool "comm"
  default y
  help
    usage: comm [-123] FILE1 FILE2

    Read FILE1 and FILE2, which should be ordered, and produce three text
    columns as output: lines only in FILE1; lines only in FILE2; and lines
    in both files. Filename "-" is a synonym for stdin.

    -1	Suppress the output column of lines unique to FILE1
    -2	Suppress the output column of lines unique to FILE2
    -3	Suppress the output column of lines duplicated in FILE1 and FILE2
*/

#define FOR_comm
#include "toys.h"

static void writeline(const char *line, int col)
{
  if (!col && FLAG(1)) return;
  else if (col == 1) {
    if (FLAG(2)) return;
    if (!FLAG(1)) putchar('\t');
  } else if (col == 2) {
    if (FLAG(3)) return;
    if (!FLAG(1)) putchar('\t');
    if (!FLAG(2)) putchar('\t');
  }
  puts(line);
}

void comm_main(void)
{
  FILE *file[2];
  char *line[2];
  int i = 0;

  for (i = 0; i<2; i++) {
    file[i] = strcmp(toys.optargs[i], "-")?xfopen(toys.optargs[i], "r"):stdin;
    line[i] = xgetline(file[i]);
  }

  if (toys.optflags == 7) return;

  while (line[0] && line[1]) {
    int order = strcmp(line[0], line[1]);

    if (!order) {
      writeline(line[0], 2);
      for (i = 0; i < 2; i++) {
        free(line[i]);
        line[i] = xgetline(file[i]);
      }
    } else {
      i = order>0;
      writeline(line[i], i);
      free(line[i]);
      line[i] = xgetline(file[i]);
    }
  }

  // Print rest of the longer file.
  for (i = line[0] ? 0 : 1; line[i];) {
    writeline(line[i], i);
    free(line[i]);
    line[i] = xgetline(file[i]);
  }

  if (CFG_TOYBOX_FREE) fclose(file[0]), fclose(file[1]);
}

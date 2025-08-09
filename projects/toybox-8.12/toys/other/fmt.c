/* fmt.c - Text formatter
 *
 * Copyright 2017 The Android Open Source Project
 *
 * No standard.
 *
 * Only counts space and tab for indent level (eats other low ascii chars,
 * treats all UTF8 chars as non-whitespace), preserves indentation but squashes
 * together runs of whitespace. No header/footer logic, no end-of-sentence
 * double-space, preserves initial tab/space mix when indenting new lines.

USE_FMT(NEWTOY(fmt, "w#<0=75", TOYFLAG_USR|TOYFLAG_BIN))

config FMT
  bool "fmt"
  default y
  help
    usage: fmt [-w WIDTH] [FILE...]

    Reformat input to wordwrap at a given line length, preserving existing
    indentation level, writing to stdout.

    -w WIDTH	Maximum characters per line (default 75)
*/

#define FOR_fmt
#include "toys.h"

GLOBALS(
  long width;

  int level, pos;
)

static void newline(void)
{
  if (TT.pos) xputc('\n');
  TT.pos = 0;
}

// Process lines of input, with (0,0) flush between files
static void fmt_line(char **pline, long len)
{
  char *line;
  int idx, indent, count;

  // Flush line on EOF
  if (!pline) return newline();

  // Measure indentation
  for (line = *pline, idx = count = 0; isspace(line[idx]); idx++) {
    if (line[idx]=='\t') count += 8-(count&7);
    else if (line[idx]==' ') count++;
  }
  indent = idx;

  // Blank lines (even with same indentation) flush line
  if (idx==len) {
    xputc('\n');
    TT.level = 0;

    return newline();
  }

  // Did indentation change?
  if (count!=TT.level) newline();
  TT.level = count;

  // Loop through words
  while (idx<len) {
    char *word = line+idx;

    // Measure this word (unicode width) and end
    while (idx<len && !isspace(line[idx])) idx++;
    line[idx++] = 0;
    count = utf8len(word);
    if (TT.pos+count+!!TT.pos>=TT.width) newline();

    // When indenting a new line, preserve tab/space mixture of input
    if (!TT.pos) {
      TT.pos = TT.level;
      if (indent) printf("%.*s", indent, line);
    } else count++;
    printf(" %s"+!(TT.pos!=TT.level), word);
    TT.pos += count;
    while (isspace(line[idx])) idx++;
  }
}

void fmt_main(void)
{
  loopfiles_lines(toys.optargs, fmt_line);
}

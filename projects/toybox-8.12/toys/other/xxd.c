/* xxd.c - hexdump.
 *
 * Copyright 2015 The Android Open Source Project
 *
 * No obvious standard.
 * Regular output:
 *   "00000000: 4c69 6e75 7820 7665 7273 696f 6e20 342e  Linux version 4."
 * xxd -i "include" or "initializer" output:
 *   "  0x4c, 0x69, 0x6e, 0x75, 0x78, 0x20, 0x76, 0x65, 0x72, 0x73, 0x69, 0x6f,"
 * xxd -p "plain" output:
 *   "4c696e75782076657273696f6e20342e392e302d342d616d643634202864"

USE_XXD(NEWTOY(xxd, ">1c#<0>256l#o#g#<0=2eiprs#[!rs][!re]", TOYFLAG_USR|TOYFLAG_BIN))

config XXD
  bool "xxd"
  default y
  help
    usage: xxd [-eipr] [-cglos N] [file]

    Hexdump a file to stdout. If no file is listed, copy from stdin.
    Filename "-" is a synonym for stdin.

    -c N	Show N bytes per line (default 16)
    -e	Little-endian
    -g N	Group bytes by adding a ' ' every N bytes (default 2)
    -i	Output include file (CSV hex bytes, plus C header/footer if not stdin)
    -l N	Limit of N bytes before stopping (default is no limit)
    -o N	Add N to display offset
    -p	Plain hexdump (30 bytes/line, no grouping. With -c 0 no wrap/group)
    -r	Reverse operation: turn a hexdump into a binary file
    -s N	Skip to offset N
*/

#define FOR_xxd
#include "toys.h"

GLOBALS(
  long s, g, o, l, c;
)

static void do_xxd(int fd, char *name)
{
  FILE *fp = xfdopen(xdup(fd), "r");
  long long pos = 0;
  long long limit = TT.l;
  int i, j, k, len, space, c = TT.c ? : sizeof(toybuf);

  if (FLAG(s)) {
    if (fseek(fp, TT.s, SEEK_SET)) perror_exit("seek %ld", TT.s);
    pos = TT.s;
    if (limit) limit += TT.s;
  }

  while ((len=fread(toybuf, 1, (limit && limit-pos<c) ? limit-pos : c, fp))>0){
    if (!FLAG(p)) printf("%08llx: ", TT.o + pos);
    pos += len;
    space = 2*TT.c;
    space += TT.g ? (TT.c+TT.g-1)/TT.g+1 : 2;

    for (i=0, j=1; i<len; ) {
      // Insert padding for short groups in little-endian mode.
      if (FLAG(e) && j==1 && (len-i)<TT.g) {
        for (k=0; k<(TT.g-(len-i)); k++) {
          space -= printf("  ");
          j++;
        }
      }

      space -= printf("%02x", toybuf[FLAG(e) ? (i + TT.g - j) : i]);
      i++,j+=2;
      if (!FLAG(p) && TT.g && !(i%TT.g)) {
        putchar(' ');
        space--;
        j=1;
      }
    }

    if (!FLAG(p)) {
      printf("%*s", space, "");
      for (i = 0; i<len; i++)
        putchar((toybuf[i]>=' ' && toybuf[i]<='~') ? toybuf[i] : '.');
    }
    if (TT.c || !FLAG(p)) putchar('\n');
  }
  if (!TT.c && FLAG(p)) putchar('\n');
  if (len<0) perror_exit("read");
  fclose(fp);
}

static void do_xxd_include(int fd, char *name)
{
  int c = 1, i, len;

  // The original xxd outputs a header/footer if given a filename (not stdin).
  // We don't, which means that unlike the original we can implement -ri.
  while ((len = read(fd, toybuf, sizeof(toybuf))) > 0) {
    for (i = 0; i < len; ++i) {
      printf("%s%#.02x", c > 1 ? ", " : "  ", toybuf[i]);
      if (c++ == TT.c) {
        xprintf(",\n");
        c = 1;
      }
    }
  }
  if (len < 0) perror_msg_raw(name);
  if (c > 1) xputc('\n');
}

static int dehex(char ch)
{
  if (ch >= '0' && ch <= '9') return ch - '0';
  if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
  if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
  return (ch == '\n') ? -2 : -1;
}

static void do_xxd_reverse(int fd, char *name)
{
  FILE *fp = xfdopen(xdup(fd), "r");
  long long pos, current_pos = 0;
  int tmp;

  // -ri is a very easy special case.
  if (FLAG(i)) while (fscanf(fp, " 0x%02x,", &tmp) == 1) xputc(tmp);
  else while (!feof(fp)) {
    int col = 0;
    char ch;

    // Each line of a regular hexdump starts with an offset/address.
    // Each line of a plain hexdump just goes straight into the bytes.
    if (!FLAG(p) && fscanf(fp, "%llx%c ", &pos, &ch) == 2) {
      if (ch != ':' && ch != ' ')
        error_exit("%s: no separator between offset/address and bytes "
            "(missing -p?)", name);
      if (pos != current_pos && fseek(stdout, pos, SEEK_SET)) {
        // TODO: just write out zeros if non-seekable?
        perror_exit("%s: seek %llx failed", name, pos);
      }
    }

    // A plain hexdump can have as many bytes per line as you like,
    // but a non-plain hexdump assumes garbage after it's seen the
    // specified number of bytes.
    while (FLAG(p) || !TT.c || col < TT.c) {
      int n1, n2;

      // If we're at EOF or EOL or we read some non-hex...
      if ((n1 = n2 = dehex(fgetc(fp))) < 0 || (n2 = dehex(fgetc(fp))) < 0) {
        // If we're at EOL, start on that line.
        if (n1 == -2 || n2 == -2) continue;
        // Otherwise, skip to the next line.
        break;
      }

      fputc((n1 << 4) | (n2 & 0xf), stdout);
      col++;
      current_pos++;

      // Is there any grouping going on? Ignore a single space.
      tmp = fgetc(fp);
      if (tmp != ' ') ungetc(tmp, fp);
    }

    // Skip anything else on this line (such as the ASCII dump).
    while ((tmp = fgetc(fp)) != EOF && tmp != '\n');
  }

  if (ferror(fp)) perror_msg_raw(name);
  fclose(fp);
}

void xxd_main(void)
{
  // Plain style is 30 bytes/line, no grouping.
  if (!FLAG(c)) TT.c = FLAG(p) ? 30 : FLAG(i) ? 12 : 16;
  if (FLAG(e) && !FLAG(g)) TT.g = 4;
  if (FLAG(p) && !FLAG(g)) TT.g = TT.c;

  loopfiles(toys.optargs,
    FLAG(r) ? do_xxd_reverse : (FLAG(i) ? do_xxd_include : do_xxd));
}

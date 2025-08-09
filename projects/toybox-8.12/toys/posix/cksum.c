/* cksum.c - produce crc32 checksum value for each input
 *
 * Copyright 2008 Rob Landley <rob@landley.net>
 *
 * See http://opengroup.org/onlinepubs/9699919799/utilities/cksum.html

USE_CKSUM(NEWTOY(cksum, "HIPLN", TOYFLAG_BIN))
USE_CRC32(NEWTOY(crc32, 0, TOYFLAG_BIN))

config CKSUM
  bool "cksum"
  default y
  help
    usage: cksum [-HIPLN] [FILE...]

    For each file, output crc32 checksum value, length and name of file.
    If no files listed, copy from stdin.  Filename "-" is a synonym for stdin.

    -H	Hexadecimal checksum (defaults to decimal)
    -I	Skip post-inversion
    -P	Pre-inversion
    -L	Little endian (defaults to big endian)
    -N	Do not include length in CRC calculation (or output)

config CRC32
  bool "crc32"
  default y
  help
    usage: crc32 [file...]

    Output crc32 checksum for each file.
*/

#define FOR_cksum
#define FORCE_FLAGS
#include "toys.h"

static void do_cksum(int fd, char *name)
{
  unsigned crc_table[256], crc = FLAG(P) ? ~0 : 0;
  unsigned long long llen = 0, llen2 = 0;
  int len, i, done = 0;

  // Init table, loop through data
  crc_init(crc_table, FLAG(L));
  for (;;) {
    len = read(fd, toybuf, sizeof(toybuf));
    if (len<0) perror_msg_raw(name);
    if (len<1) {
      // CRC the length at end
      if (FLAG(N)) break;
      for (llen2 = llen, len = 0; llen2; llen2 >>= 8) toybuf[len++] = llen2;
      done++;
    } else llen += len;
    for (i = 0; i<len; i++)
      crc = FLAG(L) ? crc_table[(crc^toybuf[i])&0xff] ^ (crc>>8)
                    : (crc<<8) ^ crc_table[(crc>>24)^toybuf[i]];
    if (done) break;
  }

  printf(FLAG(H) ? "%08x" : "%u", FLAG(I) ? crc : ~crc);
  if (!FLAG(N)) printf(" %llu", llen);
  if (toys.optc) printf(" %s", name);
  xputc('\n');
}

void cksum_main(void)
{
  loopfiles(toys.optargs, do_cksum);
}

void crc32_main(void)
{
  toys.optflags |= FLAG_H|FLAG_N|FLAG_P|FLAG_L;
  if (toys.optc) toys.optc--;
  cksum_main();
}

/* sha3sum.c - Keccak-f[1600] permutation, sponge construction
 *
 * Copyright 2014 David Leon Gil <coruus@gmail.com>
 *
 * https://keccak.team/files/Keccak-reference-3.0.pdf
 * https://csrc.nist.gov/publications/detail/fips/202/final
 * https://nvlpubs.nist.gov/nistpubs/specialpublications/nist.sp.800-185.pdf

USE_SHA3SUM(NEWTOY(sha3sum, "bSa#<128>512=224", TOYFLAG_USR|TOYFLAG_BIN))

config SHA3SUM
  bool "sha3sum"
  default y
  help
    usage: sha3sum [-bS] [-a BITS] [FILE...]

    Hash function du jour.

    -a	Produce a hash BITS long (default 224)
    -b	Brief (hash only, no filename)
    -S	Use SHAKE termination byte instead of SHA3 (ask FIPS why)
*/

#define FOR_sha3sum
#include "toys.h"

GLOBALS(
  long a;
  unsigned long long rc[24];
)

static const char rho[] =
  {1,3,6,10,15,21,28,36,45,55,2,14,27,41,56,8,25,43,62,18,39,61,20,44};
static const char pi[] =
  {10,7,11,17,18,3,5,16,8,21,24,4,15,23,19,13,12,2,20,14,22,9,6,1};
static const char rcpack[] =
  {0x33,0x07,0xdd,0x16,0x38,0x1b,0x7b,0x2b,0xad,0x6a,0xce,0x4c,0x29,0xfe,0x31,
   0x68,0x9d,0xb0,0x8f,0x2f,0x0a};

static void keccak(unsigned long long *a)
{
  unsigned long long b[5] = {0}, t;
  int i, x, y;

  for (i = 0; i < 24; i++) {
    for (x = 0; x<5; x++) for (b[x] = 0, y = 0; y<25; y += 5) b[x] ^= a[x+y];
    for (x = 0; x<5; x++) for (y = 0; y<25; y += 5) {
      t = b[(x+1)%5];
      a[y+x] ^= b[(x+4)%5]^(t<<1|t>>63);
    }
    for (t = a[1], x = 0; x<24; x++) {
      *b = a[pi[x]];
      a[pi[x]] = (t<<rho[x])|(t>>(64-rho[x]));
      t = *b;
    }
    for (y = 0; y<25; y += 5) {
      for (x = 0; x<5; x++) b[x] = a[y + x];
      for (x = 0; x<5; x++) a[y + x] = b[x]^((~b[(x+1)%5])&b[(x+2)%5]);
    }
    *a ^= TT.rc[i];
  }
}

static void do_sha3sum(int fd, char *name)
{
  int span, ii, len, rate = 200-TT.a/4;
  char *ss = toybuf, buf[200];

  memset(buf, 0, sizeof(buf));
  for (len = 0;; ss += rate) {
    if ((span = len-(ss-toybuf))<rate) {
      memcpy(toybuf, ss, span);
      len = span += readall(fd, (ss = toybuf)+span, sizeof(toybuf)-span);
    }
    if (span>rate) span = rate;
    for (ii = 0; ii<span; ii++) buf[ii] ^= ss[ii];
    if (rate!=span) {
      buf[span] ^= FLAG(S) ? 0x1f : 0x06;
      buf[rate-1] ^= 0x80;
    }
    keccak((void *)buf);
    if (span<rate) break;
  }

  for (ii = 0; ii<TT.a/8; ) {
    printf("%02x", buf[ii%rate]);
    if (!(++ii%rate)) keccak((void *)buf);
  }
  memset(buf, 0, sizeof(buf));

  xprintf("  %s\n"+(FLAG(b)<<2), name);
}

// TODO test 224 256 384 512, and shake 128 256
void sha3sum_main(void)
{
  int i, j, k;
  char *s;

  // Decompress RC table
  for (s = (void *)rcpack, i = 127; i; s += 3) for (i>>=1,k = j = 0; k<24; k++)
    if (1&(s[k>>3]>>(7-(k&7)))) TT.rc[k] |= 1ULL<<i;

  loopfiles(toys.optargs, do_sha3sum);
}

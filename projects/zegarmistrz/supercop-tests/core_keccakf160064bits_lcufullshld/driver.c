/* supercop-tests suite driver for crypto_core/* implementations.
 *
 * Differential-checksum methodology: deterministic LCG inputs, several
 * core invocations across 3 seeds, FNV-1a folding of the CRYPTO_OUTPUTBYTES
 * output bytes over REPEAT iterations per seed. One "FNVCHECK <16hex>" line
 * is printed per seed and a final "<core>-ok" line concludes. A test passes
 * iff the byte-identical output appears on both legs: native (hardware) and
 * under ../zegarmistrz.
 *
 * Numerics (LCG constants, iteration counts, seed set, per-iteration input
 * scheme and fold order) are the ones djb's checksum campaign used for
 * crypto_core; only the per-seed process runs are folded into one process.
 *
 * Freestanding-friendly plain C: no arguments, exit 0. */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "api.h"

#ifndef FNV_CHECK_ID
#define FNV_CHECK_ID "unset"
#endif

#ifndef REPEAT
#define REPEAT 8
#endif
#define MAXB 4096
#define NSEEDS 3

static const unsigned long seeds[NSEEDS] = { 1, 2, 3 };

static unsigned long long lcg;
static unsigned char nbyte(void) {
  lcg = lcg * 6364136223846793005ULL + 1442695040888963407ULL;
  return (unsigned char) (lcg >> 33);
}
static void fill(unsigned char *b, size_t n) {
  size_t i;
  for (i = 0; i < n; ++i) b[i] = nbyte();
}
static unsigned long long fnv;
static void fnv_add(const unsigned char *p, size_t n) {
  size_t i;
  for (i = 0; i < n; ++i) { fnv ^= p[i]; fnv *= 1099511628211ULL; }
}

int crypto_core(unsigned char *,const unsigned char *,const unsigned char *,const unsigned char *);

int main(void) {
  static unsigned char out[MAXB], in[MAXB], k[MAXB], c[MAXB];
  size_t outn = CRYPTO_OUTPUTBYTES ? (size_t) CRYPTO_OUTPUTBYTES : 1;
  size_t inn  = CRYPTO_INPUTBYTES  ? (size_t) CRYPTO_INPUTBYTES  : 1;
  size_t kn   = CRYPTO_KEYBYTES    ? (size_t) CRYPTO_KEYBYTES    : 1;
  size_t cn   = CRYPTO_CONSTBYTES  ? (size_t) CRYPTO_CONSTBYTES  : 1;
  int s, it;
  if (outn > MAXB || inn > MAXB || kn > MAXB || cn > MAXB) return 2;
  for (s = 0; s < NSEEDS; ++s) {
    lcg = seeds[s] * 2862933555777941757ULL + 3037000493ULL;
    if (!lcg) lcg = 88172645463325252ULL;
    fnv = 14695981039346656037ULL;
    for (it = 0; it < REPEAT; ++it) {
      memset(out, 0xAA, outn);
      fill(in, inn);
      fill(k, kn);
      fill(c, cn);
      crypto_core(out, in, k, c);
      fnv_add(out, outn);
    }
    printf("FNVCHECK %016llx\n", (unsigned long long) fnv);
  }
  printf(FNV_CHECK_ID "-ok\n");
  return 0;
}

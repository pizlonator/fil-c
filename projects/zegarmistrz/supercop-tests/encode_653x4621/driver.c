/* supercop-tests suite driver for crypto_encode/* implementations.
 *
 * Differential-checksum methodology: deterministic LCG input of
 * CRYPTO_ITEMS*CRYPTO_ITEMBYTES bytes, FNV-1a folding of the CRYPTO_STRBYTES
 * output over REPEAT iterations, one seed. One "FNVCHECK <16hex>" line is
 * printed and a final "<core>-ok" line concludes. A test passes iff the
 * byte-identical output appears on both legs: native (hardware) and under
 * ../zegarmistrz.
 *
 * Numerics (LCG constants, iteration count, per-iteration input scheme and
 * fold order) are the ones djb's checksum campaign used for crypto_encode.
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
#define REPEAT 16
#endif
#define MAXB 8192

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

void crypto_encode(unsigned char *,const void *);

int main(void) {
  static unsigned char in[MAXB], out[MAXB];
  size_t inn = (size_t) CRYPTO_ITEMS * (size_t) CRYPTO_ITEMBYTES;
  size_t outn = (size_t) CRYPTO_STRBYTES;
  int it;
  if (inn > MAXB || outn > MAXB) return 2;
  lcg = 1 * 2862933555777941757ULL + 3037000493ULL;
  if (!lcg) lcg = 88172645463325252ULL;
  fnv = 14695981039346656037ULL;
  for (it = 0; it < REPEAT; ++it) {
    memset(out, 0xAA, outn);
    fill(in, inn);
    crypto_encode(out, in);
    fnv_add(out, outn);
  }
  printf("FNVCHECK %016llx\n", (unsigned long long) fnv);
  printf(FNV_CHECK_ID "-ok\n");
  return 0;
}

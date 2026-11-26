/* supercop-tests suite driver for crypto_onetimeauth/poly1305
 * implementations.
 *
 * Differential-checksum methodology: deterministic LCG message + key, tags
 * for a spread of message lengths (0..1024, covering partial blocks and
 * multi-block), folded into one FNV-1a per seed across 3 seeds. One
 * "FNVCHECK <16hex>" line is printed per seed and a final "<core>-ok" line
 * concludes. A test passes iff the byte-identical output appears on both
 * legs: native (hardware) and under ../zegarmistrz.
 *
 * Numerics (LCG constants, iteration count, length sweep, per-iteration
 * input scheme and fold order) are the ones djb's checksum campaign used
 * for crypto_onetimeauth; only the per-seed process runs are folded into
 * one process.
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

int crypto_onetimeauth(unsigned char *,const unsigned char *,unsigned long long,const unsigned char *);

int main(void) {
  static const unsigned long long lens[] = {
    0ULL,1ULL,2ULL,15ULL,16ULL,17ULL,31ULL,32ULL,63ULL,64ULL,
    127ULL,131ULL,135ULL,255ULL,256ULL,511ULL,1000ULL,1024ULL
  };
  unsigned char k[CRYPTO_KEYBYTES ? CRYPTO_KEYBYTES : 32];
  unsigned char tag[CRYPTO_BYTES ? CRYPTO_BYTES : 16];
  unsigned char msg[1024];
  size_t i;
  unsigned s, it;
  for (s = 0; s < NSEEDS; ++s) {
    lcg = seeds[s] * 2862933555777941757ULL + 3037000493ULL;
    if (!lcg) lcg = 88172645463325252ULL;
    fnv = 14695981039346656037ULL;
    for (it = 0; it < 4; ++it) {
      fill(k, sizeof k);
      for (i = 0; i < sizeof lens / sizeof lens[0]; ++i) {
        fill(msg, (size_t) lens[i]);
        memset(tag, 0xAA, sizeof tag);
        crypto_onetimeauth(tag, msg, lens[i], k);
        fnv_add(tag, sizeof tag);
      }
    }
    printf("FNVCHECK %016llx\n", (unsigned long long) fnv);
  }
  printf(FNV_CHECK_ID "-ok\n");
  return 0;
}

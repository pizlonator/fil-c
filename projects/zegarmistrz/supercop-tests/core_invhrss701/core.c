// 20251220 djb: more usage of cryptoint
#include <stdint.h>
#include <stdlib.h>
#include "crypto_core.h"
#include "crypto_int16.h"
#include "crypto_uint16.h"

#define NTRU_N 701
#define NTRU_N_PADDED 704

typedef struct {
  uint16_t coeffs[NTRU_N_PADDED] __attribute__((aligned(32)));
} poly;

static uint16_t mod3(uint16_t a)
{
  uint16_t r;
  int16_t t, c;

  r = (a >> 8) + (a & 0xff); // r mod 255 == a mod 255
  r = (r >> 4) + (r & 0xf); // r' mod 15 == r mod 15
  r = (r >> 2) + (r & 0x3); // r' mod 3 == r mod 3
  r = (r >> 2) + (r & 0x3); // r' mod 3 == r mod 3

  t = r - 3;
  c = crypto_int16_negative_mask(t);

  return (c&r) ^ (~c&t);
}

#define POLY_S3_FMADD(I,A,B,S)                    \
   for(I=0; I<NTRU_N; I++)                            \
   { A.coeffs[I] = mod3(A.coeffs[I] + S * B.coeffs[I]); }

static inline void poly_mulx(poly *a, int s)
{
  int i;

  for(i=1; i<NTRU_N; i++)
    a->coeffs[NTRU_N-i] = (s * a->coeffs[NTRU_N-i-1]) | ((1-s) * a->coeffs[NTRU_N-i]);
  a->coeffs[0] = ((1-s) * a->coeffs[0]);
}

static inline void poly_divx(poly *a, int s)
{
  int i;

  for(i=1; i<NTRU_N; i++)
    a->coeffs[i-1] = (s * a->coeffs[i]) | ((1-s) * a->coeffs[i-1]);
  a->coeffs[NTRU_N-1] = ((1-s) * a->coeffs[NTRU_N-1]);
}

static void cswappoly(poly *a, poly *b, int swap)
{
  int i;
  uint16_t t;
  swap = -swap;
  for(i=0;i<NTRU_N;i++)
  {
    t = (a->coeffs[i] ^ b->coeffs[i]) & swap;
    a->coeffs[i] ^= t;
    b->coeffs[i] ^= t;
  }
}

static void cmov(unsigned char *r, const unsigned char *x, size_t len, unsigned char b)
{
  size_t i;

  b = -b;
  for(i=0;i<len;i++)
    r[i] ^= b & (x[i] ^ r[i]);
}

int crypto_core(unsigned char *rbytes,const unsigned char *abytes,const unsigned char *kbytes,const unsigned char *cbytes)
{
  /* Schroeppel--Orman--O'Malley--Spatscheck
   * "Almost Inverse" algorithm as described
   * by Silverman in NTRU Tech Report #14 */
  // with several modifications to make it run in constant-time
  int i, j;
  uint16_t k = 0;
  uint16_t degf = NTRU_N-1;
  uint16_t degg = NTRU_N-1;
  int sign, fsign = 0, t, swap;
  int done = 0;
  poly b, c, f, g;
  poly r;
  poly *temp_r = &f;

  /* b(X) := 1 */
  for(i=1; i<NTRU_N; i++)
    b.coeffs[i] = 0;
  b.coeffs[0] = 1;

  /* c(X) := 0 */
  for(i=0; i<NTRU_N; i++)
    c.coeffs[i] = 0;

  /* f(X) := a(X) */
  for(i=0; i<NTRU_N; i++)
    f.coeffs[i] = mod3(abytes[2*i] & 3);

  /* g(X) := 1 + X + X^2 + ... + X^{N-1} */
  for(i=0; i<NTRU_N; i++)
    g.coeffs[i] = 1;

  for(j=0; j<2*(NTRU_N-1)-1; j++)
  {
    sign = mod3(2 * g.coeffs[0] * f.coeffs[0]);
    swap = (((sign & 2) >> 1) | sign) & (1-done) & ((degf - degg) >> 15);

    cswappoly(&f, &g, swap);
    cswappoly(&b, &c, swap);
    t = (degf ^ degg) & (-swap);
    degf ^= t;
    degg ^= t;

    POLY_S3_FMADD(i, f, g, sign*(1-done));
    POLY_S3_FMADD(i, b, c, sign*(1-done));

    poly_divx(&f, 1-done);
    poly_mulx(&c, 1-done);
    degf -= 1-done;
    k += 1-done;

    done = 1 - (((uint16_t)-degf) >> 15);
  }

  fsign = f.coeffs[0];
  k = k - NTRU_N*((uint16_t)(NTRU_N - k - 1) >> 15);

  /* Return X^{N-k} * b(X) */
  /* This is a k-coefficient rotation. We do this by looking at the binary
     representation of k, rotating for every power of 2, and performing a cmov
     if the respective bit is set. */
  for (i = 0; i < NTRU_N; i++)
    r.coeffs[i] = mod3(fsign * b.coeffs[i]);

  for (i = 0; i < 10; i++) {
    for (j = 0; j < NTRU_N; j++) {
      temp_r->coeffs[j] = r.coeffs[(j + (1 << i)) % NTRU_N];
    }
    cmov((unsigned char *)&(r.coeffs),
         (unsigned char *)&(temp_r->coeffs), sizeof(uint16_t) * NTRU_N, -crypto_uint16_bottombit_mask(k));
    k >>= 1;
  }

  /* Reduce modulo Phi_n */
  for(i=0; i<NTRU_N; i++) {
    rbytes[2*i] = mod3(r.coeffs[i] + 2*r.coeffs[NTRU_N-1]);
    rbytes[2*i+1] = 0;
  }

  for(i=NTRU_N; i<NTRU_N_PADDED; i++) {
    rbytes[2*i] = 0;
    rbytes[2*i+1] = 0;
  }

  return 0;
}

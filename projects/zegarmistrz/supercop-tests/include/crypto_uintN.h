/* auto-generated: cd cryptoint; ./autogen */
/* cryptoint 20250414 */

#ifndef crypto_uintN_h
#define crypto_uintN_h

#include <inttypes.h>
#define crypto_uintN uintN_t
#define crypto_uintN_signed intN_t

#define crypto_uintN_signed_optblocker namespace_uintN_signed_optblocker
extern volatile crypto_uintN_signed crypto_uintN_signed_optblocker;

__attribute__((unused))
static inline
crypto_uintN crypto_uintN_load(const unsigned char *crypto_uintN_s) {
  crypto_uintN crypto_uintN_z = 0;
  int crypto_uintN_k;
  for (crypto_uintN_k = 0;crypto_uintN_k < N;crypto_uintN_k += 8)
    crypto_uintN_z |= ((crypto_uintN) (*crypto_uintN_s++)) << crypto_uintN_k;
  return crypto_uintN_z;
}

__attribute__((unused))
static inline
crypto_uintN crypto_uintN_load_bigendian(const unsigned char *crypto_uintN_s) {
  crypto_uintN crypto_uintN_z = 0;
  int crypto_uintN_k;
  for (crypto_uintN_k = N - 8;crypto_uintN_k >= 0;crypto_uintN_k -= 8)
    crypto_uintN_z |= ((crypto_uintN) (*crypto_uintN_s++)) << crypto_uintN_k;
  return crypto_uintN_z;
}

__attribute__((unused))
static inline
void crypto_uintN_store(unsigned char *crypto_uintN_s,crypto_uintN crypto_uintN_x) {
  int crypto_uintN_k;
  for (crypto_uintN_k = 0;crypto_uintN_k < N;crypto_uintN_k += 8)
    *crypto_uintN_s++ = crypto_uintN_x >> crypto_uintN_k;
}

__attribute__((unused))
static inline
void crypto_uintN_store_bigendian(unsigned char *crypto_uintN_s,crypto_uintN crypto_uintN_x) {
  int crypto_uintN_k;
  for (crypto_uintN_k = N - 8;crypto_uintN_k >= 0;crypto_uintN_k -= 8)
    *crypto_uintN_s++ = crypto_uintN_x >> crypto_uintN_k;
}

__attribute__((unused))
static inline
crypto_uintN_signed crypto_uintN_signed_negative_mask(crypto_uintN_signed crypto_uintN_x) {
#if amd64
 8:  readasm("amd64; int8 crypto_uintN_x; crypto_uintN_x signed>>= 7");
16:  readasm("amd64; int16 crypto_uintN_x; crypto_uintN_x signed>>= 15");
32:  readasm("amd64; int32 crypto_uintN_x; crypto_uintN_x signed>>= 31");
64:  readasm("amd64; int64 crypto_uintN_x; crypto_uintN_x signed>>= 63");
  return crypto_uintN_x;
#elif arm64
  crypto_uintN_signed crypto_uintN_y;
 8:  readasm("arm64; int8 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = -(1 & (crypto_uintN_x unsigned>> 7))");
16:  readasm("arm64; int16 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = -(1 & (crypto_uintN_x unsigned>> 15))");
32:  readasm("arm64; int32 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = crypto_uintN_x signed>> 31");
64:  readasm("arm64; int64 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = crypto_uintN_x signed>> 63");
  return crypto_uintN_y;
#elif arm32
  crypto_uintN_signed crypto_uintN_y;
 8:  readasm("arm32; int8 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = (int8) crypto_uintN_x; crypto_uintN_y = crypto_uintN_y signed>> 31");
16:  readasm("arm32; int16 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = (int16) crypto_uintN_x; crypto_uintN_y = crypto_uintN_y signed>> 31");
32:  readasm("arm32; int32 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = crypto_uintN_x signed>> 31");
64:  readasm("arm32; int64 crypto_uintN_x crypto_uintN_y; crypto_uintN_y.lo = crypto_uintN_x.hi signed>> 31; crypto_uintN_y.hi = crypto_uintN_y.lo");
  return crypto_uintN_y;
#elif sparc32
  crypto_uintN_signed crypto_uintN_y;
 8:  readasm("sparc32; int8 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = crypto_uintN_x << 24; crypto_uintN_y = crypto_uintN_y signed>> 31");
16:  readasm("sparc32; int16 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = crypto_uintN_x << 16; crypto_uintN_y = crypto_uintN_y signed>> 31");
32:  readasm("sparc32; int32 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = crypto_uintN_x signed>> 31");
64:  readasm("sparc32; int64 crypto_uintN_x crypto_uintN_y; crypto_uintN_y.lo = crypto_uintN_x.hi signed>> 31; crypto_uintN_y.hi = crypto_uintN_y.lo");
  return crypto_uintN_y;
#else
  crypto_uintN_x >>= N-6;
  crypto_uintN_x += crypto_uintN_signed_optblocker;
  crypto_uintN_x >>= 5;
  return crypto_uintN_x;
#endif
}

__attribute__((unused))
static inline
crypto_uintN crypto_uintN_topbit_01(crypto_uintN crypto_uintN_x) {
#if amd64
 8:  readasm("amd64; int8 crypto_uintN_x; crypto_uintN_x unsigned>>= 7");
16:  readasm("amd64; int16 crypto_uintN_x; crypto_uintN_x unsigned>>= 15");
32:  readasm("amd64; int32 crypto_uintN_x; crypto_uintN_x unsigned>>= 31");
64:  readasm("amd64; int64 crypto_uintN_x; crypto_uintN_x unsigned>>= 63");
  return crypto_uintN_x;
#elif arm64
  crypto_uintN crypto_uintN_y;
 8:  readasm("arm64; int8 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = 1 & (crypto_uintN_x unsigned>> 7)");
16:  readasm("arm64; int16 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = 1 & (crypto_uintN_x unsigned>> 15)");
32:  readasm("arm64; int32 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = crypto_uintN_x unsigned>> 31");
64:  readasm("arm64; int64 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = crypto_uintN_x unsigned>> 63");
  return crypto_uintN_y;
#elif arm32
  crypto_uintN_signed crypto_uintN_y;
 8:  readasm("arm32; int8 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = (uint8) crypto_uintN_x; crypto_uintN_y = crypto_uintN_y unsigned>> 7");
16:  readasm("arm32; int16 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = (uint16) crypto_uintN_x; crypto_uintN_y = crypto_uintN_y unsigned>> 15");
32:  readasm("arm32; int32 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = crypto_uintN_x unsigned>> 31");
64:  readasm("arm32; int64 crypto_uintN_x crypto_uintN_y; crypto_uintN_y.lo = crypto_uintN_x.hi unsigned>> 31; crypto_uintN_y.hi = 0");
  return crypto_uintN_y;
#elif sparc32
  crypto_uintN crypto_uintN_y;
 8:  readasm("sparc32; int8 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = (uint8) crypto_uintN_x; crypto_uintN_y = crypto_uintN_y unsigned>> 7");
16:  readasm("sparc32; int16 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = (uint16) crypto_uintN_x; crypto_uintN_y = crypto_uintN_y unsigned>> 15");
32:  readasm("sparc32; int32 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = crypto_uintN_x unsigned>> 31");
64:  readasm("sparc32; int64 crypto_uintN_x crypto_uintN_y; crypto_uintN_y.lo = crypto_uintN_x.hi unsigned>> 31; crypto_uintN_y.hi = 0");
  return crypto_uintN_y;
#else
  crypto_uintN_x >>= N-6;
  crypto_uintN_x += crypto_uintN_signed_optblocker;
  crypto_uintN_x >>= 5;
  return crypto_uintN_x;
#endif
}

__attribute__((unused))
static inline
crypto_uintN crypto_uintN_topbit_mask(crypto_uintN crypto_uintN_x) {
  return crypto_uintN_signed_negative_mask(crypto_uintN_x);
}

__attribute__((unused))
static inline
crypto_uintN crypto_uintN_bottombit_mask(crypto_uintN crypto_uintN_x) {
#if amd64
 8:  readasm("amd64; int8 crypto_uintN_x; crypto_uintN_x &= 1");
16:  readasm("amd64; int16 crypto_uintN_x; crypto_uintN_x &= 1");
32:  readasm("amd64; int32 crypto_uintN_x; crypto_uintN_x &= 1");
64:  readasm("amd64; int64 crypto_uintN_x; crypto_uintN_x &= 1");
  return -crypto_uintN_x;
#elif arm64
  crypto_uintN crypto_uintN_y;
 8:  readasm("arm64; int8 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = -(1 & (crypto_uintN_x unsigned>> 0))");
16:  readasm("arm64; int16 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = -(1 & (crypto_uintN_x unsigned>> 0))");
32:  readasm("arm64; int32 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = -(1 & (crypto_uintN_x unsigned>> 0))");
64:  readasm("arm64; int64 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = -(1 & (crypto_uintN_x unsigned>> 0))");
  return crypto_uintN_y;
#elif arm32
  crypto_uintN crypto_uintN_y;
 8:  readasm("arm32; int8 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = crypto_uintN_x & 1; crypto_uintN_y = -crypto_uintN_y; crypto_uintN_y = (uint8) crypto_uintN_y");
16:  readasm("arm32; int16 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = crypto_uintN_x & 1; crypto_uintN_y = -crypto_uintN_y; crypto_uintN_y = (uint16) crypto_uintN_y");
32:  readasm("arm32; int32 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = crypto_uintN_x & 1; crypto_uintN_y = -crypto_uintN_y");
64:  readasm("arm32; int64 crypto_uintN_x crypto_uintN_y; crypto_uintN_y.lo = crypto_uintN_x.lo & 1; crypto_uintN_y.lo = -crypto_uintN_y.lo; crypto_uintN_y.hi = crypto_uintN_y.lo");
  return crypto_uintN_y;
#elif sparc32
  crypto_uintN crypto_uintN_y;
 8:  readasm("sparc32; int8 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = crypto_uintN_x & 1; crypto_uintN_y = -crypto_uintN_y; crypto_uintN_y = (uint8) crypto_uintN_y");
16:  readasm("sparc32; int16 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = crypto_uintN_x & 1; crypto_uintN_y = -crypto_uintN_y; crypto_uintN_y = (uint16) crypto_uintN_y");
32:  readasm("sparc32; int32 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = crypto_uintN_x & 1; crypto_uintN_y = -crypto_uintN_y");
64:  readasm("sparc32; int64 crypto_uintN_x crypto_uintN_y; crypto_uintN_y.lo = crypto_uintN_x.lo & 1; crypto_uintN_y.lo = -crypto_uintN_y.lo; crypto_uintN_y.hi = crypto_uintN_y.lo");
  return crypto_uintN_y;
#else
  crypto_uintN_x &= 1 + crypto_uintN_signed_optblocker;
  return -crypto_uintN_x;
#endif
}

__attribute__((unused))
static inline
crypto_uintN crypto_uintN_bottombit_01(crypto_uintN crypto_uintN_x) {
#if amd64
 8:  readasm("amd64; int8 crypto_uintN_x; crypto_uintN_x &= 1");
16:  readasm("amd64; int16 crypto_uintN_x; crypto_uintN_x &= 1");
32:  readasm("amd64; int32 crypto_uintN_x; crypto_uintN_x &= 1");
64:  readasm("amd64; int64 crypto_uintN_x; crypto_uintN_x &= 1");
  return crypto_uintN_x;
#elif arm64
  crypto_uintN crypto_uintN_y;
 8:  readasm("arm64; int8 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = 1 & (crypto_uintN_x unsigned>> 0)");
16:  readasm("arm64; int16 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = 1 & (crypto_uintN_x unsigned>> 0)");
32:  readasm("arm64; int32 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = 1 & (crypto_uintN_x unsigned>> 0)");
64:  readasm("arm64; int64 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = 1 & (crypto_uintN_x unsigned>> 0)");
  return crypto_uintN_y;
#elif arm32
  crypto_uintN crypto_uintN_y;
 8:  readasm("arm32; int8 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = crypto_uintN_x & 1");
16:  readasm("arm32; int16 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = crypto_uintN_x & 1");
32:  readasm("arm32; int32 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = crypto_uintN_x & 1");
64:  readasm("arm32; int64 crypto_uintN_x crypto_uintN_y; crypto_uintN_y.lo = crypto_uintN_x.lo & 1; crypto_uintN_y.hi = 0");
  return crypto_uintN_y;
#elif sparc32
  crypto_uintN crypto_uintN_y;
 8:  readasm("sparc32; int8 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = crypto_uintN_x & 1");
16:  readasm("sparc32; int16 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = crypto_uintN_x & 1");
32:  readasm("sparc32; int32 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = crypto_uintN_x & 1");
64:  readasm("sparc32; int64 crypto_uintN_x crypto_uintN_y; crypto_uintN_y.lo = crypto_uintN_x.lo & 1; crypto_uintN_y.hi = 0");
  return crypto_uintN_y;
#else
  crypto_uintN_x &= 1 + crypto_uintN_signed_optblocker;
  return crypto_uintN_x;
#endif
}

__attribute__((unused))
static inline
crypto_uintN crypto_uintN_bitinrangepublicpos_mask(crypto_uintN crypto_uintN_x,crypto_uintN crypto_uintN_s) {
#if amd64
 8:  readasm("amd64; int8 crypto_uintN_x crypto_uintN_s; crypto_uintN_x unsigned>>= crypto_uintN_s");
16:  readasm("amd64; int16 crypto_uintN_x crypto_uintN_s; crypto_uintN_x unsigned>>= crypto_uintN_s");
32:  readasm("amd64; int32 crypto_uintN_x crypto_uintN_s; crypto_uintN_x unsigned>>= crypto_uintN_s");
64:  readasm("amd64; int64 crypto_uintN_x crypto_uintN_s; crypto_uintN_x unsigned>>= crypto_uintN_s");
#elif arm64
 8:  readasm("arm64; int8 crypto_uintN_x crypto_uintN_s; crypto_uintN_x = (uint8) crypto_uintN_x; crypto_uintN_x = crypto_uintN_x unsigned>> crypto_uintN_s");
16:  readasm("arm64; int16 crypto_uintN_x crypto_uintN_s; crypto_uintN_x = (uint16) crypto_uintN_x; crypto_uintN_x = crypto_uintN_x unsigned>> crypto_uintN_s");
32:  readasm("arm64; int32 crypto_uintN_x crypto_uintN_s; crypto_uintN_x = crypto_uintN_x unsigned>> crypto_uintN_s");
64:  readasm("arm64; int64 crypto_uintN_x crypto_uintN_s; crypto_uintN_x = crypto_uintN_x unsigned>> crypto_uintN_s");
#elif arm32
 8:  readasm("arm32; int8 crypto_uintN_x crypto_uintN_s; crypto_uintN_s = crypto_uintN_s & 7; crypto_uintN_x = (uint8) crypto_uintN_x; crypto_uintN_x = crypto_uintN_x unsigned>> crypto_uintN_s");
16:  readasm("arm32; int16 crypto_uintN_x crypto_uintN_s; crypto_uintN_s = crypto_uintN_s & 15; crypto_uintN_x = (uint16) crypto_uintN_x; crypto_uintN_x = crypto_uintN_x unsigned>> crypto_uintN_s");
32:  readasm("arm32; int32 crypto_uintN_x crypto_uintN_s; crypto_uintN_s = crypto_uintN_s & 31; crypto_uintN_x = crypto_uintN_x unsigned>> crypto_uintN_s");
64:  readasm("arm32; int64 crypto_uintN_x crypto_uintN_s; crypto_uintN_s.lo = crypto_uintN_s.lo & 63; crypto_uintN_x.lo = crypto_uintN_x.lo unsigned>> crypto_uintN_s.lo; crypto_uintN_s.hi = 32 - crypto_uintN_s.lo; crypto_uintN_x.lo = crypto_uintN_x.lo | (crypto_uintN_x.hi << crypto_uintN_s.hi); crypto_uintN_s.hi = crypto_uintN_s.lo - 32; crypto_uintN_x.lo = crypto_uintN_x.lo | (crypto_uintN_x.hi unsigned>> crypto_uintN_s.hi); crypto_uintN_x.hi = crypto_uintN_x.hi unsigned>> crypto_uintN_s.lo");
#elif sparc32
 8:  readasm("sparc32; int8 crypto_uintN_x crypto_uintN_s; crypto_uintN_s = crypto_uintN_s & 7; crypto_uintN_x = crypto_uintN_x unsigned>> crypto_uintN_s");
16:  readasm("sparc32; int16 crypto_uintN_x crypto_uintN_s; crypto_uintN_s = crypto_uintN_s & 15; crypto_uintN_x = crypto_uintN_x unsigned>> crypto_uintN_s");
32:  readasm("sparc32; int32 crypto_uintN_x crypto_uintN_s; crypto_uintN_x = crypto_uintN_x unsigned>> crypto_uintN_s");
64:  crypto_uintN crypto_uintN_y, crypto_uintN_z;
64:  readasm("sparc32; int64 crypto_uintN_x crypto_uintN_y crypto_uintN_z crypto_uintN_s; crypto_uintN_s.hi = ~crypto_uintN_s.lo; crypto_uintN_z.hi = crypto_uintN_s.lo << 26; crypto_uintN_z.lo = crypto_uintN_x.hi << 1; crypto_uintN_y.lo = crypto_uintN_x.lo unsigned>> crypto_uintN_s.lo; crypto_uintN_z.lo = crypto_uintN_z.lo << crypto_uintN_s.hi; crypto_uintN_y.hi = crypto_uintN_x.hi unsigned>> crypto_uintN_s.lo; crypto_uintN_z.hi = crypto_uintN_z.hi signed>> 31; crypto_uintN_y.lo = crypto_uintN_y.lo | crypto_uintN_z.lo; crypto_uintN_z.lo = crypto_uintN_y.lo ^ crypto_uintN_y.hi; crypto_uintN_x.hi = crypto_uintN_z.hi & crypto_uintN_y.hi; crypto_uintN_z.lo = crypto_uintN_z.hi & crypto_uintN_z.lo; crypto_uintN_x.hi = crypto_uintN_x.hi ^ crypto_uintN_y.hi; crypto_uintN_x.lo = crypto_uintN_y.lo ^ crypto_uintN_z.lo");
#else
  crypto_uintN_x >>= crypto_uintN_s ^ crypto_uintN_signed_optblocker;
#endif
  return crypto_uintN_bottombit_mask(crypto_uintN_x);
}

__attribute__((unused))
static inline
crypto_uintN crypto_uintN_bitinrangepublicpos_01(crypto_uintN crypto_uintN_x,crypto_uintN crypto_uintN_s) {
#if amd64
 8:  readasm("amd64; int8 crypto_uintN_x crypto_uintN_s; crypto_uintN_x unsigned>>= crypto_uintN_s");
16:  readasm("amd64; int16 crypto_uintN_x crypto_uintN_s; crypto_uintN_x unsigned>>= crypto_uintN_s");
32:  readasm("amd64; int32 crypto_uintN_x crypto_uintN_s; crypto_uintN_x unsigned>>= crypto_uintN_s");
64:  readasm("amd64; int64 crypto_uintN_x crypto_uintN_s; crypto_uintN_x unsigned>>= crypto_uintN_s");
#elif arm64
 8:  readasm("arm64; int8 crypto_uintN_x crypto_uintN_s; crypto_uintN_x = (uint8) crypto_uintN_x; crypto_uintN_x = crypto_uintN_x unsigned>> crypto_uintN_s");
16:  readasm("arm64; int16 crypto_uintN_x crypto_uintN_s; crypto_uintN_x = (uint16) crypto_uintN_x; crypto_uintN_x = crypto_uintN_x unsigned>> crypto_uintN_s");
32:  readasm("arm64; int32 crypto_uintN_x crypto_uintN_s; crypto_uintN_x = crypto_uintN_x unsigned>> crypto_uintN_s");
64:  readasm("arm64; int64 crypto_uintN_x crypto_uintN_s; crypto_uintN_x = crypto_uintN_x unsigned>> crypto_uintN_s");
#elif arm32
 8:  readasm("arm32; int8 crypto_uintN_x crypto_uintN_s; crypto_uintN_s = crypto_uintN_s & 7; crypto_uintN_x = (uint8) crypto_uintN_x; crypto_uintN_x = crypto_uintN_x unsigned>> crypto_uintN_s");
16:  readasm("arm32; int16 crypto_uintN_x crypto_uintN_s; crypto_uintN_s = crypto_uintN_s & 15; crypto_uintN_x = (uint16) crypto_uintN_x; crypto_uintN_x = crypto_uintN_x unsigned>> crypto_uintN_s");
32:  readasm("arm32; int32 crypto_uintN_x crypto_uintN_s; crypto_uintN_s = crypto_uintN_s & 31; crypto_uintN_x = crypto_uintN_x unsigned>> crypto_uintN_s");
64:  readasm("arm32; int64 crypto_uintN_x crypto_uintN_s; crypto_uintN_s.lo = crypto_uintN_s.lo & 63; crypto_uintN_x.lo = crypto_uintN_x.lo unsigned>> crypto_uintN_s.lo; crypto_uintN_s.hi = 32 - crypto_uintN_s.lo; crypto_uintN_x.lo = crypto_uintN_x.lo | (crypto_uintN_x.hi << crypto_uintN_s.hi); crypto_uintN_s.hi = crypto_uintN_s.lo - 32; crypto_uintN_x.lo = crypto_uintN_x.lo | (crypto_uintN_x.hi unsigned>> crypto_uintN_s.hi); crypto_uintN_x.hi = crypto_uintN_x.hi unsigned>> crypto_uintN_s.lo");
#elif sparc32
 8:  readasm("sparc32; int8 crypto_uintN_x crypto_uintN_s; crypto_uintN_s = crypto_uintN_s & 7; crypto_uintN_x = crypto_uintN_x unsigned>> crypto_uintN_s");
16:  readasm("sparc32; int16 crypto_uintN_x crypto_uintN_s; crypto_uintN_s = crypto_uintN_s & 15; crypto_uintN_x = crypto_uintN_x unsigned>> crypto_uintN_s");
32:  readasm("sparc32; int32 crypto_uintN_x crypto_uintN_s; crypto_uintN_x = crypto_uintN_x unsigned>> crypto_uintN_s");
64:  crypto_uintN crypto_uintN_y, crypto_uintN_z;
64:  readasm("sparc32; int64 crypto_uintN_x crypto_uintN_y crypto_uintN_z crypto_uintN_s; crypto_uintN_s.hi = ~crypto_uintN_s.lo; crypto_uintN_z.hi = crypto_uintN_s.lo << 26; crypto_uintN_z.lo = crypto_uintN_x.hi << 1; crypto_uintN_y.lo = crypto_uintN_x.lo unsigned>> crypto_uintN_s.lo; crypto_uintN_z.lo = crypto_uintN_z.lo << crypto_uintN_s.hi; crypto_uintN_y.hi = crypto_uintN_x.hi unsigned>> crypto_uintN_s.lo; crypto_uintN_z.hi = crypto_uintN_z.hi signed>> 31; crypto_uintN_y.lo = crypto_uintN_y.lo | crypto_uintN_z.lo; crypto_uintN_z.lo = crypto_uintN_y.lo ^ crypto_uintN_y.hi; crypto_uintN_x.hi = crypto_uintN_z.hi & crypto_uintN_y.hi; crypto_uintN_z.lo = crypto_uintN_z.hi & crypto_uintN_z.lo; crypto_uintN_x.hi = crypto_uintN_x.hi ^ crypto_uintN_y.hi; crypto_uintN_x.lo = crypto_uintN_y.lo ^ crypto_uintN_z.lo");
#else
  crypto_uintN_x >>= crypto_uintN_s ^ crypto_uintN_signed_optblocker;
#endif
  return crypto_uintN_bottombit_01(crypto_uintN_x);
}

__attribute__((unused))
static inline
crypto_uintN crypto_uintN_shlmod(crypto_uintN crypto_uintN_x,crypto_uintN crypto_uintN_s) {
#if amd64
 8:  crypto_uintN_s &= 7;
 8:  readasm("amd64; int8 crypto_uintN_x crypto_uintN_s; crypto_uintN_x <<= crypto_uintN_s");
16:  crypto_uintN_s &= 15;
16:  readasm("amd64; int16 crypto_uintN_x crypto_uintN_s; crypto_uintN_x <<= crypto_uintN_s");
32:  readasm("amd64; int32 crypto_uintN_x crypto_uintN_s; crypto_uintN_x <<= crypto_uintN_s");
64:  readasm("amd64; int64 crypto_uintN_x crypto_uintN_s; crypto_uintN_x <<= crypto_uintN_s");
#elif arm64
 8:  crypto_uintN_s &= 7;
 8:  readasm("arm64; int8 crypto_uintN_x crypto_uintN_s; crypto_uintN_x = (uint8) crypto_uintN_x; crypto_uintN_x = crypto_uintN_x << crypto_uintN_s");
16:  crypto_uintN_s &= 15;
16:  readasm("arm64; int16 crypto_uintN_x crypto_uintN_s; crypto_uintN_x = (uint16) crypto_uintN_x; crypto_uintN_x = crypto_uintN_x << crypto_uintN_s");
32:  readasm("arm64; int32 crypto_uintN_x crypto_uintN_s; crypto_uintN_x = crypto_uintN_x << crypto_uintN_s");
64:  readasm("arm64; int64 crypto_uintN_x crypto_uintN_s; crypto_uintN_x = crypto_uintN_x << crypto_uintN_s");
#elif arm32
 8:  crypto_uintN_s &= 7;
 8:  readasm("arm32; int8 crypto_uintN_x crypto_uintN_s; crypto_uintN_x = crypto_uintN_x << crypto_uintN_s; crypto_uintN_x = (uint8) crypto_uintN_x");
16:  crypto_uintN_s &= 15;
16:  readasm("arm32; int16 crypto_uintN_x crypto_uintN_s; crypto_uintN_x = crypto_uintN_x << crypto_uintN_s; crypto_uintN_x = (uint16) crypto_uintN_x");
32:  crypto_uintN_s &= 31;
32:  readasm("arm32; int32 crypto_uintN_x crypto_uintN_s; crypto_uintN_x = crypto_uintN_x << crypto_uintN_s");
64:  readasm("arm32; int64 crypto_uintN_x crypto_uintN_s; crypto_uintN_s.lo = crypto_uintN_s.lo & 63; crypto_uintN_x.hi = crypto_uintN_x.hi << crypto_uintN_s.lo; crypto_uintN_s.hi = crypto_uintN_s.lo - 32; crypto_uintN_x.hi = crypto_uintN_x.hi | (crypto_uintN_x.lo << crypto_uintN_s.hi); crypto_uintN_s.hi = 32 - crypto_uintN_s.lo; crypto_uintN_x.hi = crypto_uintN_x.hi | (crypto_uintN_x.lo unsigned>> crypto_uintN_s.hi); crypto_uintN_x.lo = crypto_uintN_x.lo << crypto_uintN_s.lo");
#elif sparc32
 8:  crypto_uintN_s &= 7;
 8:  readasm("sparc32; int8 crypto_uintN_x crypto_uintN_s; crypto_uintN_x = crypto_uintN_x << crypto_uintN_s; crypto_uintN_x = (uint8) crypto_uintN_x");
16:  crypto_uintN_s &= 15;
16:  readasm("sparc32; int16 crypto_uintN_x crypto_uintN_s; crypto_uintN_x = crypto_uintN_x << crypto_uintN_s; crypto_uintN_x = (uint16) crypto_uintN_x");
32:  readasm("sparc32; int32 crypto_uintN_x crypto_uintN_s; crypto_uintN_x = crypto_uintN_x << crypto_uintN_s");
64:  crypto_uintN crypto_uintN_y, crypto_uintN_z;
64:  readasm("sparc32; int64 crypto_uintN_x crypto_uintN_y crypto_uintN_z crypto_uintN_s; crypto_uintN_s.hi = ~crypto_uintN_s.lo; crypto_uintN_z.hi = crypto_uintN_s.lo << 26; crypto_uintN_z.lo = crypto_uintN_x.lo unsigned>> 1; crypto_uintN_y.lo = crypto_uintN_x.lo << crypto_uintN_s.lo; crypto_uintN_z.lo = crypto_uintN_z.lo unsigned>> crypto_uintN_s.hi; crypto_uintN_y.hi = crypto_uintN_x.hi << crypto_uintN_s.lo; crypto_uintN_z.hi = crypto_uintN_z.hi signed>> 31; crypto_uintN_y.hi = crypto_uintN_y.hi | crypto_uintN_z.lo; crypto_uintN_z.lo = crypto_uintN_y.lo ^ crypto_uintN_y.hi; crypto_uintN_x.lo = crypto_uintN_z.hi & crypto_uintN_y.lo; crypto_uintN_z.lo = crypto_uintN_z.hi & crypto_uintN_z.lo; crypto_uintN_x.lo = crypto_uintN_x.lo ^ crypto_uintN_y.lo; crypto_uintN_x.hi = crypto_uintN_y.hi ^ crypto_uintN_z.lo");
#else
  int crypto_uintN_k, crypto_uintN_l;
  for (crypto_uintN_l = 0,crypto_uintN_k = 1;crypto_uintN_k < N;++crypto_uintN_l,crypto_uintN_k *= 2)
    crypto_uintN_x ^= (crypto_uintN_x ^ (crypto_uintN_x << crypto_uintN_k)) & crypto_uintN_bitinrangepublicpos_mask(crypto_uintN_s,crypto_uintN_l);
#endif
  return crypto_uintN_x;
}

__attribute__((unused))
static inline
crypto_uintN crypto_uintN_shrmod(crypto_uintN crypto_uintN_x,crypto_uintN crypto_uintN_s) {
#if amd64
 8:  crypto_uintN_s &= 7;
 8:  readasm("amd64; int8 crypto_uintN_x crypto_uintN_s; crypto_uintN_x unsigned>>= crypto_uintN_s");
16:  crypto_uintN_s &= 15;
16:  readasm("amd64; int16 crypto_uintN_x crypto_uintN_s; crypto_uintN_x unsigned>>= crypto_uintN_s");
32:  readasm("amd64; int32 crypto_uintN_x crypto_uintN_s; crypto_uintN_x unsigned>>= crypto_uintN_s");
64:  readasm("amd64; int64 crypto_uintN_x crypto_uintN_s; crypto_uintN_x unsigned>>= crypto_uintN_s");
#elif arm64
 8:  crypto_uintN_s &= 7;
 8:  readasm("arm64; int8 crypto_uintN_x crypto_uintN_s; crypto_uintN_x = (uint8) crypto_uintN_x; crypto_uintN_x = crypto_uintN_x unsigned>> crypto_uintN_s");
16:  crypto_uintN_s &= 15;
16:  readasm("arm64; int16 crypto_uintN_x crypto_uintN_s; crypto_uintN_x = (uint16) crypto_uintN_x; crypto_uintN_x = crypto_uintN_x unsigned>> crypto_uintN_s");
32:  readasm("arm64; int32 crypto_uintN_x crypto_uintN_s; crypto_uintN_x = crypto_uintN_x unsigned>> crypto_uintN_s");
64:  readasm("arm64; int64 crypto_uintN_x crypto_uintN_s; crypto_uintN_x = crypto_uintN_x unsigned>> crypto_uintN_s");
#elif arm32
 8:  crypto_uintN_s &= 7;
 8:  readasm("arm32; int8 crypto_uintN_x crypto_uintN_s; crypto_uintN_x = (uint8) crypto_uintN_x; crypto_uintN_x = crypto_uintN_x unsigned>> crypto_uintN_s");
16:  crypto_uintN_s &= 15;
16:  readasm("arm32; int16 crypto_uintN_x crypto_uintN_s; crypto_uintN_x = (uint16) crypto_uintN_x; crypto_uintN_x = crypto_uintN_x unsigned>> crypto_uintN_s");
32:  crypto_uintN_s &= 31;
32:  readasm("arm32; int32 crypto_uintN_x crypto_uintN_s; crypto_uintN_x = crypto_uintN_x unsigned>> crypto_uintN_s");
64:  readasm("arm32; int64 crypto_uintN_x crypto_uintN_s; crypto_uintN_s.lo = crypto_uintN_s.lo & 63; crypto_uintN_x.lo = crypto_uintN_x.lo unsigned>> crypto_uintN_s.lo; crypto_uintN_s.hi = 32 - crypto_uintN_s.lo; crypto_uintN_x.lo = crypto_uintN_x.lo | (crypto_uintN_x.hi << crypto_uintN_s.hi); crypto_uintN_s.hi = crypto_uintN_s.lo - 32; crypto_uintN_x.lo = crypto_uintN_x.lo | (crypto_uintN_x.hi unsigned>> crypto_uintN_s.hi); crypto_uintN_x.hi = crypto_uintN_x.hi unsigned>> crypto_uintN_s.lo");
#elif sparc32
 8:  crypto_uintN_s &= 7;
 8:  readasm("sparc32; int8 crypto_uintN_x crypto_uintN_s; crypto_uintN_x = crypto_uintN_x unsigned>> crypto_uintN_s");
16:  crypto_uintN_s &= 15;
16:  readasm("sparc32; int16 crypto_uintN_x crypto_uintN_s; crypto_uintN_x = crypto_uintN_x unsigned>> crypto_uintN_s");
32:  readasm("sparc32; int32 crypto_uintN_x crypto_uintN_s; crypto_uintN_x = crypto_uintN_x unsigned>> crypto_uintN_s");
64:  crypto_uintN crypto_uintN_y, crypto_uintN_z;
64:  readasm("sparc32; int64 crypto_uintN_x crypto_uintN_y crypto_uintN_z crypto_uintN_s; crypto_uintN_s.hi = ~crypto_uintN_s.lo; crypto_uintN_z.hi = crypto_uintN_s.lo << 26; crypto_uintN_z.lo = crypto_uintN_x.hi << 1; crypto_uintN_y.lo = crypto_uintN_x.lo unsigned>> crypto_uintN_s.lo; crypto_uintN_z.lo = crypto_uintN_z.lo << crypto_uintN_s.hi; crypto_uintN_y.hi = crypto_uintN_x.hi unsigned>> crypto_uintN_s.lo; crypto_uintN_z.hi = crypto_uintN_z.hi signed>> 31; crypto_uintN_y.lo = crypto_uintN_y.lo | crypto_uintN_z.lo; crypto_uintN_z.lo = crypto_uintN_y.lo ^ crypto_uintN_y.hi; crypto_uintN_x.hi = crypto_uintN_z.hi & crypto_uintN_y.hi; crypto_uintN_z.lo = crypto_uintN_z.hi & crypto_uintN_z.lo; crypto_uintN_x.hi = crypto_uintN_x.hi ^ crypto_uintN_y.hi; crypto_uintN_x.lo = crypto_uintN_y.lo ^ crypto_uintN_z.lo");
#else
  int crypto_uintN_k, crypto_uintN_l;
  for (crypto_uintN_l = 0,crypto_uintN_k = 1;crypto_uintN_k < N;++crypto_uintN_l,crypto_uintN_k *= 2)
    crypto_uintN_x ^= (crypto_uintN_x ^ (crypto_uintN_x >> crypto_uintN_k)) & crypto_uintN_bitinrangepublicpos_mask(crypto_uintN_s,crypto_uintN_l);
#endif
  return crypto_uintN_x;
}

__attribute__((unused))
static inline
crypto_uintN crypto_uintN_bitmod_mask(crypto_uintN crypto_uintN_x,crypto_uintN crypto_uintN_s) {
  crypto_uintN_x = crypto_uintN_shrmod(crypto_uintN_x,crypto_uintN_s);
  return crypto_uintN_bottombit_mask(crypto_uintN_x);
}

__attribute__((unused))
static inline
crypto_uintN crypto_uintN_bitmod_01(crypto_uintN crypto_uintN_x,crypto_uintN crypto_uintN_s) {
  crypto_uintN_x = crypto_uintN_shrmod(crypto_uintN_x,crypto_uintN_s);
  return crypto_uintN_bottombit_01(crypto_uintN_x);
}

__attribute__((unused))
static inline
crypto_uintN crypto_uintN_nonzero_mask(crypto_uintN crypto_uintN_x) {
#if amd64
 8:  crypto_uintN crypto_uintN_z;
 8:  int32_t crypto_uintN_x32 = crypto_uintN_x,crypto_uintN_q32,crypto_uintN_z32;
 8:  readasm("amd64; int32 crypto_uintN_x32 crypto_uintN_q32 crypto_uintN_z32; crypto_uintN_z32 = 0; crypto_uintN_q32 = -1; crypto_uintN_x32 & (int8) crypto_uintN_x32; crypto_uintN_z32 = crypto_uintN_q32 if !=");
 8:  crypto_uintN_z = crypto_uintN_z32;
16:  crypto_uintN crypto_uintN_q,crypto_uintN_z;
16:  readasm("amd64; int16 crypto_uintN_x crypto_uintN_q crypto_uintN_z; crypto_uintN_z = 0; crypto_uintN_q = -1; crypto_uintN_x & crypto_uintN_x; crypto_uintN_z = crypto_uintN_q if !=");
32:  crypto_uintN crypto_uintN_q,crypto_uintN_z;
32:  readasm("amd64; int32 crypto_uintN_x crypto_uintN_q crypto_uintN_z; crypto_uintN_z = 0; crypto_uintN_q = -1; crypto_uintN_x & crypto_uintN_x; crypto_uintN_z = crypto_uintN_q if !=");
64:  crypto_uintN crypto_uintN_q,crypto_uintN_z;
64:  readasm("amd64; int64 crypto_uintN_x crypto_uintN_q crypto_uintN_z; crypto_uintN_z = 0; crypto_uintN_q = -1; crypto_uintN_x & crypto_uintN_x; crypto_uintN_z = crypto_uintN_q if !=");
  return crypto_uintN_z;
#elif arm64
  crypto_uintN crypto_uintN_z;
 8:  readasm("arm64; int8 crypto_uintN_x crypto_uintN_z; crypto_uintN_x & 255; crypto_uintN_z = -1 if != else 0");
16:  readasm("arm64; int16 crypto_uintN_x crypto_uintN_z; crypto_uintN_x & 65535; crypto_uintN_z = -1 if != else 0");
32:  readasm("arm64; int32 crypto_uintN_x crypto_uintN_z; crypto_uintN_x - 0; crypto_uintN_z = -1 if != else 0");
64:  readasm("arm64; int64 crypto_uintN_x crypto_uintN_z; crypto_uintN_x - 0; crypto_uintN_z = -1 if != else 0");
  return crypto_uintN_z;
#elif arm32
 8:  readasm("arm32; int8 crypto_uintN_x; crypto_uintN_x = (uint8) crypto_uintN_x; crypto_uintN_x - 0; crypto_uintN_x = -1 if !=");
16:  readasm("arm32; int16 crypto_uintN_x; crypto_uintN_x = (uint16) crypto_uintN_x; crypto_uintN_x - 0; crypto_uintN_x = -1 if !=");
32:  readasm("arm32; int32 crypto_uintN_x; crypto_uintN_x - 0; crypto_uintN_x = -1 if !=");
64:  readasm("arm32; int64 crypto_uintN_x; flags, crypto_uintN_x.lo = crypto_uintN_x.lo | crypto_uintN_x.hi; crypto_uintN_x.lo = -1 if !=; crypto_uintN_x.hi = crypto_uintN_x.lo");
  return crypto_uintN_x;
#elif sparc32
  crypto_uintN crypto_uintN_z;
 8:  readasm("sparc32; int8 crypto_uintN_x crypto_uintN_z; crypto_uintN_x = (uint8) crypto_uintN_x; 0 - crypto_uintN_x; crypto_uintN_z = -carry");
16:  readasm("sparc32; int16 crypto_uintN_x crypto_uintN_z; crypto_uintN_x = (uint16) crypto_uintN_x; 0 - crypto_uintN_x; crypto_uintN_z = -carry");
32:  readasm("sparc32; int32 crypto_uintN_x crypto_uintN_z; 0 - crypto_uintN_x; crypto_uintN_z = -carry");
64:  readasm("sparc32; int64 crypto_uintN_x crypto_uintN_z; crypto_uintN_z.lo = crypto_uintN_x.lo | crypto_uintN_x.hi; 0 - crypto_uintN_z.lo; crypto_uintN_z.lo = -carry; crypto_uintN_z.hi = crypto_uintN_z.lo");
  return crypto_uintN_z;
#else
  crypto_uintN_x |= -crypto_uintN_x;
  return crypto_uintN_signed_negative_mask(crypto_uintN_x);
#endif
}

__attribute__((unused))
static inline
crypto_uintN crypto_uintN_nonzero_01(crypto_uintN crypto_uintN_x) {
#if amd64
 8:  crypto_uintN crypto_uintN_z;
 8:  int32_t crypto_uintN_x32 = crypto_uintN_x,crypto_uintN_q32,crypto_uintN_z32;
 8:  readasm("amd64; int32 crypto_uintN_x32 crypto_uintN_q32 crypto_uintN_z32; crypto_uintN_z32 = 0; crypto_uintN_q32 = 1; crypto_uintN_x32 & (int8) crypto_uintN_x32; crypto_uintN_z32 = crypto_uintN_q32 if !=");
 8:  crypto_uintN_z = crypto_uintN_z32;
16:  crypto_uintN crypto_uintN_q,crypto_uintN_z;
16:  readasm("amd64; int16 crypto_uintN_x crypto_uintN_q crypto_uintN_z; crypto_uintN_z = 0; crypto_uintN_q = 1; crypto_uintN_x & crypto_uintN_x; crypto_uintN_z = crypto_uintN_q if !=");
32:  crypto_uintN crypto_uintN_q,crypto_uintN_z;
32:  readasm("amd64; int32 crypto_uintN_x crypto_uintN_q crypto_uintN_z; crypto_uintN_z = 0; crypto_uintN_q = 1; crypto_uintN_x & crypto_uintN_x; crypto_uintN_z = crypto_uintN_q if !=");
64:  crypto_uintN crypto_uintN_q,crypto_uintN_z;
64:  readasm("amd64; int64 crypto_uintN_x crypto_uintN_q crypto_uintN_z; crypto_uintN_z = 0; crypto_uintN_q = 1; crypto_uintN_x & crypto_uintN_x; crypto_uintN_z = crypto_uintN_q if !=");
  return crypto_uintN_z;
#elif arm64
  crypto_uintN crypto_uintN_z;
 8:  readasm("arm64; int8 crypto_uintN_x crypto_uintN_z; crypto_uintN_x & 255; crypto_uintN_z = 1 if != else 0");
16:  readasm("arm64; int16 crypto_uintN_x crypto_uintN_z; crypto_uintN_x & 65535; crypto_uintN_z = 1 if != else 0");
32:  readasm("arm64; int32 crypto_uintN_x crypto_uintN_z; crypto_uintN_x - 0; crypto_uintN_z = 1 if != else 0");
64:  readasm("arm64; int64 crypto_uintN_x crypto_uintN_z; crypto_uintN_x - 0; crypto_uintN_z = 1 if != else 0");
  return crypto_uintN_z;
#elif arm32
 8:  readasm("arm32; int8 crypto_uintN_x; crypto_uintN_x = (uint8) crypto_uintN_x; crypto_uintN_x - 0; crypto_uintN_x = 1 if !=");
16:  readasm("arm32; int16 crypto_uintN_x; crypto_uintN_x = (uint16) crypto_uintN_x; crypto_uintN_x - 0; crypto_uintN_x = 1 if !=");
32:  readasm("arm32; int32 crypto_uintN_x; crypto_uintN_x - 0; crypto_uintN_x = 1 if !=");
64:  readasm("arm32; int64 crypto_uintN_x; flags, crypto_uintN_x.lo = crypto_uintN_x.lo | crypto_uintN_x.hi; crypto_uintN_x.lo = 1 if !=; crypto_uintN_x.hi = 0");
  return crypto_uintN_x;
#elif sparc32
  crypto_uintN crypto_uintN_z;
 8:  readasm("sparc32; int8 crypto_uintN_x crypto_uintN_z; crypto_uintN_x = (uint8) crypto_uintN_x; 0 - crypto_uintN_x; crypto_uintN_z = carry");
16:  readasm("sparc32; int16 crypto_uintN_x crypto_uintN_z; crypto_uintN_x = (uint16) crypto_uintN_x; 0 - crypto_uintN_x; crypto_uintN_z = carry");
32:  readasm("sparc32; int32 crypto_uintN_x crypto_uintN_z; 0 - crypto_uintN_x; crypto_uintN_z = carry");
64:  readasm("sparc32; int64 crypto_uintN_x crypto_uintN_z; crypto_uintN_z.lo = crypto_uintN_x.lo | crypto_uintN_x.hi; 0 - crypto_uintN_z.lo; crypto_uintN_z.lo = carry; crypto_uintN_z.hi = 0");
  return crypto_uintN_z;
#else
  crypto_uintN_x |= -crypto_uintN_x;
  return crypto_uintN_topbit_01(crypto_uintN_x);
#endif
}

__attribute__((unused))
static inline
crypto_uintN crypto_uintN_zero_mask(crypto_uintN crypto_uintN_x) {
#if amd64
 8:  crypto_uintN crypto_uintN_z;
 8:  int32_t crypto_uintN_x32 = crypto_uintN_x,crypto_uintN_q32,crypto_uintN_z32;
 8:  readasm("amd64; int32 crypto_uintN_x32 crypto_uintN_q32 crypto_uintN_z32; crypto_uintN_z32 = 0; crypto_uintN_q32 = -1; crypto_uintN_x32 & (int8) crypto_uintN_x32; crypto_uintN_z32 = crypto_uintN_q32 if =");
 8:  crypto_uintN_z = crypto_uintN_z32;
16:  crypto_uintN crypto_uintN_q,crypto_uintN_z;
16:  readasm("amd64; int16 crypto_uintN_x crypto_uintN_q crypto_uintN_z; crypto_uintN_z = 0; crypto_uintN_q = -1; crypto_uintN_x & crypto_uintN_x; crypto_uintN_z = crypto_uintN_q if =");
32:  crypto_uintN crypto_uintN_q,crypto_uintN_z;
32:  readasm("amd64; int32 crypto_uintN_x crypto_uintN_q crypto_uintN_z; crypto_uintN_z = 0; crypto_uintN_q = -1; crypto_uintN_x & crypto_uintN_x; crypto_uintN_z = crypto_uintN_q if =");
64:  crypto_uintN crypto_uintN_q,crypto_uintN_z;
64:  readasm("amd64; int64 crypto_uintN_x crypto_uintN_q crypto_uintN_z; crypto_uintN_z = 0; crypto_uintN_q = -1; crypto_uintN_x & crypto_uintN_x; crypto_uintN_z = crypto_uintN_q if =");
  return crypto_uintN_z;
#elif arm64
  crypto_uintN crypto_uintN_z;
 8:  readasm("arm64; int8 crypto_uintN_x crypto_uintN_z; crypto_uintN_x & 255; crypto_uintN_z = -1 if = else 0");
16:  readasm("arm64; int16 crypto_uintN_x crypto_uintN_z; crypto_uintN_x & 65535; crypto_uintN_z = -1 if = else 0");
32:  readasm("arm64; int32 crypto_uintN_x crypto_uintN_z; crypto_uintN_x - 0; crypto_uintN_z = -1 if = else 0");
64:  readasm("arm64; int64 crypto_uintN_x crypto_uintN_z; crypto_uintN_x - 0; crypto_uintN_z = -1 if = else 0");
  return crypto_uintN_z;
#elif sparc32
  crypto_uintN crypto_uintN_z;
 8:  readasm("sparc32; int8 crypto_uintN_x crypto_uintN_z; crypto_uintN_x = (uint8) crypto_uintN_x; 0 - crypto_uintN_x; crypto_uintN_z = carry - 1");
16:  readasm("sparc32; int16 crypto_uintN_x crypto_uintN_z; crypto_uintN_x = (uint16) crypto_uintN_x; 0 - crypto_uintN_x; crypto_uintN_z = carry - 1");
32:  readasm("sparc32; int32 crypto_uintN_x crypto_uintN_z; 0 - crypto_uintN_x; crypto_uintN_z = carry - 1");
64:  readasm("sparc32; int64 crypto_uintN_x crypto_uintN_z; crypto_uintN_z.lo = crypto_uintN_x.lo | crypto_uintN_x.hi; 0 - crypto_uintN_z.lo; crypto_uintN_z.lo = carry - 1; crypto_uintN_z.hi = crypto_uintN_z.lo");
  return crypto_uintN_z;
#else
  return ~crypto_uintN_nonzero_mask(crypto_uintN_x);
#endif
}

__attribute__((unused))
static inline
crypto_uintN crypto_uintN_zero_01(crypto_uintN crypto_uintN_x) {
#if amd64
 8:  crypto_uintN crypto_uintN_z;
 8:  int32_t crypto_uintN_x32 = crypto_uintN_x,crypto_uintN_q32,crypto_uintN_z32;
 8:  readasm("amd64; int32 crypto_uintN_x32 crypto_uintN_q32 crypto_uintN_z32; crypto_uintN_z32 = 0; crypto_uintN_q32 = 1; crypto_uintN_x32 & (int8) crypto_uintN_x32; crypto_uintN_z32 = crypto_uintN_q32 if =");
 8:  crypto_uintN_z = crypto_uintN_z32;
16:  crypto_uintN crypto_uintN_q,crypto_uintN_z;
16:  readasm("amd64; int16 crypto_uintN_x crypto_uintN_q crypto_uintN_z; crypto_uintN_z = 0; crypto_uintN_q = 1; crypto_uintN_x & crypto_uintN_x; crypto_uintN_z = crypto_uintN_q if =");
32:  crypto_uintN crypto_uintN_q,crypto_uintN_z;
32:  readasm("amd64; int32 crypto_uintN_x crypto_uintN_q crypto_uintN_z; crypto_uintN_z = 0; crypto_uintN_q = 1; crypto_uintN_x & crypto_uintN_x; crypto_uintN_z = crypto_uintN_q if =");
64:  crypto_uintN crypto_uintN_q,crypto_uintN_z;
64:  readasm("amd64; int64 crypto_uintN_x crypto_uintN_q crypto_uintN_z; crypto_uintN_z = 0; crypto_uintN_q = 1; crypto_uintN_x & crypto_uintN_x; crypto_uintN_z = crypto_uintN_q if =");
  return crypto_uintN_z;
#elif arm64
  crypto_uintN crypto_uintN_z;
 8:  readasm("arm64; int8 crypto_uintN_x crypto_uintN_z; crypto_uintN_x & 255; crypto_uintN_z = 1 if = else 0");
16:  readasm("arm64; int16 crypto_uintN_x crypto_uintN_z; crypto_uintN_x & 65535; crypto_uintN_z = 1 if = else 0");
32:  readasm("arm64; int32 crypto_uintN_x crypto_uintN_z; crypto_uintN_x - 0; crypto_uintN_z = 1 if = else 0");
64:  readasm("arm64; int64 crypto_uintN_x crypto_uintN_z; crypto_uintN_x - 0; crypto_uintN_z = 1 if = else 0");
  return crypto_uintN_z;
#elif sparc32
  crypto_uintN crypto_uintN_z;
 8:  readasm("sparc32; int8 crypto_uintN_x crypto_uintN_z; crypto_uintN_x = (uint8) crypto_uintN_x; 0 - crypto_uintN_x; crypto_uintN_z = 1 - carry");
16:  readasm("sparc32; int16 crypto_uintN_x crypto_uintN_z; crypto_uintN_x = (uint16) crypto_uintN_x; 0 - crypto_uintN_x; crypto_uintN_z = 1 - carry");
32:  readasm("sparc32; int32 crypto_uintN_x crypto_uintN_z; 0 - crypto_uintN_x; crypto_uintN_z = 1 - carry");
64:  readasm("sparc32; int64 crypto_uintN_x crypto_uintN_z; crypto_uintN_z.lo = crypto_uintN_x.lo | crypto_uintN_x.hi; 0 - crypto_uintN_z.lo; crypto_uintN_z.lo = 1 - carry; crypto_uintN_z.hi = 0");
  return crypto_uintN_z;
#else
  return 1-crypto_uintN_nonzero_01(crypto_uintN_x);
#endif
}

__attribute__((unused))
static inline
crypto_uintN crypto_uintN_unequal_mask(crypto_uintN crypto_uintN_x,crypto_uintN crypto_uintN_y) {
#if amd64
 8:  crypto_uintN crypto_uintN_z;
 8:  int32_t crypto_uintN_x32 = crypto_uintN_x,crypto_uintN_y32 = crypto_uintN_y,crypto_uintN_q32,crypto_uintN_z32;
 8:  readasm("amd64; int32 crypto_uintN_x32 crypto_uintN_q32 crypto_uintN_y32 crypto_uintN_z32; crypto_uintN_z32 = 0; crypto_uintN_q32 = -1; crypto_uintN_x32 - (int8) crypto_uintN_y32; crypto_uintN_z32 = crypto_uintN_q32 if !=");
 8:  crypto_uintN_z = crypto_uintN_z32;
16:  crypto_uintN crypto_uintN_q,crypto_uintN_z;
16:  readasm("amd64; int16 crypto_uintN_x crypto_uintN_q crypto_uintN_y crypto_uintN_z; crypto_uintN_z = 0; crypto_uintN_q = -1; crypto_uintN_x - crypto_uintN_y; crypto_uintN_z = crypto_uintN_q if !=");
32:  crypto_uintN crypto_uintN_q,crypto_uintN_z;
32:  readasm("amd64; int32 crypto_uintN_x crypto_uintN_q crypto_uintN_y crypto_uintN_z; crypto_uintN_z = 0; crypto_uintN_q = -1; crypto_uintN_x - crypto_uintN_y; crypto_uintN_z = crypto_uintN_q if !=");
64:  crypto_uintN crypto_uintN_q,crypto_uintN_z;
64:  readasm("amd64; int64 crypto_uintN_x crypto_uintN_q crypto_uintN_y crypto_uintN_z; crypto_uintN_z = 0; crypto_uintN_q = -1; crypto_uintN_x - crypto_uintN_y; crypto_uintN_z = crypto_uintN_q if !=");
  return crypto_uintN_z;
#elif arm64
  crypto_uintN crypto_uintN_z;
 8:  readasm("arm64; int8 crypto_uintN_x crypto_uintN_y crypto_uintN_z; crypto_uintN_z = (uint8) crypto_uintN_x; crypto_uintN_z - (uint8) crypto_uintN_y; crypto_uintN_z = -1 if != else 0");
16:  readasm("arm64; int16 crypto_uintN_x crypto_uintN_y crypto_uintN_z; crypto_uintN_z = (uint16) crypto_uintN_x; crypto_uintN_z - (uint16) crypto_uintN_y; crypto_uintN_z = -1 if != else 0");
32:  readasm("arm64; int32 crypto_uintN_x crypto_uintN_y crypto_uintN_z; crypto_uintN_x - crypto_uintN_y; crypto_uintN_z = -1 if != else 0");
64:  readasm("arm64; int64 crypto_uintN_x crypto_uintN_y crypto_uintN_z; crypto_uintN_x - crypto_uintN_y; crypto_uintN_z = -1 if != else 0");
  return crypto_uintN_z;
#else
  return crypto_uintN_nonzero_mask(crypto_uintN_x ^ crypto_uintN_y);
#endif
}

__attribute__((unused))
static inline
crypto_uintN crypto_uintN_unequal_01(crypto_uintN crypto_uintN_x,crypto_uintN crypto_uintN_y) {
#if amd64
 8:  crypto_uintN crypto_uintN_z;
 8:  int32_t crypto_uintN_x32 = crypto_uintN_x,crypto_uintN_y32 = crypto_uintN_y,crypto_uintN_q32,crypto_uintN_z32;
 8:  readasm("amd64; int32 crypto_uintN_x32 crypto_uintN_q32 crypto_uintN_y32 crypto_uintN_z32; crypto_uintN_z32 = 0; crypto_uintN_q32 = 1; crypto_uintN_x32 - (int8) crypto_uintN_y32; crypto_uintN_z32 = crypto_uintN_q32 if !=");
 8:  crypto_uintN_z = crypto_uintN_z32;
16:  crypto_uintN crypto_uintN_q,crypto_uintN_z;
16:  readasm("amd64; int16 crypto_uintN_x crypto_uintN_q crypto_uintN_y crypto_uintN_z; crypto_uintN_z = 0; crypto_uintN_q = 1; crypto_uintN_x - crypto_uintN_y; crypto_uintN_z = crypto_uintN_q if !=");
32:  crypto_uintN crypto_uintN_q,crypto_uintN_z;
32:  readasm("amd64; int32 crypto_uintN_x crypto_uintN_q crypto_uintN_y crypto_uintN_z; crypto_uintN_z = 0; crypto_uintN_q = 1; crypto_uintN_x - crypto_uintN_y; crypto_uintN_z = crypto_uintN_q if !=");
64:  crypto_uintN crypto_uintN_q,crypto_uintN_z;
64:  readasm("amd64; int64 crypto_uintN_x crypto_uintN_q crypto_uintN_y crypto_uintN_z; crypto_uintN_z = 0; crypto_uintN_q = 1; crypto_uintN_x - crypto_uintN_y; crypto_uintN_z = crypto_uintN_q if !=");
  return crypto_uintN_z;
#elif arm64
  crypto_uintN crypto_uintN_z;
 8:  readasm("arm64; int8 crypto_uintN_x crypto_uintN_y crypto_uintN_z; crypto_uintN_z = (uint8) crypto_uintN_x; crypto_uintN_z - (uint8) crypto_uintN_y; crypto_uintN_z = 1 if != else 0");
16:  readasm("arm64; int16 crypto_uintN_x crypto_uintN_y crypto_uintN_z; crypto_uintN_z = (uint16) crypto_uintN_x; crypto_uintN_z - (uint16) crypto_uintN_y; crypto_uintN_z = 1 if != else 0");
32:  readasm("arm64; int32 crypto_uintN_x crypto_uintN_y crypto_uintN_z; crypto_uintN_x - crypto_uintN_y; crypto_uintN_z = 1 if != else 0");
64:  readasm("arm64; int64 crypto_uintN_x crypto_uintN_y crypto_uintN_z; crypto_uintN_x - crypto_uintN_y; crypto_uintN_z = 1 if != else 0");
  return crypto_uintN_z;
#else
  return crypto_uintN_nonzero_01(crypto_uintN_x ^ crypto_uintN_y);
#endif
}

__attribute__((unused))
static inline
crypto_uintN crypto_uintN_equal_mask(crypto_uintN crypto_uintN_x,crypto_uintN crypto_uintN_y) {
#if amd64
 8:  crypto_uintN crypto_uintN_z;
 8:  int32_t crypto_uintN_x32 = crypto_uintN_x,crypto_uintN_y32 = crypto_uintN_y,crypto_uintN_q32,crypto_uintN_z32;
 8:  readasm("amd64; int32 crypto_uintN_x32 crypto_uintN_q32 crypto_uintN_y32 crypto_uintN_z32; crypto_uintN_z32 = 0; crypto_uintN_q32 = -1; crypto_uintN_x32 - (int8) crypto_uintN_y32; crypto_uintN_z32 = crypto_uintN_q32 if =");
 8:  crypto_uintN_z = crypto_uintN_z32;
16:  crypto_uintN crypto_uintN_q,crypto_uintN_z;
16:  readasm("amd64; int16 crypto_uintN_x crypto_uintN_q crypto_uintN_y crypto_uintN_z; crypto_uintN_z = 0; crypto_uintN_q = -1; crypto_uintN_x - crypto_uintN_y; crypto_uintN_z = crypto_uintN_q if =");
32:  crypto_uintN crypto_uintN_q,crypto_uintN_z;
32:  readasm("amd64; int32 crypto_uintN_x crypto_uintN_q crypto_uintN_y crypto_uintN_z; crypto_uintN_z = 0; crypto_uintN_q = -1; crypto_uintN_x - crypto_uintN_y; crypto_uintN_z = crypto_uintN_q if =");
64:  crypto_uintN crypto_uintN_q,crypto_uintN_z;
64:  readasm("amd64; int64 crypto_uintN_x crypto_uintN_q crypto_uintN_y crypto_uintN_z; crypto_uintN_z = 0; crypto_uintN_q = -1; crypto_uintN_x - crypto_uintN_y; crypto_uintN_z = crypto_uintN_q if =");
  return crypto_uintN_z;
#elif arm64
  crypto_uintN crypto_uintN_z;
 8:  readasm("arm64; int8 crypto_uintN_x crypto_uintN_y crypto_uintN_z; crypto_uintN_z = (uint8) crypto_uintN_x; crypto_uintN_z - (uint8) crypto_uintN_y; crypto_uintN_z = -1 if = else 0");
16:  readasm("arm64; int16 crypto_uintN_x crypto_uintN_y crypto_uintN_z; crypto_uintN_z = (uint16) crypto_uintN_x; crypto_uintN_z - (uint16) crypto_uintN_y; crypto_uintN_z = -1 if = else 0");
32:  readasm("arm64; int32 crypto_uintN_x crypto_uintN_y crypto_uintN_z; crypto_uintN_x - crypto_uintN_y; crypto_uintN_z = -1 if = else 0");
64:  readasm("arm64; int64 crypto_uintN_x crypto_uintN_y crypto_uintN_z; crypto_uintN_x - crypto_uintN_y; crypto_uintN_z = -1 if = else 0");
  return crypto_uintN_z;
#else
  return crypto_uintN_zero_mask(crypto_uintN_x ^ crypto_uintN_y);
#endif
}

__attribute__((unused))
static inline
crypto_uintN crypto_uintN_equal_01(crypto_uintN crypto_uintN_x,crypto_uintN crypto_uintN_y) {
#if amd64
 8:  crypto_uintN crypto_uintN_z;
 8:  int32_t crypto_uintN_x32 = crypto_uintN_x,crypto_uintN_y32 = crypto_uintN_y,crypto_uintN_q32,crypto_uintN_z32;
 8:  readasm("amd64; int32 crypto_uintN_x32 crypto_uintN_q32 crypto_uintN_y32 crypto_uintN_z32; crypto_uintN_z32 = 0; crypto_uintN_q32 = 1; crypto_uintN_x32 - (int8) crypto_uintN_y32; crypto_uintN_z32 = crypto_uintN_q32 if =");
 8:  crypto_uintN_z = crypto_uintN_z32;
16:  crypto_uintN crypto_uintN_q,crypto_uintN_z;
16:  readasm("amd64; int16 crypto_uintN_x crypto_uintN_q crypto_uintN_y crypto_uintN_z; crypto_uintN_z = 0; crypto_uintN_q = 1; crypto_uintN_x - crypto_uintN_y; crypto_uintN_z = crypto_uintN_q if =");
32:  crypto_uintN crypto_uintN_q,crypto_uintN_z;
32:  readasm("amd64; int32 crypto_uintN_x crypto_uintN_q crypto_uintN_y crypto_uintN_z; crypto_uintN_z = 0; crypto_uintN_q = 1; crypto_uintN_x - crypto_uintN_y; crypto_uintN_z = crypto_uintN_q if =");
64:  crypto_uintN crypto_uintN_q,crypto_uintN_z;
64:  readasm("amd64; int64 crypto_uintN_x crypto_uintN_q crypto_uintN_y crypto_uintN_z; crypto_uintN_z = 0; crypto_uintN_q = 1; crypto_uintN_x - crypto_uintN_y; crypto_uintN_z = crypto_uintN_q if =");
  return crypto_uintN_z;
#elif arm64
  crypto_uintN crypto_uintN_z;
 8:  readasm("arm64; int8 crypto_uintN_x crypto_uintN_y crypto_uintN_z; crypto_uintN_z = (uint8) crypto_uintN_x; crypto_uintN_z - (uint8) crypto_uintN_y; crypto_uintN_z = 1 if = else 0");
16:  readasm("arm64; int16 crypto_uintN_x crypto_uintN_y crypto_uintN_z; crypto_uintN_z = (uint16) crypto_uintN_x; crypto_uintN_z - (uint16) crypto_uintN_y; crypto_uintN_z = 1 if = else 0");
32:  readasm("arm64; int32 crypto_uintN_x crypto_uintN_y crypto_uintN_z; crypto_uintN_x - crypto_uintN_y; crypto_uintN_z = 1 if = else 0");
64:  readasm("arm64; int64 crypto_uintN_x crypto_uintN_y crypto_uintN_z; crypto_uintN_x - crypto_uintN_y; crypto_uintN_z = 1 if = else 0");
  return crypto_uintN_z;
#else
  return crypto_uintN_zero_01(crypto_uintN_x ^ crypto_uintN_y);
#endif
}

__attribute__((unused))
static inline
crypto_uintN crypto_uintN_min(crypto_uintN crypto_uintN_x,crypto_uintN crypto_uintN_y) {
#if amd64
 8:  uint32_t crypto_uintN_x32 = crypto_uintN_x,crypto_uintN_y32 = crypto_uintN_y;
 8:  readasm("amd64; int32 crypto_uintN_x32 crypto_uintN_y32; crypto_uintN_x32 - (int8) crypto_uintN_y32; crypto_uintN_x32 = crypto_uintN_y32 if unsigned>");
 8:  crypto_uintN_x = crypto_uintN_x32;
16:  readasm("amd64; int16 crypto_uintN_x crypto_uintN_y; crypto_uintN_x - crypto_uintN_y; crypto_uintN_x = crypto_uintN_y if unsigned>");
32:  readasm("amd64; int32 crypto_uintN_x crypto_uintN_y; crypto_uintN_x - crypto_uintN_y; crypto_uintN_x = crypto_uintN_y if unsigned>");
64:  readasm("amd64; int64 crypto_uintN_x crypto_uintN_y; crypto_uintN_x - crypto_uintN_y; crypto_uintN_x = crypto_uintN_y if unsigned>");
  return crypto_uintN_x;
#elif arm64
 8:  readasm("arm64; int8 crypto_uintN_x crypto_uintN_y; crypto_uintN_x = (uint8) crypto_uintN_x; crypto_uintN_x - (uint8) crypto_uintN_y; crypto_uintN_x = crypto_uintN_x if unsigned< else crypto_uintN_y");
16:  readasm("arm64; int16 crypto_uintN_x crypto_uintN_y; crypto_uintN_x = (uint16) crypto_uintN_x; crypto_uintN_x - (uint16) crypto_uintN_y; crypto_uintN_x = crypto_uintN_x if unsigned< else crypto_uintN_y");
32:  readasm("arm64; int32 crypto_uintN_x crypto_uintN_y; crypto_uintN_x - crypto_uintN_y; crypto_uintN_x = crypto_uintN_x if unsigned< else crypto_uintN_y");
64:  readasm("arm64; int64 crypto_uintN_x crypto_uintN_y; crypto_uintN_x - crypto_uintN_y; crypto_uintN_x = crypto_uintN_x if unsigned< else crypto_uintN_y");
  return crypto_uintN_x;
#else
  crypto_uintN crypto_uintN_r = crypto_uintN_y ^ crypto_uintN_x;
  crypto_uintN crypto_uintN_z = crypto_uintN_y - crypto_uintN_x;
  crypto_uintN_z ^= crypto_uintN_r & (crypto_uintN_z ^ crypto_uintN_y ^ (((crypto_uintN) 1) << (N-1)));
  crypto_uintN_z = crypto_uintN_signed_negative_mask(crypto_uintN_z);
  crypto_uintN_z &= crypto_uintN_r;
  return crypto_uintN_x ^ crypto_uintN_z;
#endif
}

__attribute__((unused))
static inline
crypto_uintN crypto_uintN_max(crypto_uintN crypto_uintN_x,crypto_uintN crypto_uintN_y) {
#if amd64
 8:  uint32_t crypto_uintN_x32 = crypto_uintN_x,crypto_uintN_y32 = crypto_uintN_y;
 8:  readasm("amd64; int32 crypto_uintN_x32 crypto_uintN_y32; crypto_uintN_x32 - (int8) crypto_uintN_y32; crypto_uintN_x32 = crypto_uintN_y32 if unsigned<");
 8:  crypto_uintN_x = crypto_uintN_x32;
16:  readasm("amd64; int16 crypto_uintN_x crypto_uintN_y; crypto_uintN_x - crypto_uintN_y; crypto_uintN_x = crypto_uintN_y if unsigned<");
32:  readasm("amd64; int32 crypto_uintN_x crypto_uintN_y; crypto_uintN_x - crypto_uintN_y; crypto_uintN_x = crypto_uintN_y if unsigned<");
64:  readasm("amd64; int64 crypto_uintN_x crypto_uintN_y; crypto_uintN_x - crypto_uintN_y; crypto_uintN_x = crypto_uintN_y if unsigned<");
  return crypto_uintN_x;
#elif arm64
 8:  readasm("arm64; int8 crypto_uintN_x crypto_uintN_y; crypto_uintN_x = (uint8) crypto_uintN_x; crypto_uintN_x - (uint8) crypto_uintN_y; crypto_uintN_x = crypto_uintN_y if unsigned< else crypto_uintN_x");
16:  readasm("arm64; int16 crypto_uintN_x crypto_uintN_y; crypto_uintN_x = (uint16) crypto_uintN_x; crypto_uintN_x - (uint16) crypto_uintN_y; crypto_uintN_x = crypto_uintN_y if unsigned< else crypto_uintN_x");
32:  readasm("arm64; int32 crypto_uintN_x crypto_uintN_y; crypto_uintN_x - crypto_uintN_y; crypto_uintN_x = crypto_uintN_y if unsigned< else crypto_uintN_x");
64:  readasm("arm64; int64 crypto_uintN_x crypto_uintN_y; crypto_uintN_x - crypto_uintN_y; crypto_uintN_x = crypto_uintN_y if unsigned< else crypto_uintN_x");
  return crypto_uintN_x;
#else
  crypto_uintN crypto_uintN_r = crypto_uintN_y ^ crypto_uintN_x;
  crypto_uintN crypto_uintN_z = crypto_uintN_y - crypto_uintN_x;
  crypto_uintN_z ^= crypto_uintN_r & (crypto_uintN_z ^ crypto_uintN_y ^ (((crypto_uintN) 1) << (N-1)));
  crypto_uintN_z = crypto_uintN_signed_negative_mask(crypto_uintN_z);
  crypto_uintN_z &= crypto_uintN_r;
  return crypto_uintN_y ^ crypto_uintN_z;
#endif
}

__attribute__((unused))
static inline
void crypto_uintN_minmax(crypto_uintN *crypto_uintN_p,crypto_uintN *crypto_uintN_q) {
  crypto_uintN crypto_uintN_x = *crypto_uintN_p;
  crypto_uintN crypto_uintN_y = *crypto_uintN_q;
#if amd64
 8:  uint32_t crypto_uintN_x32 = crypto_uintN_x,crypto_uintN_y32 = crypto_uintN_y,crypto_uintN_z32;
 8:  readasm("amd64; int32 crypto_uintN_x32 crypto_uintN_y32 crypto_uintN_z32; crypto_uintN_x32 - (int8) crypto_uintN_y32; crypto_uintN_z32 = crypto_uintN_x32; crypto_uintN_x32 = crypto_uintN_y32 if unsigned>; crypto_uintN_y32 = crypto_uintN_z32 if unsigned>");
 8:  crypto_uintN_x = crypto_uintN_x32; crypto_uintN_y = crypto_uintN_y32;
16:  crypto_uintN crypto_uintN_z;
16:  readasm("amd64; int16 crypto_uintN_x crypto_uintN_y crypto_uintN_z; crypto_uintN_x - crypto_uintN_y; crypto_uintN_z = crypto_uintN_x; crypto_uintN_x = crypto_uintN_y if unsigned>; crypto_uintN_y = crypto_uintN_z if unsigned>");
32:  crypto_uintN crypto_uintN_z;
32:  readasm("amd64; int32 crypto_uintN_x crypto_uintN_y crypto_uintN_z; crypto_uintN_x - crypto_uintN_y; crypto_uintN_z = crypto_uintN_x; crypto_uintN_x = crypto_uintN_y if unsigned>; crypto_uintN_y = crypto_uintN_z if unsigned>");
64:  crypto_uintN crypto_uintN_z;
64:  readasm("amd64; int64 crypto_uintN_x crypto_uintN_y crypto_uintN_z; crypto_uintN_x - crypto_uintN_y; crypto_uintN_z = crypto_uintN_x; crypto_uintN_x = crypto_uintN_y if unsigned>; crypto_uintN_y = crypto_uintN_z if unsigned>");
  *crypto_uintN_p = crypto_uintN_x;
  *crypto_uintN_q = crypto_uintN_y;
#elif arm64
  crypto_uintN crypto_uintN_r, crypto_uintN_s;
 8:  readasm("arm64; int8 crypto_uintN_x crypto_uintN_y crypto_uintN_r crypto_uintN_s; crypto_uintN_x = (uint8) crypto_uintN_x; crypto_uintN_x - (uint8) crypto_uintN_y; crypto_uintN_r = crypto_uintN_x if unsigned< else crypto_uintN_y; crypto_uintN_s = crypto_uintN_y if unsigned< else crypto_uintN_x");
16:  readasm("arm64; int16 crypto_uintN_x crypto_uintN_y crypto_uintN_r crypto_uintN_s; crypto_uintN_x = (uint16) crypto_uintN_x; crypto_uintN_x - (uint16) crypto_uintN_y; crypto_uintN_r = crypto_uintN_x if unsigned< else crypto_uintN_y; crypto_uintN_s = crypto_uintN_y if unsigned< else crypto_uintN_x");
32:  readasm("arm64; int32 crypto_uintN_x crypto_uintN_y crypto_uintN_r crypto_uintN_s; crypto_uintN_x - crypto_uintN_y; crypto_uintN_r = crypto_uintN_x if unsigned< else crypto_uintN_y; crypto_uintN_s = crypto_uintN_y if unsigned< else crypto_uintN_x");
64:  readasm("arm64; int64 crypto_uintN_x crypto_uintN_y crypto_uintN_r crypto_uintN_s; crypto_uintN_x - crypto_uintN_y; crypto_uintN_r = crypto_uintN_x if unsigned< else crypto_uintN_y; crypto_uintN_s = crypto_uintN_y if unsigned< else crypto_uintN_x");
  *crypto_uintN_p = crypto_uintN_r;
  *crypto_uintN_q = crypto_uintN_s;
#else
  crypto_uintN crypto_uintN_r = crypto_uintN_y ^ crypto_uintN_x;
  crypto_uintN crypto_uintN_z = crypto_uintN_y - crypto_uintN_x;
  crypto_uintN_z ^= crypto_uintN_r & (crypto_uintN_z ^ crypto_uintN_y ^ (((crypto_uintN) 1) << (N-1)));
  crypto_uintN_z = crypto_uintN_signed_negative_mask(crypto_uintN_z);
  crypto_uintN_z &= crypto_uintN_r;
  crypto_uintN_x ^= crypto_uintN_z;
  crypto_uintN_y ^= crypto_uintN_z;
  *crypto_uintN_p = crypto_uintN_x;
  *crypto_uintN_q = crypto_uintN_y;
#endif
}

__attribute__((unused))
static inline
crypto_uintN crypto_uintN_smaller_mask(crypto_uintN crypto_uintN_x,crypto_uintN crypto_uintN_y) {
#if amd64
 8:  crypto_uintN crypto_uintN_z;
 8:  int32_t crypto_uintN_x32 = crypto_uintN_x,crypto_uintN_y32 = crypto_uintN_y,crypto_uintN_q32,crypto_uintN_z32;
 8:  readasm("amd64; int32 crypto_uintN_x32 crypto_uintN_q32 crypto_uintN_y32 crypto_uintN_z32; crypto_uintN_z32 = 0; crypto_uintN_q32 = -1; crypto_uintN_x32 - (int8) crypto_uintN_y32; crypto_uintN_z32 = crypto_uintN_q32 if unsigned<");
 8:  crypto_uintN_z = crypto_uintN_z32;
16:  crypto_uintN crypto_uintN_q,crypto_uintN_z;
16:  readasm("amd64; int16 crypto_uintN_x crypto_uintN_q crypto_uintN_y crypto_uintN_z; crypto_uintN_z = 0; crypto_uintN_q = -1; crypto_uintN_x - crypto_uintN_y; crypto_uintN_z = crypto_uintN_q if unsigned<");
32:  crypto_uintN crypto_uintN_q,crypto_uintN_z;
32:  readasm("amd64; int32 crypto_uintN_x crypto_uintN_q crypto_uintN_y crypto_uintN_z; crypto_uintN_z = 0; crypto_uintN_q = -1; crypto_uintN_x - crypto_uintN_y; crypto_uintN_z = crypto_uintN_q if unsigned<");
64:  crypto_uintN crypto_uintN_q,crypto_uintN_z;
64:  readasm("amd64; int64 crypto_uintN_x crypto_uintN_q crypto_uintN_y crypto_uintN_z; crypto_uintN_z = 0; crypto_uintN_q = -1; crypto_uintN_x - crypto_uintN_y; crypto_uintN_z = crypto_uintN_q if unsigned<");
  return crypto_uintN_z;
#elif arm64
  crypto_uintN crypto_uintN_z;
 8:  readasm("arm64; int8 crypto_uintN_x crypto_uintN_y crypto_uintN_z; crypto_uintN_z = (uint8) crypto_uintN_x; crypto_uintN_z - (uint8) crypto_uintN_y; crypto_uintN_z = -1 if unsigned< else 0");
16:  readasm("arm64; int16 crypto_uintN_x crypto_uintN_y crypto_uintN_z; crypto_uintN_z = (uint16) crypto_uintN_x; crypto_uintN_z - (uint16) crypto_uintN_y; crypto_uintN_z = -1 if unsigned< else 0");
32:  readasm("arm64; int32 crypto_uintN_x crypto_uintN_y crypto_uintN_z; crypto_uintN_x - crypto_uintN_y; crypto_uintN_z = -1 if unsigned< else 0");
64:  readasm("arm64; int64 crypto_uintN_x crypto_uintN_y crypto_uintN_z; crypto_uintN_x - crypto_uintN_y; crypto_uintN_z = -1 if unsigned< else 0");
  return crypto_uintN_z;
#else
  crypto_uintN crypto_uintN_r = crypto_uintN_x ^ crypto_uintN_y;
  crypto_uintN crypto_uintN_z = crypto_uintN_x - crypto_uintN_y;
  crypto_uintN_z ^= crypto_uintN_r & (crypto_uintN_z ^ crypto_uintN_x ^ (((crypto_uintN) 1) << (N-1)));
  return crypto_uintN_signed_negative_mask(crypto_uintN_z);
#endif
}

__attribute__((unused))
static inline
crypto_uintN crypto_uintN_smaller_01(crypto_uintN crypto_uintN_x,crypto_uintN crypto_uintN_y) {
#if amd64
 8:  crypto_uintN crypto_uintN_z;
 8:  int32_t crypto_uintN_x32 = crypto_uintN_x,crypto_uintN_y32 = crypto_uintN_y,crypto_uintN_q32,crypto_uintN_z32;
 8:  readasm("amd64; int32 crypto_uintN_x32 crypto_uintN_q32 crypto_uintN_y32 crypto_uintN_z32; crypto_uintN_z32 = 0; crypto_uintN_q32 = 1; crypto_uintN_x32 - (int8) crypto_uintN_y32; crypto_uintN_z32 = crypto_uintN_q32 if unsigned<");
 8:  crypto_uintN_z = crypto_uintN_z32;
16:  crypto_uintN crypto_uintN_q,crypto_uintN_z;
16:  readasm("amd64; int16 crypto_uintN_x crypto_uintN_q crypto_uintN_y crypto_uintN_z; crypto_uintN_z = 0; crypto_uintN_q = 1; crypto_uintN_x - crypto_uintN_y; crypto_uintN_z = crypto_uintN_q if unsigned<");
32:  crypto_uintN crypto_uintN_q,crypto_uintN_z;
32:  readasm("amd64; int32 crypto_uintN_x crypto_uintN_q crypto_uintN_y crypto_uintN_z; crypto_uintN_z = 0; crypto_uintN_q = 1; crypto_uintN_x - crypto_uintN_y; crypto_uintN_z = crypto_uintN_q if unsigned<");
64:  crypto_uintN crypto_uintN_q,crypto_uintN_z;
64:  readasm("amd64; int64 crypto_uintN_x crypto_uintN_q crypto_uintN_y crypto_uintN_z; crypto_uintN_z = 0; crypto_uintN_q = 1; crypto_uintN_x - crypto_uintN_y; crypto_uintN_z = crypto_uintN_q if unsigned<");
  return crypto_uintN_z;
#elif arm64
  crypto_uintN crypto_uintN_z;
 8:  readasm("arm64; int8 crypto_uintN_x crypto_uintN_y crypto_uintN_z; crypto_uintN_z = (uint8) crypto_uintN_x; crypto_uintN_z - (uint8) crypto_uintN_y; crypto_uintN_z = 1 if unsigned< else 0");
16:  readasm("arm64; int16 crypto_uintN_x crypto_uintN_y crypto_uintN_z; crypto_uintN_z = (uint16) crypto_uintN_x; crypto_uintN_z - (uint16) crypto_uintN_y; crypto_uintN_z = 1 if unsigned< else 0");
32:  readasm("arm64; int32 crypto_uintN_x crypto_uintN_y crypto_uintN_z; crypto_uintN_x - crypto_uintN_y; crypto_uintN_z = 1 if unsigned< else 0");
64:  readasm("arm64; int64 crypto_uintN_x crypto_uintN_y crypto_uintN_z; crypto_uintN_x - crypto_uintN_y; crypto_uintN_z = 1 if unsigned< else 0");
  return crypto_uintN_z;
#else
  crypto_uintN crypto_uintN_r = crypto_uintN_x ^ crypto_uintN_y;
  crypto_uintN crypto_uintN_z = crypto_uintN_x - crypto_uintN_y;
  crypto_uintN_z ^= crypto_uintN_r & (crypto_uintN_z ^ crypto_uintN_x ^ (((crypto_uintN) 1) << (N-1)));
  return crypto_uintN_topbit_01(crypto_uintN_z);
#endif
}

__attribute__((unused))
static inline
crypto_uintN crypto_uintN_leq_mask(crypto_uintN crypto_uintN_x,crypto_uintN crypto_uintN_y) {
#if amd64
 8:  crypto_uintN crypto_uintN_z;
 8:  int32_t crypto_uintN_x32 = crypto_uintN_x,crypto_uintN_y32 = crypto_uintN_y,crypto_uintN_q32,crypto_uintN_z32;
 8:  readasm("amd64; int32 crypto_uintN_x32 crypto_uintN_q32 crypto_uintN_y32 crypto_uintN_z32; crypto_uintN_z32 = 0; crypto_uintN_q32 = -1; crypto_uintN_x32 - (int8) crypto_uintN_y32; crypto_uintN_z32 = crypto_uintN_q32 if unsigned<=");
 8:  crypto_uintN_z = crypto_uintN_z32;
16:  crypto_uintN crypto_uintN_q,crypto_uintN_z;
16:  readasm("amd64; int16 crypto_uintN_x crypto_uintN_q crypto_uintN_y crypto_uintN_z; crypto_uintN_z = 0; crypto_uintN_q = -1; crypto_uintN_x - crypto_uintN_y; crypto_uintN_z = crypto_uintN_q if unsigned<=");
32:  crypto_uintN crypto_uintN_q,crypto_uintN_z;
32:  readasm("amd64; int32 crypto_uintN_x crypto_uintN_q crypto_uintN_y crypto_uintN_z; crypto_uintN_z = 0; crypto_uintN_q = -1; crypto_uintN_x - crypto_uintN_y; crypto_uintN_z = crypto_uintN_q if unsigned<=");
64:  crypto_uintN crypto_uintN_q,crypto_uintN_z;
64:  readasm("amd64; int64 crypto_uintN_x crypto_uintN_q crypto_uintN_y crypto_uintN_z; crypto_uintN_z = 0; crypto_uintN_q = -1; crypto_uintN_x - crypto_uintN_y; crypto_uintN_z = crypto_uintN_q if unsigned<=");
  return crypto_uintN_z;
#elif arm64
  crypto_uintN crypto_uintN_z;
 8:  readasm("arm64; int8 crypto_uintN_x crypto_uintN_y crypto_uintN_z; crypto_uintN_z = (uint8) crypto_uintN_x; crypto_uintN_z - (uint8) crypto_uintN_y; crypto_uintN_z = -1 if unsigned<= else 0");
16:  readasm("arm64; int16 crypto_uintN_x crypto_uintN_y crypto_uintN_z; crypto_uintN_z = (uint16) crypto_uintN_x; crypto_uintN_z - (uint16) crypto_uintN_y; crypto_uintN_z = -1 if unsigned<= else 0");
32:  readasm("arm64; int32 crypto_uintN_x crypto_uintN_y crypto_uintN_z; crypto_uintN_x - crypto_uintN_y; crypto_uintN_z = -1 if unsigned<= else 0");
64:  readasm("arm64; int64 crypto_uintN_x crypto_uintN_y crypto_uintN_z; crypto_uintN_x - crypto_uintN_y; crypto_uintN_z = -1 if unsigned<= else 0");
  return crypto_uintN_z;
#else
  return ~crypto_uintN_smaller_mask(crypto_uintN_y,crypto_uintN_x);
#endif
}

__attribute__((unused))
static inline
crypto_uintN crypto_uintN_leq_01(crypto_uintN crypto_uintN_x,crypto_uintN crypto_uintN_y) {
#if amd64
 8:  crypto_uintN crypto_uintN_z;
 8:  int32_t crypto_uintN_x32 = crypto_uintN_x,crypto_uintN_y32 = crypto_uintN_y,crypto_uintN_q32,crypto_uintN_z32;
 8:  readasm("amd64; int32 crypto_uintN_x32 crypto_uintN_q32 crypto_uintN_y32 crypto_uintN_z32; crypto_uintN_z32 = 0; crypto_uintN_q32 = 1; crypto_uintN_x32 - (int8) crypto_uintN_y32; crypto_uintN_z32 = crypto_uintN_q32 if unsigned<=");
 8:  crypto_uintN_z = crypto_uintN_z32;
16:  crypto_uintN crypto_uintN_q,crypto_uintN_z;
16:  readasm("amd64; int16 crypto_uintN_x crypto_uintN_q crypto_uintN_y crypto_uintN_z; crypto_uintN_z = 0; crypto_uintN_q = 1; crypto_uintN_x - crypto_uintN_y; crypto_uintN_z = crypto_uintN_q if unsigned<=");
32:  crypto_uintN crypto_uintN_q,crypto_uintN_z;
32:  readasm("amd64; int32 crypto_uintN_x crypto_uintN_q crypto_uintN_y crypto_uintN_z; crypto_uintN_z = 0; crypto_uintN_q = 1; crypto_uintN_x - crypto_uintN_y; crypto_uintN_z = crypto_uintN_q if unsigned<=");
64:  crypto_uintN crypto_uintN_q,crypto_uintN_z;
64:  readasm("amd64; int64 crypto_uintN_x crypto_uintN_q crypto_uintN_y crypto_uintN_z; crypto_uintN_z = 0; crypto_uintN_q = 1; crypto_uintN_x - crypto_uintN_y; crypto_uintN_z = crypto_uintN_q if unsigned<=");
  return crypto_uintN_z;
#elif arm64
  crypto_uintN crypto_uintN_z;
 8:  readasm("arm64; int8 crypto_uintN_x crypto_uintN_y crypto_uintN_z; crypto_uintN_z = (uint8) crypto_uintN_x; crypto_uintN_z - (uint8) crypto_uintN_y; crypto_uintN_z = 1 if unsigned<= else 0");
16:  readasm("arm64; int16 crypto_uintN_x crypto_uintN_y crypto_uintN_z; crypto_uintN_z = (uint16) crypto_uintN_x; crypto_uintN_z - (uint16) crypto_uintN_y; crypto_uintN_z = 1 if unsigned<= else 0");
32:  readasm("arm64; int32 crypto_uintN_x crypto_uintN_y crypto_uintN_z; crypto_uintN_x - crypto_uintN_y; crypto_uintN_z = 1 if unsigned<= else 0");
64:  readasm("arm64; int64 crypto_uintN_x crypto_uintN_y crypto_uintN_z; crypto_uintN_x - crypto_uintN_y; crypto_uintN_z = 1 if unsigned<= else 0");
  return crypto_uintN_z;
#else
  return 1-crypto_uintN_smaller_01(crypto_uintN_y,crypto_uintN_x);
#endif
}

__attribute__((unused))
static inline
int crypto_uintN_ones_num(crypto_uintN crypto_uintN_x) {
  crypto_uintN crypto_uintN_y = crypto_uintN_x;
 8:  const crypto_uintN C0 = 0x55;
16:  const crypto_uintN C0 = 0x5555;
32:  const crypto_uintN C0 = 0x55555555;
64:  const crypto_uintN C0 = 0x5555555555555555;
 8:  const crypto_uintN C1 = 0x33;
16:  const crypto_uintN C1 = 0x3333;
32:  const crypto_uintN C1 = 0x33333333;
64:  const crypto_uintN C1 = 0x3333333333333333;
 8:  const crypto_uintN C2 = 0x0f;
16:  const crypto_uintN C2 = 0x0f0f;
32:  const crypto_uintN C2 = 0x0f0f0f0f;
64:  const crypto_uintN C2 = 0x0f0f0f0f0f0f0f0f;
  crypto_uintN_y -= ((crypto_uintN_y >> 1) & C0);
  crypto_uintN_y = (crypto_uintN_y & C1) + ((crypto_uintN_y >> 2) & C1);
  crypto_uintN_y = (crypto_uintN_y + (crypto_uintN_y >> 4)) & C2;
16:  crypto_uintN_y = (crypto_uintN_y + (crypto_uintN_y >> 8)) & 0xff;
32:  crypto_uintN_y += crypto_uintN_y >> 8;
32:  crypto_uintN_y = (crypto_uintN_y + (crypto_uintN_y >> 16)) & 0xff;
64:  crypto_uintN_y += crypto_uintN_y >> 8;
64:  crypto_uintN_y += crypto_uintN_y >> 16;
64:  crypto_uintN_y = (crypto_uintN_y + (crypto_uintN_y >> 32)) & 0xff;
  return crypto_uintN_y;
}

__attribute__((unused))
static inline
int crypto_uintN_bottomzeros_num(crypto_uintN crypto_uintN_x) {
#if amd64
 8:  int32_t fallback = N;
 8:  int32_t crypto_uintN_x32 = crypto_uintN_x;
 8:  readasm("amd64; int32 crypto_uintN_x32 fallback; crypto_uintN_x32 = numbottomzeros_tricky crypto_uintN_x32; crypto_uintN_x32 = fallback if =");
 8:  crypto_uintN_x = crypto_uintN_x32;
16:  crypto_uintN fallback = N;
16:  readasm("amd64; int16 crypto_uintN_x fallback; crypto_uintN_x = numbottomzeros_tricky crypto_uintN_x; crypto_uintN_x = fallback if =");
32:  crypto_uintN fallback = N;
32:  readasm("amd64; int32 crypto_uintN_x fallback; crypto_uintN_x = numbottomzeros_tricky crypto_uintN_x; crypto_uintN_x = fallback if =");
64:  crypto_uintN fallback = N;
64:  readasm("amd64; int64 crypto_uintN_x fallback; crypto_uintN_x = numbottomzeros_tricky crypto_uintN_x; crypto_uintN_x = fallback if =");
  return crypto_uintN_x;
#elif arm64
  int64_t crypto_uintN_y;
 8:  readasm("arm64; int8 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = crypto_uintN_x | -256; crypto_uintN_y = bitrev32 crypto_uintN_y; crypto_uintN_y = numbottomzeros crypto_uintN_y");
16:  readasm("arm64; int16 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = crypto_uintN_x | -65536; crypto_uintN_y = bitrev32 crypto_uintN_y; crypto_uintN_y = numbottomzeros crypto_uintN_y");
32:  readasm("arm64; int32 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = bitrev32 crypto_uintN_x; crypto_uintN_y = numbottomzeros crypto_uintN_y");
64:  readasm("arm64; int64 crypto_uintN_x crypto_uintN_y; crypto_uintN_y = bitrev64 crypto_uintN_x; crypto_uintN_y = numbottomzeros crypto_uintN_y");
  return crypto_uintN_y;
#else
  crypto_uintN crypto_uintN_y = crypto_uintN_x ^ (crypto_uintN_x-1);
  crypto_uintN_y = ((crypto_uintN_signed) crypto_uintN_y) >> 1;
  crypto_uintN_y &= ~(crypto_uintN_x & (((crypto_uintN) 1) << (N-1)));
  return crypto_uintN_ones_num(crypto_uintN_y);
#endif
}

#endif

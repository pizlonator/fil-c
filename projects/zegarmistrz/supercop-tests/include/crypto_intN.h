/* auto-generated: cd cryptoint; ./autogen */
/* cryptoint 20250414 */

#ifndef crypto_intN_h
#define crypto_intN_h

#include <inttypes.h>
#define crypto_intN intN_t
#define crypto_intN_unsigned uintN_t

#define crypto_intN_optblocker namespace_intN_optblocker
extern volatile crypto_intN crypto_intN_optblocker;

__attribute__((unused))
static inline
crypto_intN crypto_intN_load(const unsigned char *crypto_intN_s) {
  crypto_intN crypto_intN_z = 0;
  int crypto_intN_k;
  for (crypto_intN_k = 0;crypto_intN_k < N;crypto_intN_k += 8)
    crypto_intN_z |= ((crypto_intN) (*crypto_intN_s++)) << crypto_intN_k;
  return crypto_intN_z;
}

__attribute__((unused))
static inline
crypto_intN crypto_intN_load_bigendian(const unsigned char *crypto_intN_s) {
  crypto_intN crypto_intN_z = 0;
  int crypto_intN_k;
  for (crypto_intN_k = N - 8;crypto_intN_k >= 0;crypto_intN_k -= 8)
    crypto_intN_z |= ((crypto_intN) (*crypto_intN_s++)) << crypto_intN_k;
  return crypto_intN_z;
}

__attribute__((unused))
static inline
void crypto_intN_store(unsigned char *crypto_intN_s,crypto_intN crypto_intN_x) {
  int crypto_intN_k;
  for (crypto_intN_k = 0;crypto_intN_k < N;crypto_intN_k += 8)
    *crypto_intN_s++ = crypto_intN_x >> crypto_intN_k;
}

__attribute__((unused))
static inline
void crypto_intN_store_bigendian(unsigned char *crypto_intN_s,crypto_intN crypto_intN_x) {
  int crypto_intN_k;
  for (crypto_intN_k = N - 8;crypto_intN_k >= 0;crypto_intN_k -= 8)
    *crypto_intN_s++ = crypto_intN_x >> crypto_intN_k;
}

__attribute__((unused))
static inline
crypto_intN crypto_intN_negative_mask(crypto_intN crypto_intN_x) {
#if amd64
 8:  readasm("amd64; int8 crypto_intN_x; crypto_intN_x signed>>= 7");
16:  readasm("amd64; int16 crypto_intN_x; crypto_intN_x signed>>= 15");
32:  readasm("amd64; int32 crypto_intN_x; crypto_intN_x signed>>= 31");
64:  readasm("amd64; int64 crypto_intN_x; crypto_intN_x signed>>= 63");
  return crypto_intN_x;
#elif arm64
  crypto_intN crypto_intN_y;
 8:  readasm("arm64; int8 crypto_intN_x crypto_intN_y; crypto_intN_y = -(1 & (crypto_intN_x unsigned>> 7))");
16:  readasm("arm64; int16 crypto_intN_x crypto_intN_y; crypto_intN_y = -(1 & (crypto_intN_x unsigned>> 15))");
32:  readasm("arm64; int32 crypto_intN_x crypto_intN_y; crypto_intN_y = crypto_intN_x signed>> 31");
64:  readasm("arm64; int64 crypto_intN_x crypto_intN_y; crypto_intN_y = crypto_intN_x signed>> 63");
  return crypto_intN_y;
#elif arm32
  crypto_intN crypto_intN_y;
 8:  readasm("arm32; int8 crypto_intN_x crypto_intN_y; crypto_intN_y = (int8) crypto_intN_x; crypto_intN_y = crypto_intN_y signed>> 31");
16:  readasm("arm32; int16 crypto_intN_x crypto_intN_y; crypto_intN_y = (int16) crypto_intN_x; crypto_intN_y = crypto_intN_y signed>> 31");
32:  readasm("arm32; int32 crypto_intN_x crypto_intN_y; crypto_intN_y = crypto_intN_x signed>> 31");
64:  readasm("arm32; int64 crypto_intN_x crypto_intN_y; crypto_intN_y.lo = crypto_intN_x.hi signed>> 31; crypto_intN_y.hi = crypto_intN_y.lo");
  return crypto_intN_y;
#elif sparc32
  crypto_intN crypto_intN_y;
 8:  readasm("sparc32; int8 crypto_intN_x crypto_intN_y; crypto_intN_y = crypto_intN_x << 24; crypto_intN_y = crypto_intN_y signed>> 31");
16:  readasm("sparc32; int16 crypto_intN_x crypto_intN_y; crypto_intN_y = crypto_intN_x << 16; crypto_intN_y = crypto_intN_y signed>> 31");
32:  readasm("sparc32; int32 crypto_intN_x crypto_intN_y; crypto_intN_y = crypto_intN_x signed>> 31");
64:  readasm("sparc32; int64 crypto_intN_x crypto_intN_y; crypto_intN_y.lo = crypto_intN_x.hi signed>> 31; crypto_intN_y.hi = crypto_intN_y.lo");
  return crypto_intN_y;
#else
  crypto_intN_x >>= N-6;
  crypto_intN_x += crypto_intN_optblocker;
  crypto_intN_x >>= 5;
  return crypto_intN_x;
#endif
}

__attribute__((unused))
static inline
crypto_intN_unsigned crypto_intN_unsigned_topbit_01(crypto_intN_unsigned crypto_intN_x) {
#if amd64
 8:  readasm("amd64; int8 crypto_intN_x; crypto_intN_x unsigned>>= 7");
16:  readasm("amd64; int16 crypto_intN_x; crypto_intN_x unsigned>>= 15");
32:  readasm("amd64; int32 crypto_intN_x; crypto_intN_x unsigned>>= 31");
64:  readasm("amd64; int64 crypto_intN_x; crypto_intN_x unsigned>>= 63");
  return crypto_intN_x;
#elif arm64
  crypto_intN_unsigned crypto_intN_y;
 8:  readasm("arm64; int8 crypto_intN_x crypto_intN_y; crypto_intN_y = 1 & (crypto_intN_x unsigned>> 7)");
16:  readasm("arm64; int16 crypto_intN_x crypto_intN_y; crypto_intN_y = 1 & (crypto_intN_x unsigned>> 15)");
32:  readasm("arm64; int32 crypto_intN_x crypto_intN_y; crypto_intN_y = crypto_intN_x unsigned>> 31");
64:  readasm("arm64; int64 crypto_intN_x crypto_intN_y; crypto_intN_y = crypto_intN_x unsigned>> 63");
  return crypto_intN_y;
#elif arm32
  crypto_intN crypto_intN_y;
 8:  readasm("arm32; int8 crypto_intN_x crypto_intN_y; crypto_intN_y = (uint8) crypto_intN_x; crypto_intN_y = crypto_intN_y unsigned>> 7");
16:  readasm("arm32; int16 crypto_intN_x crypto_intN_y; crypto_intN_y = (uint16) crypto_intN_x; crypto_intN_y = crypto_intN_y unsigned>> 15");
32:  readasm("arm32; int32 crypto_intN_x crypto_intN_y; crypto_intN_y = crypto_intN_x unsigned>> 31");
64:  readasm("arm32; int64 crypto_intN_x crypto_intN_y; crypto_intN_y.lo = crypto_intN_x.hi unsigned>> 31; crypto_intN_y.hi = 0");
  return crypto_intN_y;
#elif sparc32
  crypto_intN_unsigned crypto_intN_y;
 8:  readasm("sparc32; int8 crypto_intN_x crypto_intN_y; crypto_intN_y = (uint8) crypto_intN_x; crypto_intN_y = crypto_intN_y unsigned>> 7");
16:  readasm("sparc32; int16 crypto_intN_x crypto_intN_y; crypto_intN_y = (uint16) crypto_intN_x; crypto_intN_y = crypto_intN_y unsigned>> 15");
32:  readasm("sparc32; int32 crypto_intN_x crypto_intN_y; crypto_intN_y = crypto_intN_x unsigned>> 31");
64:  readasm("sparc32; int64 crypto_intN_x crypto_intN_y; crypto_intN_y.lo = crypto_intN_x.hi unsigned>> 31; crypto_intN_y.hi = 0");
  return crypto_intN_y;
#else
  crypto_intN_x >>= N-6;
  crypto_intN_x += crypto_intN_optblocker;
  crypto_intN_x >>= 5;
  return crypto_intN_x;
#endif
}

__attribute__((unused))
static inline
crypto_intN crypto_intN_negative_01(crypto_intN crypto_intN_x) {
  return crypto_intN_unsigned_topbit_01(crypto_intN_x);
}

__attribute__((unused))
static inline
crypto_intN crypto_intN_topbit_mask(crypto_intN crypto_intN_x) {
  return crypto_intN_negative_mask(crypto_intN_x);
}

__attribute__((unused))
static inline
crypto_intN crypto_intN_topbit_01(crypto_intN crypto_intN_x) {
  return crypto_intN_unsigned_topbit_01(crypto_intN_x);
}

__attribute__((unused))
static inline
crypto_intN crypto_intN_bottombit_mask(crypto_intN crypto_intN_x) {
#if amd64
 8:  readasm("amd64; int8 crypto_intN_x; crypto_intN_x &= 1");
16:  readasm("amd64; int16 crypto_intN_x; crypto_intN_x &= 1");
32:  readasm("amd64; int32 crypto_intN_x; crypto_intN_x &= 1");
64:  readasm("amd64; int64 crypto_intN_x; crypto_intN_x &= 1");
  return -crypto_intN_x;
#elif arm64
  crypto_intN crypto_intN_y;
 8:  readasm("arm64; int8 crypto_intN_x crypto_intN_y; crypto_intN_y = -(1 & (crypto_intN_x unsigned>> 0))");
16:  readasm("arm64; int16 crypto_intN_x crypto_intN_y; crypto_intN_y = -(1 & (crypto_intN_x unsigned>> 0))");
32:  readasm("arm64; int32 crypto_intN_x crypto_intN_y; crypto_intN_y = -(1 & (crypto_intN_x unsigned>> 0))");
64:  readasm("arm64; int64 crypto_intN_x crypto_intN_y; crypto_intN_y = -(1 & (crypto_intN_x unsigned>> 0))");
  return crypto_intN_y;
#elif arm32
  crypto_intN crypto_intN_y;
 8:  readasm("arm32; int8 crypto_intN_x crypto_intN_y; crypto_intN_y = crypto_intN_x & 1; crypto_intN_y = -crypto_intN_y");
16:  readasm("arm32; int16 crypto_intN_x crypto_intN_y; crypto_intN_y = crypto_intN_x & 1; crypto_intN_y = -crypto_intN_y");
32:  readasm("arm32; int32 crypto_intN_x crypto_intN_y; crypto_intN_y = crypto_intN_x & 1; crypto_intN_y = -crypto_intN_y");
64:  readasm("arm32; int64 crypto_intN_x crypto_intN_y; crypto_intN_y.lo = crypto_intN_x.lo & 1; crypto_intN_y.lo = -crypto_intN_y.lo; crypto_intN_y.hi = crypto_intN_y.lo");
  return crypto_intN_y;
#elif sparc32
  crypto_intN crypto_intN_y;
 8:  readasm("sparc32; int8 crypto_intN_x crypto_intN_y; crypto_intN_y = crypto_intN_x & 1; crypto_intN_y = -crypto_intN_y");
16:  readasm("sparc32; int16 crypto_intN_x crypto_intN_y; crypto_intN_y = crypto_intN_x & 1; crypto_intN_y = -crypto_intN_y");
32:  readasm("sparc32; int32 crypto_intN_x crypto_intN_y; crypto_intN_y = crypto_intN_x & 1; crypto_intN_y = -crypto_intN_y");
64:  readasm("sparc32; int64 crypto_intN_x crypto_intN_y; crypto_intN_y.lo = crypto_intN_x.lo & 1; crypto_intN_y.lo = -crypto_intN_y.lo; crypto_intN_y.hi = crypto_intN_y.lo");
  return crypto_intN_y;
#else
  crypto_intN_x &= 1 + crypto_intN_optblocker;
  return -crypto_intN_x;
#endif
}

__attribute__((unused))
static inline
crypto_intN crypto_intN_bottombit_01(crypto_intN crypto_intN_x) {
#if amd64
 8:  readasm("amd64; int8 crypto_intN_x; crypto_intN_x &= 1");
16:  readasm("amd64; int16 crypto_intN_x; crypto_intN_x &= 1");
32:  readasm("amd64; int32 crypto_intN_x; crypto_intN_x &= 1");
64:  readasm("amd64; int64 crypto_intN_x; crypto_intN_x &= 1");
  return crypto_intN_x;
#elif arm64
  crypto_intN crypto_intN_y;
 8:  readasm("arm64; int8 crypto_intN_x crypto_intN_y; crypto_intN_y = 1 & (crypto_intN_x unsigned>> 0)");
16:  readasm("arm64; int16 crypto_intN_x crypto_intN_y; crypto_intN_y = 1 & (crypto_intN_x unsigned>> 0)");
32:  readasm("arm64; int32 crypto_intN_x crypto_intN_y; crypto_intN_y = 1 & (crypto_intN_x unsigned>> 0)");
64:  readasm("arm64; int64 crypto_intN_x crypto_intN_y; crypto_intN_y = 1 & (crypto_intN_x unsigned>> 0)");
  return crypto_intN_y;
#elif arm32
  crypto_intN crypto_intN_y;
 8:  readasm("arm32; int8 crypto_intN_x crypto_intN_y; crypto_intN_y = crypto_intN_x & 1");
16:  readasm("arm32; int16 crypto_intN_x crypto_intN_y; crypto_intN_y = crypto_intN_x & 1");
32:  readasm("arm32; int32 crypto_intN_x crypto_intN_y; crypto_intN_y = crypto_intN_x & 1");
64:  readasm("arm32; int64 crypto_intN_x crypto_intN_y; crypto_intN_y.lo = crypto_intN_x.lo & 1; crypto_intN_y.hi = 0");
  return crypto_intN_y;
#elif sparc32
  crypto_intN crypto_intN_y;
 8:  readasm("sparc32; int8 crypto_intN_x crypto_intN_y; crypto_intN_y = crypto_intN_x & 1");
16:  readasm("sparc32; int16 crypto_intN_x crypto_intN_y; crypto_intN_y = crypto_intN_x & 1");
32:  readasm("sparc32; int32 crypto_intN_x crypto_intN_y; crypto_intN_y = crypto_intN_x & 1");
64:  readasm("sparc32; int64 crypto_intN_x crypto_intN_y; crypto_intN_y.lo = crypto_intN_x.lo & 1; crypto_intN_y.hi = 0");
  return crypto_intN_y;
#else
  crypto_intN_x &= 1 + crypto_intN_optblocker;
  return crypto_intN_x;
#endif
}

__attribute__((unused))
static inline
crypto_intN crypto_intN_bitinrangepublicpos_mask(crypto_intN crypto_intN_x,crypto_intN crypto_intN_s) {
#if amd64
 8:  readasm("amd64; int8 crypto_intN_x crypto_intN_s; crypto_intN_x signed>>= crypto_intN_s");
16:  readasm("amd64; int16 crypto_intN_x crypto_intN_s; crypto_intN_x signed>>= crypto_intN_s");
32:  readasm("amd64; int32 crypto_intN_x crypto_intN_s; crypto_intN_x signed>>= crypto_intN_s");
64:  readasm("amd64; int64 crypto_intN_x crypto_intN_s; crypto_intN_x signed>>= crypto_intN_s");
#elif arm64
 8:  readasm("arm64; int8 crypto_intN_x crypto_intN_s; crypto_intN_x = (int8) crypto_intN_x; crypto_intN_x = crypto_intN_x signed>> crypto_intN_s");
16:  readasm("arm64; int16 crypto_intN_x crypto_intN_s; crypto_intN_x = (int16) crypto_intN_x; crypto_intN_x = crypto_intN_x signed>> crypto_intN_s");
32:  readasm("arm64; int32 crypto_intN_x crypto_intN_s; crypto_intN_x = crypto_intN_x signed>> crypto_intN_s");
64:  readasm("arm64; int64 crypto_intN_x crypto_intN_s; crypto_intN_x = crypto_intN_x signed>> crypto_intN_s");
#elif arm32
 8:  readasm("arm32; int8 crypto_intN_x crypto_intN_s; crypto_intN_s = crypto_intN_s & 7; crypto_intN_x = (int8) crypto_intN_x; crypto_intN_x = crypto_intN_x signed>> crypto_intN_s");
16:  readasm("arm32; int16 crypto_intN_x crypto_intN_s; crypto_intN_s = crypto_intN_s & 15; crypto_intN_x = (int16) crypto_intN_x; crypto_intN_x = crypto_intN_x signed>> crypto_intN_s");
32:  readasm("arm32; int32 crypto_intN_x crypto_intN_s; crypto_intN_s = crypto_intN_s & 31; crypto_intN_x = crypto_intN_x signed>> crypto_intN_s");
64:  readasm("arm32; int64 crypto_intN_x crypto_intN_s; crypto_intN_s.lo = crypto_intN_s.lo & 63; crypto_intN_x.lo = crypto_intN_x.lo unsigned>> crypto_intN_s.lo; crypto_intN_s.hi = 32 - crypto_intN_s.lo; crypto_intN_x.lo = crypto_intN_x.lo | (crypto_intN_x.hi << crypto_intN_s.hi); flags, crypto_intN_s.hi = crypto_intN_s.lo - 32; crypto_intN_x.lo = crypto_intN_x.lo | (crypto_intN_x.hi signed>> crypto_intN_s.hi) if unsigned>=; crypto_intN_x.hi = crypto_intN_x.hi signed>> crypto_intN_s.lo");
#elif sparc32
 8:  readasm("sparc32; int8 crypto_intN_x crypto_intN_s; crypto_intN_s = crypto_intN_s & 7; crypto_intN_x = crypto_intN_x signed>> crypto_intN_s");
16:  readasm("sparc32; int16 crypto_intN_x crypto_intN_s; crypto_intN_s = crypto_intN_s & 15; crypto_intN_x = crypto_intN_x signed>> crypto_intN_s");
32:  readasm("sparc32; int32 crypto_intN_x crypto_intN_s; crypto_intN_x = crypto_intN_x signed>> crypto_intN_s");
64:  crypto_intN crypto_intN_y, crypto_intN_z;
64:  readasm("sparc32; int64 crypto_intN_x crypto_intN_y crypto_intN_z crypto_intN_s; crypto_intN_s.hi = ~crypto_intN_s.lo; crypto_intN_z.hi = crypto_intN_s.lo << 26; crypto_intN_z.lo = crypto_intN_x.hi << 1; crypto_intN_y.lo = crypto_intN_x.lo unsigned>> crypto_intN_s.lo; crypto_intN_z.lo = crypto_intN_z.lo << crypto_intN_s.hi; crypto_intN_y.hi = crypto_intN_x.hi signed>> crypto_intN_s.lo; crypto_intN_z.hi = crypto_intN_z.hi signed>> 31; crypto_intN_y.lo = crypto_intN_y.lo | crypto_intN_z.lo; crypto_intN_z.lo = crypto_intN_y.lo ^ crypto_intN_y.hi; crypto_intN_z.lo = crypto_intN_z.hi & crypto_intN_z.lo; crypto_intN_x.hi = crypto_intN_y.hi signed>> crypto_intN_z.hi; crypto_intN_x.lo = crypto_intN_y.lo ^ crypto_intN_z.lo");
#else
  crypto_intN_x >>= crypto_intN_s ^ crypto_intN_optblocker;
#endif
  return crypto_intN_bottombit_mask(crypto_intN_x);
}

__attribute__((unused))
static inline
crypto_intN crypto_intN_bitinrangepublicpos_01(crypto_intN crypto_intN_x,crypto_intN crypto_intN_s) {
#if amd64
 8:  readasm("amd64; int8 crypto_intN_x crypto_intN_s; crypto_intN_x signed>>= crypto_intN_s");
16:  readasm("amd64; int16 crypto_intN_x crypto_intN_s; crypto_intN_x signed>>= crypto_intN_s");
32:  readasm("amd64; int32 crypto_intN_x crypto_intN_s; crypto_intN_x signed>>= crypto_intN_s");
64:  readasm("amd64; int64 crypto_intN_x crypto_intN_s; crypto_intN_x signed>>= crypto_intN_s");
#elif arm64
 8:  readasm("arm64; int8 crypto_intN_x crypto_intN_s; crypto_intN_x = (int8) crypto_intN_x; crypto_intN_x = crypto_intN_x signed>> crypto_intN_s");
16:  readasm("arm64; int16 crypto_intN_x crypto_intN_s; crypto_intN_x = (int16) crypto_intN_x; crypto_intN_x = crypto_intN_x signed>> crypto_intN_s");
32:  readasm("arm64; int32 crypto_intN_x crypto_intN_s; crypto_intN_x = crypto_intN_x signed>> crypto_intN_s");
64:  readasm("arm64; int64 crypto_intN_x crypto_intN_s; crypto_intN_x = crypto_intN_x signed>> crypto_intN_s");
#elif arm32
 8:  readasm("arm32; int8 crypto_intN_x crypto_intN_s; crypto_intN_s = crypto_intN_s & 7; crypto_intN_x = (int8) crypto_intN_x; crypto_intN_x = crypto_intN_x signed>> crypto_intN_s");
16:  readasm("arm32; int16 crypto_intN_x crypto_intN_s; crypto_intN_s = crypto_intN_s & 15; crypto_intN_x = (int16) crypto_intN_x; crypto_intN_x = crypto_intN_x signed>> crypto_intN_s");
32:  readasm("arm32; int32 crypto_intN_x crypto_intN_s; crypto_intN_s = crypto_intN_s & 31; crypto_intN_x = crypto_intN_x signed>> crypto_intN_s");
64:  readasm("arm32; int64 crypto_intN_x crypto_intN_s; crypto_intN_s.lo = crypto_intN_s.lo & 63; crypto_intN_x.lo = crypto_intN_x.lo unsigned>> crypto_intN_s.lo; crypto_intN_s.hi = 32 - crypto_intN_s.lo; crypto_intN_x.lo = crypto_intN_x.lo | (crypto_intN_x.hi << crypto_intN_s.hi); flags, crypto_intN_s.hi = crypto_intN_s.lo - 32; crypto_intN_x.lo = crypto_intN_x.lo | (crypto_intN_x.hi signed>> crypto_intN_s.hi) if unsigned>=; crypto_intN_x.hi = crypto_intN_x.hi signed>> crypto_intN_s.lo");
#elif sparc32
 8:  readasm("sparc32; int8 crypto_intN_x crypto_intN_s; crypto_intN_s = crypto_intN_s & 7; crypto_intN_x = crypto_intN_x signed>> crypto_intN_s");
16:  readasm("sparc32; int16 crypto_intN_x crypto_intN_s; crypto_intN_s = crypto_intN_s & 15; crypto_intN_x = crypto_intN_x signed>> crypto_intN_s");
32:  readasm("sparc32; int32 crypto_intN_x crypto_intN_s; crypto_intN_x = crypto_intN_x signed>> crypto_intN_s");
64:  crypto_intN crypto_intN_y, crypto_intN_z;
64:  readasm("sparc32; int64 crypto_intN_x crypto_intN_y crypto_intN_z crypto_intN_s; crypto_intN_s.hi = ~crypto_intN_s.lo; crypto_intN_z.hi = crypto_intN_s.lo << 26; crypto_intN_z.lo = crypto_intN_x.hi << 1; crypto_intN_y.lo = crypto_intN_x.lo unsigned>> crypto_intN_s.lo; crypto_intN_z.lo = crypto_intN_z.lo << crypto_intN_s.hi; crypto_intN_y.hi = crypto_intN_x.hi signed>> crypto_intN_s.lo; crypto_intN_z.hi = crypto_intN_z.hi signed>> 31; crypto_intN_y.lo = crypto_intN_y.lo | crypto_intN_z.lo; crypto_intN_z.lo = crypto_intN_y.lo ^ crypto_intN_y.hi; crypto_intN_z.lo = crypto_intN_z.hi & crypto_intN_z.lo; crypto_intN_x.hi = crypto_intN_y.hi signed>> crypto_intN_z.hi; crypto_intN_x.lo = crypto_intN_y.lo ^ crypto_intN_z.lo");
#else
  crypto_intN_x >>= crypto_intN_s ^ crypto_intN_optblocker;
#endif
  return crypto_intN_bottombit_01(crypto_intN_x);
}

__attribute__((unused))
static inline
crypto_intN crypto_intN_shlmod(crypto_intN crypto_intN_x,crypto_intN crypto_intN_s) {
#if amd64
 8:  crypto_intN_s &= 7;
 8:  readasm("amd64; int8 crypto_intN_x crypto_intN_s; crypto_intN_x <<= crypto_intN_s");
16:  crypto_intN_s &= 15;
16:  readasm("amd64; int16 crypto_intN_x crypto_intN_s; crypto_intN_x <<= crypto_intN_s");
32:  readasm("amd64; int32 crypto_intN_x crypto_intN_s; crypto_intN_x <<= crypto_intN_s");
64:  readasm("amd64; int64 crypto_intN_x crypto_intN_s; crypto_intN_x <<= crypto_intN_s");
#elif arm64
 8:  crypto_intN_s &= 7;
 8:  readasm("arm64; int8 crypto_intN_x crypto_intN_s; crypto_intN_x = (uint8) crypto_intN_x; crypto_intN_x = crypto_intN_x << crypto_intN_s");
16:  crypto_intN_s &= 15;
16:  readasm("arm64; int16 crypto_intN_x crypto_intN_s; crypto_intN_x = (uint16) crypto_intN_x; crypto_intN_x = crypto_intN_x << crypto_intN_s");
32:  readasm("arm64; int32 crypto_intN_x crypto_intN_s; crypto_intN_x = crypto_intN_x << crypto_intN_s");
64:  readasm("arm64; int64 crypto_intN_x crypto_intN_s; crypto_intN_x = crypto_intN_x << crypto_intN_s");
#elif arm32
 8:  crypto_intN_s &= 7;
 8:  readasm("arm32; int8 crypto_intN_x crypto_intN_s; crypto_intN_x = crypto_intN_x << crypto_intN_s; crypto_intN_x = (int8) crypto_intN_x");
16:  crypto_intN_s &= 15;
16:  readasm("arm32; int16 crypto_intN_x crypto_intN_s; crypto_intN_x = crypto_intN_x << crypto_intN_s; crypto_intN_x = (int16) crypto_intN_x");
32:  crypto_intN_s &= 31;
32:  readasm("arm32; int32 crypto_intN_x crypto_intN_s; crypto_intN_x = crypto_intN_x << crypto_intN_s");
64:  readasm("arm32; int64 crypto_intN_x crypto_intN_s; crypto_intN_s.lo = crypto_intN_s.lo & 63; crypto_intN_x.hi = crypto_intN_x.hi << crypto_intN_s.lo; crypto_intN_s.hi = crypto_intN_s.lo - 32; crypto_intN_x.hi = crypto_intN_x.hi | (crypto_intN_x.lo << crypto_intN_s.hi); crypto_intN_s.hi = 32 - crypto_intN_s.lo; crypto_intN_x.hi = crypto_intN_x.hi | (crypto_intN_x.lo unsigned>> crypto_intN_s.hi); crypto_intN_x.lo = crypto_intN_x.lo << crypto_intN_s.lo");
#elif sparc32
 8:  crypto_intN_s &= 7; crypto_intN_s += 24;
 8:  readasm("sparc32; int8 crypto_intN_x crypto_intN_s; crypto_intN_x = crypto_intN_x << crypto_intN_s; crypto_intN_x = crypto_intN_x signed>> 24");
16:  crypto_intN_s &= 15; crypto_intN_s += 16;
16:  readasm("sparc32; int16 crypto_intN_x crypto_intN_s; crypto_intN_x = crypto_intN_x << crypto_intN_s; crypto_intN_x = crypto_intN_x signed>> 16");
32:  readasm("sparc32; int32 crypto_intN_x crypto_intN_s; crypto_intN_x = crypto_intN_x << crypto_intN_s");
64:  crypto_intN crypto_intN_y, crypto_intN_z;
64:  readasm("sparc32; int64 crypto_intN_x crypto_intN_y crypto_intN_z crypto_intN_s; crypto_intN_s.hi = ~crypto_intN_s.lo; crypto_intN_z.hi = crypto_intN_s.lo << 26; crypto_intN_z.lo = crypto_intN_x.lo unsigned>> 1; crypto_intN_y.lo = crypto_intN_x.lo << crypto_intN_s.lo; crypto_intN_z.lo = crypto_intN_z.lo unsigned>> crypto_intN_s.hi; crypto_intN_y.hi = crypto_intN_x.hi << crypto_intN_s.lo; crypto_intN_z.hi = crypto_intN_z.hi signed>> 31; crypto_intN_y.hi = crypto_intN_y.hi | crypto_intN_z.lo; crypto_intN_z.lo = crypto_intN_y.lo ^ crypto_intN_y.hi; crypto_intN_x.lo = crypto_intN_z.hi & crypto_intN_y.lo; crypto_intN_z.lo = crypto_intN_z.hi & crypto_intN_z.lo; crypto_intN_x.lo = crypto_intN_x.lo ^ crypto_intN_y.lo; crypto_intN_x.hi = crypto_intN_y.hi ^ crypto_intN_z.lo");
#else
  int crypto_intN_k, crypto_intN_l;
  for (crypto_intN_l = 0,crypto_intN_k = 1;crypto_intN_k < N;++crypto_intN_l,crypto_intN_k *= 2)
    crypto_intN_x ^= (crypto_intN_x ^ (crypto_intN_x << crypto_intN_k)) & crypto_intN_bitinrangepublicpos_mask(crypto_intN_s,crypto_intN_l);
#endif
  return crypto_intN_x;
}

__attribute__((unused))
static inline
crypto_intN crypto_intN_shrmod(crypto_intN crypto_intN_x,crypto_intN crypto_intN_s) {
#if amd64
 8:  crypto_intN_s &= 7;
 8:  readasm("amd64; int8 crypto_intN_x crypto_intN_s; crypto_intN_x signed>>= crypto_intN_s");
16:  crypto_intN_s &= 15;
16:  readasm("amd64; int16 crypto_intN_x crypto_intN_s; crypto_intN_x signed>>= crypto_intN_s");
32:  readasm("amd64; int32 crypto_intN_x crypto_intN_s; crypto_intN_x signed>>= crypto_intN_s");
64:  readasm("amd64; int64 crypto_intN_x crypto_intN_s; crypto_intN_x signed>>= crypto_intN_s");
#elif arm64
 8:  crypto_intN_s &= 7;
 8:  readasm("arm64; int8 crypto_intN_x crypto_intN_s; crypto_intN_x = (int8) crypto_intN_x; crypto_intN_x = crypto_intN_x signed>> crypto_intN_s");
16:  crypto_intN_s &= 15;
16:  readasm("arm64; int16 crypto_intN_x crypto_intN_s; crypto_intN_x = (int16) crypto_intN_x; crypto_intN_x = crypto_intN_x signed>> crypto_intN_s");
32:  readasm("arm64; int32 crypto_intN_x crypto_intN_s; crypto_intN_x = crypto_intN_x signed>> crypto_intN_s");
64:  readasm("arm64; int64 crypto_intN_x crypto_intN_s; crypto_intN_x = crypto_intN_x signed>> crypto_intN_s");
#elif arm32
 8:  crypto_intN_s &= 7;
 8:  readasm("arm32; int8 crypto_intN_x crypto_intN_s; crypto_intN_x = (int8) crypto_intN_x; crypto_intN_x = crypto_intN_x signed>> crypto_intN_s");
16:  crypto_intN_s &= 15;
16:  readasm("arm32; int16 crypto_intN_x crypto_intN_s; crypto_intN_x = (int16) crypto_intN_x; crypto_intN_x = crypto_intN_x signed>> crypto_intN_s");
32:  crypto_intN_s &= 31;
32:  readasm("arm32; int32 crypto_intN_x crypto_intN_s; crypto_intN_x = crypto_intN_x signed>> crypto_intN_s");
64:  readasm("arm32; int64 crypto_intN_x crypto_intN_s; crypto_intN_s.lo = crypto_intN_s.lo & 63; crypto_intN_x.lo = crypto_intN_x.lo unsigned>> crypto_intN_s.lo; crypto_intN_s.hi = 32 - crypto_intN_s.lo; crypto_intN_x.lo = crypto_intN_x.lo | (crypto_intN_x.hi << crypto_intN_s.hi); flags, crypto_intN_s.hi = crypto_intN_s.lo - 32; crypto_intN_x.lo = crypto_intN_x.lo | (crypto_intN_x.hi signed>> crypto_intN_s.hi) if unsigned>=; crypto_intN_x.hi = crypto_intN_x.hi signed>> crypto_intN_s.lo");
#elif sparc32
 8:  crypto_intN_s &= 7;
 8:  readasm("sparc32; int8 crypto_intN_x crypto_intN_s; crypto_intN_x = crypto_intN_x signed>> crypto_intN_s");
16:  crypto_intN_s &= 15;
16:  readasm("sparc32; int16 crypto_intN_x crypto_intN_s; crypto_intN_x = crypto_intN_x signed>> crypto_intN_s");
32:  readasm("sparc32; int32 crypto_intN_x crypto_intN_s; crypto_intN_x = crypto_intN_x signed>> crypto_intN_s");
64:  crypto_intN crypto_intN_y, crypto_intN_z;
64:  readasm("sparc32; int64 crypto_intN_x crypto_intN_y crypto_intN_z crypto_intN_s; crypto_intN_s.hi = ~crypto_intN_s.lo; crypto_intN_z.hi = crypto_intN_s.lo << 26; crypto_intN_z.lo = crypto_intN_x.hi << 1; crypto_intN_y.lo = crypto_intN_x.lo unsigned>> crypto_intN_s.lo; crypto_intN_z.lo = crypto_intN_z.lo << crypto_intN_s.hi; crypto_intN_y.hi = crypto_intN_x.hi signed>> crypto_intN_s.lo; crypto_intN_z.hi = crypto_intN_z.hi signed>> 31; crypto_intN_y.lo = crypto_intN_y.lo | crypto_intN_z.lo; crypto_intN_z.lo = crypto_intN_y.lo ^ crypto_intN_y.hi; crypto_intN_z.lo = crypto_intN_z.hi & crypto_intN_z.lo; crypto_intN_x.hi = crypto_intN_y.hi signed>> crypto_intN_z.hi; crypto_intN_x.lo = crypto_intN_y.lo ^ crypto_intN_z.lo");
#else
  int crypto_intN_k, crypto_intN_l;
  for (crypto_intN_l = 0,crypto_intN_k = 1;crypto_intN_k < N;++crypto_intN_l,crypto_intN_k *= 2)
    crypto_intN_x ^= (crypto_intN_x ^ (crypto_intN_x >> crypto_intN_k)) & crypto_intN_bitinrangepublicpos_mask(crypto_intN_s,crypto_intN_l);
#endif
  return crypto_intN_x;
}

__attribute__((unused))
static inline
crypto_intN crypto_intN_bitmod_mask(crypto_intN crypto_intN_x,crypto_intN crypto_intN_s) {
  crypto_intN_x = crypto_intN_shrmod(crypto_intN_x,crypto_intN_s);
  return crypto_intN_bottombit_mask(crypto_intN_x);
}

__attribute__((unused))
static inline
crypto_intN crypto_intN_bitmod_01(crypto_intN crypto_intN_x,crypto_intN crypto_intN_s) {
  crypto_intN_x = crypto_intN_shrmod(crypto_intN_x,crypto_intN_s);
  return crypto_intN_bottombit_01(crypto_intN_x);
}

__attribute__((unused))
static inline
crypto_intN crypto_intN_nonzero_mask(crypto_intN crypto_intN_x) {
#if amd64
 8:  crypto_intN crypto_intN_z;
 8:  int32_t crypto_intN_x32 = crypto_intN_x,crypto_intN_q32,crypto_intN_z32;
 8:  readasm("amd64; int32 crypto_intN_x32 crypto_intN_q32 crypto_intN_z32; crypto_intN_z32 = 0; crypto_intN_q32 = -1; crypto_intN_x32 & (int8) crypto_intN_x32; crypto_intN_z32 = crypto_intN_q32 if !=");
 8:  crypto_intN_z = crypto_intN_z32;
16:  crypto_intN crypto_intN_q,crypto_intN_z;
16:  readasm("amd64; int16 crypto_intN_x crypto_intN_q crypto_intN_z; crypto_intN_z = 0; crypto_intN_q = -1; crypto_intN_x & crypto_intN_x; crypto_intN_z = crypto_intN_q if !=");
32:  crypto_intN crypto_intN_q,crypto_intN_z;
32:  readasm("amd64; int32 crypto_intN_x crypto_intN_q crypto_intN_z; crypto_intN_z = 0; crypto_intN_q = -1; crypto_intN_x & crypto_intN_x; crypto_intN_z = crypto_intN_q if !=");
64:  crypto_intN crypto_intN_q,crypto_intN_z;
64:  readasm("amd64; int64 crypto_intN_x crypto_intN_q crypto_intN_z; crypto_intN_z = 0; crypto_intN_q = -1; crypto_intN_x & crypto_intN_x; crypto_intN_z = crypto_intN_q if !=");
  return crypto_intN_z;
#elif arm64
  crypto_intN crypto_intN_z;
 8:  readasm("arm64; int8 crypto_intN_x crypto_intN_z; crypto_intN_x & 255; crypto_intN_z = -1 if != else 0");
16:  readasm("arm64; int16 crypto_intN_x crypto_intN_z; crypto_intN_x & 65535; crypto_intN_z = -1 if != else 0");
32:  readasm("arm64; int32 crypto_intN_x crypto_intN_z; crypto_intN_x - 0; crypto_intN_z = -1 if != else 0");
64:  readasm("arm64; int64 crypto_intN_x crypto_intN_z; crypto_intN_x - 0; crypto_intN_z = -1 if != else 0");
  return crypto_intN_z;
#elif arm32
 8:  readasm("arm32; int8 crypto_intN_x; crypto_intN_x = (uint8) crypto_intN_x; crypto_intN_x - 0; crypto_intN_x = -1 if !=");
16:  readasm("arm32; int16 crypto_intN_x; crypto_intN_x = (uint16) crypto_intN_x; crypto_intN_x - 0; crypto_intN_x = -1 if !=");
32:  readasm("arm32; int32 crypto_intN_x; crypto_intN_x - 0; crypto_intN_x = -1 if !=");
64:  readasm("arm32; int64 crypto_intN_x; flags, crypto_intN_x.lo = crypto_intN_x.lo | crypto_intN_x.hi; crypto_intN_x.lo = -1 if !=; crypto_intN_x.hi = crypto_intN_x.lo");
  return crypto_intN_x;
#elif sparc32
  crypto_intN crypto_intN_z;
 8:  readasm("sparc32; int8 crypto_intN_x crypto_intN_z; crypto_intN_x = (uint8) crypto_intN_x; 0 - crypto_intN_x; crypto_intN_z = -carry");
16:  readasm("sparc32; int16 crypto_intN_x crypto_intN_z; crypto_intN_x = (uint16) crypto_intN_x; 0 - crypto_intN_x; crypto_intN_z = -carry");
32:  readasm("sparc32; int32 crypto_intN_x crypto_intN_z; 0 - crypto_intN_x; crypto_intN_z = -carry");
64:  readasm("sparc32; int64 crypto_intN_x crypto_intN_z; crypto_intN_z.lo = crypto_intN_x.lo | crypto_intN_x.hi; 0 - crypto_intN_z.lo; crypto_intN_z.lo = -carry; crypto_intN_z.hi = crypto_intN_z.lo");
  return crypto_intN_z;
#else
  crypto_intN_x |= -crypto_intN_x;
  return crypto_intN_negative_mask(crypto_intN_x);
#endif
}

__attribute__((unused))
static inline
crypto_intN crypto_intN_nonzero_01(crypto_intN crypto_intN_x) {
#if amd64
 8:  crypto_intN crypto_intN_z;
 8:  int32_t crypto_intN_x32 = crypto_intN_x,crypto_intN_q32,crypto_intN_z32;
 8:  readasm("amd64; int32 crypto_intN_x32 crypto_intN_q32 crypto_intN_z32; crypto_intN_z32 = 0; crypto_intN_q32 = 1; crypto_intN_x32 & (int8) crypto_intN_x32; crypto_intN_z32 = crypto_intN_q32 if !=");
 8:  crypto_intN_z = crypto_intN_z32;
16:  crypto_intN crypto_intN_q,crypto_intN_z;
16:  readasm("amd64; int16 crypto_intN_x crypto_intN_q crypto_intN_z; crypto_intN_z = 0; crypto_intN_q = 1; crypto_intN_x & crypto_intN_x; crypto_intN_z = crypto_intN_q if !=");
32:  crypto_intN crypto_intN_q,crypto_intN_z;
32:  readasm("amd64; int32 crypto_intN_x crypto_intN_q crypto_intN_z; crypto_intN_z = 0; crypto_intN_q = 1; crypto_intN_x & crypto_intN_x; crypto_intN_z = crypto_intN_q if !=");
64:  crypto_intN crypto_intN_q,crypto_intN_z;
64:  readasm("amd64; int64 crypto_intN_x crypto_intN_q crypto_intN_z; crypto_intN_z = 0; crypto_intN_q = 1; crypto_intN_x & crypto_intN_x; crypto_intN_z = crypto_intN_q if !=");
  return crypto_intN_z;
#elif arm64
  crypto_intN crypto_intN_z;
 8:  readasm("arm64; int8 crypto_intN_x crypto_intN_z; crypto_intN_x & 255; crypto_intN_z = 1 if != else 0");
16:  readasm("arm64; int16 crypto_intN_x crypto_intN_z; crypto_intN_x & 65535; crypto_intN_z = 1 if != else 0");
32:  readasm("arm64; int32 crypto_intN_x crypto_intN_z; crypto_intN_x - 0; crypto_intN_z = 1 if != else 0");
64:  readasm("arm64; int64 crypto_intN_x crypto_intN_z; crypto_intN_x - 0; crypto_intN_z = 1 if != else 0");
  return crypto_intN_z;
#elif arm32
 8:  readasm("arm32; int8 crypto_intN_x; crypto_intN_x = (uint8) crypto_intN_x; crypto_intN_x - 0; crypto_intN_x = 1 if !=");
16:  readasm("arm32; int16 crypto_intN_x; crypto_intN_x = (uint16) crypto_intN_x; crypto_intN_x - 0; crypto_intN_x = 1 if !=");
32:  readasm("arm32; int32 crypto_intN_x; crypto_intN_x - 0; crypto_intN_x = 1 if !=");
64:  readasm("arm32; int64 crypto_intN_x; flags, crypto_intN_x.lo = crypto_intN_x.lo | crypto_intN_x.hi; crypto_intN_x.lo = 1 if !=; crypto_intN_x.hi = 0");
  return crypto_intN_x;
#elif sparc32
  crypto_intN crypto_intN_z;
 8:  readasm("sparc32; int8 crypto_intN_x crypto_intN_z; crypto_intN_x = (uint8) crypto_intN_x; 0 - crypto_intN_x; crypto_intN_z = carry");
16:  readasm("sparc32; int16 crypto_intN_x crypto_intN_z; crypto_intN_x = (uint16) crypto_intN_x; 0 - crypto_intN_x; crypto_intN_z = carry");
32:  readasm("sparc32; int32 crypto_intN_x crypto_intN_z; 0 - crypto_intN_x; crypto_intN_z = carry");
64:  readasm("sparc32; int64 crypto_intN_x crypto_intN_z; crypto_intN_z.lo = crypto_intN_x.lo | crypto_intN_x.hi; 0 - crypto_intN_z.lo; crypto_intN_z.lo = carry; crypto_intN_z.hi = 0");
  return crypto_intN_z;
#else
  crypto_intN_x |= -crypto_intN_x;
  return crypto_intN_unsigned_topbit_01(crypto_intN_x);
#endif
}

__attribute__((unused))
static inline
crypto_intN crypto_intN_positive_mask(crypto_intN crypto_intN_x) {
#if amd64
 8:  crypto_intN crypto_intN_z;
 8:  int32_t crypto_intN_x32 = crypto_intN_x,crypto_intN_q32,crypto_intN_z32;
 8:  readasm("amd64; int32 crypto_intN_x32 crypto_intN_q32 crypto_intN_z32; crypto_intN_z32 = 0; crypto_intN_q32 = -1; crypto_intN_x32 & (int8) crypto_intN_x32; crypto_intN_z32 = crypto_intN_q32 if signed>");
 8:  crypto_intN_z = crypto_intN_z32;
16:  crypto_intN crypto_intN_q,crypto_intN_z;
16:  readasm("amd64; int16 crypto_intN_x crypto_intN_q crypto_intN_z; crypto_intN_z = 0; crypto_intN_q = -1; crypto_intN_x & crypto_intN_x; crypto_intN_z = crypto_intN_q if signed>");
32:  crypto_intN crypto_intN_q,crypto_intN_z;
32:  readasm("amd64; int32 crypto_intN_x crypto_intN_q crypto_intN_z; crypto_intN_z = 0; crypto_intN_q = -1; crypto_intN_x & crypto_intN_x; crypto_intN_z = crypto_intN_q if signed>");
64:  crypto_intN crypto_intN_q,crypto_intN_z;
64:  readasm("amd64; int64 crypto_intN_x crypto_intN_q crypto_intN_z; crypto_intN_z = 0; crypto_intN_q = -1; crypto_intN_x & crypto_intN_x; crypto_intN_z = crypto_intN_q if signed>");
  return crypto_intN_z;
#elif arm64
  crypto_intN crypto_intN_z;
 8:  readasm("arm64; int8 crypto_intN_x crypto_intN_z; crypto_intN_z = (int8) crypto_intN_x; crypto_intN_z - 0; crypto_intN_z = -1 if signed> else 0");
16:  readasm("arm64; int16 crypto_intN_x crypto_intN_z; crypto_intN_z = (int16) crypto_intN_x; crypto_intN_z - 0; crypto_intN_z = -1 if signed> else 0");
32:  readasm("arm64; int32 crypto_intN_x crypto_intN_z; crypto_intN_x - 0; crypto_intN_z = -1 if signed> else 0");
64:  readasm("arm64; int64 crypto_intN_x crypto_intN_z; crypto_intN_x - 0; crypto_intN_z = -1 if signed> else 0");
  return crypto_intN_z;
#elif sparc32
  crypto_intN crypto_intN_z;
 8:  readasm("sparc32; int8 crypto_intN_x crypto_intN_z; crypto_intN_z = crypto_intN_x << 24; crypto_intN_z = crypto_intN_z signed>> 31; crypto_intN_z = crypto_intN_z - crypto_intN_x; crypto_intN_z = crypto_intN_z signed>> 31");
16:  readasm("sparc32; int16 crypto_intN_x crypto_intN_z; crypto_intN_z = crypto_intN_x << 16; crypto_intN_z = crypto_intN_z signed>> 31; crypto_intN_z = crypto_intN_z - crypto_intN_x; crypto_intN_z = crypto_intN_z signed>> 31");
32:  readasm("sparc32; int32 crypto_intN_x crypto_intN_z; crypto_intN_z = crypto_intN_x signed>> 31; crypto_intN_z = crypto_intN_z - crypto_intN_x; crypto_intN_z = crypto_intN_z signed>> 31");
64:  readasm("sparc32; int64 crypto_intN_x crypto_intN_z; crypto_intN_z.lo = crypto_intN_x.hi signed>> 31; carry, crypto_intN_z.hi = crypto_intN_z.lo - crypto_intN_x.lo; crypto_intN_z.hi = crypto_intN_z.lo - crypto_intN_x.hi - carry; crypto_intN_z.hi = crypto_intN_z.hi signed>> 31; crypto_intN_z.lo = crypto_intN_z.hi");
  return crypto_intN_z;
#else
  crypto_intN crypto_intN_z = -crypto_intN_x;
  crypto_intN_z ^= crypto_intN_x & crypto_intN_z;
  return crypto_intN_negative_mask(crypto_intN_z);
#endif
}

__attribute__((unused))
static inline
crypto_intN crypto_intN_positive_01(crypto_intN crypto_intN_x) {
#if amd64
 8:  crypto_intN crypto_intN_z;
 8:  int32_t crypto_intN_x32 = crypto_intN_x,crypto_intN_q32,crypto_intN_z32;
 8:  readasm("amd64; int32 crypto_intN_x32 crypto_intN_q32 crypto_intN_z32; crypto_intN_z32 = 0; crypto_intN_q32 = 1; crypto_intN_x32 & (int8) crypto_intN_x32; crypto_intN_z32 = crypto_intN_q32 if signed>");
 8:  crypto_intN_z = crypto_intN_z32;
16:  crypto_intN crypto_intN_q,crypto_intN_z;
16:  readasm("amd64; int16 crypto_intN_x crypto_intN_q crypto_intN_z; crypto_intN_z = 0; crypto_intN_q = 1; crypto_intN_x & crypto_intN_x; crypto_intN_z = crypto_intN_q if signed>");
32:  crypto_intN crypto_intN_q,crypto_intN_z;
32:  readasm("amd64; int32 crypto_intN_x crypto_intN_q crypto_intN_z; crypto_intN_z = 0; crypto_intN_q = 1; crypto_intN_x & crypto_intN_x; crypto_intN_z = crypto_intN_q if signed>");
64:  crypto_intN crypto_intN_q,crypto_intN_z;
64:  readasm("amd64; int64 crypto_intN_x crypto_intN_q crypto_intN_z; crypto_intN_z = 0; crypto_intN_q = 1; crypto_intN_x & crypto_intN_x; crypto_intN_z = crypto_intN_q if signed>");
  return crypto_intN_z;
#elif arm64
  crypto_intN crypto_intN_z;
 8:  readasm("arm64; int8 crypto_intN_x crypto_intN_z; crypto_intN_z = (int8) crypto_intN_x; crypto_intN_z - 0; crypto_intN_z = 1 if signed> else 0");
16:  readasm("arm64; int16 crypto_intN_x crypto_intN_z; crypto_intN_z = (int16) crypto_intN_x; crypto_intN_z - 0; crypto_intN_z = 1 if signed> else 0");
32:  readasm("arm64; int32 crypto_intN_x crypto_intN_z; crypto_intN_x - 0; crypto_intN_z = 1 if signed> else 0");
64:  readasm("arm64; int64 crypto_intN_x crypto_intN_z; crypto_intN_x - 0; crypto_intN_z = 1 if signed> else 0");
  return crypto_intN_z;
#elif sparc32
  crypto_intN crypto_intN_z;
 8:  readasm("sparc32; int8 crypto_intN_x crypto_intN_z; crypto_intN_z = crypto_intN_x << 24; crypto_intN_z = crypto_intN_z signed>> 31; crypto_intN_z = crypto_intN_z - crypto_intN_x; crypto_intN_z = crypto_intN_z unsigned>> 31");
16:  readasm("sparc32; int16 crypto_intN_x crypto_intN_z; crypto_intN_z = crypto_intN_x << 16; crypto_intN_z = crypto_intN_z signed>> 31; crypto_intN_z = crypto_intN_z - crypto_intN_x; crypto_intN_z = crypto_intN_z unsigned>> 31");
32:  readasm("sparc32; int32 crypto_intN_x crypto_intN_z; crypto_intN_z = crypto_intN_x signed>> 31; crypto_intN_z = crypto_intN_z - crypto_intN_x; crypto_intN_z = crypto_intN_z unsigned>> 31");
64:  readasm("sparc32; int64 crypto_intN_x crypto_intN_z; crypto_intN_z.hi = crypto_intN_x.hi signed>> 31; carry, crypto_intN_z.lo = crypto_intN_z.hi - crypto_intN_x.lo; crypto_intN_z.lo = crypto_intN_z.hi - crypto_intN_x.hi - carry; crypto_intN_z.lo = crypto_intN_z.lo unsigned>> 31; crypto_intN_z.hi = 0");
  return crypto_intN_z;
#else
  crypto_intN crypto_intN_z = -crypto_intN_x;
  crypto_intN_z ^= crypto_intN_x & crypto_intN_z;
  return crypto_intN_unsigned_topbit_01(crypto_intN_z);
#endif
}

__attribute__((unused))
static inline
crypto_intN crypto_intN_zero_mask(crypto_intN crypto_intN_x) {
#if amd64
 8:  crypto_intN crypto_intN_z;
 8:  int32_t crypto_intN_x32 = crypto_intN_x,crypto_intN_q32,crypto_intN_z32;
 8:  readasm("amd64; int32 crypto_intN_x32 crypto_intN_q32 crypto_intN_z32; crypto_intN_z32 = 0; crypto_intN_q32 = -1; crypto_intN_x32 & (int8) crypto_intN_x32; crypto_intN_z32 = crypto_intN_q32 if =");
 8:  crypto_intN_z = crypto_intN_z32;
16:  crypto_intN crypto_intN_q,crypto_intN_z;
16:  readasm("amd64; int16 crypto_intN_x crypto_intN_q crypto_intN_z; crypto_intN_z = 0; crypto_intN_q = -1; crypto_intN_x & crypto_intN_x; crypto_intN_z = crypto_intN_q if =");
32:  crypto_intN crypto_intN_q,crypto_intN_z;
32:  readasm("amd64; int32 crypto_intN_x crypto_intN_q crypto_intN_z; crypto_intN_z = 0; crypto_intN_q = -1; crypto_intN_x & crypto_intN_x; crypto_intN_z = crypto_intN_q if =");
64:  crypto_intN crypto_intN_q,crypto_intN_z;
64:  readasm("amd64; int64 crypto_intN_x crypto_intN_q crypto_intN_z; crypto_intN_z = 0; crypto_intN_q = -1; crypto_intN_x & crypto_intN_x; crypto_intN_z = crypto_intN_q if =");
  return crypto_intN_z;
#elif arm64
  crypto_intN crypto_intN_z;
 8:  readasm("arm64; int8 crypto_intN_x crypto_intN_z; crypto_intN_x & 255; crypto_intN_z = -1 if = else 0");
16:  readasm("arm64; int16 crypto_intN_x crypto_intN_z; crypto_intN_x & 65535; crypto_intN_z = -1 if = else 0");
32:  readasm("arm64; int32 crypto_intN_x crypto_intN_z; crypto_intN_x - 0; crypto_intN_z = -1 if = else 0");
64:  readasm("arm64; int64 crypto_intN_x crypto_intN_z; crypto_intN_x - 0; crypto_intN_z = -1 if = else 0");
  return crypto_intN_z;
#elif sparc32
  crypto_intN crypto_intN_z;
 8:  readasm("sparc32; int8 crypto_intN_x crypto_intN_z; crypto_intN_x = (uint8) crypto_intN_x; 0 - crypto_intN_x; crypto_intN_z = carry - 1");
16:  readasm("sparc32; int16 crypto_intN_x crypto_intN_z; crypto_intN_x = (uint16) crypto_intN_x; 0 - crypto_intN_x; crypto_intN_z = carry - 1");
32:  readasm("sparc32; int32 crypto_intN_x crypto_intN_z; 0 - crypto_intN_x; crypto_intN_z = carry - 1");
64:  readasm("sparc32; int64 crypto_intN_x crypto_intN_z; crypto_intN_z.lo = crypto_intN_x.lo | crypto_intN_x.hi; 0 - crypto_intN_z.lo; crypto_intN_z.lo = carry - 1; crypto_intN_z.hi = crypto_intN_z.lo");
  return crypto_intN_z;
#else
  return ~crypto_intN_nonzero_mask(crypto_intN_x);
#endif
}

__attribute__((unused))
static inline
crypto_intN crypto_intN_zero_01(crypto_intN crypto_intN_x) {
#if amd64
 8:  crypto_intN crypto_intN_z;
 8:  int32_t crypto_intN_x32 = crypto_intN_x,crypto_intN_q32,crypto_intN_z32;
 8:  readasm("amd64; int32 crypto_intN_x32 crypto_intN_q32 crypto_intN_z32; crypto_intN_z32 = 0; crypto_intN_q32 = 1; crypto_intN_x32 & (int8) crypto_intN_x32; crypto_intN_z32 = crypto_intN_q32 if =");
 8:  crypto_intN_z = crypto_intN_z32;
16:  crypto_intN crypto_intN_q,crypto_intN_z;
16:  readasm("amd64; int16 crypto_intN_x crypto_intN_q crypto_intN_z; crypto_intN_z = 0; crypto_intN_q = 1; crypto_intN_x & crypto_intN_x; crypto_intN_z = crypto_intN_q if =");
32:  crypto_intN crypto_intN_q,crypto_intN_z;
32:  readasm("amd64; int32 crypto_intN_x crypto_intN_q crypto_intN_z; crypto_intN_z = 0; crypto_intN_q = 1; crypto_intN_x & crypto_intN_x; crypto_intN_z = crypto_intN_q if =");
64:  crypto_intN crypto_intN_q,crypto_intN_z;
64:  readasm("amd64; int64 crypto_intN_x crypto_intN_q crypto_intN_z; crypto_intN_z = 0; crypto_intN_q = 1; crypto_intN_x & crypto_intN_x; crypto_intN_z = crypto_intN_q if =");
  return crypto_intN_z;
#elif arm64
  crypto_intN crypto_intN_z;
 8:  readasm("arm64; int8 crypto_intN_x crypto_intN_z; crypto_intN_x & 255; crypto_intN_z = 1 if = else 0");
16:  readasm("arm64; int16 crypto_intN_x crypto_intN_z; crypto_intN_x & 65535; crypto_intN_z = 1 if = else 0");
32:  readasm("arm64; int32 crypto_intN_x crypto_intN_z; crypto_intN_x - 0; crypto_intN_z = 1 if = else 0");
64:  readasm("arm64; int64 crypto_intN_x crypto_intN_z; crypto_intN_x - 0; crypto_intN_z = 1 if = else 0");
  return crypto_intN_z;
#elif sparc32
  crypto_intN crypto_intN_z;
 8:  readasm("sparc32; int8 crypto_intN_x crypto_intN_z; crypto_intN_x = (uint8) crypto_intN_x; 0 - crypto_intN_x; crypto_intN_z = 1 - carry");
16:  readasm("sparc32; int16 crypto_intN_x crypto_intN_z; crypto_intN_x = (uint16) crypto_intN_x; 0 - crypto_intN_x; crypto_intN_z = 1 - carry");
32:  readasm("sparc32; int32 crypto_intN_x crypto_intN_z; 0 - crypto_intN_x; crypto_intN_z = 1 - carry");
64:  readasm("sparc32; int64 crypto_intN_x crypto_intN_z; crypto_intN_z.lo = crypto_intN_x.lo | crypto_intN_x.hi; 0 - crypto_intN_z.lo; crypto_intN_z.lo = 1 - carry; crypto_intN_z.hi = 0");
  return crypto_intN_z;
#else
  return 1-crypto_intN_nonzero_01(crypto_intN_x);
#endif
}

__attribute__((unused))
static inline
crypto_intN crypto_intN_unequal_mask(crypto_intN crypto_intN_x,crypto_intN crypto_intN_y) {
#if amd64
 8:  crypto_intN crypto_intN_z;
 8:  int32_t crypto_intN_x32 = crypto_intN_x,crypto_intN_y32 = crypto_intN_y,crypto_intN_q32,crypto_intN_z32;
 8:  readasm("amd64; int32 crypto_intN_x32 crypto_intN_q32 crypto_intN_y32 crypto_intN_z32; crypto_intN_z32 = 0; crypto_intN_q32 = -1; crypto_intN_x32 - (int8) crypto_intN_y32; crypto_intN_z32 = crypto_intN_q32 if !=");
 8:  crypto_intN_z = crypto_intN_z32;
16:  crypto_intN crypto_intN_q,crypto_intN_z;
16:  readasm("amd64; int16 crypto_intN_x crypto_intN_q crypto_intN_y crypto_intN_z; crypto_intN_z = 0; crypto_intN_q = -1; crypto_intN_x - crypto_intN_y; crypto_intN_z = crypto_intN_q if !=");
32:  crypto_intN crypto_intN_q,crypto_intN_z;
32:  readasm("amd64; int32 crypto_intN_x crypto_intN_q crypto_intN_y crypto_intN_z; crypto_intN_z = 0; crypto_intN_q = -1; crypto_intN_x - crypto_intN_y; crypto_intN_z = crypto_intN_q if !=");
64:  crypto_intN crypto_intN_q,crypto_intN_z;
64:  readasm("amd64; int64 crypto_intN_x crypto_intN_q crypto_intN_y crypto_intN_z; crypto_intN_z = 0; crypto_intN_q = -1; crypto_intN_x - crypto_intN_y; crypto_intN_z = crypto_intN_q if !=");
  return crypto_intN_z;
#elif arm64
  crypto_intN crypto_intN_z;
 8:  readasm("arm64; int8 crypto_intN_x crypto_intN_y crypto_intN_z; crypto_intN_z = (uint8) crypto_intN_x; crypto_intN_z - (uint8) crypto_intN_y; crypto_intN_z = -1 if != else 0");
16:  readasm("arm64; int16 crypto_intN_x crypto_intN_y crypto_intN_z; crypto_intN_z = (uint16) crypto_intN_x; crypto_intN_z - (uint16) crypto_intN_y; crypto_intN_z = -1 if != else 0");
32:  readasm("arm64; int32 crypto_intN_x crypto_intN_y crypto_intN_z; crypto_intN_x - crypto_intN_y; crypto_intN_z = -1 if != else 0");
64:  readasm("arm64; int64 crypto_intN_x crypto_intN_y crypto_intN_z; crypto_intN_x - crypto_intN_y; crypto_intN_z = -1 if != else 0");
  return crypto_intN_z;
#else
  return crypto_intN_nonzero_mask(crypto_intN_x ^ crypto_intN_y);
#endif
}

__attribute__((unused))
static inline
crypto_intN crypto_intN_unequal_01(crypto_intN crypto_intN_x,crypto_intN crypto_intN_y) {
#if amd64
 8:  crypto_intN crypto_intN_z;
 8:  int32_t crypto_intN_x32 = crypto_intN_x,crypto_intN_y32 = crypto_intN_y,crypto_intN_q32,crypto_intN_z32;
 8:  readasm("amd64; int32 crypto_intN_x32 crypto_intN_q32 crypto_intN_y32 crypto_intN_z32; crypto_intN_z32 = 0; crypto_intN_q32 = 1; crypto_intN_x32 - (int8) crypto_intN_y32; crypto_intN_z32 = crypto_intN_q32 if !=");
 8:  crypto_intN_z = crypto_intN_z32;
16:  crypto_intN crypto_intN_q,crypto_intN_z;
16:  readasm("amd64; int16 crypto_intN_x crypto_intN_q crypto_intN_y crypto_intN_z; crypto_intN_z = 0; crypto_intN_q = 1; crypto_intN_x - crypto_intN_y; crypto_intN_z = crypto_intN_q if !=");
32:  crypto_intN crypto_intN_q,crypto_intN_z;
32:  readasm("amd64; int32 crypto_intN_x crypto_intN_q crypto_intN_y crypto_intN_z; crypto_intN_z = 0; crypto_intN_q = 1; crypto_intN_x - crypto_intN_y; crypto_intN_z = crypto_intN_q if !=");
64:  crypto_intN crypto_intN_q,crypto_intN_z;
64:  readasm("amd64; int64 crypto_intN_x crypto_intN_q crypto_intN_y crypto_intN_z; crypto_intN_z = 0; crypto_intN_q = 1; crypto_intN_x - crypto_intN_y; crypto_intN_z = crypto_intN_q if !=");
  return crypto_intN_z;
#elif arm64
  crypto_intN crypto_intN_z;
 8:  readasm("arm64; int8 crypto_intN_x crypto_intN_y crypto_intN_z; crypto_intN_z = (uint8) crypto_intN_x; crypto_intN_z - (uint8) crypto_intN_y; crypto_intN_z = 1 if != else 0");
16:  readasm("arm64; int16 crypto_intN_x crypto_intN_y crypto_intN_z; crypto_intN_z = (uint16) crypto_intN_x; crypto_intN_z - (uint16) crypto_intN_y; crypto_intN_z = 1 if != else 0");
32:  readasm("arm64; int32 crypto_intN_x crypto_intN_y crypto_intN_z; crypto_intN_x - crypto_intN_y; crypto_intN_z = 1 if != else 0");
64:  readasm("arm64; int64 crypto_intN_x crypto_intN_y crypto_intN_z; crypto_intN_x - crypto_intN_y; crypto_intN_z = 1 if != else 0");
  return crypto_intN_z;
#else
  return crypto_intN_nonzero_01(crypto_intN_x ^ crypto_intN_y);
#endif
}

__attribute__((unused))
static inline
crypto_intN crypto_intN_equal_mask(crypto_intN crypto_intN_x,crypto_intN crypto_intN_y) {
#if amd64
 8:  crypto_intN crypto_intN_z;
 8:  int32_t crypto_intN_x32 = crypto_intN_x,crypto_intN_y32 = crypto_intN_y,crypto_intN_q32,crypto_intN_z32;
 8:  readasm("amd64; int32 crypto_intN_x32 crypto_intN_q32 crypto_intN_y32 crypto_intN_z32; crypto_intN_z32 = 0; crypto_intN_q32 = -1; crypto_intN_x32 - (int8) crypto_intN_y32; crypto_intN_z32 = crypto_intN_q32 if =");
 8:  crypto_intN_z = crypto_intN_z32;
16:  crypto_intN crypto_intN_q,crypto_intN_z;
16:  readasm("amd64; int16 crypto_intN_x crypto_intN_q crypto_intN_y crypto_intN_z; crypto_intN_z = 0; crypto_intN_q = -1; crypto_intN_x - crypto_intN_y; crypto_intN_z = crypto_intN_q if =");
32:  crypto_intN crypto_intN_q,crypto_intN_z;
32:  readasm("amd64; int32 crypto_intN_x crypto_intN_q crypto_intN_y crypto_intN_z; crypto_intN_z = 0; crypto_intN_q = -1; crypto_intN_x - crypto_intN_y; crypto_intN_z = crypto_intN_q if =");
64:  crypto_intN crypto_intN_q,crypto_intN_z;
64:  readasm("amd64; int64 crypto_intN_x crypto_intN_q crypto_intN_y crypto_intN_z; crypto_intN_z = 0; crypto_intN_q = -1; crypto_intN_x - crypto_intN_y; crypto_intN_z = crypto_intN_q if =");
  return crypto_intN_z;
#elif arm64
  crypto_intN crypto_intN_z;
 8:  readasm("arm64; int8 crypto_intN_x crypto_intN_y crypto_intN_z; crypto_intN_z = (uint8) crypto_intN_x; crypto_intN_z - (uint8) crypto_intN_y; crypto_intN_z = -1 if = else 0");
16:  readasm("arm64; int16 crypto_intN_x crypto_intN_y crypto_intN_z; crypto_intN_z = (uint16) crypto_intN_x; crypto_intN_z - (uint16) crypto_intN_y; crypto_intN_z = -1 if = else 0");
32:  readasm("arm64; int32 crypto_intN_x crypto_intN_y crypto_intN_z; crypto_intN_x - crypto_intN_y; crypto_intN_z = -1 if = else 0");
64:  readasm("arm64; int64 crypto_intN_x crypto_intN_y crypto_intN_z; crypto_intN_x - crypto_intN_y; crypto_intN_z = -1 if = else 0");
  return crypto_intN_z;
#else
  return crypto_intN_zero_mask(crypto_intN_x ^ crypto_intN_y);
#endif
}

__attribute__((unused))
static inline
crypto_intN crypto_intN_equal_01(crypto_intN crypto_intN_x,crypto_intN crypto_intN_y) {
#if amd64
 8:  crypto_intN crypto_intN_z;
 8:  int32_t crypto_intN_x32 = crypto_intN_x,crypto_intN_y32 = crypto_intN_y,crypto_intN_q32,crypto_intN_z32;
 8:  readasm("amd64; int32 crypto_intN_x32 crypto_intN_q32 crypto_intN_y32 crypto_intN_z32; crypto_intN_z32 = 0; crypto_intN_q32 = 1; crypto_intN_x32 - (int8) crypto_intN_y32; crypto_intN_z32 = crypto_intN_q32 if =");
 8:  crypto_intN_z = crypto_intN_z32;
16:  crypto_intN crypto_intN_q,crypto_intN_z;
16:  readasm("amd64; int16 crypto_intN_x crypto_intN_q crypto_intN_y crypto_intN_z; crypto_intN_z = 0; crypto_intN_q = 1; crypto_intN_x - crypto_intN_y; crypto_intN_z = crypto_intN_q if =");
32:  crypto_intN crypto_intN_q,crypto_intN_z;
32:  readasm("amd64; int32 crypto_intN_x crypto_intN_q crypto_intN_y crypto_intN_z; crypto_intN_z = 0; crypto_intN_q = 1; crypto_intN_x - crypto_intN_y; crypto_intN_z = crypto_intN_q if =");
64:  crypto_intN crypto_intN_q,crypto_intN_z;
64:  readasm("amd64; int64 crypto_intN_x crypto_intN_q crypto_intN_y crypto_intN_z; crypto_intN_z = 0; crypto_intN_q = 1; crypto_intN_x - crypto_intN_y; crypto_intN_z = crypto_intN_q if =");
  return crypto_intN_z;
#elif arm64
  crypto_intN crypto_intN_z;
 8:  readasm("arm64; int8 crypto_intN_x crypto_intN_y crypto_intN_z; crypto_intN_z = (uint8) crypto_intN_x; crypto_intN_z - (uint8) crypto_intN_y; crypto_intN_z = 1 if = else 0");
16:  readasm("arm64; int16 crypto_intN_x crypto_intN_y crypto_intN_z; crypto_intN_z = (uint16) crypto_intN_x; crypto_intN_z - (uint16) crypto_intN_y; crypto_intN_z = 1 if = else 0");
32:  readasm("arm64; int32 crypto_intN_x crypto_intN_y crypto_intN_z; crypto_intN_x - crypto_intN_y; crypto_intN_z = 1 if = else 0");
64:  readasm("arm64; int64 crypto_intN_x crypto_intN_y crypto_intN_z; crypto_intN_x - crypto_intN_y; crypto_intN_z = 1 if = else 0");
  return crypto_intN_z;
#else
  return crypto_intN_zero_01(crypto_intN_x ^ crypto_intN_y);
#endif
}

__attribute__((unused))
static inline
crypto_intN crypto_intN_min(crypto_intN crypto_intN_x,crypto_intN crypto_intN_y) {
#if amd64
 8:  int32_t crypto_intN_x32 = crypto_intN_x,crypto_intN_y32 = crypto_intN_y;
 8:  readasm("amd64; int32 crypto_intN_x32 crypto_intN_y32; crypto_intN_x32 - (int8) crypto_intN_y32; crypto_intN_x32 = crypto_intN_y32 if signed>");
 8:  crypto_intN_x = crypto_intN_x32;
16:  readasm("amd64; int16 crypto_intN_x crypto_intN_y; crypto_intN_x - crypto_intN_y; crypto_intN_x = crypto_intN_y if signed>");
32:  readasm("amd64; int32 crypto_intN_x crypto_intN_y; crypto_intN_x - crypto_intN_y; crypto_intN_x = crypto_intN_y if signed>");
64:  readasm("amd64; int64 crypto_intN_x crypto_intN_y; crypto_intN_x - crypto_intN_y; crypto_intN_x = crypto_intN_y if signed>");
  return crypto_intN_x;
#elif arm64
 8:  readasm("arm64; int8 crypto_intN_x crypto_intN_y; crypto_intN_x = (int8) crypto_intN_x; crypto_intN_x - (int8) crypto_intN_y; crypto_intN_x = crypto_intN_x if signed< else crypto_intN_y");
16:  readasm("arm64; int16 crypto_intN_x crypto_intN_y; crypto_intN_x = (int16) crypto_intN_x; crypto_intN_x - (int16) crypto_intN_y; crypto_intN_x = crypto_intN_x if signed< else crypto_intN_y");
32:  readasm("arm64; int32 crypto_intN_x crypto_intN_y; crypto_intN_x - crypto_intN_y; crypto_intN_x = crypto_intN_x if signed< else crypto_intN_y");
64:  readasm("arm64; int64 crypto_intN_x crypto_intN_y; crypto_intN_x - crypto_intN_y; crypto_intN_x = crypto_intN_x if signed< else crypto_intN_y");
  return crypto_intN_x;
#else
  crypto_intN crypto_intN_r = crypto_intN_y ^ crypto_intN_x;
  crypto_intN crypto_intN_z = crypto_intN_y - crypto_intN_x;
  crypto_intN_z ^= crypto_intN_r & (crypto_intN_z ^ crypto_intN_y);
  crypto_intN_z = crypto_intN_negative_mask(crypto_intN_z);
  crypto_intN_z &= crypto_intN_r;
  return crypto_intN_x ^ crypto_intN_z;
#endif
}

__attribute__((unused))
static inline
crypto_intN crypto_intN_max(crypto_intN crypto_intN_x,crypto_intN crypto_intN_y) {
#if amd64
 8:  int32_t crypto_intN_x32 = crypto_intN_x,crypto_intN_y32 = crypto_intN_y;
 8:  readasm("amd64; int32 crypto_intN_x32 crypto_intN_y32; crypto_intN_x32 - (int8) crypto_intN_y32; crypto_intN_x32 = crypto_intN_y32 if signed<");
 8:  crypto_intN_x = crypto_intN_x32;
16:  readasm("amd64; int16 crypto_intN_x crypto_intN_y; crypto_intN_x - crypto_intN_y; crypto_intN_x = crypto_intN_y if signed<");
32:  readasm("amd64; int32 crypto_intN_x crypto_intN_y; crypto_intN_x - crypto_intN_y; crypto_intN_x = crypto_intN_y if signed<");
64:  readasm("amd64; int64 crypto_intN_x crypto_intN_y; crypto_intN_x - crypto_intN_y; crypto_intN_x = crypto_intN_y if signed<");
  return crypto_intN_x;
#elif arm64
 8:  readasm("arm64; int8 crypto_intN_x crypto_intN_y; crypto_intN_x = (int8) crypto_intN_x; crypto_intN_x - (int8) crypto_intN_y; crypto_intN_x = crypto_intN_y if signed< else crypto_intN_x");
16:  readasm("arm64; int16 crypto_intN_x crypto_intN_y; crypto_intN_x = (int16) crypto_intN_x; crypto_intN_x - (int16) crypto_intN_y; crypto_intN_x = crypto_intN_y if signed< else crypto_intN_x");
32:  readasm("arm64; int32 crypto_intN_x crypto_intN_y; crypto_intN_x - crypto_intN_y; crypto_intN_x = crypto_intN_y if signed< else crypto_intN_x");
64:  readasm("arm64; int64 crypto_intN_x crypto_intN_y; crypto_intN_x - crypto_intN_y; crypto_intN_x = crypto_intN_y if signed< else crypto_intN_x");
  return crypto_intN_x;
#else
  crypto_intN crypto_intN_r = crypto_intN_y ^ crypto_intN_x;
  crypto_intN crypto_intN_z = crypto_intN_y - crypto_intN_x;
  crypto_intN_z ^= crypto_intN_r & (crypto_intN_z ^ crypto_intN_y);
  crypto_intN_z = crypto_intN_negative_mask(crypto_intN_z);
  crypto_intN_z &= crypto_intN_r;
  return crypto_intN_y ^ crypto_intN_z;
#endif
}

__attribute__((unused))
static inline
void crypto_intN_minmax(crypto_intN *crypto_intN_p,crypto_intN *crypto_intN_q) {
  crypto_intN crypto_intN_x = *crypto_intN_p;
  crypto_intN crypto_intN_y = *crypto_intN_q;
#if amd64
 8:  int32_t crypto_intN_x32 = crypto_intN_x,crypto_intN_y32 = crypto_intN_y,crypto_intN_z32;
 8:  readasm("amd64; int32 crypto_intN_x32 crypto_intN_y32 crypto_intN_z32; crypto_intN_x32 - (int8) crypto_intN_y32; crypto_intN_z32 = crypto_intN_x32; crypto_intN_x32 = crypto_intN_y32 if signed>; crypto_intN_y32 = crypto_intN_z32 if signed>");
 8:  crypto_intN_x = crypto_intN_x32; crypto_intN_y = crypto_intN_y32;
16:  crypto_intN crypto_intN_z;
16:  readasm("amd64; int16 crypto_intN_x crypto_intN_y crypto_intN_z; crypto_intN_x - crypto_intN_y; crypto_intN_z = crypto_intN_x; crypto_intN_x = crypto_intN_y if signed>; crypto_intN_y = crypto_intN_z if signed>");
32:  crypto_intN crypto_intN_z;
32:  readasm("amd64; int32 crypto_intN_x crypto_intN_y crypto_intN_z; crypto_intN_x - crypto_intN_y; crypto_intN_z = crypto_intN_x; crypto_intN_x = crypto_intN_y if signed>; crypto_intN_y = crypto_intN_z if signed>");
64:  crypto_intN crypto_intN_z;
64:  readasm("amd64; int64 crypto_intN_x crypto_intN_y crypto_intN_z; crypto_intN_x - crypto_intN_y; crypto_intN_z = crypto_intN_x; crypto_intN_x = crypto_intN_y if signed>; crypto_intN_y = crypto_intN_z if signed>");
  *crypto_intN_p = crypto_intN_x;
  *crypto_intN_q = crypto_intN_y;
#elif arm64
  crypto_intN crypto_intN_r, crypto_intN_s;
 8:  readasm("arm64; int8 crypto_intN_x crypto_intN_y crypto_intN_r crypto_intN_s; crypto_intN_x = (int8) crypto_intN_x; crypto_intN_x - (int8) crypto_intN_y; crypto_intN_r = crypto_intN_x if signed< else crypto_intN_y; crypto_intN_s = crypto_intN_y if signed< else crypto_intN_x");
16:  readasm("arm64; int16 crypto_intN_x crypto_intN_y crypto_intN_r crypto_intN_s; crypto_intN_x = (int16) crypto_intN_x; crypto_intN_x - (int16) crypto_intN_y; crypto_intN_r = crypto_intN_x if signed< else crypto_intN_y; crypto_intN_s = crypto_intN_y if signed< else crypto_intN_x");
32:  readasm("arm64; int32 crypto_intN_x crypto_intN_y crypto_intN_r crypto_intN_s; crypto_intN_x - crypto_intN_y; crypto_intN_r = crypto_intN_x if signed< else crypto_intN_y; crypto_intN_s = crypto_intN_y if signed< else crypto_intN_x");
64:  readasm("arm64; int64 crypto_intN_x crypto_intN_y crypto_intN_r crypto_intN_s; crypto_intN_x - crypto_intN_y; crypto_intN_r = crypto_intN_x if signed< else crypto_intN_y; crypto_intN_s = crypto_intN_y if signed< else crypto_intN_x");
  *crypto_intN_p = crypto_intN_r;
  *crypto_intN_q = crypto_intN_s;
#else
  crypto_intN crypto_intN_r = crypto_intN_y ^ crypto_intN_x;
  crypto_intN crypto_intN_z = crypto_intN_y - crypto_intN_x;
  crypto_intN_z ^= crypto_intN_r & (crypto_intN_z ^ crypto_intN_y);
  crypto_intN_z = crypto_intN_negative_mask(crypto_intN_z);
  crypto_intN_z &= crypto_intN_r;
  crypto_intN_x ^= crypto_intN_z;
  crypto_intN_y ^= crypto_intN_z;
  *crypto_intN_p = crypto_intN_x;
  *crypto_intN_q = crypto_intN_y;
#endif
}

__attribute__((unused))
static inline
crypto_intN crypto_intN_smaller_mask(crypto_intN crypto_intN_x,crypto_intN crypto_intN_y) {
#if amd64
 8:  crypto_intN crypto_intN_z;
 8:  int32_t crypto_intN_x32 = crypto_intN_x,crypto_intN_y32 = crypto_intN_y,crypto_intN_q32,crypto_intN_z32;
 8:  readasm("amd64; int32 crypto_intN_x32 crypto_intN_q32 crypto_intN_y32 crypto_intN_z32; crypto_intN_z32 = 0; crypto_intN_q32 = -1; crypto_intN_x32 - (int8) crypto_intN_y32; crypto_intN_z32 = crypto_intN_q32 if signed<");
 8:  crypto_intN_z = crypto_intN_z32;
16:  crypto_intN crypto_intN_q,crypto_intN_z;
16:  readasm("amd64; int16 crypto_intN_x crypto_intN_q crypto_intN_y crypto_intN_z; crypto_intN_z = 0; crypto_intN_q = -1; crypto_intN_x - crypto_intN_y; crypto_intN_z = crypto_intN_q if signed<");
32:  crypto_intN crypto_intN_q,crypto_intN_z;
32:  readasm("amd64; int32 crypto_intN_x crypto_intN_q crypto_intN_y crypto_intN_z; crypto_intN_z = 0; crypto_intN_q = -1; crypto_intN_x - crypto_intN_y; crypto_intN_z = crypto_intN_q if signed<");
64:  crypto_intN crypto_intN_q,crypto_intN_z;
64:  readasm("amd64; int64 crypto_intN_x crypto_intN_q crypto_intN_y crypto_intN_z; crypto_intN_z = 0; crypto_intN_q = -1; crypto_intN_x - crypto_intN_y; crypto_intN_z = crypto_intN_q if signed<");
  return crypto_intN_z;
#elif arm64
  crypto_intN crypto_intN_z;
 8:  readasm("arm64; int8 crypto_intN_x crypto_intN_y crypto_intN_z; crypto_intN_z = (int8) crypto_intN_x; crypto_intN_z - (int8) crypto_intN_y; crypto_intN_z = -1 if signed< else 0");
16:  readasm("arm64; int16 crypto_intN_x crypto_intN_y crypto_intN_z; crypto_intN_z = (int16) crypto_intN_x; crypto_intN_z - (int16) crypto_intN_y; crypto_intN_z = -1 if signed< else 0");
32:  readasm("arm64; int32 crypto_intN_x crypto_intN_y crypto_intN_z; crypto_intN_x - crypto_intN_y; crypto_intN_z = -1 if signed< else 0");
64:  readasm("arm64; int64 crypto_intN_x crypto_intN_y crypto_intN_z; crypto_intN_x - crypto_intN_y; crypto_intN_z = -1 if signed< else 0");
  return crypto_intN_z;
#else
  crypto_intN crypto_intN_r = crypto_intN_x ^ crypto_intN_y;
  crypto_intN crypto_intN_z = crypto_intN_x - crypto_intN_y;
  crypto_intN_z ^= crypto_intN_r & (crypto_intN_z ^ crypto_intN_x);
  return crypto_intN_negative_mask(crypto_intN_z);
#endif
}

__attribute__((unused))
static inline
crypto_intN crypto_intN_smaller_01(crypto_intN crypto_intN_x,crypto_intN crypto_intN_y) {
#if amd64
 8:  crypto_intN crypto_intN_z;
 8:  int32_t crypto_intN_x32 = crypto_intN_x,crypto_intN_y32 = crypto_intN_y,crypto_intN_q32,crypto_intN_z32;
 8:  readasm("amd64; int32 crypto_intN_x32 crypto_intN_q32 crypto_intN_y32 crypto_intN_z32; crypto_intN_z32 = 0; crypto_intN_q32 = 1; crypto_intN_x32 - (int8) crypto_intN_y32; crypto_intN_z32 = crypto_intN_q32 if signed<");
 8:  crypto_intN_z = crypto_intN_z32;
16:  crypto_intN crypto_intN_q,crypto_intN_z;
16:  readasm("amd64; int16 crypto_intN_x crypto_intN_q crypto_intN_y crypto_intN_z; crypto_intN_z = 0; crypto_intN_q = 1; crypto_intN_x - crypto_intN_y; crypto_intN_z = crypto_intN_q if signed<");
32:  crypto_intN crypto_intN_q,crypto_intN_z;
32:  readasm("amd64; int32 crypto_intN_x crypto_intN_q crypto_intN_y crypto_intN_z; crypto_intN_z = 0; crypto_intN_q = 1; crypto_intN_x - crypto_intN_y; crypto_intN_z = crypto_intN_q if signed<");
64:  crypto_intN crypto_intN_q,crypto_intN_z;
64:  readasm("amd64; int64 crypto_intN_x crypto_intN_q crypto_intN_y crypto_intN_z; crypto_intN_z = 0; crypto_intN_q = 1; crypto_intN_x - crypto_intN_y; crypto_intN_z = crypto_intN_q if signed<");
  return crypto_intN_z;
#elif arm64
  crypto_intN crypto_intN_z;
 8:  readasm("arm64; int8 crypto_intN_x crypto_intN_y crypto_intN_z; crypto_intN_z = (int8) crypto_intN_x; crypto_intN_z - (int8) crypto_intN_y; crypto_intN_z = 1 if signed< else 0");
16:  readasm("arm64; int16 crypto_intN_x crypto_intN_y crypto_intN_z; crypto_intN_z = (int16) crypto_intN_x; crypto_intN_z - (int16) crypto_intN_y; crypto_intN_z = 1 if signed< else 0");
32:  readasm("arm64; int32 crypto_intN_x crypto_intN_y crypto_intN_z; crypto_intN_x - crypto_intN_y; crypto_intN_z = 1 if signed< else 0");
64:  readasm("arm64; int64 crypto_intN_x crypto_intN_y crypto_intN_z; crypto_intN_x - crypto_intN_y; crypto_intN_z = 1 if signed< else 0");
  return crypto_intN_z;
#else
  crypto_intN crypto_intN_r = crypto_intN_x ^ crypto_intN_y;
  crypto_intN crypto_intN_z = crypto_intN_x - crypto_intN_y;
  crypto_intN_z ^= crypto_intN_r & (crypto_intN_z ^ crypto_intN_x);
  return crypto_intN_unsigned_topbit_01(crypto_intN_z);
#endif
}

__attribute__((unused))
static inline
crypto_intN crypto_intN_leq_mask(crypto_intN crypto_intN_x,crypto_intN crypto_intN_y) {
#if amd64
 8:  crypto_intN crypto_intN_z;
 8:  int32_t crypto_intN_x32 = crypto_intN_x,crypto_intN_y32 = crypto_intN_y,crypto_intN_q32,crypto_intN_z32;
 8:  readasm("amd64; int32 crypto_intN_x32 crypto_intN_q32 crypto_intN_y32 crypto_intN_z32; crypto_intN_z32 = 0; crypto_intN_q32 = -1; crypto_intN_x32 - (int8) crypto_intN_y32; crypto_intN_z32 = crypto_intN_q32 if signed<=");
 8:  crypto_intN_z = crypto_intN_z32;
16:  crypto_intN crypto_intN_q,crypto_intN_z;
16:  readasm("amd64; int16 crypto_intN_x crypto_intN_q crypto_intN_y crypto_intN_z; crypto_intN_z = 0; crypto_intN_q = -1; crypto_intN_x - crypto_intN_y; crypto_intN_z = crypto_intN_q if signed<=");
32:  crypto_intN crypto_intN_q,crypto_intN_z;
32:  readasm("amd64; int32 crypto_intN_x crypto_intN_q crypto_intN_y crypto_intN_z; crypto_intN_z = 0; crypto_intN_q = -1; crypto_intN_x - crypto_intN_y; crypto_intN_z = crypto_intN_q if signed<=");
64:  crypto_intN crypto_intN_q,crypto_intN_z;
64:  readasm("amd64; int64 crypto_intN_x crypto_intN_q crypto_intN_y crypto_intN_z; crypto_intN_z = 0; crypto_intN_q = -1; crypto_intN_x - crypto_intN_y; crypto_intN_z = crypto_intN_q if signed<=");
  return crypto_intN_z;
#elif arm64
  crypto_intN crypto_intN_z;
 8:  readasm("arm64; int8 crypto_intN_x crypto_intN_y crypto_intN_z; crypto_intN_z = (int8) crypto_intN_x; crypto_intN_z - (int8) crypto_intN_y; crypto_intN_z = -1 if signed<= else 0");
16:  readasm("arm64; int16 crypto_intN_x crypto_intN_y crypto_intN_z; crypto_intN_z = (int16) crypto_intN_x; crypto_intN_z - (int16) crypto_intN_y; crypto_intN_z = -1 if signed<= else 0");
32:  readasm("arm64; int32 crypto_intN_x crypto_intN_y crypto_intN_z; crypto_intN_x - crypto_intN_y; crypto_intN_z = -1 if signed<= else 0");
64:  readasm("arm64; int64 crypto_intN_x crypto_intN_y crypto_intN_z; crypto_intN_x - crypto_intN_y; crypto_intN_z = -1 if signed<= else 0");
  return crypto_intN_z;
#else
  return ~crypto_intN_smaller_mask(crypto_intN_y,crypto_intN_x);
#endif
}

__attribute__((unused))
static inline
crypto_intN crypto_intN_leq_01(crypto_intN crypto_intN_x,crypto_intN crypto_intN_y) {
#if amd64
 8:  crypto_intN crypto_intN_z;
 8:  int32_t crypto_intN_x32 = crypto_intN_x,crypto_intN_y32 = crypto_intN_y,crypto_intN_q32,crypto_intN_z32;
 8:  readasm("amd64; int32 crypto_intN_x32 crypto_intN_q32 crypto_intN_y32 crypto_intN_z32; crypto_intN_z32 = 0; crypto_intN_q32 = 1; crypto_intN_x32 - (int8) crypto_intN_y32; crypto_intN_z32 = crypto_intN_q32 if signed<=");
 8:  crypto_intN_z = crypto_intN_z32;
16:  crypto_intN crypto_intN_q,crypto_intN_z;
16:  readasm("amd64; int16 crypto_intN_x crypto_intN_q crypto_intN_y crypto_intN_z; crypto_intN_z = 0; crypto_intN_q = 1; crypto_intN_x - crypto_intN_y; crypto_intN_z = crypto_intN_q if signed<=");
32:  crypto_intN crypto_intN_q,crypto_intN_z;
32:  readasm("amd64; int32 crypto_intN_x crypto_intN_q crypto_intN_y crypto_intN_z; crypto_intN_z = 0; crypto_intN_q = 1; crypto_intN_x - crypto_intN_y; crypto_intN_z = crypto_intN_q if signed<=");
64:  crypto_intN crypto_intN_q,crypto_intN_z;
64:  readasm("amd64; int64 crypto_intN_x crypto_intN_q crypto_intN_y crypto_intN_z; crypto_intN_z = 0; crypto_intN_q = 1; crypto_intN_x - crypto_intN_y; crypto_intN_z = crypto_intN_q if signed<=");
  return crypto_intN_z;
#elif arm64
  crypto_intN crypto_intN_z;
 8:  readasm("arm64; int8 crypto_intN_x crypto_intN_y crypto_intN_z; crypto_intN_z = (int8) crypto_intN_x; crypto_intN_z - (int8) crypto_intN_y; crypto_intN_z = 1 if signed<= else 0");
16:  readasm("arm64; int16 crypto_intN_x crypto_intN_y crypto_intN_z; crypto_intN_z = (int16) crypto_intN_x; crypto_intN_z - (int16) crypto_intN_y; crypto_intN_z = 1 if signed<= else 0");
32:  readasm("arm64; int32 crypto_intN_x crypto_intN_y crypto_intN_z; crypto_intN_x - crypto_intN_y; crypto_intN_z = 1 if signed<= else 0");
64:  readasm("arm64; int64 crypto_intN_x crypto_intN_y crypto_intN_z; crypto_intN_x - crypto_intN_y; crypto_intN_z = 1 if signed<= else 0");
  return crypto_intN_z;
#else
  return 1-crypto_intN_smaller_01(crypto_intN_y,crypto_intN_x);
#endif
}

__attribute__((unused))
static inline
int crypto_intN_ones_num(crypto_intN crypto_intN_x) {
  crypto_intN_unsigned crypto_intN_y = crypto_intN_x;
 8:  const crypto_intN C0 = 0x55;
16:  const crypto_intN C0 = 0x5555;
32:  const crypto_intN C0 = 0x55555555;
64:  const crypto_intN C0 = 0x5555555555555555;
 8:  const crypto_intN C1 = 0x33;
16:  const crypto_intN C1 = 0x3333;
32:  const crypto_intN C1 = 0x33333333;
64:  const crypto_intN C1 = 0x3333333333333333;
 8:  const crypto_intN C2 = 0x0f;
16:  const crypto_intN C2 = 0x0f0f;
32:  const crypto_intN C2 = 0x0f0f0f0f;
64:  const crypto_intN C2 = 0x0f0f0f0f0f0f0f0f;
  crypto_intN_y -= ((crypto_intN_y >> 1) & C0);
  crypto_intN_y = (crypto_intN_y & C1) + ((crypto_intN_y >> 2) & C1);
  crypto_intN_y = (crypto_intN_y + (crypto_intN_y >> 4)) & C2;
16:  crypto_intN_y = (crypto_intN_y + (crypto_intN_y >> 8)) & 0xff;
32:  crypto_intN_y += crypto_intN_y >> 8;
32:  crypto_intN_y = (crypto_intN_y + (crypto_intN_y >> 16)) & 0xff;
64:  crypto_intN_y += crypto_intN_y >> 8;
64:  crypto_intN_y += crypto_intN_y >> 16;
64:  crypto_intN_y = (crypto_intN_y + (crypto_intN_y >> 32)) & 0xff;
  return crypto_intN_y;
}

__attribute__((unused))
static inline
int crypto_intN_bottomzeros_num(crypto_intN crypto_intN_x) {
#if amd64
 8:  int32_t fallback = N;
 8:  int32_t crypto_intN_x32 = crypto_intN_x;
 8:  readasm("amd64; int32 crypto_intN_x32 fallback; crypto_intN_x32 = numbottomzeros_tricky crypto_intN_x32; crypto_intN_x32 = fallback if =");
 8:  crypto_intN_x = crypto_intN_x32;
16:  crypto_intN fallback = N;
16:  readasm("amd64; int16 crypto_intN_x fallback; crypto_intN_x = numbottomzeros_tricky crypto_intN_x; crypto_intN_x = fallback if =");
32:  crypto_intN fallback = N;
32:  readasm("amd64; int32 crypto_intN_x fallback; crypto_intN_x = numbottomzeros_tricky crypto_intN_x; crypto_intN_x = fallback if =");
64:  crypto_intN fallback = N;
64:  readasm("amd64; int64 crypto_intN_x fallback; crypto_intN_x = numbottomzeros_tricky crypto_intN_x; crypto_intN_x = fallback if =");
  return crypto_intN_x;
#elif arm64
  int64_t crypto_intN_y;
 8:  readasm("arm64; int8 crypto_intN_x crypto_intN_y; crypto_intN_y = crypto_intN_x | -256; crypto_intN_y = bitrev32 crypto_intN_y; crypto_intN_y = numbottomzeros crypto_intN_y");
16:  readasm("arm64; int16 crypto_intN_x crypto_intN_y; crypto_intN_y = crypto_intN_x | -65536; crypto_intN_y = bitrev32 crypto_intN_y; crypto_intN_y = numbottomzeros crypto_intN_y");
32:  readasm("arm64; int32 crypto_intN_x crypto_intN_y; crypto_intN_y = bitrev32 crypto_intN_x; crypto_intN_y = numbottomzeros crypto_intN_y");
64:  readasm("arm64; int64 crypto_intN_x crypto_intN_y; crypto_intN_y = bitrev64 crypto_intN_x; crypto_intN_y = numbottomzeros crypto_intN_y");
  return crypto_intN_y;
#else
  crypto_intN crypto_intN_y = crypto_intN_x ^ (crypto_intN_x-1);
  crypto_intN_y = ((crypto_intN) crypto_intN_y) >> 1;
  crypto_intN_y &= ~(crypto_intN_x & (((crypto_intN) 1) << (N-1)));
  return crypto_intN_ones_num(crypto_intN_y);
#endif
}

#endif

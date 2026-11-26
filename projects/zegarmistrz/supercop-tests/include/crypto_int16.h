/* auto-generated: cd cryptoint; ./autogen */
/* cryptoint 20250414 */

#ifndef crypto_int16_h
#define crypto_int16_h

#include <inttypes.h>
#define crypto_int16 int16_t
#define crypto_int16_unsigned uint16_t

#define crypto_int16_optblocker supercop_int16_optblocker
extern volatile crypto_int16 crypto_int16_optblocker;

__attribute__((unused))
static inline
crypto_int16 crypto_int16_load(const unsigned char *crypto_int16_s) {
  crypto_int16 crypto_int16_z = 0;
  crypto_int16_z |= ((crypto_int16) (*crypto_int16_s++)) << 0;
  crypto_int16_z |= ((crypto_int16) (*crypto_int16_s++)) << 8;
  return crypto_int16_z;
}

__attribute__((unused))
static inline
crypto_int16 crypto_int16_load_bigendian(const unsigned char *crypto_int16_s) {
  crypto_int16 crypto_int16_z = 0;
  crypto_int16_z |= ((crypto_int16) (*crypto_int16_s++)) << 8;
  crypto_int16_z |= ((crypto_int16) (*crypto_int16_s++)) << 0;
  return crypto_int16_z;
}

__attribute__((unused))
static inline
void crypto_int16_store(unsigned char *crypto_int16_s,crypto_int16 crypto_int16_x) {
  *crypto_int16_s++ = crypto_int16_x >> 0;
  *crypto_int16_s++ = crypto_int16_x >> 8;
}

__attribute__((unused))
static inline
void crypto_int16_store_bigendian(unsigned char *crypto_int16_s,crypto_int16 crypto_int16_x) {
  *crypto_int16_s++ = crypto_int16_x >> 8;
  *crypto_int16_s++ = crypto_int16_x >> 0;
}

__attribute__((unused))
static inline
crypto_int16 crypto_int16_negative_mask(crypto_int16 crypto_int16_x) {
#if defined(__GNUC__) && defined(__x86_64__)
  __asm__ ("sarw $15,%0" : "+r"(crypto_int16_x) : : "cc");
  return crypto_int16_x;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_int16 crypto_int16_y;
  __asm__ ("sbfx %w0,%w1,15,1" : "=r"(crypto_int16_y) : "r"(crypto_int16_x) : );
  return crypto_int16_y;
#elif defined(__GNUC__) && defined(__arm__) && defined(__ARM_ARCH) && (__ARM_ARCH >= 6) && !defined(__thumb__)
  crypto_int16 crypto_int16_y;
  __asm__ ("sxth %0,%1\n asr %0,%0,#31" : "=r"(crypto_int16_y) : "r"(crypto_int16_x) : );
  return crypto_int16_y;
#elif defined(__GNUC__) && defined(__sparc_v8__)
  crypto_int16 crypto_int16_y;
  __asm__ ("sll %1,16,%0\n sra %0,31,%0" : "=r"(crypto_int16_y) : "r"(crypto_int16_x) : );
  return crypto_int16_y;
#else
  crypto_int16_x >>= 16-6;
  crypto_int16_x += crypto_int16_optblocker;
  crypto_int16_x >>= 5;
  return crypto_int16_x;
#endif
}

__attribute__((unused))
static inline
crypto_int16_unsigned crypto_int16_unsigned_topbit_01(crypto_int16_unsigned crypto_int16_x) {
#if defined(__GNUC__) && defined(__x86_64__)
  __asm__ ("shrw $15,%0" : "+r"(crypto_int16_x) : : "cc");
  return crypto_int16_x;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_int16_unsigned crypto_int16_y;
  __asm__ ("ubfx %w0,%w1,15,1" : "=r"(crypto_int16_y) : "r"(crypto_int16_x) : );
  return crypto_int16_y;
#elif defined(__GNUC__) && defined(__arm__) && defined(__ARM_ARCH) && (__ARM_ARCH >= 6) && !defined(__thumb__)
  crypto_int16 crypto_int16_y;
  __asm__ ("uxth %0,%1\n lsr %0,%0,#15" : "=r"(crypto_int16_y) : "r"(crypto_int16_x) : );
  return crypto_int16_y;
#elif defined(__GNUC__) && defined(__sparc_v8__)
  crypto_int16_unsigned crypto_int16_y;
  __asm__ ("sll %1,16,%0\n srl %0,16,%0\n srl %0,15,%0" : "=r"(crypto_int16_y) : "r"(crypto_int16_x) : );
  return crypto_int16_y;
#else
  crypto_int16_x >>= 16-6;
  crypto_int16_x += crypto_int16_optblocker;
  crypto_int16_x >>= 5;
  return crypto_int16_x;
#endif
}

__attribute__((unused))
static inline
crypto_int16 crypto_int16_negative_01(crypto_int16 crypto_int16_x) {
  return crypto_int16_unsigned_topbit_01(crypto_int16_x);
}

__attribute__((unused))
static inline
crypto_int16 crypto_int16_topbit_mask(crypto_int16 crypto_int16_x) {
  return crypto_int16_negative_mask(crypto_int16_x);
}

__attribute__((unused))
static inline
crypto_int16 crypto_int16_topbit_01(crypto_int16 crypto_int16_x) {
  return crypto_int16_unsigned_topbit_01(crypto_int16_x);
}

__attribute__((unused))
static inline
crypto_int16 crypto_int16_bottombit_mask(crypto_int16 crypto_int16_x) {
#if defined(__GNUC__) && defined(__x86_64__)
  __asm__ ("andw $1,%0" : "+r"(crypto_int16_x) : : "cc");
  return -crypto_int16_x;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_int16 crypto_int16_y;
  __asm__ ("sbfx %w0,%w1,0,1" : "=r"(crypto_int16_y) : "r"(crypto_int16_x) : );
  return crypto_int16_y;
#elif defined(__GNUC__) && defined(__arm__) && defined(__ARM_ARCH) && (__ARM_ARCH >= 6) && !defined(__thumb__)
  crypto_int16 crypto_int16_y;
  __asm__ ("and %0,%1,#1\n neg %0,%0" : "=r"(crypto_int16_y) : "r"(crypto_int16_x) : );
  return crypto_int16_y;
#elif defined(__GNUC__) && defined(__sparc_v8__)
  crypto_int16 crypto_int16_y;
  __asm__ ("and %1,1,%0\n neg %0,%0" : "=r"(crypto_int16_y) : "r"(crypto_int16_x) : );
  return crypto_int16_y;
#else
  crypto_int16_x &= 1 + crypto_int16_optblocker;
  return -crypto_int16_x;
#endif
}

__attribute__((unused))
static inline
crypto_int16 crypto_int16_bottombit_01(crypto_int16 crypto_int16_x) {
#if defined(__GNUC__) && defined(__x86_64__)
  __asm__ ("andw $1,%0" : "+r"(crypto_int16_x) : : "cc");
  return crypto_int16_x;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_int16 crypto_int16_y;
  __asm__ ("ubfx %w0,%w1,0,1" : "=r"(crypto_int16_y) : "r"(crypto_int16_x) : );
  return crypto_int16_y;
#elif defined(__GNUC__) && defined(__arm__) && defined(__ARM_ARCH) && (__ARM_ARCH >= 6) && !defined(__thumb__)
  crypto_int16 crypto_int16_y;
  __asm__ ("and %0,%1,#1" : "=r"(crypto_int16_y) : "r"(crypto_int16_x) : );
  return crypto_int16_y;
#elif defined(__GNUC__) && defined(__sparc_v8__)
  crypto_int16 crypto_int16_y;
  __asm__ ("and %1,1,%0" : "=r"(crypto_int16_y) : "r"(crypto_int16_x) : );
  return crypto_int16_y;
#else
  crypto_int16_x &= 1 + crypto_int16_optblocker;
  return crypto_int16_x;
#endif
}

__attribute__((unused))
static inline
crypto_int16 crypto_int16_bitinrangepublicpos_mask(crypto_int16 crypto_int16_x,crypto_int16 crypto_int16_s) {
#if defined(__GNUC__) && defined(__x86_64__)
  __asm__ ("sarw %%cl,%0" : "+r"(crypto_int16_x) : "c"(crypto_int16_s) : "cc");
#elif defined(__GNUC__) && defined(__aarch64__)
  __asm__ ("sxth %w0,%w0\n asr %w0,%w0,%w1" : "+&r"(crypto_int16_x) : "r"(crypto_int16_s) : );
#elif defined(__GNUC__) && defined(__arm__) && defined(__ARM_ARCH) && (__ARM_ARCH >= 6) && !defined(__thumb__)
  __asm__ ("and %0,%0,#15\n sxth %1,%1\n asr %1,%1,%0" : "+&r"(crypto_int16_s), "+r"(crypto_int16_x) : : );
#elif defined(__GNUC__) && defined(__sparc_v8__)
  __asm__ ("and %0,15,%0\n sra %1,%0,%1" : "+&r"(crypto_int16_s), "+r"(crypto_int16_x) : : );
#else
  crypto_int16_x >>= crypto_int16_s ^ crypto_int16_optblocker;
#endif
  return crypto_int16_bottombit_mask(crypto_int16_x);
}

__attribute__((unused))
static inline
crypto_int16 crypto_int16_bitinrangepublicpos_01(crypto_int16 crypto_int16_x,crypto_int16 crypto_int16_s) {
#if defined(__GNUC__) && defined(__x86_64__)
  __asm__ ("sarw %%cl,%0" : "+r"(crypto_int16_x) : "c"(crypto_int16_s) : "cc");
#elif defined(__GNUC__) && defined(__aarch64__)
  __asm__ ("sxth %w0,%w0\n asr %w0,%w0,%w1" : "+&r"(crypto_int16_x) : "r"(crypto_int16_s) : );
#elif defined(__GNUC__) && defined(__arm__) && defined(__ARM_ARCH) && (__ARM_ARCH >= 6) && !defined(__thumb__)
  __asm__ ("and %0,%0,#15\n sxth %1,%1\n asr %1,%1,%0" : "+&r"(crypto_int16_s), "+r"(crypto_int16_x) : : );
#elif defined(__GNUC__) && defined(__sparc_v8__)
  __asm__ ("and %0,15,%0\n sra %1,%0,%1" : "+&r"(crypto_int16_s), "+r"(crypto_int16_x) : : );
#else
  crypto_int16_x >>= crypto_int16_s ^ crypto_int16_optblocker;
#endif
  return crypto_int16_bottombit_01(crypto_int16_x);
}

__attribute__((unused))
static inline
crypto_int16 crypto_int16_shlmod(crypto_int16 crypto_int16_x,crypto_int16 crypto_int16_s) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_int16_s &= 15;
  __asm__ ("shlw %%cl,%0" : "+r"(crypto_int16_x) : "c"(crypto_int16_s) : "cc");
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_int16_s &= 15;
  __asm__ ("and %w0,%w0,65535\n lsl %w0,%w0,%w1" : "+&r"(crypto_int16_x) : "r"(crypto_int16_s) : );
#elif defined(__GNUC__) && defined(__arm__) && defined(__ARM_ARCH) && (__ARM_ARCH >= 6) && !defined(__thumb__)
  crypto_int16_s &= 15;
  __asm__ ("lsl %0,%0,%1\n sxth %0,%0" : "+r"(crypto_int16_x) : "r"(crypto_int16_s) : );
#elif defined(__GNUC__) && defined(__sparc_v8__)
  crypto_int16_s &= 15; crypto_int16_s += 16;
  __asm__ ("sll %0,%1,%0\n sra %0,16,%0" : "+r"(crypto_int16_x) : "r"(crypto_int16_s) : );
#else
  int crypto_int16_k, crypto_int16_l;
  for (crypto_int16_l = 0,crypto_int16_k = 1;crypto_int16_k < 16;++crypto_int16_l,crypto_int16_k *= 2)
    crypto_int16_x ^= (crypto_int16_x ^ (crypto_int16_x << crypto_int16_k)) & crypto_int16_bitinrangepublicpos_mask(crypto_int16_s,crypto_int16_l);
#endif
  return crypto_int16_x;
}

__attribute__((unused))
static inline
crypto_int16 crypto_int16_shrmod(crypto_int16 crypto_int16_x,crypto_int16 crypto_int16_s) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_int16_s &= 15;
  __asm__ ("sarw %%cl,%0" : "+r"(crypto_int16_x) : "c"(crypto_int16_s) : "cc");
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_int16_s &= 15;
  __asm__ ("sxth %w0,%w0\n asr %w0,%w0,%w1" : "+&r"(crypto_int16_x) : "r"(crypto_int16_s) : );
#elif defined(__GNUC__) && defined(__arm__) && defined(__ARM_ARCH) && (__ARM_ARCH >= 6) && !defined(__thumb__)
  crypto_int16_s &= 15;
  __asm__ ("sxth %0,%0\n asr %0,%0,%1" : "+&r"(crypto_int16_x) : "r"(crypto_int16_s) : );
#elif defined(__GNUC__) && defined(__sparc_v8__)
  crypto_int16_s &= 15;
  __asm__ ("sra %0,%1,%0" : "+r"(crypto_int16_x) : "r"(crypto_int16_s) : );
#else
  int crypto_int16_k, crypto_int16_l;
  for (crypto_int16_l = 0,crypto_int16_k = 1;crypto_int16_k < 16;++crypto_int16_l,crypto_int16_k *= 2)
    crypto_int16_x ^= (crypto_int16_x ^ (crypto_int16_x >> crypto_int16_k)) & crypto_int16_bitinrangepublicpos_mask(crypto_int16_s,crypto_int16_l);
#endif
  return crypto_int16_x;
}

__attribute__((unused))
static inline
crypto_int16 crypto_int16_bitmod_mask(crypto_int16 crypto_int16_x,crypto_int16 crypto_int16_s) {
  crypto_int16_x = crypto_int16_shrmod(crypto_int16_x,crypto_int16_s);
  return crypto_int16_bottombit_mask(crypto_int16_x);
}

__attribute__((unused))
static inline
crypto_int16 crypto_int16_bitmod_01(crypto_int16 crypto_int16_x,crypto_int16 crypto_int16_s) {
  crypto_int16_x = crypto_int16_shrmod(crypto_int16_x,crypto_int16_s);
  return crypto_int16_bottombit_01(crypto_int16_x);
}

__attribute__((unused))
static inline
crypto_int16 crypto_int16_nonzero_mask(crypto_int16 crypto_int16_x) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_int16 crypto_int16_q,crypto_int16_z;
  __asm__ ("xorw %0,%0\n movw $-1,%1\n testw %2,%2\n cmovnew %1,%0" : "=&r"(crypto_int16_z), "=&r"(crypto_int16_q) : "r"(crypto_int16_x) : "cc");
  return crypto_int16_z;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_int16 crypto_int16_z;
  __asm__ ("tst %w1,65535\n csetm %w0,ne" : "=r"(crypto_int16_z) : "r"(crypto_int16_x) : "cc");
  return crypto_int16_z;
#elif defined(__GNUC__) && defined(__arm__) && defined(__ARM_ARCH) && (__ARM_ARCH >= 6) && !defined(__thumb__)
  __asm__ ("uxth %0,%0\n cmp %0,#0\n movne %0,#-1" : "+r"(crypto_int16_x) : : "cc");
  return crypto_int16_x;
#elif defined(__GNUC__) && defined(__sparc_v8__)
  crypto_int16 crypto_int16_z;
  __asm__ ("sll %0,16,%0\n srl %0,16,%0\n cmp %%g0,%0\n subx %%g0,0,%1" : "+r"(crypto_int16_x), "=r"(crypto_int16_z) : : "cc");
  return crypto_int16_z;
#else
  crypto_int16_x |= -crypto_int16_x;
  return crypto_int16_negative_mask(crypto_int16_x);
#endif
}

__attribute__((unused))
static inline
crypto_int16 crypto_int16_nonzero_01(crypto_int16 crypto_int16_x) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_int16 crypto_int16_q,crypto_int16_z;
  __asm__ ("xorw %0,%0\n movw $1,%1\n testw %2,%2\n cmovnew %1,%0" : "=&r"(crypto_int16_z), "=&r"(crypto_int16_q) : "r"(crypto_int16_x) : "cc");
  return crypto_int16_z;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_int16 crypto_int16_z;
  __asm__ ("tst %w1,65535\n cset %w0,ne" : "=r"(crypto_int16_z) : "r"(crypto_int16_x) : "cc");
  return crypto_int16_z;
#elif defined(__GNUC__) && defined(__arm__) && defined(__ARM_ARCH) && (__ARM_ARCH >= 6) && !defined(__thumb__)
  __asm__ ("uxth %0,%0\n cmp %0,#0\n movne %0,#1" : "+r"(crypto_int16_x) : : "cc");
  return crypto_int16_x;
#elif defined(__GNUC__) && defined(__sparc_v8__)
  crypto_int16 crypto_int16_z;
  __asm__ ("sll %0,16,%0\n srl %0,16,%0\n cmp %%g0,%0\n addx %%g0,0,%1" : "+r"(crypto_int16_x), "=r"(crypto_int16_z) : : "cc");
  return crypto_int16_z;
#else
  crypto_int16_x |= -crypto_int16_x;
  return crypto_int16_unsigned_topbit_01(crypto_int16_x);
#endif
}

__attribute__((unused))
static inline
crypto_int16 crypto_int16_positive_mask(crypto_int16 crypto_int16_x) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_int16 crypto_int16_q,crypto_int16_z;
  __asm__ ("xorw %0,%0\n movw $-1,%1\n testw %2,%2\n cmovgw %1,%0" : "=&r"(crypto_int16_z), "=&r"(crypto_int16_q) : "r"(crypto_int16_x) : "cc");
  return crypto_int16_z;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_int16 crypto_int16_z;
  __asm__ ("sxth %w0,%w1\n cmp %w0,0\n csetm %w0,gt" : "=r"(crypto_int16_z) : "r"(crypto_int16_x) : "cc");
  return crypto_int16_z;
#elif defined(__GNUC__) && defined(__sparc_v8__)
  crypto_int16 crypto_int16_z;
  __asm__ ("sll %1,16,%0\n sra %0,31,%0\n sub %0,%1,%0\n sra %0,31,%0" : "=&r"(crypto_int16_z) : "r"(crypto_int16_x) : );
  return crypto_int16_z;
#else
  crypto_int16 crypto_int16_z = -crypto_int16_x;
  crypto_int16_z ^= crypto_int16_x & crypto_int16_z;
  return crypto_int16_negative_mask(crypto_int16_z);
#endif
}

__attribute__((unused))
static inline
crypto_int16 crypto_int16_positive_01(crypto_int16 crypto_int16_x) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_int16 crypto_int16_q,crypto_int16_z;
  __asm__ ("xorw %0,%0\n movw $1,%1\n testw %2,%2\n cmovgw %1,%0" : "=&r"(crypto_int16_z), "=&r"(crypto_int16_q) : "r"(crypto_int16_x) : "cc");
  return crypto_int16_z;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_int16 crypto_int16_z;
  __asm__ ("sxth %w0,%w1\n cmp %w0,0\n cset %w0,gt" : "=r"(crypto_int16_z) : "r"(crypto_int16_x) : "cc");
  return crypto_int16_z;
#elif defined(__GNUC__) && defined(__sparc_v8__)
  crypto_int16 crypto_int16_z;
  __asm__ ("sll %1,16,%0\n sra %0,31,%0\n sub %0,%1,%0\n srl %0,31,%0" : "=&r"(crypto_int16_z) : "r"(crypto_int16_x) : );
  return crypto_int16_z;
#else
  crypto_int16 crypto_int16_z = -crypto_int16_x;
  crypto_int16_z ^= crypto_int16_x & crypto_int16_z;
  return crypto_int16_unsigned_topbit_01(crypto_int16_z);
#endif
}

__attribute__((unused))
static inline
crypto_int16 crypto_int16_zero_mask(crypto_int16 crypto_int16_x) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_int16 crypto_int16_q,crypto_int16_z;
  __asm__ ("xorw %0,%0\n movw $-1,%1\n testw %2,%2\n cmovew %1,%0" : "=&r"(crypto_int16_z), "=&r"(crypto_int16_q) : "r"(crypto_int16_x) : "cc");
  return crypto_int16_z;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_int16 crypto_int16_z;
  __asm__ ("tst %w1,65535\n csetm %w0,eq" : "=r"(crypto_int16_z) : "r"(crypto_int16_x) : "cc");
  return crypto_int16_z;
#elif defined(__GNUC__) && defined(__sparc_v8__)
  crypto_int16 crypto_int16_z;
  __asm__ ("sll %0,16,%0\n srl %0,16,%0\n cmp %%g0,%0\n addx %%g0,-1,%1" : "+r"(crypto_int16_x), "=r"(crypto_int16_z) : : "cc");
  return crypto_int16_z;
#else
  return ~crypto_int16_nonzero_mask(crypto_int16_x);
#endif
}

__attribute__((unused))
static inline
crypto_int16 crypto_int16_zero_01(crypto_int16 crypto_int16_x) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_int16 crypto_int16_q,crypto_int16_z;
  __asm__ ("xorw %0,%0\n movw $1,%1\n testw %2,%2\n cmovew %1,%0" : "=&r"(crypto_int16_z), "=&r"(crypto_int16_q) : "r"(crypto_int16_x) : "cc");
  return crypto_int16_z;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_int16 crypto_int16_z;
  __asm__ ("tst %w1,65535\n cset %w0,eq" : "=r"(crypto_int16_z) : "r"(crypto_int16_x) : "cc");
  return crypto_int16_z;
#elif defined(__GNUC__) && defined(__sparc_v8__)
  crypto_int16 crypto_int16_z;
  __asm__ ("sll %0,16,%0\n srl %0,16,%0\n cmp %%g0,%0\n subx %%g0,-1,%1" : "+r"(crypto_int16_x), "=r"(crypto_int16_z) : : "cc");
  return crypto_int16_z;
#else
  return 1-crypto_int16_nonzero_01(crypto_int16_x);
#endif
}

__attribute__((unused))
static inline
crypto_int16 crypto_int16_unequal_mask(crypto_int16 crypto_int16_x,crypto_int16 crypto_int16_y) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_int16 crypto_int16_q,crypto_int16_z;
  __asm__ ("xorw %0,%0\n movw $-1,%1\n cmpw %3,%2\n cmovnew %1,%0" : "=&r"(crypto_int16_z), "=&r"(crypto_int16_q) : "r"(crypto_int16_x), "r"(crypto_int16_y) : "cc");
  return crypto_int16_z;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_int16 crypto_int16_z;
  __asm__ ("and %w0,%w1,65535\n cmp %w0,%w2,uxth\n csetm %w0,ne" : "=&r"(crypto_int16_z) : "r"(crypto_int16_x), "r"(crypto_int16_y) : "cc");
  return crypto_int16_z;
#else
  return crypto_int16_nonzero_mask(crypto_int16_x ^ crypto_int16_y);
#endif
}

__attribute__((unused))
static inline
crypto_int16 crypto_int16_unequal_01(crypto_int16 crypto_int16_x,crypto_int16 crypto_int16_y) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_int16 crypto_int16_q,crypto_int16_z;
  __asm__ ("xorw %0,%0\n movw $1,%1\n cmpw %3,%2\n cmovnew %1,%0" : "=&r"(crypto_int16_z), "=&r"(crypto_int16_q) : "r"(crypto_int16_x), "r"(crypto_int16_y) : "cc");
  return crypto_int16_z;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_int16 crypto_int16_z;
  __asm__ ("and %w0,%w1,65535\n cmp %w0,%w2,uxth\n cset %w0,ne" : "=&r"(crypto_int16_z) : "r"(crypto_int16_x), "r"(crypto_int16_y) : "cc");
  return crypto_int16_z;
#else
  return crypto_int16_nonzero_01(crypto_int16_x ^ crypto_int16_y);
#endif
}

__attribute__((unused))
static inline
crypto_int16 crypto_int16_equal_mask(crypto_int16 crypto_int16_x,crypto_int16 crypto_int16_y) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_int16 crypto_int16_q,crypto_int16_z;
  __asm__ ("xorw %0,%0\n movw $-1,%1\n cmpw %3,%2\n cmovew %1,%0" : "=&r"(crypto_int16_z), "=&r"(crypto_int16_q) : "r"(crypto_int16_x), "r"(crypto_int16_y) : "cc");
  return crypto_int16_z;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_int16 crypto_int16_z;
  __asm__ ("and %w0,%w1,65535\n cmp %w0,%w2,uxth\n csetm %w0,eq" : "=&r"(crypto_int16_z) : "r"(crypto_int16_x), "r"(crypto_int16_y) : "cc");
  return crypto_int16_z;
#else
  return crypto_int16_zero_mask(crypto_int16_x ^ crypto_int16_y);
#endif
}

__attribute__((unused))
static inline
crypto_int16 crypto_int16_equal_01(crypto_int16 crypto_int16_x,crypto_int16 crypto_int16_y) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_int16 crypto_int16_q,crypto_int16_z;
  __asm__ ("xorw %0,%0\n movw $1,%1\n cmpw %3,%2\n cmovew %1,%0" : "=&r"(crypto_int16_z), "=&r"(crypto_int16_q) : "r"(crypto_int16_x), "r"(crypto_int16_y) : "cc");
  return crypto_int16_z;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_int16 crypto_int16_z;
  __asm__ ("and %w0,%w1,65535\n cmp %w0,%w2,uxth\n cset %w0,eq" : "=&r"(crypto_int16_z) : "r"(crypto_int16_x), "r"(crypto_int16_y) : "cc");
  return crypto_int16_z;
#else
  return crypto_int16_zero_01(crypto_int16_x ^ crypto_int16_y);
#endif
}

__attribute__((unused))
static inline
crypto_int16 crypto_int16_min(crypto_int16 crypto_int16_x,crypto_int16 crypto_int16_y) {
#if defined(__GNUC__) && defined(__x86_64__)
  __asm__ ("cmpw %1,%0\n cmovgw %1,%0" : "+r"(crypto_int16_x) : "r"(crypto_int16_y) : "cc");
  return crypto_int16_x;
#elif defined(__GNUC__) && defined(__aarch64__)
  __asm__ ("sxth %w0,%w0\n cmp %w0,%w1,sxth\n csel %w0,%w0,%w1,lt" : "+&r"(crypto_int16_x) : "r"(crypto_int16_y) : "cc");
  return crypto_int16_x;
#else
  crypto_int16 crypto_int16_r = crypto_int16_y ^ crypto_int16_x;
  crypto_int16 crypto_int16_z = crypto_int16_y - crypto_int16_x;
  crypto_int16_z ^= crypto_int16_r & (crypto_int16_z ^ crypto_int16_y);
  crypto_int16_z = crypto_int16_negative_mask(crypto_int16_z);
  crypto_int16_z &= crypto_int16_r;
  return crypto_int16_x ^ crypto_int16_z;
#endif
}

__attribute__((unused))
static inline
crypto_int16 crypto_int16_max(crypto_int16 crypto_int16_x,crypto_int16 crypto_int16_y) {
#if defined(__GNUC__) && defined(__x86_64__)
  __asm__ ("cmpw %1,%0\n cmovlw %1,%0" : "+r"(crypto_int16_x) : "r"(crypto_int16_y) : "cc");
  return crypto_int16_x;
#elif defined(__GNUC__) && defined(__aarch64__)
  __asm__ ("sxth %w0,%w0\n cmp %w0,%w1,sxth\n csel %w0,%w1,%w0,lt" : "+&r"(crypto_int16_x) : "r"(crypto_int16_y) : "cc");
  return crypto_int16_x;
#else
  crypto_int16 crypto_int16_r = crypto_int16_y ^ crypto_int16_x;
  crypto_int16 crypto_int16_z = crypto_int16_y - crypto_int16_x;
  crypto_int16_z ^= crypto_int16_r & (crypto_int16_z ^ crypto_int16_y);
  crypto_int16_z = crypto_int16_negative_mask(crypto_int16_z);
  crypto_int16_z &= crypto_int16_r;
  return crypto_int16_y ^ crypto_int16_z;
#endif
}

__attribute__((unused))
static inline
void crypto_int16_minmax(crypto_int16 *crypto_int16_p,crypto_int16 *crypto_int16_q) {
  crypto_int16 crypto_int16_x = *crypto_int16_p;
  crypto_int16 crypto_int16_y = *crypto_int16_q;
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_int16 crypto_int16_z;
  __asm__ ("cmpw %2,%1\n movw %1,%0\n cmovgw %2,%1\n cmovgw %0,%2" : "=&r"(crypto_int16_z), "+&r"(crypto_int16_x), "+r"(crypto_int16_y) : : "cc");
  *crypto_int16_p = crypto_int16_x;
  *crypto_int16_q = crypto_int16_y;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_int16 crypto_int16_r, crypto_int16_s;
  __asm__ ("sxth %w0,%w0\n cmp %w0,%w3,sxth\n csel %w1,%w0,%w3,lt\n csel %w2,%w3,%w0,lt" : "+&r"(crypto_int16_x), "=&r"(crypto_int16_r), "=r"(crypto_int16_s) : "r"(crypto_int16_y) : "cc");
  *crypto_int16_p = crypto_int16_r;
  *crypto_int16_q = crypto_int16_s;
#else
  crypto_int16 crypto_int16_r = crypto_int16_y ^ crypto_int16_x;
  crypto_int16 crypto_int16_z = crypto_int16_y - crypto_int16_x;
  crypto_int16_z ^= crypto_int16_r & (crypto_int16_z ^ crypto_int16_y);
  crypto_int16_z = crypto_int16_negative_mask(crypto_int16_z);
  crypto_int16_z &= crypto_int16_r;
  crypto_int16_x ^= crypto_int16_z;
  crypto_int16_y ^= crypto_int16_z;
  *crypto_int16_p = crypto_int16_x;
  *crypto_int16_q = crypto_int16_y;
#endif
}

__attribute__((unused))
static inline
crypto_int16 crypto_int16_smaller_mask(crypto_int16 crypto_int16_x,crypto_int16 crypto_int16_y) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_int16 crypto_int16_q,crypto_int16_z;
  __asm__ ("xorw %0,%0\n movw $-1,%1\n cmpw %3,%2\n cmovlw %1,%0" : "=&r"(crypto_int16_z), "=&r"(crypto_int16_q) : "r"(crypto_int16_x), "r"(crypto_int16_y) : "cc");
  return crypto_int16_z;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_int16 crypto_int16_z;
  __asm__ ("sxth %w0,%w1\n cmp %w0,%w2,sxth\n csetm %w0,lt" : "=&r"(crypto_int16_z) : "r"(crypto_int16_x), "r"(crypto_int16_y) : "cc");
  return crypto_int16_z;
#else
  crypto_int16 crypto_int16_r = crypto_int16_x ^ crypto_int16_y;
  crypto_int16 crypto_int16_z = crypto_int16_x - crypto_int16_y;
  crypto_int16_z ^= crypto_int16_r & (crypto_int16_z ^ crypto_int16_x);
  return crypto_int16_negative_mask(crypto_int16_z);
#endif
}

__attribute__((unused))
static inline
crypto_int16 crypto_int16_smaller_01(crypto_int16 crypto_int16_x,crypto_int16 crypto_int16_y) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_int16 crypto_int16_q,crypto_int16_z;
  __asm__ ("xorw %0,%0\n movw $1,%1\n cmpw %3,%2\n cmovlw %1,%0" : "=&r"(crypto_int16_z), "=&r"(crypto_int16_q) : "r"(crypto_int16_x), "r"(crypto_int16_y) : "cc");
  return crypto_int16_z;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_int16 crypto_int16_z;
  __asm__ ("sxth %w0,%w1\n cmp %w0,%w2,sxth\n cset %w0,lt" : "=&r"(crypto_int16_z) : "r"(crypto_int16_x), "r"(crypto_int16_y) : "cc");
  return crypto_int16_z;
#else
  crypto_int16 crypto_int16_r = crypto_int16_x ^ crypto_int16_y;
  crypto_int16 crypto_int16_z = crypto_int16_x - crypto_int16_y;
  crypto_int16_z ^= crypto_int16_r & (crypto_int16_z ^ crypto_int16_x);
  return crypto_int16_unsigned_topbit_01(crypto_int16_z);
#endif
}

__attribute__((unused))
static inline
crypto_int16 crypto_int16_leq_mask(crypto_int16 crypto_int16_x,crypto_int16 crypto_int16_y) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_int16 crypto_int16_q,crypto_int16_z;
  __asm__ ("xorw %0,%0\n movw $-1,%1\n cmpw %3,%2\n cmovlew %1,%0" : "=&r"(crypto_int16_z), "=&r"(crypto_int16_q) : "r"(crypto_int16_x), "r"(crypto_int16_y) : "cc");
  return crypto_int16_z;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_int16 crypto_int16_z;
  __asm__ ("sxth %w0,%w1\n cmp %w0,%w2,sxth\n csetm %w0,le" : "=&r"(crypto_int16_z) : "r"(crypto_int16_x), "r"(crypto_int16_y) : "cc");
  return crypto_int16_z;
#else
  return ~crypto_int16_smaller_mask(crypto_int16_y,crypto_int16_x);
#endif
}

__attribute__((unused))
static inline
crypto_int16 crypto_int16_leq_01(crypto_int16 crypto_int16_x,crypto_int16 crypto_int16_y) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_int16 crypto_int16_q,crypto_int16_z;
  __asm__ ("xorw %0,%0\n movw $1,%1\n cmpw %3,%2\n cmovlew %1,%0" : "=&r"(crypto_int16_z), "=&r"(crypto_int16_q) : "r"(crypto_int16_x), "r"(crypto_int16_y) : "cc");
  return crypto_int16_z;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_int16 crypto_int16_z;
  __asm__ ("sxth %w0,%w1\n cmp %w0,%w2,sxth\n cset %w0,le" : "=&r"(crypto_int16_z) : "r"(crypto_int16_x), "r"(crypto_int16_y) : "cc");
  return crypto_int16_z;
#else
  return 1-crypto_int16_smaller_01(crypto_int16_y,crypto_int16_x);
#endif
}

__attribute__((unused))
static inline
int crypto_int16_ones_num(crypto_int16 crypto_int16_x) {
  crypto_int16_unsigned crypto_int16_y = crypto_int16_x;
  const crypto_int16 C0 = 0x5555;
  const crypto_int16 C1 = 0x3333;
  const crypto_int16 C2 = 0x0f0f;
  crypto_int16_y -= ((crypto_int16_y >> 1) & C0);
  crypto_int16_y = (crypto_int16_y & C1) + ((crypto_int16_y >> 2) & C1);
  crypto_int16_y = (crypto_int16_y + (crypto_int16_y >> 4)) & C2;
  crypto_int16_y = (crypto_int16_y + (crypto_int16_y >> 8)) & 0xff;
  return crypto_int16_y;
}

__attribute__((unused))
static inline
int crypto_int16_bottomzeros_num(crypto_int16 crypto_int16_x) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_int16 fallback = 16;
  __asm__ ("bsfw %0,%0\n cmovew %1,%0" : "+&r"(crypto_int16_x) : "r"(fallback) : "cc");
  return crypto_int16_x;
#elif defined(__GNUC__) && defined(__aarch64__)
  int64_t crypto_int16_y;
  __asm__ ("orr %w0,%w1,-65536\n rbit %w0,%w0\n clz %w0,%w0" : "=r"(crypto_int16_y) : "r"(crypto_int16_x) : );
  return crypto_int16_y;
#else
  crypto_int16 crypto_int16_y = crypto_int16_x ^ (crypto_int16_x-1);
  crypto_int16_y = ((crypto_int16) crypto_int16_y) >> 1;
  crypto_int16_y &= ~(crypto_int16_x & (((crypto_int16) 1) << (16-1)));
  return crypto_int16_ones_num(crypto_int16_y);
#endif
}

#endif

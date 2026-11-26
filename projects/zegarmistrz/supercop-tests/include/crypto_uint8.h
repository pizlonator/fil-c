/* auto-generated: cd cryptoint; ./autogen */
/* cryptoint 20250414 */

#ifndef crypto_uint8_h
#define crypto_uint8_h

#include <inttypes.h>
#define crypto_uint8 uint8_t
#define crypto_uint8_signed int8_t

#define crypto_uint8_signed_optblocker supercop_uint8_signed_optblocker
extern volatile crypto_uint8_signed crypto_uint8_signed_optblocker;

__attribute__((unused))
static inline
crypto_uint8 crypto_uint8_load(const unsigned char *crypto_uint8_s) {
  crypto_uint8 crypto_uint8_z = 0;
  crypto_uint8_z |= ((crypto_uint8) (*crypto_uint8_s++)) << 0;
  return crypto_uint8_z;
}

__attribute__((unused))
static inline
crypto_uint8 crypto_uint8_load_bigendian(const unsigned char *crypto_uint8_s) {
  crypto_uint8 crypto_uint8_z = 0;
  crypto_uint8_z |= ((crypto_uint8) (*crypto_uint8_s++)) << 0;
  return crypto_uint8_z;
}

__attribute__((unused))
static inline
void crypto_uint8_store(unsigned char *crypto_uint8_s,crypto_uint8 crypto_uint8_x) {
  *crypto_uint8_s++ = crypto_uint8_x >> 0;
}

__attribute__((unused))
static inline
void crypto_uint8_store_bigendian(unsigned char *crypto_uint8_s,crypto_uint8 crypto_uint8_x) {
  *crypto_uint8_s++ = crypto_uint8_x >> 0;
}

__attribute__((unused))
static inline
crypto_uint8_signed crypto_uint8_signed_negative_mask(crypto_uint8_signed crypto_uint8_x) {
#if defined(__GNUC__) && defined(__x86_64__)
  __asm__ ("sarb $7,%0" : "+r"(crypto_uint8_x) : : "cc");
  return crypto_uint8_x;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_uint8_signed crypto_uint8_y;
  __asm__ ("sbfx %w0,%w1,7,1" : "=r"(crypto_uint8_y) : "r"(crypto_uint8_x) : );
  return crypto_uint8_y;
#elif defined(__GNUC__) && defined(__arm__) && defined(__ARM_ARCH) && (__ARM_ARCH >= 6) && !defined(__thumb__)
  crypto_uint8_signed crypto_uint8_y;
  __asm__ ("sxtb %0,%1\n asr %0,%0,#31" : "=r"(crypto_uint8_y) : "r"(crypto_uint8_x) : );
  return crypto_uint8_y;
#elif defined(__GNUC__) && defined(__sparc_v8__)
  crypto_uint8_signed crypto_uint8_y;
  __asm__ ("sll %1,24,%0\n sra %0,31,%0" : "=r"(crypto_uint8_y) : "r"(crypto_uint8_x) : );
  return crypto_uint8_y;
#else
  crypto_uint8_x >>= 8-6;
  crypto_uint8_x += crypto_uint8_signed_optblocker;
  crypto_uint8_x >>= 5;
  return crypto_uint8_x;
#endif
}

__attribute__((unused))
static inline
crypto_uint8 crypto_uint8_topbit_01(crypto_uint8 crypto_uint8_x) {
#if defined(__GNUC__) && defined(__x86_64__)
  __asm__ ("shrb $7,%0" : "+r"(crypto_uint8_x) : : "cc");
  return crypto_uint8_x;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_uint8 crypto_uint8_y;
  __asm__ ("ubfx %w0,%w1,7,1" : "=r"(crypto_uint8_y) : "r"(crypto_uint8_x) : );
  return crypto_uint8_y;
#elif defined(__GNUC__) && defined(__arm__) && defined(__ARM_ARCH) && (__ARM_ARCH >= 6) && !defined(__thumb__)
  crypto_uint8_signed crypto_uint8_y;
  __asm__ ("uxtb %0,%1\n lsr %0,%0,#7" : "=r"(crypto_uint8_y) : "r"(crypto_uint8_x) : );
  return crypto_uint8_y;
#elif defined(__GNUC__) && defined(__sparc_v8__)
  crypto_uint8 crypto_uint8_y;
  __asm__ ("and %1,255,%0\n srl %0,7,%0" : "=r"(crypto_uint8_y) : "r"(crypto_uint8_x) : );
  return crypto_uint8_y;
#else
  crypto_uint8_x >>= 8-6;
  crypto_uint8_x += crypto_uint8_signed_optblocker;
  crypto_uint8_x >>= 5;
  return crypto_uint8_x;
#endif
}

__attribute__((unused))
static inline
crypto_uint8 crypto_uint8_topbit_mask(crypto_uint8 crypto_uint8_x) {
  return crypto_uint8_signed_negative_mask(crypto_uint8_x);
}

__attribute__((unused))
static inline
crypto_uint8 crypto_uint8_bottombit_mask(crypto_uint8 crypto_uint8_x) {
#if defined(__GNUC__) && defined(__x86_64__)
  __asm__ ("andb $1,%0" : "+r"(crypto_uint8_x) : : "cc");
  return -crypto_uint8_x;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_uint8 crypto_uint8_y;
  __asm__ ("sbfx %w0,%w1,0,1" : "=r"(crypto_uint8_y) : "r"(crypto_uint8_x) : );
  return crypto_uint8_y;
#elif defined(__GNUC__) && defined(__arm__) && defined(__ARM_ARCH) && (__ARM_ARCH >= 6) && !defined(__thumb__)
  crypto_uint8 crypto_uint8_y;
  __asm__ ("and %0,%1,#1\n neg %0,%0\n uxtb %0,%0" : "=r"(crypto_uint8_y) : "r"(crypto_uint8_x) : );
  return crypto_uint8_y;
#elif defined(__GNUC__) && defined(__sparc_v8__)
  crypto_uint8 crypto_uint8_y;
  __asm__ ("and %1,1,%0\n neg %0,%0\n and %0,255,%0" : "=r"(crypto_uint8_y) : "r"(crypto_uint8_x) : );
  return crypto_uint8_y;
#else
  crypto_uint8_x &= 1 + crypto_uint8_signed_optblocker;
  return -crypto_uint8_x;
#endif
}

__attribute__((unused))
static inline
crypto_uint8 crypto_uint8_bottombit_01(crypto_uint8 crypto_uint8_x) {
#if defined(__GNUC__) && defined(__x86_64__)
  __asm__ ("andb $1,%0" : "+r"(crypto_uint8_x) : : "cc");
  return crypto_uint8_x;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_uint8 crypto_uint8_y;
  __asm__ ("ubfx %w0,%w1,0,1" : "=r"(crypto_uint8_y) : "r"(crypto_uint8_x) : );
  return crypto_uint8_y;
#elif defined(__GNUC__) && defined(__arm__) && defined(__ARM_ARCH) && (__ARM_ARCH >= 6) && !defined(__thumb__)
  crypto_uint8 crypto_uint8_y;
  __asm__ ("and %0,%1,#1" : "=r"(crypto_uint8_y) : "r"(crypto_uint8_x) : );
  return crypto_uint8_y;
#elif defined(__GNUC__) && defined(__sparc_v8__)
  crypto_uint8 crypto_uint8_y;
  __asm__ ("and %1,1,%0" : "=r"(crypto_uint8_y) : "r"(crypto_uint8_x) : );
  return crypto_uint8_y;
#else
  crypto_uint8_x &= 1 + crypto_uint8_signed_optblocker;
  return crypto_uint8_x;
#endif
}

__attribute__((unused))
static inline
crypto_uint8 crypto_uint8_bitinrangepublicpos_mask(crypto_uint8 crypto_uint8_x,crypto_uint8 crypto_uint8_s) {
#if defined(__GNUC__) && defined(__x86_64__)
  __asm__ ("shrb %%cl,%0" : "+r"(crypto_uint8_x) : "c"(crypto_uint8_s) : "cc");
#elif defined(__GNUC__) && defined(__aarch64__)
  __asm__ ("and %w0,%w0,255\n lsr %w0,%w0,%w1" : "+&r"(crypto_uint8_x) : "r"(crypto_uint8_s) : );
#elif defined(__GNUC__) && defined(__arm__) && defined(__ARM_ARCH) && (__ARM_ARCH >= 6) && !defined(__thumb__)
  __asm__ ("and %0,%0,#7\n uxtb %1,%1\n lsr %1,%1,%0" : "+&r"(crypto_uint8_s), "+r"(crypto_uint8_x) : : );
#elif defined(__GNUC__) && defined(__sparc_v8__)
  __asm__ ("and %0,7,%0\n srl %1,%0,%1" : "+&r"(crypto_uint8_s), "+r"(crypto_uint8_x) : : );
#else
  crypto_uint8_x >>= crypto_uint8_s ^ crypto_uint8_signed_optblocker;
#endif
  return crypto_uint8_bottombit_mask(crypto_uint8_x);
}

__attribute__((unused))
static inline
crypto_uint8 crypto_uint8_bitinrangepublicpos_01(crypto_uint8 crypto_uint8_x,crypto_uint8 crypto_uint8_s) {
#if defined(__GNUC__) && defined(__x86_64__)
  __asm__ ("shrb %%cl,%0" : "+r"(crypto_uint8_x) : "c"(crypto_uint8_s) : "cc");
#elif defined(__GNUC__) && defined(__aarch64__)
  __asm__ ("and %w0,%w0,255\n lsr %w0,%w0,%w1" : "+&r"(crypto_uint8_x) : "r"(crypto_uint8_s) : );
#elif defined(__GNUC__) && defined(__arm__) && defined(__ARM_ARCH) && (__ARM_ARCH >= 6) && !defined(__thumb__)
  __asm__ ("and %0,%0,#7\n uxtb %1,%1\n lsr %1,%1,%0" : "+&r"(crypto_uint8_s), "+r"(crypto_uint8_x) : : );
#elif defined(__GNUC__) && defined(__sparc_v8__)
  __asm__ ("and %0,7,%0\n srl %1,%0,%1" : "+&r"(crypto_uint8_s), "+r"(crypto_uint8_x) : : );
#else
  crypto_uint8_x >>= crypto_uint8_s ^ crypto_uint8_signed_optblocker;
#endif
  return crypto_uint8_bottombit_01(crypto_uint8_x);
}

__attribute__((unused))
static inline
crypto_uint8 crypto_uint8_shlmod(crypto_uint8 crypto_uint8_x,crypto_uint8 crypto_uint8_s) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_uint8_s &= 7;
  __asm__ ("shlb %%cl,%0" : "+r"(crypto_uint8_x) : "c"(crypto_uint8_s) : "cc");
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_uint8_s &= 7;
  __asm__ ("and %w0,%w0,255\n lsl %w0,%w0,%w1" : "+&r"(crypto_uint8_x) : "r"(crypto_uint8_s) : );
#elif defined(__GNUC__) && defined(__arm__) && defined(__ARM_ARCH) && (__ARM_ARCH >= 6) && !defined(__thumb__)
  crypto_uint8_s &= 7;
  __asm__ ("lsl %0,%0,%1\n uxtb %0,%0" : "+r"(crypto_uint8_x) : "r"(crypto_uint8_s) : );
#elif defined(__GNUC__) && defined(__sparc_v8__)
  crypto_uint8_s &= 7;
  __asm__ ("sll %0,%1,%0\n and %0,255,%0" : "+r"(crypto_uint8_x) : "r"(crypto_uint8_s) : );
#else
  int crypto_uint8_k, crypto_uint8_l;
  for (crypto_uint8_l = 0,crypto_uint8_k = 1;crypto_uint8_k < 8;++crypto_uint8_l,crypto_uint8_k *= 2)
    crypto_uint8_x ^= (crypto_uint8_x ^ (crypto_uint8_x << crypto_uint8_k)) & crypto_uint8_bitinrangepublicpos_mask(crypto_uint8_s,crypto_uint8_l);
#endif
  return crypto_uint8_x;
}

__attribute__((unused))
static inline
crypto_uint8 crypto_uint8_shrmod(crypto_uint8 crypto_uint8_x,crypto_uint8 crypto_uint8_s) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_uint8_s &= 7;
  __asm__ ("shrb %%cl,%0" : "+r"(crypto_uint8_x) : "c"(crypto_uint8_s) : "cc");
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_uint8_s &= 7;
  __asm__ ("and %w0,%w0,255\n lsr %w0,%w0,%w1" : "+&r"(crypto_uint8_x) : "r"(crypto_uint8_s) : );
#elif defined(__GNUC__) && defined(__arm__) && defined(__ARM_ARCH) && (__ARM_ARCH >= 6) && !defined(__thumb__)
  crypto_uint8_s &= 7;
  __asm__ ("uxtb %0,%0\n lsr %0,%0,%1" : "+&r"(crypto_uint8_x) : "r"(crypto_uint8_s) : );
#elif defined(__GNUC__) && defined(__sparc_v8__)
  crypto_uint8_s &= 7;
  __asm__ ("srl %0,%1,%0" : "+r"(crypto_uint8_x) : "r"(crypto_uint8_s) : );
#else
  int crypto_uint8_k, crypto_uint8_l;
  for (crypto_uint8_l = 0,crypto_uint8_k = 1;crypto_uint8_k < 8;++crypto_uint8_l,crypto_uint8_k *= 2)
    crypto_uint8_x ^= (crypto_uint8_x ^ (crypto_uint8_x >> crypto_uint8_k)) & crypto_uint8_bitinrangepublicpos_mask(crypto_uint8_s,crypto_uint8_l);
#endif
  return crypto_uint8_x;
}

__attribute__((unused))
static inline
crypto_uint8 crypto_uint8_bitmod_mask(crypto_uint8 crypto_uint8_x,crypto_uint8 crypto_uint8_s) {
  crypto_uint8_x = crypto_uint8_shrmod(crypto_uint8_x,crypto_uint8_s);
  return crypto_uint8_bottombit_mask(crypto_uint8_x);
}

__attribute__((unused))
static inline
crypto_uint8 crypto_uint8_bitmod_01(crypto_uint8 crypto_uint8_x,crypto_uint8 crypto_uint8_s) {
  crypto_uint8_x = crypto_uint8_shrmod(crypto_uint8_x,crypto_uint8_s);
  return crypto_uint8_bottombit_01(crypto_uint8_x);
}

__attribute__((unused))
static inline
crypto_uint8 crypto_uint8_nonzero_mask(crypto_uint8 crypto_uint8_x) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_uint8 crypto_uint8_z;
  int32_t crypto_uint8_x32 = crypto_uint8_x,crypto_uint8_q32,crypto_uint8_z32;
  __asm__ ("xorl %0,%0\n movl $-1,%1\n testb %b2,%b2\n cmovnel %1,%0" : "=&r"(crypto_uint8_z32), "=&r"(crypto_uint8_q32) : "q"(crypto_uint8_x32) : "cc");
  crypto_uint8_z = crypto_uint8_z32;
  return crypto_uint8_z;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_uint8 crypto_uint8_z;
  __asm__ ("tst %w1,255\n csetm %w0,ne" : "=r"(crypto_uint8_z) : "r"(crypto_uint8_x) : "cc");
  return crypto_uint8_z;
#elif defined(__GNUC__) && defined(__arm__) && defined(__ARM_ARCH) && (__ARM_ARCH >= 6) && !defined(__thumb__)
  __asm__ ("uxtb %0,%0\n cmp %0,#0\n movne %0,#-1" : "+r"(crypto_uint8_x) : : "cc");
  return crypto_uint8_x;
#elif defined(__GNUC__) && defined(__sparc_v8__)
  crypto_uint8 crypto_uint8_z;
  __asm__ ("and %0,255,%0\n cmp %%g0,%0\n subx %%g0,0,%1" : "+r"(crypto_uint8_x), "=r"(crypto_uint8_z) : : "cc");
  return crypto_uint8_z;
#else
  crypto_uint8_x |= -crypto_uint8_x;
  return crypto_uint8_signed_negative_mask(crypto_uint8_x);
#endif
}

__attribute__((unused))
static inline
crypto_uint8 crypto_uint8_nonzero_01(crypto_uint8 crypto_uint8_x) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_uint8 crypto_uint8_z;
  int32_t crypto_uint8_x32 = crypto_uint8_x,crypto_uint8_q32,crypto_uint8_z32;
  __asm__ ("xorl %0,%0\n movl $1,%1\n testb %b2,%b2\n cmovnel %1,%0" : "=&r"(crypto_uint8_z32), "=&r"(crypto_uint8_q32) : "q"(crypto_uint8_x32) : "cc");
  crypto_uint8_z = crypto_uint8_z32;
  return crypto_uint8_z;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_uint8 crypto_uint8_z;
  __asm__ ("tst %w1,255\n cset %w0,ne" : "=r"(crypto_uint8_z) : "r"(crypto_uint8_x) : "cc");
  return crypto_uint8_z;
#elif defined(__GNUC__) && defined(__arm__) && defined(__ARM_ARCH) && (__ARM_ARCH >= 6) && !defined(__thumb__)
  __asm__ ("uxtb %0,%0\n cmp %0,#0\n movne %0,#1" : "+r"(crypto_uint8_x) : : "cc");
  return crypto_uint8_x;
#elif defined(__GNUC__) && defined(__sparc_v8__)
  crypto_uint8 crypto_uint8_z;
  __asm__ ("and %0,255,%0\n cmp %%g0,%0\n addx %%g0,0,%1" : "+r"(crypto_uint8_x), "=r"(crypto_uint8_z) : : "cc");
  return crypto_uint8_z;
#else
  crypto_uint8_x |= -crypto_uint8_x;
  return crypto_uint8_topbit_01(crypto_uint8_x);
#endif
}

__attribute__((unused))
static inline
crypto_uint8 crypto_uint8_zero_mask(crypto_uint8 crypto_uint8_x) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_uint8 crypto_uint8_z;
  int32_t crypto_uint8_x32 = crypto_uint8_x,crypto_uint8_q32,crypto_uint8_z32;
  __asm__ ("xorl %0,%0\n movl $-1,%1\n testb %b2,%b2\n cmovel %1,%0" : "=&r"(crypto_uint8_z32), "=&r"(crypto_uint8_q32) : "q"(crypto_uint8_x32) : "cc");
  crypto_uint8_z = crypto_uint8_z32;
  return crypto_uint8_z;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_uint8 crypto_uint8_z;
  __asm__ ("tst %w1,255\n csetm %w0,eq" : "=r"(crypto_uint8_z) : "r"(crypto_uint8_x) : "cc");
  return crypto_uint8_z;
#elif defined(__GNUC__) && defined(__sparc_v8__)
  crypto_uint8 crypto_uint8_z;
  __asm__ ("and %0,255,%0\n cmp %%g0,%0\n addx %%g0,-1,%1" : "+r"(crypto_uint8_x), "=r"(crypto_uint8_z) : : "cc");
  return crypto_uint8_z;
#else
  return ~crypto_uint8_nonzero_mask(crypto_uint8_x);
#endif
}

__attribute__((unused))
static inline
crypto_uint8 crypto_uint8_zero_01(crypto_uint8 crypto_uint8_x) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_uint8 crypto_uint8_z;
  int32_t crypto_uint8_x32 = crypto_uint8_x,crypto_uint8_q32,crypto_uint8_z32;
  __asm__ ("xorl %0,%0\n movl $1,%1\n testb %b2,%b2\n cmovel %1,%0" : "=&r"(crypto_uint8_z32), "=&r"(crypto_uint8_q32) : "q"(crypto_uint8_x32) : "cc");
  crypto_uint8_z = crypto_uint8_z32;
  return crypto_uint8_z;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_uint8 crypto_uint8_z;
  __asm__ ("tst %w1,255\n cset %w0,eq" : "=r"(crypto_uint8_z) : "r"(crypto_uint8_x) : "cc");
  return crypto_uint8_z;
#elif defined(__GNUC__) && defined(__sparc_v8__)
  crypto_uint8 crypto_uint8_z;
  __asm__ ("and %0,255,%0\n cmp %%g0,%0\n subx %%g0,-1,%1" : "+r"(crypto_uint8_x), "=r"(crypto_uint8_z) : : "cc");
  return crypto_uint8_z;
#else
  return 1-crypto_uint8_nonzero_01(crypto_uint8_x);
#endif
}

__attribute__((unused))
static inline
crypto_uint8 crypto_uint8_unequal_mask(crypto_uint8 crypto_uint8_x,crypto_uint8 crypto_uint8_y) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_uint8 crypto_uint8_z;
  int32_t crypto_uint8_x32 = crypto_uint8_x,crypto_uint8_y32 = crypto_uint8_y,crypto_uint8_q32,crypto_uint8_z32;
  __asm__ ("xorl %0,%0\n movl $-1,%1\n cmpb %b3,%b2\n cmovnel %1,%0" : "=&r"(crypto_uint8_z32), "=&r"(crypto_uint8_q32) : "q"(crypto_uint8_x32), "q"(crypto_uint8_y32) : "cc");
  crypto_uint8_z = crypto_uint8_z32;
  return crypto_uint8_z;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_uint8 crypto_uint8_z;
  __asm__ ("and %w0,%w1,255\n cmp %w0,%w2,uxtb\n csetm %w0,ne" : "=&r"(crypto_uint8_z) : "r"(crypto_uint8_x), "r"(crypto_uint8_y) : "cc");
  return crypto_uint8_z;
#else
  return crypto_uint8_nonzero_mask(crypto_uint8_x ^ crypto_uint8_y);
#endif
}

__attribute__((unused))
static inline
crypto_uint8 crypto_uint8_unequal_01(crypto_uint8 crypto_uint8_x,crypto_uint8 crypto_uint8_y) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_uint8 crypto_uint8_z;
  int32_t crypto_uint8_x32 = crypto_uint8_x,crypto_uint8_y32 = crypto_uint8_y,crypto_uint8_q32,crypto_uint8_z32;
  __asm__ ("xorl %0,%0\n movl $1,%1\n cmpb %b3,%b2\n cmovnel %1,%0" : "=&r"(crypto_uint8_z32), "=&r"(crypto_uint8_q32) : "q"(crypto_uint8_x32), "q"(crypto_uint8_y32) : "cc");
  crypto_uint8_z = crypto_uint8_z32;
  return crypto_uint8_z;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_uint8 crypto_uint8_z;
  __asm__ ("and %w0,%w1,255\n cmp %w0,%w2,uxtb\n cset %w0,ne" : "=&r"(crypto_uint8_z) : "r"(crypto_uint8_x), "r"(crypto_uint8_y) : "cc");
  return crypto_uint8_z;
#else
  return crypto_uint8_nonzero_01(crypto_uint8_x ^ crypto_uint8_y);
#endif
}

__attribute__((unused))
static inline
crypto_uint8 crypto_uint8_equal_mask(crypto_uint8 crypto_uint8_x,crypto_uint8 crypto_uint8_y) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_uint8 crypto_uint8_z;
  int32_t crypto_uint8_x32 = crypto_uint8_x,crypto_uint8_y32 = crypto_uint8_y,crypto_uint8_q32,crypto_uint8_z32;
  __asm__ ("xorl %0,%0\n movl $-1,%1\n cmpb %b3,%b2\n cmovel %1,%0" : "=&r"(crypto_uint8_z32), "=&r"(crypto_uint8_q32) : "q"(crypto_uint8_x32), "q"(crypto_uint8_y32) : "cc");
  crypto_uint8_z = crypto_uint8_z32;
  return crypto_uint8_z;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_uint8 crypto_uint8_z;
  __asm__ ("and %w0,%w1,255\n cmp %w0,%w2,uxtb\n csetm %w0,eq" : "=&r"(crypto_uint8_z) : "r"(crypto_uint8_x), "r"(crypto_uint8_y) : "cc");
  return crypto_uint8_z;
#else
  return crypto_uint8_zero_mask(crypto_uint8_x ^ crypto_uint8_y);
#endif
}

__attribute__((unused))
static inline
crypto_uint8 crypto_uint8_equal_01(crypto_uint8 crypto_uint8_x,crypto_uint8 crypto_uint8_y) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_uint8 crypto_uint8_z;
  int32_t crypto_uint8_x32 = crypto_uint8_x,crypto_uint8_y32 = crypto_uint8_y,crypto_uint8_q32,crypto_uint8_z32;
  __asm__ ("xorl %0,%0\n movl $1,%1\n cmpb %b3,%b2\n cmovel %1,%0" : "=&r"(crypto_uint8_z32), "=&r"(crypto_uint8_q32) : "q"(crypto_uint8_x32), "q"(crypto_uint8_y32) : "cc");
  crypto_uint8_z = crypto_uint8_z32;
  return crypto_uint8_z;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_uint8 crypto_uint8_z;
  __asm__ ("and %w0,%w1,255\n cmp %w0,%w2,uxtb\n cset %w0,eq" : "=&r"(crypto_uint8_z) : "r"(crypto_uint8_x), "r"(crypto_uint8_y) : "cc");
  return crypto_uint8_z;
#else
  return crypto_uint8_zero_01(crypto_uint8_x ^ crypto_uint8_y);
#endif
}

__attribute__((unused))
static inline
crypto_uint8 crypto_uint8_min(crypto_uint8 crypto_uint8_x,crypto_uint8 crypto_uint8_y) {
#if defined(__GNUC__) && defined(__x86_64__)
  uint32_t crypto_uint8_x32 = crypto_uint8_x,crypto_uint8_y32 = crypto_uint8_y;
  __asm__ ("cmpb %b1,%b0\n cmoval %1,%0" : "+q"(crypto_uint8_x32) : "q"(crypto_uint8_y32) : "cc");
  crypto_uint8_x = crypto_uint8_x32;
  return crypto_uint8_x;
#elif defined(__GNUC__) && defined(__aarch64__)
  __asm__ ("and %w0,%w0,255\n cmp %w0,%w1,uxtb\n csel %w0,%w0,%w1,lo" : "+&r"(crypto_uint8_x) : "r"(crypto_uint8_y) : "cc");
  return crypto_uint8_x;
#else
  crypto_uint8 crypto_uint8_r = crypto_uint8_y ^ crypto_uint8_x;
  crypto_uint8 crypto_uint8_z = crypto_uint8_y - crypto_uint8_x;
  crypto_uint8_z ^= crypto_uint8_r & (crypto_uint8_z ^ crypto_uint8_y ^ (((crypto_uint8) 1) << (8-1)));
  crypto_uint8_z = crypto_uint8_signed_negative_mask(crypto_uint8_z);
  crypto_uint8_z &= crypto_uint8_r;
  return crypto_uint8_x ^ crypto_uint8_z;
#endif
}

__attribute__((unused))
static inline
crypto_uint8 crypto_uint8_max(crypto_uint8 crypto_uint8_x,crypto_uint8 crypto_uint8_y) {
#if defined(__GNUC__) && defined(__x86_64__)
  uint32_t crypto_uint8_x32 = crypto_uint8_x,crypto_uint8_y32 = crypto_uint8_y;
  __asm__ ("cmpb %b1,%b0\n cmovbl %1,%0" : "+q"(crypto_uint8_x32) : "q"(crypto_uint8_y32) : "cc");
  crypto_uint8_x = crypto_uint8_x32;
  return crypto_uint8_x;
#elif defined(__GNUC__) && defined(__aarch64__)
  __asm__ ("and %w0,%w0,255\n cmp %w0,%w1,uxtb\n csel %w0,%w1,%w0,lo" : "+&r"(crypto_uint8_x) : "r"(crypto_uint8_y) : "cc");
  return crypto_uint8_x;
#else
  crypto_uint8 crypto_uint8_r = crypto_uint8_y ^ crypto_uint8_x;
  crypto_uint8 crypto_uint8_z = crypto_uint8_y - crypto_uint8_x;
  crypto_uint8_z ^= crypto_uint8_r & (crypto_uint8_z ^ crypto_uint8_y ^ (((crypto_uint8) 1) << (8-1)));
  crypto_uint8_z = crypto_uint8_signed_negative_mask(crypto_uint8_z);
  crypto_uint8_z &= crypto_uint8_r;
  return crypto_uint8_y ^ crypto_uint8_z;
#endif
}

__attribute__((unused))
static inline
void crypto_uint8_minmax(crypto_uint8 *crypto_uint8_p,crypto_uint8 *crypto_uint8_q) {
  crypto_uint8 crypto_uint8_x = *crypto_uint8_p;
  crypto_uint8 crypto_uint8_y = *crypto_uint8_q;
#if defined(__GNUC__) && defined(__x86_64__)
  uint32_t crypto_uint8_x32 = crypto_uint8_x,crypto_uint8_y32 = crypto_uint8_y,crypto_uint8_z32;
  __asm__ ("cmpb %b2,%b1\n movl %1,%0\n cmoval %2,%1\n cmoval %0,%2" : "=&r"(crypto_uint8_z32), "+&q"(crypto_uint8_x32), "+q"(crypto_uint8_y32) : : "cc");
  crypto_uint8_x = crypto_uint8_x32; crypto_uint8_y = crypto_uint8_y32;
  *crypto_uint8_p = crypto_uint8_x;
  *crypto_uint8_q = crypto_uint8_y;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_uint8 crypto_uint8_r, crypto_uint8_s;
  __asm__ ("and %w0,%w0,255\n cmp %w0,%w3,uxtb\n csel %w1,%w0,%w3,lo\n csel %w2,%w3,%w0,lo" : "+&r"(crypto_uint8_x), "=&r"(crypto_uint8_r), "=r"(crypto_uint8_s) : "r"(crypto_uint8_y) : "cc");
  *crypto_uint8_p = crypto_uint8_r;
  *crypto_uint8_q = crypto_uint8_s;
#else
  crypto_uint8 crypto_uint8_r = crypto_uint8_y ^ crypto_uint8_x;
  crypto_uint8 crypto_uint8_z = crypto_uint8_y - crypto_uint8_x;
  crypto_uint8_z ^= crypto_uint8_r & (crypto_uint8_z ^ crypto_uint8_y ^ (((crypto_uint8) 1) << (8-1)));
  crypto_uint8_z = crypto_uint8_signed_negative_mask(crypto_uint8_z);
  crypto_uint8_z &= crypto_uint8_r;
  crypto_uint8_x ^= crypto_uint8_z;
  crypto_uint8_y ^= crypto_uint8_z;
  *crypto_uint8_p = crypto_uint8_x;
  *crypto_uint8_q = crypto_uint8_y;
#endif
}

__attribute__((unused))
static inline
crypto_uint8 crypto_uint8_smaller_mask(crypto_uint8 crypto_uint8_x,crypto_uint8 crypto_uint8_y) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_uint8 crypto_uint8_z;
  int32_t crypto_uint8_x32 = crypto_uint8_x,crypto_uint8_y32 = crypto_uint8_y,crypto_uint8_q32,crypto_uint8_z32;
  __asm__ ("xorl %0,%0\n movl $-1,%1\n cmpb %b3,%b2\n cmovbl %1,%0" : "=&r"(crypto_uint8_z32), "=&r"(crypto_uint8_q32) : "q"(crypto_uint8_x32), "q"(crypto_uint8_y32) : "cc");
  crypto_uint8_z = crypto_uint8_z32;
  return crypto_uint8_z;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_uint8 crypto_uint8_z;
  __asm__ ("and %w0,%w1,255\n cmp %w0,%w2,uxtb\n csetm %w0,lo" : "=&r"(crypto_uint8_z) : "r"(crypto_uint8_x), "r"(crypto_uint8_y) : "cc");
  return crypto_uint8_z;
#else
  crypto_uint8 crypto_uint8_r = crypto_uint8_x ^ crypto_uint8_y;
  crypto_uint8 crypto_uint8_z = crypto_uint8_x - crypto_uint8_y;
  crypto_uint8_z ^= crypto_uint8_r & (crypto_uint8_z ^ crypto_uint8_x ^ (((crypto_uint8) 1) << (8-1)));
  return crypto_uint8_signed_negative_mask(crypto_uint8_z);
#endif
}

__attribute__((unused))
static inline
crypto_uint8 crypto_uint8_smaller_01(crypto_uint8 crypto_uint8_x,crypto_uint8 crypto_uint8_y) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_uint8 crypto_uint8_z;
  int32_t crypto_uint8_x32 = crypto_uint8_x,crypto_uint8_y32 = crypto_uint8_y,crypto_uint8_q32,crypto_uint8_z32;
  __asm__ ("xorl %0,%0\n movl $1,%1\n cmpb %b3,%b2\n cmovbl %1,%0" : "=&r"(crypto_uint8_z32), "=&r"(crypto_uint8_q32) : "q"(crypto_uint8_x32), "q"(crypto_uint8_y32) : "cc");
  crypto_uint8_z = crypto_uint8_z32;
  return crypto_uint8_z;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_uint8 crypto_uint8_z;
  __asm__ ("and %w0,%w1,255\n cmp %w0,%w2,uxtb\n cset %w0,lo" : "=&r"(crypto_uint8_z) : "r"(crypto_uint8_x), "r"(crypto_uint8_y) : "cc");
  return crypto_uint8_z;
#else
  crypto_uint8 crypto_uint8_r = crypto_uint8_x ^ crypto_uint8_y;
  crypto_uint8 crypto_uint8_z = crypto_uint8_x - crypto_uint8_y;
  crypto_uint8_z ^= crypto_uint8_r & (crypto_uint8_z ^ crypto_uint8_x ^ (((crypto_uint8) 1) << (8-1)));
  return crypto_uint8_topbit_01(crypto_uint8_z);
#endif
}

__attribute__((unused))
static inline
crypto_uint8 crypto_uint8_leq_mask(crypto_uint8 crypto_uint8_x,crypto_uint8 crypto_uint8_y) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_uint8 crypto_uint8_z;
  int32_t crypto_uint8_x32 = crypto_uint8_x,crypto_uint8_y32 = crypto_uint8_y,crypto_uint8_q32,crypto_uint8_z32;
  __asm__ ("xorl %0,%0\n movl $-1,%1\n cmpb %b3,%b2\n cmovbel %1,%0" : "=&r"(crypto_uint8_z32), "=&r"(crypto_uint8_q32) : "q"(crypto_uint8_x32), "q"(crypto_uint8_y32) : "cc");
  crypto_uint8_z = crypto_uint8_z32;
  return crypto_uint8_z;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_uint8 crypto_uint8_z;
  __asm__ ("and %w0,%w1,255\n cmp %w0,%w2,uxtb\n csetm %w0,ls" : "=&r"(crypto_uint8_z) : "r"(crypto_uint8_x), "r"(crypto_uint8_y) : "cc");
  return crypto_uint8_z;
#else
  return ~crypto_uint8_smaller_mask(crypto_uint8_y,crypto_uint8_x);
#endif
}

__attribute__((unused))
static inline
crypto_uint8 crypto_uint8_leq_01(crypto_uint8 crypto_uint8_x,crypto_uint8 crypto_uint8_y) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_uint8 crypto_uint8_z;
  int32_t crypto_uint8_x32 = crypto_uint8_x,crypto_uint8_y32 = crypto_uint8_y,crypto_uint8_q32,crypto_uint8_z32;
  __asm__ ("xorl %0,%0\n movl $1,%1\n cmpb %b3,%b2\n cmovbel %1,%0" : "=&r"(crypto_uint8_z32), "=&r"(crypto_uint8_q32) : "q"(crypto_uint8_x32), "q"(crypto_uint8_y32) : "cc");
  crypto_uint8_z = crypto_uint8_z32;
  return crypto_uint8_z;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_uint8 crypto_uint8_z;
  __asm__ ("and %w0,%w1,255\n cmp %w0,%w2,uxtb\n cset %w0,ls" : "=&r"(crypto_uint8_z) : "r"(crypto_uint8_x), "r"(crypto_uint8_y) : "cc");
  return crypto_uint8_z;
#else
  return 1-crypto_uint8_smaller_01(crypto_uint8_y,crypto_uint8_x);
#endif
}

__attribute__((unused))
static inline
int crypto_uint8_ones_num(crypto_uint8 crypto_uint8_x) {
  crypto_uint8 crypto_uint8_y = crypto_uint8_x;
  const crypto_uint8 C0 = 0x55;
  const crypto_uint8 C1 = 0x33;
  const crypto_uint8 C2 = 0x0f;
  crypto_uint8_y -= ((crypto_uint8_y >> 1) & C0);
  crypto_uint8_y = (crypto_uint8_y & C1) + ((crypto_uint8_y >> 2) & C1);
  crypto_uint8_y = (crypto_uint8_y + (crypto_uint8_y >> 4)) & C2;
  return crypto_uint8_y;
}

__attribute__((unused))
static inline
int crypto_uint8_bottomzeros_num(crypto_uint8 crypto_uint8_x) {
#if defined(__GNUC__) && defined(__x86_64__)
  int32_t fallback = 8;
  int32_t crypto_uint8_x32 = crypto_uint8_x;
  __asm__ ("bsfl %0,%0\n cmovel %1,%0" : "+&r"(crypto_uint8_x32) : "r"(fallback) : "cc");
  crypto_uint8_x = crypto_uint8_x32;
  return crypto_uint8_x;
#elif defined(__GNUC__) && defined(__aarch64__)
  int64_t crypto_uint8_y;
  __asm__ ("orr %w0,%w1,-256\n rbit %w0,%w0\n clz %w0,%w0" : "=r"(crypto_uint8_y) : "r"(crypto_uint8_x) : );
  return crypto_uint8_y;
#else
  crypto_uint8 crypto_uint8_y = crypto_uint8_x ^ (crypto_uint8_x-1);
  crypto_uint8_y = ((crypto_uint8_signed) crypto_uint8_y) >> 1;
  crypto_uint8_y &= ~(crypto_uint8_x & (((crypto_uint8) 1) << (8-1)));
  return crypto_uint8_ones_num(crypto_uint8_y);
#endif
}

#endif

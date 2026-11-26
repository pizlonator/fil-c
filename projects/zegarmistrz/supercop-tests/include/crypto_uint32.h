/* auto-generated: cd cryptoint; ./autogen */
/* cryptoint 20250414 */

#ifndef crypto_uint32_h
#define crypto_uint32_h

#include <inttypes.h>
#define crypto_uint32 uint32_t
#define crypto_uint32_signed int32_t

#define crypto_uint32_signed_optblocker supercop_uint32_signed_optblocker
extern volatile crypto_uint32_signed crypto_uint32_signed_optblocker;

__attribute__((unused))
static inline
crypto_uint32 crypto_uint32_load(const unsigned char *crypto_uint32_s) {
  crypto_uint32 crypto_uint32_z = 0;
  crypto_uint32_z |= ((crypto_uint32) (*crypto_uint32_s++)) << 0;
  crypto_uint32_z |= ((crypto_uint32) (*crypto_uint32_s++)) << 8;
  crypto_uint32_z |= ((crypto_uint32) (*crypto_uint32_s++)) << 16;
  crypto_uint32_z |= ((crypto_uint32) (*crypto_uint32_s++)) << 24;
  return crypto_uint32_z;
}

__attribute__((unused))
static inline
crypto_uint32 crypto_uint32_load_bigendian(const unsigned char *crypto_uint32_s) {
  crypto_uint32 crypto_uint32_z = 0;
  crypto_uint32_z |= ((crypto_uint32) (*crypto_uint32_s++)) << 24;
  crypto_uint32_z |= ((crypto_uint32) (*crypto_uint32_s++)) << 16;
  crypto_uint32_z |= ((crypto_uint32) (*crypto_uint32_s++)) << 8;
  crypto_uint32_z |= ((crypto_uint32) (*crypto_uint32_s++)) << 0;
  return crypto_uint32_z;
}

__attribute__((unused))
static inline
void crypto_uint32_store(unsigned char *crypto_uint32_s,crypto_uint32 crypto_uint32_x) {
  *crypto_uint32_s++ = crypto_uint32_x >> 0;
  *crypto_uint32_s++ = crypto_uint32_x >> 8;
  *crypto_uint32_s++ = crypto_uint32_x >> 16;
  *crypto_uint32_s++ = crypto_uint32_x >> 24;
}

__attribute__((unused))
static inline
void crypto_uint32_store_bigendian(unsigned char *crypto_uint32_s,crypto_uint32 crypto_uint32_x) {
  *crypto_uint32_s++ = crypto_uint32_x >> 24;
  *crypto_uint32_s++ = crypto_uint32_x >> 16;
  *crypto_uint32_s++ = crypto_uint32_x >> 8;
  *crypto_uint32_s++ = crypto_uint32_x >> 0;
}

__attribute__((unused))
static inline
crypto_uint32_signed crypto_uint32_signed_negative_mask(crypto_uint32_signed crypto_uint32_x) {
#if defined(__GNUC__) && defined(__x86_64__)
  __asm__ ("sarl $31,%0" : "+r"(crypto_uint32_x) : : "cc");
  return crypto_uint32_x;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_uint32_signed crypto_uint32_y;
  __asm__ ("asr %w0,%w1,31" : "=r"(crypto_uint32_y) : "r"(crypto_uint32_x) : );
  return crypto_uint32_y;
#elif defined(__GNUC__) && defined(__arm__) && defined(__ARM_ARCH) && (__ARM_ARCH >= 6) && !defined(__thumb__)
  crypto_uint32_signed crypto_uint32_y;
  __asm__ ("asr %0,%1,#31" : "=r"(crypto_uint32_y) : "r"(crypto_uint32_x) : );
  return crypto_uint32_y;
#elif defined(__GNUC__) && defined(__sparc_v8__)
  crypto_uint32_signed crypto_uint32_y;
  __asm__ ("sra %1,31,%0" : "=r"(crypto_uint32_y) : "r"(crypto_uint32_x) : );
  return crypto_uint32_y;
#else
  crypto_uint32_x >>= 32-6;
  crypto_uint32_x += crypto_uint32_signed_optblocker;
  crypto_uint32_x >>= 5;
  return crypto_uint32_x;
#endif
}

__attribute__((unused))
static inline
crypto_uint32 crypto_uint32_topbit_01(crypto_uint32 crypto_uint32_x) {
#if defined(__GNUC__) && defined(__x86_64__)
  __asm__ ("shrl $31,%0" : "+r"(crypto_uint32_x) : : "cc");
  return crypto_uint32_x;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_uint32 crypto_uint32_y;
  __asm__ ("lsr %w0,%w1,31" : "=r"(crypto_uint32_y) : "r"(crypto_uint32_x) : );
  return crypto_uint32_y;
#elif defined(__GNUC__) && defined(__arm__) && defined(__ARM_ARCH) && (__ARM_ARCH >= 6) && !defined(__thumb__)
  crypto_uint32_signed crypto_uint32_y;
  __asm__ ("lsr %0,%1,#31" : "=r"(crypto_uint32_y) : "r"(crypto_uint32_x) : );
  return crypto_uint32_y;
#elif defined(__GNUC__) && defined(__sparc_v8__)
  crypto_uint32 crypto_uint32_y;
  __asm__ ("srl %1,31,%0" : "=r"(crypto_uint32_y) : "r"(crypto_uint32_x) : );
  return crypto_uint32_y;
#else
  crypto_uint32_x >>= 32-6;
  crypto_uint32_x += crypto_uint32_signed_optblocker;
  crypto_uint32_x >>= 5;
  return crypto_uint32_x;
#endif
}

__attribute__((unused))
static inline
crypto_uint32 crypto_uint32_topbit_mask(crypto_uint32 crypto_uint32_x) {
  return crypto_uint32_signed_negative_mask(crypto_uint32_x);
}

__attribute__((unused))
static inline
crypto_uint32 crypto_uint32_bottombit_mask(crypto_uint32 crypto_uint32_x) {
#if defined(__GNUC__) && defined(__x86_64__)
  __asm__ ("andl $1,%0" : "+r"(crypto_uint32_x) : : "cc");
  return -crypto_uint32_x;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_uint32 crypto_uint32_y;
  __asm__ ("sbfx %w0,%w1,0,1" : "=r"(crypto_uint32_y) : "r"(crypto_uint32_x) : );
  return crypto_uint32_y;
#elif defined(__GNUC__) && defined(__arm__) && defined(__ARM_ARCH) && (__ARM_ARCH >= 6) && !defined(__thumb__)
  crypto_uint32 crypto_uint32_y;
  __asm__ ("and %0,%1,#1\n neg %0,%0" : "=r"(crypto_uint32_y) : "r"(crypto_uint32_x) : );
  return crypto_uint32_y;
#elif defined(__GNUC__) && defined(__sparc_v8__)
  crypto_uint32 crypto_uint32_y;
  __asm__ ("and %1,1,%0\n neg %0,%0" : "=r"(crypto_uint32_y) : "r"(crypto_uint32_x) : );
  return crypto_uint32_y;
#else
  crypto_uint32_x &= 1 + crypto_uint32_signed_optblocker;
  return -crypto_uint32_x;
#endif
}

__attribute__((unused))
static inline
crypto_uint32 crypto_uint32_bottombit_01(crypto_uint32 crypto_uint32_x) {
#if defined(__GNUC__) && defined(__x86_64__)
  __asm__ ("andl $1,%0" : "+r"(crypto_uint32_x) : : "cc");
  return crypto_uint32_x;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_uint32 crypto_uint32_y;
  __asm__ ("ubfx %w0,%w1,0,1" : "=r"(crypto_uint32_y) : "r"(crypto_uint32_x) : );
  return crypto_uint32_y;
#elif defined(__GNUC__) && defined(__arm__) && defined(__ARM_ARCH) && (__ARM_ARCH >= 6) && !defined(__thumb__)
  crypto_uint32 crypto_uint32_y;
  __asm__ ("and %0,%1,#1" : "=r"(crypto_uint32_y) : "r"(crypto_uint32_x) : );
  return crypto_uint32_y;
#elif defined(__GNUC__) && defined(__sparc_v8__)
  crypto_uint32 crypto_uint32_y;
  __asm__ ("and %1,1,%0" : "=r"(crypto_uint32_y) : "r"(crypto_uint32_x) : );
  return crypto_uint32_y;
#else
  crypto_uint32_x &= 1 + crypto_uint32_signed_optblocker;
  return crypto_uint32_x;
#endif
}

__attribute__((unused))
static inline
crypto_uint32 crypto_uint32_bitinrangepublicpos_mask(crypto_uint32 crypto_uint32_x,crypto_uint32 crypto_uint32_s) {
#if defined(__GNUC__) && defined(__x86_64__)
  __asm__ ("shrl %%cl,%0" : "+r"(crypto_uint32_x) : "c"(crypto_uint32_s) : "cc");
#elif defined(__GNUC__) && defined(__aarch64__)
  __asm__ ("lsr %w0,%w0,%w1" : "+r"(crypto_uint32_x) : "r"(crypto_uint32_s) : );
#elif defined(__GNUC__) && defined(__arm__) && defined(__ARM_ARCH) && (__ARM_ARCH >= 6) && !defined(__thumb__)
  __asm__ ("and %0,%0,#31\n lsr %1,%1,%0" : "+&r"(crypto_uint32_s), "+r"(crypto_uint32_x) : : );
#elif defined(__GNUC__) && defined(__sparc_v8__)
  __asm__ ("srl %0,%1,%0" : "+r"(crypto_uint32_x) : "r"(crypto_uint32_s) : );
#else
  crypto_uint32_x >>= crypto_uint32_s ^ crypto_uint32_signed_optblocker;
#endif
  return crypto_uint32_bottombit_mask(crypto_uint32_x);
}

__attribute__((unused))
static inline
crypto_uint32 crypto_uint32_bitinrangepublicpos_01(crypto_uint32 crypto_uint32_x,crypto_uint32 crypto_uint32_s) {
#if defined(__GNUC__) && defined(__x86_64__)
  __asm__ ("shrl %%cl,%0" : "+r"(crypto_uint32_x) : "c"(crypto_uint32_s) : "cc");
#elif defined(__GNUC__) && defined(__aarch64__)
  __asm__ ("lsr %w0,%w0,%w1" : "+r"(crypto_uint32_x) : "r"(crypto_uint32_s) : );
#elif defined(__GNUC__) && defined(__arm__) && defined(__ARM_ARCH) && (__ARM_ARCH >= 6) && !defined(__thumb__)
  __asm__ ("and %0,%0,#31\n lsr %1,%1,%0" : "+&r"(crypto_uint32_s), "+r"(crypto_uint32_x) : : );
#elif defined(__GNUC__) && defined(__sparc_v8__)
  __asm__ ("srl %0,%1,%0" : "+r"(crypto_uint32_x) : "r"(crypto_uint32_s) : );
#else
  crypto_uint32_x >>= crypto_uint32_s ^ crypto_uint32_signed_optblocker;
#endif
  return crypto_uint32_bottombit_01(crypto_uint32_x);
}

__attribute__((unused))
static inline
crypto_uint32 crypto_uint32_shlmod(crypto_uint32 crypto_uint32_x,crypto_uint32 crypto_uint32_s) {
#if defined(__GNUC__) && defined(__x86_64__)
  __asm__ ("shll %%cl,%0" : "+r"(crypto_uint32_x) : "c"(crypto_uint32_s) : "cc");
#elif defined(__GNUC__) && defined(__aarch64__)
  __asm__ ("lsl %w0,%w0,%w1" : "+r"(crypto_uint32_x) : "r"(crypto_uint32_s) : );
#elif defined(__GNUC__) && defined(__arm__) && defined(__ARM_ARCH) && (__ARM_ARCH >= 6) && !defined(__thumb__)
  crypto_uint32_s &= 31;
  __asm__ ("lsl %0,%0,%1" : "+r"(crypto_uint32_x) : "r"(crypto_uint32_s) : );
#elif defined(__GNUC__) && defined(__sparc_v8__)
  __asm__ ("sll %0,%1,%0" : "+r"(crypto_uint32_x) : "r"(crypto_uint32_s) : );
#else
  int crypto_uint32_k, crypto_uint32_l;
  for (crypto_uint32_l = 0,crypto_uint32_k = 1;crypto_uint32_k < 32;++crypto_uint32_l,crypto_uint32_k *= 2)
    crypto_uint32_x ^= (crypto_uint32_x ^ (crypto_uint32_x << crypto_uint32_k)) & crypto_uint32_bitinrangepublicpos_mask(crypto_uint32_s,crypto_uint32_l);
#endif
  return crypto_uint32_x;
}

__attribute__((unused))
static inline
crypto_uint32 crypto_uint32_shrmod(crypto_uint32 crypto_uint32_x,crypto_uint32 crypto_uint32_s) {
#if defined(__GNUC__) && defined(__x86_64__)
  __asm__ ("shrl %%cl,%0" : "+r"(crypto_uint32_x) : "c"(crypto_uint32_s) : "cc");
#elif defined(__GNUC__) && defined(__aarch64__)
  __asm__ ("lsr %w0,%w0,%w1" : "+r"(crypto_uint32_x) : "r"(crypto_uint32_s) : );
#elif defined(__GNUC__) && defined(__arm__) && defined(__ARM_ARCH) && (__ARM_ARCH >= 6) && !defined(__thumb__)
  crypto_uint32_s &= 31;
  __asm__ ("lsr %0,%0,%1" : "+r"(crypto_uint32_x) : "r"(crypto_uint32_s) : );
#elif defined(__GNUC__) && defined(__sparc_v8__)
  __asm__ ("srl %0,%1,%0" : "+r"(crypto_uint32_x) : "r"(crypto_uint32_s) : );
#else
  int crypto_uint32_k, crypto_uint32_l;
  for (crypto_uint32_l = 0,crypto_uint32_k = 1;crypto_uint32_k < 32;++crypto_uint32_l,crypto_uint32_k *= 2)
    crypto_uint32_x ^= (crypto_uint32_x ^ (crypto_uint32_x >> crypto_uint32_k)) & crypto_uint32_bitinrangepublicpos_mask(crypto_uint32_s,crypto_uint32_l);
#endif
  return crypto_uint32_x;
}

__attribute__((unused))
static inline
crypto_uint32 crypto_uint32_bitmod_mask(crypto_uint32 crypto_uint32_x,crypto_uint32 crypto_uint32_s) {
  crypto_uint32_x = crypto_uint32_shrmod(crypto_uint32_x,crypto_uint32_s);
  return crypto_uint32_bottombit_mask(crypto_uint32_x);
}

__attribute__((unused))
static inline
crypto_uint32 crypto_uint32_bitmod_01(crypto_uint32 crypto_uint32_x,crypto_uint32 crypto_uint32_s) {
  crypto_uint32_x = crypto_uint32_shrmod(crypto_uint32_x,crypto_uint32_s);
  return crypto_uint32_bottombit_01(crypto_uint32_x);
}

__attribute__((unused))
static inline
crypto_uint32 crypto_uint32_nonzero_mask(crypto_uint32 crypto_uint32_x) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_uint32 crypto_uint32_q,crypto_uint32_z;
  __asm__ ("xorl %0,%0\n movl $-1,%1\n testl %2,%2\n cmovnel %1,%0" : "=&r"(crypto_uint32_z), "=&r"(crypto_uint32_q) : "r"(crypto_uint32_x) : "cc");
  return crypto_uint32_z;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_uint32 crypto_uint32_z;
  __asm__ ("cmp %w1,0\n csetm %w0,ne" : "=r"(crypto_uint32_z) : "r"(crypto_uint32_x) : "cc");
  return crypto_uint32_z;
#elif defined(__GNUC__) && defined(__arm__) && defined(__ARM_ARCH) && (__ARM_ARCH >= 6) && !defined(__thumb__)
  __asm__ ("cmp %0,#0\n movne %0,#-1" : "+r"(crypto_uint32_x) : : "cc");
  return crypto_uint32_x;
#elif defined(__GNUC__) && defined(__sparc_v8__)
  crypto_uint32 crypto_uint32_z;
  __asm__ ("cmp %%g0,%1\n subx %%g0,0,%0" : "=r"(crypto_uint32_z) : "r"(crypto_uint32_x) : "cc");
  return crypto_uint32_z;
#else
  crypto_uint32_x |= -crypto_uint32_x;
  return crypto_uint32_signed_negative_mask(crypto_uint32_x);
#endif
}

__attribute__((unused))
static inline
crypto_uint32 crypto_uint32_nonzero_01(crypto_uint32 crypto_uint32_x) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_uint32 crypto_uint32_q,crypto_uint32_z;
  __asm__ ("xorl %0,%0\n movl $1,%1\n testl %2,%2\n cmovnel %1,%0" : "=&r"(crypto_uint32_z), "=&r"(crypto_uint32_q) : "r"(crypto_uint32_x) : "cc");
  return crypto_uint32_z;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_uint32 crypto_uint32_z;
  __asm__ ("cmp %w1,0\n cset %w0,ne" : "=r"(crypto_uint32_z) : "r"(crypto_uint32_x) : "cc");
  return crypto_uint32_z;
#elif defined(__GNUC__) && defined(__arm__) && defined(__ARM_ARCH) && (__ARM_ARCH >= 6) && !defined(__thumb__)
  __asm__ ("cmp %0,#0\n movne %0,#1" : "+r"(crypto_uint32_x) : : "cc");
  return crypto_uint32_x;
#elif defined(__GNUC__) && defined(__sparc_v8__)
  crypto_uint32 crypto_uint32_z;
  __asm__ ("cmp %%g0,%1\n addx %%g0,0,%0" : "=r"(crypto_uint32_z) : "r"(crypto_uint32_x) : "cc");
  return crypto_uint32_z;
#else
  crypto_uint32_x |= -crypto_uint32_x;
  return crypto_uint32_topbit_01(crypto_uint32_x);
#endif
}

__attribute__((unused))
static inline
crypto_uint32 crypto_uint32_zero_mask(crypto_uint32 crypto_uint32_x) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_uint32 crypto_uint32_q,crypto_uint32_z;
  __asm__ ("xorl %0,%0\n movl $-1,%1\n testl %2,%2\n cmovel %1,%0" : "=&r"(crypto_uint32_z), "=&r"(crypto_uint32_q) : "r"(crypto_uint32_x) : "cc");
  return crypto_uint32_z;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_uint32 crypto_uint32_z;
  __asm__ ("cmp %w1,0\n csetm %w0,eq" : "=r"(crypto_uint32_z) : "r"(crypto_uint32_x) : "cc");
  return crypto_uint32_z;
#elif defined(__GNUC__) && defined(__sparc_v8__)
  crypto_uint32 crypto_uint32_z;
  __asm__ ("cmp %%g0,%1\n addx %%g0,-1,%0" : "=r"(crypto_uint32_z) : "r"(crypto_uint32_x) : "cc");
  return crypto_uint32_z;
#else
  return ~crypto_uint32_nonzero_mask(crypto_uint32_x);
#endif
}

__attribute__((unused))
static inline
crypto_uint32 crypto_uint32_zero_01(crypto_uint32 crypto_uint32_x) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_uint32 crypto_uint32_q,crypto_uint32_z;
  __asm__ ("xorl %0,%0\n movl $1,%1\n testl %2,%2\n cmovel %1,%0" : "=&r"(crypto_uint32_z), "=&r"(crypto_uint32_q) : "r"(crypto_uint32_x) : "cc");
  return crypto_uint32_z;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_uint32 crypto_uint32_z;
  __asm__ ("cmp %w1,0\n cset %w0,eq" : "=r"(crypto_uint32_z) : "r"(crypto_uint32_x) : "cc");
  return crypto_uint32_z;
#elif defined(__GNUC__) && defined(__sparc_v8__)
  crypto_uint32 crypto_uint32_z;
  __asm__ ("cmp %%g0,%1\n subx %%g0,-1,%0" : "=r"(crypto_uint32_z) : "r"(crypto_uint32_x) : "cc");
  return crypto_uint32_z;
#else
  return 1-crypto_uint32_nonzero_01(crypto_uint32_x);
#endif
}

__attribute__((unused))
static inline
crypto_uint32 crypto_uint32_unequal_mask(crypto_uint32 crypto_uint32_x,crypto_uint32 crypto_uint32_y) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_uint32 crypto_uint32_q,crypto_uint32_z;
  __asm__ ("xorl %0,%0\n movl $-1,%1\n cmpl %3,%2\n cmovnel %1,%0" : "=&r"(crypto_uint32_z), "=&r"(crypto_uint32_q) : "r"(crypto_uint32_x), "r"(crypto_uint32_y) : "cc");
  return crypto_uint32_z;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_uint32 crypto_uint32_z;
  __asm__ ("cmp %w1,%w2\n csetm %w0,ne" : "=r"(crypto_uint32_z) : "r"(crypto_uint32_x), "r"(crypto_uint32_y) : "cc");
  return crypto_uint32_z;
#else
  return crypto_uint32_nonzero_mask(crypto_uint32_x ^ crypto_uint32_y);
#endif
}

__attribute__((unused))
static inline
crypto_uint32 crypto_uint32_unequal_01(crypto_uint32 crypto_uint32_x,crypto_uint32 crypto_uint32_y) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_uint32 crypto_uint32_q,crypto_uint32_z;
  __asm__ ("xorl %0,%0\n movl $1,%1\n cmpl %3,%2\n cmovnel %1,%0" : "=&r"(crypto_uint32_z), "=&r"(crypto_uint32_q) : "r"(crypto_uint32_x), "r"(crypto_uint32_y) : "cc");
  return crypto_uint32_z;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_uint32 crypto_uint32_z;
  __asm__ ("cmp %w1,%w2\n cset %w0,ne" : "=r"(crypto_uint32_z) : "r"(crypto_uint32_x), "r"(crypto_uint32_y) : "cc");
  return crypto_uint32_z;
#else
  return crypto_uint32_nonzero_01(crypto_uint32_x ^ crypto_uint32_y);
#endif
}

__attribute__((unused))
static inline
crypto_uint32 crypto_uint32_equal_mask(crypto_uint32 crypto_uint32_x,crypto_uint32 crypto_uint32_y) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_uint32 crypto_uint32_q,crypto_uint32_z;
  __asm__ ("xorl %0,%0\n movl $-1,%1\n cmpl %3,%2\n cmovel %1,%0" : "=&r"(crypto_uint32_z), "=&r"(crypto_uint32_q) : "r"(crypto_uint32_x), "r"(crypto_uint32_y) : "cc");
  return crypto_uint32_z;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_uint32 crypto_uint32_z;
  __asm__ ("cmp %w1,%w2\n csetm %w0,eq" : "=r"(crypto_uint32_z) : "r"(crypto_uint32_x), "r"(crypto_uint32_y) : "cc");
  return crypto_uint32_z;
#else
  return crypto_uint32_zero_mask(crypto_uint32_x ^ crypto_uint32_y);
#endif
}

__attribute__((unused))
static inline
crypto_uint32 crypto_uint32_equal_01(crypto_uint32 crypto_uint32_x,crypto_uint32 crypto_uint32_y) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_uint32 crypto_uint32_q,crypto_uint32_z;
  __asm__ ("xorl %0,%0\n movl $1,%1\n cmpl %3,%2\n cmovel %1,%0" : "=&r"(crypto_uint32_z), "=&r"(crypto_uint32_q) : "r"(crypto_uint32_x), "r"(crypto_uint32_y) : "cc");
  return crypto_uint32_z;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_uint32 crypto_uint32_z;
  __asm__ ("cmp %w1,%w2\n cset %w0,eq" : "=r"(crypto_uint32_z) : "r"(crypto_uint32_x), "r"(crypto_uint32_y) : "cc");
  return crypto_uint32_z;
#else
  return crypto_uint32_zero_01(crypto_uint32_x ^ crypto_uint32_y);
#endif
}

__attribute__((unused))
static inline
crypto_uint32 crypto_uint32_min(crypto_uint32 crypto_uint32_x,crypto_uint32 crypto_uint32_y) {
#if defined(__GNUC__) && defined(__x86_64__)
  __asm__ ("cmpl %1,%0\n cmoval %1,%0" : "+r"(crypto_uint32_x) : "r"(crypto_uint32_y) : "cc");
  return crypto_uint32_x;
#elif defined(__GNUC__) && defined(__aarch64__)
  __asm__ ("cmp %w0,%w1\n csel %w0,%w0,%w1,lo" : "+r"(crypto_uint32_x) : "r"(crypto_uint32_y) : "cc");
  return crypto_uint32_x;
#else
  crypto_uint32 crypto_uint32_r = crypto_uint32_y ^ crypto_uint32_x;
  crypto_uint32 crypto_uint32_z = crypto_uint32_y - crypto_uint32_x;
  crypto_uint32_z ^= crypto_uint32_r & (crypto_uint32_z ^ crypto_uint32_y ^ (((crypto_uint32) 1) << (32-1)));
  crypto_uint32_z = crypto_uint32_signed_negative_mask(crypto_uint32_z);
  crypto_uint32_z &= crypto_uint32_r;
  return crypto_uint32_x ^ crypto_uint32_z;
#endif
}

__attribute__((unused))
static inline
crypto_uint32 crypto_uint32_max(crypto_uint32 crypto_uint32_x,crypto_uint32 crypto_uint32_y) {
#if defined(__GNUC__) && defined(__x86_64__)
  __asm__ ("cmpl %1,%0\n cmovbl %1,%0" : "+r"(crypto_uint32_x) : "r"(crypto_uint32_y) : "cc");
  return crypto_uint32_x;
#elif defined(__GNUC__) && defined(__aarch64__)
  __asm__ ("cmp %w0,%w1\n csel %w0,%w1,%w0,lo" : "+r"(crypto_uint32_x) : "r"(crypto_uint32_y) : "cc");
  return crypto_uint32_x;
#else
  crypto_uint32 crypto_uint32_r = crypto_uint32_y ^ crypto_uint32_x;
  crypto_uint32 crypto_uint32_z = crypto_uint32_y - crypto_uint32_x;
  crypto_uint32_z ^= crypto_uint32_r & (crypto_uint32_z ^ crypto_uint32_y ^ (((crypto_uint32) 1) << (32-1)));
  crypto_uint32_z = crypto_uint32_signed_negative_mask(crypto_uint32_z);
  crypto_uint32_z &= crypto_uint32_r;
  return crypto_uint32_y ^ crypto_uint32_z;
#endif
}

__attribute__((unused))
static inline
void crypto_uint32_minmax(crypto_uint32 *crypto_uint32_p,crypto_uint32 *crypto_uint32_q) {
  crypto_uint32 crypto_uint32_x = *crypto_uint32_p;
  crypto_uint32 crypto_uint32_y = *crypto_uint32_q;
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_uint32 crypto_uint32_z;
  __asm__ ("cmpl %2,%1\n movl %1,%0\n cmoval %2,%1\n cmoval %0,%2" : "=&r"(crypto_uint32_z), "+&r"(crypto_uint32_x), "+r"(crypto_uint32_y) : : "cc");
  *crypto_uint32_p = crypto_uint32_x;
  *crypto_uint32_q = crypto_uint32_y;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_uint32 crypto_uint32_r, crypto_uint32_s;
  __asm__ ("cmp %w2,%w3\n csel %w0,%w2,%w3,lo\n csel %w1,%w3,%w2,lo" : "=&r"(crypto_uint32_r), "=r"(crypto_uint32_s) : "r"(crypto_uint32_x), "r"(crypto_uint32_y) : "cc");
  *crypto_uint32_p = crypto_uint32_r;
  *crypto_uint32_q = crypto_uint32_s;
#else
  crypto_uint32 crypto_uint32_r = crypto_uint32_y ^ crypto_uint32_x;
  crypto_uint32 crypto_uint32_z = crypto_uint32_y - crypto_uint32_x;
  crypto_uint32_z ^= crypto_uint32_r & (crypto_uint32_z ^ crypto_uint32_y ^ (((crypto_uint32) 1) << (32-1)));
  crypto_uint32_z = crypto_uint32_signed_negative_mask(crypto_uint32_z);
  crypto_uint32_z &= crypto_uint32_r;
  crypto_uint32_x ^= crypto_uint32_z;
  crypto_uint32_y ^= crypto_uint32_z;
  *crypto_uint32_p = crypto_uint32_x;
  *crypto_uint32_q = crypto_uint32_y;
#endif
}

__attribute__((unused))
static inline
crypto_uint32 crypto_uint32_smaller_mask(crypto_uint32 crypto_uint32_x,crypto_uint32 crypto_uint32_y) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_uint32 crypto_uint32_q,crypto_uint32_z;
  __asm__ ("xorl %0,%0\n movl $-1,%1\n cmpl %3,%2\n cmovbl %1,%0" : "=&r"(crypto_uint32_z), "=&r"(crypto_uint32_q) : "r"(crypto_uint32_x), "r"(crypto_uint32_y) : "cc");
  return crypto_uint32_z;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_uint32 crypto_uint32_z;
  __asm__ ("cmp %w1,%w2\n csetm %w0,lo" : "=r"(crypto_uint32_z) : "r"(crypto_uint32_x), "r"(crypto_uint32_y) : "cc");
  return crypto_uint32_z;
#else
  crypto_uint32 crypto_uint32_r = crypto_uint32_x ^ crypto_uint32_y;
  crypto_uint32 crypto_uint32_z = crypto_uint32_x - crypto_uint32_y;
  crypto_uint32_z ^= crypto_uint32_r & (crypto_uint32_z ^ crypto_uint32_x ^ (((crypto_uint32) 1) << (32-1)));
  return crypto_uint32_signed_negative_mask(crypto_uint32_z);
#endif
}

__attribute__((unused))
static inline
crypto_uint32 crypto_uint32_smaller_01(crypto_uint32 crypto_uint32_x,crypto_uint32 crypto_uint32_y) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_uint32 crypto_uint32_q,crypto_uint32_z;
  __asm__ ("xorl %0,%0\n movl $1,%1\n cmpl %3,%2\n cmovbl %1,%0" : "=&r"(crypto_uint32_z), "=&r"(crypto_uint32_q) : "r"(crypto_uint32_x), "r"(crypto_uint32_y) : "cc");
  return crypto_uint32_z;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_uint32 crypto_uint32_z;
  __asm__ ("cmp %w1,%w2\n cset %w0,lo" : "=r"(crypto_uint32_z) : "r"(crypto_uint32_x), "r"(crypto_uint32_y) : "cc");
  return crypto_uint32_z;
#else
  crypto_uint32 crypto_uint32_r = crypto_uint32_x ^ crypto_uint32_y;
  crypto_uint32 crypto_uint32_z = crypto_uint32_x - crypto_uint32_y;
  crypto_uint32_z ^= crypto_uint32_r & (crypto_uint32_z ^ crypto_uint32_x ^ (((crypto_uint32) 1) << (32-1)));
  return crypto_uint32_topbit_01(crypto_uint32_z);
#endif
}

__attribute__((unused))
static inline
crypto_uint32 crypto_uint32_leq_mask(crypto_uint32 crypto_uint32_x,crypto_uint32 crypto_uint32_y) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_uint32 crypto_uint32_q,crypto_uint32_z;
  __asm__ ("xorl %0,%0\n movl $-1,%1\n cmpl %3,%2\n cmovbel %1,%0" : "=&r"(crypto_uint32_z), "=&r"(crypto_uint32_q) : "r"(crypto_uint32_x), "r"(crypto_uint32_y) : "cc");
  return crypto_uint32_z;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_uint32 crypto_uint32_z;
  __asm__ ("cmp %w1,%w2\n csetm %w0,ls" : "=r"(crypto_uint32_z) : "r"(crypto_uint32_x), "r"(crypto_uint32_y) : "cc");
  return crypto_uint32_z;
#else
  return ~crypto_uint32_smaller_mask(crypto_uint32_y,crypto_uint32_x);
#endif
}

__attribute__((unused))
static inline
crypto_uint32 crypto_uint32_leq_01(crypto_uint32 crypto_uint32_x,crypto_uint32 crypto_uint32_y) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_uint32 crypto_uint32_q,crypto_uint32_z;
  __asm__ ("xorl %0,%0\n movl $1,%1\n cmpl %3,%2\n cmovbel %1,%0" : "=&r"(crypto_uint32_z), "=&r"(crypto_uint32_q) : "r"(crypto_uint32_x), "r"(crypto_uint32_y) : "cc");
  return crypto_uint32_z;
#elif defined(__GNUC__) && defined(__aarch64__)
  crypto_uint32 crypto_uint32_z;
  __asm__ ("cmp %w1,%w2\n cset %w0,ls" : "=r"(crypto_uint32_z) : "r"(crypto_uint32_x), "r"(crypto_uint32_y) : "cc");
  return crypto_uint32_z;
#else
  return 1-crypto_uint32_smaller_01(crypto_uint32_y,crypto_uint32_x);
#endif
}

__attribute__((unused))
static inline
int crypto_uint32_ones_num(crypto_uint32 crypto_uint32_x) {
  crypto_uint32 crypto_uint32_y = crypto_uint32_x;
  const crypto_uint32 C0 = 0x55555555;
  const crypto_uint32 C1 = 0x33333333;
  const crypto_uint32 C2 = 0x0f0f0f0f;
  crypto_uint32_y -= ((crypto_uint32_y >> 1) & C0);
  crypto_uint32_y = (crypto_uint32_y & C1) + ((crypto_uint32_y >> 2) & C1);
  crypto_uint32_y = (crypto_uint32_y + (crypto_uint32_y >> 4)) & C2;
  crypto_uint32_y += crypto_uint32_y >> 8;
  crypto_uint32_y = (crypto_uint32_y + (crypto_uint32_y >> 16)) & 0xff;
  return crypto_uint32_y;
}

__attribute__((unused))
static inline
int crypto_uint32_bottomzeros_num(crypto_uint32 crypto_uint32_x) {
#if defined(__GNUC__) && defined(__x86_64__)
  crypto_uint32 fallback = 32;
  __asm__ ("bsfl %0,%0\n cmovel %1,%0" : "+&r"(crypto_uint32_x) : "r"(fallback) : "cc");
  return crypto_uint32_x;
#elif defined(__GNUC__) && defined(__aarch64__)
  int64_t crypto_uint32_y;
  __asm__ ("rbit %w0,%w1\n clz %w0,%w0" : "=r"(crypto_uint32_y) : "r"(crypto_uint32_x) : );
  return crypto_uint32_y;
#else
  crypto_uint32 crypto_uint32_y = crypto_uint32_x ^ (crypto_uint32_x-1);
  crypto_uint32_y = ((crypto_uint32_signed) crypto_uint32_y) >> 1;
  crypto_uint32_y &= ~(crypto_uint32_x & (((crypto_uint32) 1) << (32-1)));
  return crypto_uint32_ones_num(crypto_uint32_y);
#endif
}

#endif

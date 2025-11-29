/* Copyright © 2023 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef __GENX_CL_HELPERS_H__
#define __GENX_CL_HELPERS_H__

#define ALWAYS_INLINE inline __attribute__((always_inline))
#define UNUSED
#define BITFIELD64_MASK(bits) ((1ul << bits) - 1)
#define CLAMP( X, MIN, MAX )  ( (X)>(MIN) ? ((X)>(MAX) ? (MAX) : (X)) : (MIN) )
#define INT64_MAX (0x7FFFFFFFFFFFFFFFL)

ALWAYS_INLINE static uint64_t
u_uintN_max(uint32_t bits)
{
   return (1ul << bits) - 1;
}

ALWAYS_INLINE static int64_t
u_intN_max(uint32_t bit_size)
{
   return INT64_MAX >> (64 - bit_size);
}

ALWAYS_INLINE static int64_t
u_intN_min(uint32_t bit_size)
{
   /* On 2's compliment platforms, which is every platform Mesa is likely to
    * every worry about, stdint.h generally calculated INT##_MIN in this
    * manner.
    */
   return (-u_intN_max(bit_size)) - 1;
}

ALWAYS_INLINE static uint64_t
util_bitpack_uint(uint64_t v, uint32_t start, UNUSED uint32_t end)
{
   return v << start;
}

ALWAYS_INLINE static uint64_t
util_bitpack_uint_nonzero(uint64_t v, uint32_t start, uint32_t end)
{
   return util_bitpack_uint(v, start, end);
}

ALWAYS_INLINE static uint64_t
util_bitpack_sint(int64_t v, uint32_t start, uint32_t end)
{
   int32_t bits = end - start + 1;

   uint64_t mask = BITFIELD64_MASK(bits);

   return (v & mask) << start;
}

ALWAYS_INLINE static uint64_t
util_bitpack_sint_nonzero(int64_t v, uint32_t start, uint32_t end)
{
   return util_bitpack_sint(v, start, end);
}

ALWAYS_INLINE static uint32_t
util_bitpack_float(float v)
{
   union { float f; uint32_t dw; } x;
   x.f = v;
   return x.dw;
}

ALWAYS_INLINE static uint32_t
util_bitpack_float_nonzero(float v)
{
   return util_bitpack_float(v);
}

ALWAYS_INLINE static uint64_t
util_bitpack_sfixed(float v, uint32_t start, uint32_t end,
                    uint32_t fract_bits)
{
   float factor = (1 << fract_bits);

   int64_t int_val = round(v * factor);
   uint64_t mask = ~0ul >> (64 - (end - start + 1));

   return (int_val & mask) << start;
}

ALWAYS_INLINE static uint64_t
util_bitpack_sfixed_clamp(float v, uint32_t start, uint32_t end,
                          uint32_t fract_bits)
{
   float factor = (1 << fract_bits);

   uint32_t total_bits = end - start + 1;
   float min = u_intN_min(total_bits) / factor;
   float max = u_intN_max(total_bits) / factor;

   int64_t int_val = round(CLAMP(v, min, max) * factor);
   uint64_t mask = ~0ul >> (64 - (end - start + 1));

   return (int_val & mask) << start;
}

ALWAYS_INLINE static uint64_t
util_bitpack_sfixed_nonzero(float v, uint32_t start, uint32_t end,
                            uint32_t fract_bits)
{
   return util_bitpack_sfixed(v, start, end, fract_bits);
}

ALWAYS_INLINE static uint64_t
util_bitpack_ufixed(float v, uint32_t start, uint32_t end, uint32_t fract_bits)
{
   float factor = (1 << fract_bits);

   uint64_t uint_val = round(v * factor);

   return uint_val << start;
}

ALWAYS_INLINE static uint64_t
util_bitpack_ufixed_clamp(float v, uint32_t start, uint32_t end, uint32_t fract_bits)
{
   float factor = (1 << fract_bits);

   int total_bits = end - start + 1;
   float min = 0.0f;
   float max = u_uintN_max(total_bits) / factor;

   uint64_t uint_val = round(CLAMP(v, min, max) * factor);

   return uint_val << start;
}

ALWAYS_INLINE static uint64_t
util_bitpack_ufixed_nonzero(float v, uint32_t start, uint32_t end,
                            uint32_t fract_bits)
{
   return util_bitpack_ufixed(v, start, end, fract_bits);
}

#ifndef __gen_validate_value
#define __gen_validate_value(x)
#endif

#ifndef __intel_field_functions
#define __intel_field_functions
#endif

static inline __attribute__((always_inline)) uint64_t
__gen_offset(uint64_t v, UNUSED uint32_t start, UNUSED uint32_t end)
{
   return v;
}

static inline __attribute__((always_inline)) uint64_t
__gen_offset_nonzero(uint64_t v, uint32_t start, uint32_t end)
{
   return __gen_offset(v, start, end);
}

static inline __attribute__((always_inline)) uint64_t
__gen_address(uint64_t address,
              __attribute__((unused)) uint32_t start, uint32_t end)
{
   if (end < 63) {
      uint32_t shift = 63 - end;
      return (address << shift) >> shift;
   } else {
      return address;
   }
}

#endif /* __GENX_CL_HELPERS_H__ */

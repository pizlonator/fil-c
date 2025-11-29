/*
 * Copyright © 2024 Collabora, Ltd.
 * SPDX-License-Identifier: MIT
 */
#ifndef MME_BITPACK_HELPERS_H
#define MME_BITPACK_HELPERS_H

#include "util/bitpack_helpers.h"
#include "util/u_math.h"

static inline void
pack_uint(uint32_t *b, unsigned start, unsigned end, uint64_t data)
{
   assert(end >= start);

   uint32_t dw = start / 32;
   start -= dw * 32;
   end -= dw * 32;
   assert(end < 64);

   uint64_t packed = util_bitpack_uint(data, start, end);

   b[dw] |= packed;
   if (end >= 32)
      b[dw + 1] |= packed >> 32;
}

static inline void
pack_sint(uint32_t *b, unsigned start, unsigned end, int64_t data)
{
   assert(end >= start);

   uint32_t dw = start / 32;
   start -= dw * 32;
   end -= dw * 32;
   assert(end < 64);

   uint64_t packed = util_bitpack_sint(data, start, end);

   b[dw] |= packed;
   if (end >= 32)
      b[dw + 1] |= packed >> 32;
}

static inline uint64_t
unpack_uint(const uint32_t *b, unsigned start, unsigned end)
{
   assert(end >= start);

   uint32_t dw = start / 32;
   start -= dw * 32;
   end -= dw * 32;
   assert(end < 64);

   uint64_t packed = b[dw];
   if (end >= 32)
      packed |= (uint64_t)b[dw + 1] << 32;

   packed &= util_bitpack_ones(start, end);

   return packed >> start;
}

static inline uint64_t
unpack_sint(const uint32_t *b, unsigned start, unsigned end)
{
   unsigned bits = end - start + 1;
   return util_sign_extend(unpack_uint(b, start, end), bits);
}

#endif /* MME_BITPACK_HELPERS_H */

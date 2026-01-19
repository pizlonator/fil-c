/* camellia-aarch64-ce.c - ARMv8/CE Camellia implementation
 * Copyright (C) 2023 Jussi Kivilinna <jussi.kivilinna@iki.fi>
 *
 * This file is part of Libgcrypt.
 *
 * Libgcrypt is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * Libgcrypt is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#if defined(__AARCH64EL__) && \
    defined(HAVE_COMPATIBLE_GCC_AARCH64_PLATFORM_AS) && \
    defined(HAVE_GCC_INLINE_ASM_AARCH64_CRYPTO) && \
    defined(HAVE_COMPATIBLE_CC_AARCH64_NEON_INTRINSICS) && \
    (__GNUC__ >= 4)

#ifdef HAVE_GCC_ATTRIBUTE_OPTIMIZE
# define FUNC_ATTR_OPT __attribute__((optimize("-O2")))
#else
# define FUNC_ATTR_OPT
#endif

#define SIMD128_OPT_ATTR FUNC_ATTR_OPT

#define FUNC_ENC_BLK16 _gcry_camellia_aarch64ce_encrypt_blk16
#define FUNC_DEC_BLK16 _gcry_camellia_aarch64ce_decrypt_blk16
#define FUNC_KEY_SETUP _gcry_camellia_aarch64ce_keygen

#include "camellia-simd128.h"

#endif /* __AARCH64EL__ */

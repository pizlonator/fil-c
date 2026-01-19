/* camellia-ppc9le.c - POWER9 Vector Crypto Camellia implementation
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

#if !defined(WORDS_BIGENDIAN) && defined(ENABLE_PPC_CRYPTO_SUPPORT) && \
    defined(HAVE_COMPATIBLE_CC_PPC_ALTIVEC) && \
    defined(HAVE_GCC_INLINE_ASM_PPC_ALTIVEC) && \
    (SIZEOF_UNSIGNED_LONG == 8) && (__GNUC__ >= 4)

#ifdef HAVE_GCC_ATTRIBUTE_OPTIMIZE
# define FUNC_ATTR_OPT __attribute__((optimize("-O2")))
#else
# define FUNC_ATTR_OPT
#endif

#if defined(__clang__) && defined(HAVE_CLANG_ATTRIBUTE_PPC_TARGET)
# define SIMD128_OPT_ATTR __attribute__((target("arch=pwr9"))) FUNC_ATTR_OPT
#elif defined(HAVE_GCC_ATTRIBUTE_PPC_TARGET)
# define SIMD128_OPT_ATTR __attribute__((target("cpu=power9"))) FUNC_ATTR_OPT
#else
# define SIMD128_OPT_ATTR FUNC_ATTR_OPT
#endif

#define FUNC_ENC_BLK16 _gcry_camellia_ppc9_encrypt_blk16
#define FUNC_DEC_BLK16 _gcry_camellia_ppc9_decrypt_blk16
#define FUNC_KEY_SETUP _gcry_camellia_ppc9_keygen

#include "camellia-simd128.h"

#endif /* ENABLE_PPC_CRYPTO_SUPPORT */

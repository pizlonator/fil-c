
/*
 * BZip3 - A spiritual successor to BZip2.
 * Copyright (C) 2022-2024 Kamila Szewczyk
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of  MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _COMMON_H
#define _COMMON_H

#define KiB(x) ((x)*1024)
#define MiB(x) ((x)*1024 * 1024)
#define BWT_BOUND(x) (bz3_bound(x) + 128)

#include <inttypes.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;

static s32 read_neutral_s32(const u8 * data) {
    return ((u32)data[0]) | (((u32)data[1]) << 8) | (((u32)data[2]) << 16) | (((u32)data[3]) << 24);
}

static void write_neutral_s32(u8 * data, s32 value) {
    data[0] = value & 0xFF;
    data[1] = (value >> 8) & 0xFF;
    data[2] = (value >> 16) & 0xFF;
    data[3] = (value >> 24) & 0xFF;
}

#if defined(__GNUC__) || defined(__clang__)
    #define RESTRICT __restrict__
#elif defined(_MSC_VER) || defined(__INTEL_COMPILER)
    #define RESTRICT __restrict
#else
    #define RESTRICT restrict
    #warning Your compiler, configuration or platform might not be supported.
#endif

#if defined(__has_builtin)
    #if __has_builtin(__builtin_prefetch)
        #define HAS_BUILTIN_PREFETCH
    #endif
#elif defined(__GNUC__) && (((__GNUC__ == 3) && (__GNUC_MINOR__ >= 2)) || (__GNUC__ >= 4))
    #define HAS_BUILTIN_PREFETCH
#endif

#if defined(__has_builtin)
    #if __has_builtin(__builtin_bswap16)
        #define HAS_BUILTIN_BSWAP16
    #endif
#elif defined(__GNUC__) && (((__GNUC__ == 4) && (__GNUC_MINOR__ >= 8)) || (__GNUC__ >= 5))
    #define HAS_BUILTIN_BSWAP16
#endif

#if defined(HAS_BUILTIN_PREFETCH)
    #define prefetch(address) __builtin_prefetch((const void *)(address), 0, 0)
    #define prefetchw(address) __builtin_prefetch((const void *)(address), 1, 0)
#elif defined(_M_IX86) || defined(_M_AMD64) || defined(__x86_64__) || defined(i386) || defined(__i386__) || \
    defined(__i386)
    #include <intrin.h>
    #define prefetch(address) _mm_prefetch((const void *)(address), _MM_HINT_NTA)
    #define prefetchw(address) _m_prefetchw((const void *)(address))
#elif defined(_M_ARM) || defined(__ARM_ARCH_7__) || defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__) || \
    defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7S__)
    #include <intrin.h>
    #define prefetch(address) __prefetch((const void *)(address))
    #define prefetchw(address) __prefetchw((const void *)(address))
#elif defined(_M_ARM64) || defined(__aarch64__)
    #include <intrin.h>
    #define prefetch(address) __prefetch2((const void *)(address), 1)
    #define prefetchw(address) __prefetch2((const void *)(address), 17)
#else
    #error Your compiler, configuration or platform is not supported.
#endif

#if !defined(__LITTLE_ENDIAN__) && !defined(__BIG_ENDIAN__)
    #if defined(_LITTLE_ENDIAN) || (defined(BYTE_ORDER) && defined(LITTLE_ENDIAN) && BYTE_ORDER == LITTLE_ENDIAN) || \
        (defined(_BYTE_ORDER) && defined(_LITTLE_ENDIAN) && _BYTE_ORDER == _LITTLE_ENDIAN) ||                        \
        (defined(__BYTE_ORDER) && defined(__LITTLE_ENDIAN) && __BYTE_ORDER == __LITTLE_ENDIAN) ||                    \
        (defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
        #define __LITTLE_ENDIAN__
    #elif defined(_BIG_ENDIAN) || (defined(BYTE_ORDER) && defined(BIG_ENDIAN) && BYTE_ORDER == BIG_ENDIAN) || \
        (defined(_BYTE_ORDER) && defined(_BIG_ENDIAN) && _BYTE_ORDER == _BIG_ENDIAN) ||                       \
        (defined(__BYTE_ORDER) && defined(__BIG_ENDIAN) && __BYTE_ORDER == __BIG_ENDIAN) ||                   \
        (defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
        #define __BIG_ENDIAN__
    #elif defined(_WIN32)
        #define __LITTLE_ENDIAN__
    #endif
#endif

#if defined(__LITTLE_ENDIAN__) && !defined(__BIG_ENDIAN__)
    #if defined(HAS_BUILTIN_BSWAP16)
        #define bswap16(x) (__builtin_bswap16(x))
    #elif defined(_MSC_VER) && !defined(__INTEL_COMPILER)
        #define bswap16(x) (_byteswap_ushort(x))
    #else
        #define bswap16(x) ((u16)(x >> 8) | (u16)(x << 8))
    #endif
#elif !defined(__LITTLE_ENDIAN__) && defined(__BIG_ENDIAN__)
    #define bswap16(x) (x)
#else
    #error Your compiler, configuration or platform is not supported.
#endif

#endif

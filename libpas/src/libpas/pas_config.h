/*
 * Copyright (c) 2018-2021 Apple Inc. All rights reserved.
 * Copyright (c) 2023 Epic Games, Inc. All Rights Reserved.
 * Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef PAS_CONFIG_H
#define PAS_CONFIG_H

#include "pas_platform.h"

#define LIBPAS_ENABLED 1

#if defined(PAS_BMALLOC)
#include "BPlatform.h"
#if !BENABLE(LIBPAS)
#undef LIBPAS_ENABLED
#define LIBPAS_ENABLED 0
#endif
#endif

#if defined(ENABLE_PAS_TESTING)
#define PAS_ENABLE_TESTING 1
#else
#define PAS_ENABLE_TESTING 0
#endif

#ifndef __PAS_ARM64
#define __PAS_ARM64 0
#endif

#ifndef __PAS_ARM64E
#define __PAS_ARM64E 0
#endif

#if (defined(__arm64__) && defined(__APPLE__)) || defined(__aarch64__) || defined(__arm64e__)
#define PAS_ARM64 1
#if defined(__arm64e__)
#define PAS_ARM64E 1
#else
#define PAS_ARM64E 0
#endif
#else
#define PAS_ARM64 0
#define PAS_ARM64E 0
#endif

#if (defined(arm) || defined(__arm__) || defined(ARM) || defined(_ARM_)) && !__PAS_ARM64
#define PAS_ARM32 1
#else
#define PAS_ARM32 0
#endif

#define PAS_ARM (!!PAS_ARM64 || !!PAS_ARM32)

#if defined(__i386__) || defined(i386) || defined(_M_IX86) || defined(_X86_) || defined(__THW_INTEL)
#define PAS_X86 1
#else
#define PAS_X86 0
#endif

#if defined(__x86_64__) || defined(_M_X64)
#define PAS_X86_64 1
#else
#define PAS_X86_64 0
#endif

#if defined(__riscv)
#define PAS_RISCV 1
#else
#define PAS_RISCV 0
#endif

#define PAS_PTR_SIZE                     8
#define PAS_PAIR_SIZE                    16

#define PAS_ADDRESS_BITS                 48
#define PAS_MAX_ADDRESS                  (((uintptr_t)1 << PAS_ADDRESS_BITS) - 1)
#define PAS_ADDRESS_MASK                 PAS_MAX_ADDRESS

#if (PAS_ARM && defined(__APPLE__)) || PAS_PLATFORM(PLAYSTATION)
#define PAS_MAX_GRANULES                 256
#else
#define PAS_MAX_GRANULES                 1024
#endif

#define PAS_INTERNAL_MIN_ALIGN_SHIFT     3
#define PAS_INTERNAL_MIN_ALIGN           ((size_t)1 << PAS_INTERNAL_MIN_ALIGN_SHIFT)

#if defined(PAS_BMALLOC)
#define PAS_ENABLE_THINGY                0
#define PAS_ENABLE_ISO                   0
#define PAS_ENABLE_ISO_TEST              0
#define PAS_ENABLE_MINALIGN32            0
#define PAS_ENABLE_PAGESIZE64K           0
#define PAS_ENABLE_BMALLOC               1
#define PAS_ENABLE_HOTBIT                0
#define PAS_ENABLE_JIT                   1
#define PAS_ENABLE_VERSE                 0
#define PAS_ENABLE_INLINE_MEDIUM_PAGE_HEADER 0
#define PAS_ENABLE_OUTLINE_MEDIUM_PAGE_HEADER 0
#define PAS_ENABLE_INLINE_NON_COMMITTABLE_GRANULES 0
#define PAS_ENABLE_OUTLINE_NON_COMMITTABLE_GRANULES 0
#define PAS_ENABLE_FILC                  0
#elif defined(PAS_LIBMALLOC)
#define PAS_ENABLE_THINGY                0
#define PAS_ENABLE_ISO                   1
#define PAS_ENABLE_ISO_TEST              0
#define PAS_ENABLE_MINALIGN32            0
#define PAS_ENABLE_PAGESIZE64K           0
#define PAS_ENABLE_BMALLOC               0
#define PAS_ENABLE_HOTBIT                0
#define PAS_ENABLE_JIT                   0
#define PAS_ENABLE_VERSE                 0
#define PAS_ENABLE_INLINE_MEDIUM_PAGE_HEADER 0
#define PAS_ENABLE_OUTLINE_MEDIUM_PAGE_HEADER 0
#define PAS_ENABLE_INLINE_NON_COMMITTABLE_GRANULES 0
#define PAS_ENABLE_OUTLINE_NON_COMMITTABLE_GRANULES 0
#define PAS_ENABLE_FILC                  0
#elif defined(PAS_FILC)
#define PAS_ENABLE_THINGY                0
#define PAS_ENABLE_ISO                   0
#define PAS_ENABLE_ISO_TEST              0
#define PAS_ENABLE_MINALIGN32            0
#define PAS_ENABLE_PAGESIZE64K           0
#define PAS_ENABLE_BMALLOC               1
#define PAS_ENABLE_HOTBIT                0
/* Fil-C enables the JIT heap so -enable-filc-jit processes have executable
   heap memory available. The LLVM JIT shim's Initialize mmaps a 128MB RWX
   range and hands it to jit_heap_add_fresh_memory (directly when that symbol
   is visible from the shim, otherwise relayed by the runtime through
   FilLLVMJIT_GetHeapMemory); the runtime's JIT init asserts this heap is
   compiled in. */
#define PAS_ENABLE_JIT                   1
#define PAS_ENABLE_VERSE                 1
#define PAS_ENABLE_INLINE_MEDIUM_PAGE_HEADER 0
#define PAS_ENABLE_OUTLINE_MEDIUM_PAGE_HEADER 0
#define PAS_ENABLE_INLINE_NON_COMMITTABLE_GRANULES 0
#define PAS_ENABLE_OUTLINE_NON_COMMITTABLE_GRANULES 0
#define PAS_ENABLE_FILC                  1
#else /* libpas standalone testing library */
#define PAS_ENABLE_THINGY                1
#define PAS_ENABLE_ISO                   1
#define PAS_ENABLE_ISO_TEST              1
#define PAS_ENABLE_MINALIGN32            1
#define PAS_ENABLE_PAGESIZE64K           1
#define PAS_ENABLE_BMALLOC               1
#define PAS_ENABLE_HOTBIT                1
#define PAS_ENABLE_JIT                   1
#define PAS_ENABLE_VERSE                 1
#define PAS_ENABLE_INLINE_MEDIUM_PAGE_HEADER 1
#define PAS_ENABLE_OUTLINE_MEDIUM_PAGE_HEADER 1
#define PAS_ENABLE_INLINE_NON_COMMITTABLE_GRANULES 1
#define PAS_ENABLE_OUTLINE_NON_COMMITTABLE_GRANULES 1
#define PAS_ENABLE_FILC                  0 /* Fil-C code only works in the Fil-C build. */
#endif

#define PAS_COMPACT_PTR_SIZE             3
#define PAS_COMPACT_PTR_BITS             (PAS_COMPACT_PTR_SIZE << 3)
#define PAS_COMPACT_PTR_MASK             ((uintptr_t)(((uint64_t)1 \
                                                       << (PAS_COMPACT_PTR_BITS & 63)) - 1))

#if PAS_OS(DARWIN) || defined(_WIN32) || PAS_OS(LINUX)
#define PAS_USE_SPINLOCKS                0
#else
#define PAS_USE_SPINLOCKS                1
#endif

#endif /* PAS_CONFIG_H */


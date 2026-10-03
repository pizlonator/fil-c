// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// zegarmistrz guest client library (header-only C).
//
// zegarmistrz reveals its presence via the CPUID hypervisor convention
// (see cpuid.txt in the repo root):
//
//   1. CPUID.1:ECX bit 31 — the "hypervisor present" bit. Real silicon
//      keeps it zero; the emulator sets it.
//   2. CPUID.0x40000000 (the software-reserved hypervisor leaf range)
//      returns the max hypervisor leaf in EAX (0x40000000) and the
//      12-byte vendor signature "Zegarmistrz\0" in EBX:ECX:EDX.
//
// Detection is deliberately uncached: is_in_zegarmistrz() performs both
// CPUID instructions (inline assembly, always both, on every call).
//
// Service requests are issued by CALLing the magic address
// 0x1410141014101410 as
//
//     ((uint64_t (*)(uint64_t op, void* ptr, size_t size, uint64_t flags))
//          0x1410141014101410)(op, ptr, size, flags)
//
// with RDI=op, RSI=ptr, RDX=size, RCX=flags (SysV AMD64) and the result in
// RAX:
//
//   op 0: no-op/query, returns 0.
//   op 1: poison [ptr, ptr+size) with the flags bitmask (see the
//         ZEGARMISTRZ_POISON_* bits), returns 0.
//   op 2: unpoison [ptr, ptr+size), returns 0.
//
// The service-call helpers below are no-ops when not running under
// zegarmistrz. They are also written compiler-defensively: inline asm
// opaques the call target and arguments, so the compiler can never see
// that we are (from its point of view) calling a bogus pointer, and can
// neither reject, elide, nor optimize around the call. Note this library
// is for yolo C guests, not Fil-C (the magic call is not a real function).

#ifndef ZEGARMISTRZ_H
#define ZEGARMISTRZ_H

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
#define ZEGARMISTRZ_BOOL bool
#else
#define ZEGARMISTRZ_BOOL _Bool
#endif

// CPUID hypervisor signature (see cpuid.txt): CPUID.0x40000000 puts the
// 12-byte vendor string "Zegarmistrz\0" in EBX:ECX:EDX (little-endian
// 32-bit words, in that order).
#define ZEGARMISTRZ_HYPERVISOR_LEAF 0x40000000u
#define ZEGARMISTRZ_VENDOR_EBX 0x6167655Au /* "Zega" */
#define ZEGARMISTRZ_VENDOR_ECX 0x73696D72u /* "rmis" */
#define ZEGARMISTRZ_VENDOR_EDX 0x007A7274u /* "trz\0" */

// Magic client-request call target.
#define ZEGARMISTRZ_MAGIC_CALL_ADDR 0x1410141014101410ULL

// Poisoning flags (op 1); matches the emulator's POISON_* bitmask.
#define ZEGARMISTRZ_POISON_CANNOT_LOAD 0x1u
#define ZEGARMISTRZ_POISON_CANNOT_STORE 0x2u
#define ZEGARMISTRZ_POISON_CANNOT_BRANCH 0x4u
#define ZEGARMISTRZ_POISON_CANNOT_INDEX 0x8u
#define ZEGARMISTRZ_POISON_CLEAR_BRANCH_ON_STORE 0x10u
#define ZEGARMISTRZ_POISON_CLEAR_INDEX_ON_STORE 0x20u

// Detects zegarmistrz using the two-CPUID technique from cpuid.txt: gate
// on CPUID.1:ECX bit 31 ("hypervisor present"), then match the vendor
// signature at CPUID.0x40000000. No caching: both CPUID instructions run
// (volatile inline assembly) on every call; only the final AND decides.
static inline ZEGARMISTRZ_BOOL is_in_zegarmistrz(void) {
#if defined(__x86_64__) || defined(__amd64__)
    unsigned a, b, c, d;
    // Call 1: the gate. On real silicon, bit 31 of ECX is reserved zero.
    __asm__ __volatile__("cpuid"
                         : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
                         : "a"(1u), "c"(0u));
    unsigned hypervisor_present = (c >> 31) & 1u;
    // Call 2: the signature. Always executed, even if the gate is clear.
    __asm__ __volatile__("cpuid"
                         : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
                         : "a"(ZEGARMISTRZ_HYPERVISOR_LEAF), "c"(0u));
    return (ZEGARMISTRZ_BOOL)(hypervisor_present && b == ZEGARMISTRZ_VENDOR_EBX &&
                              c == ZEGARMISTRZ_VENDOR_ECX && d == ZEGARMISTRZ_VENDOR_EDX);
#else
    return 0;
#endif
}

// Raw client request (op 0 = query, 1 = poison, 2 = unpoison). Returns the
// RAX result of the magic call, or 0 without doing anything when not
// running under zegarmistrz.
static inline uint64_t zegarmistrz_client_request(uint64_t op, void* ptr,
                                                  size_t size,
                                                  uint64_t flags) {
    if (!is_in_zegarmistrz())
        return 0;
    uint64_t (*request)(uint64_t, void*, size_t, uint64_t) =
        (uint64_t (*)(uint64_t, void*, size_t, uint64_t))
            (uintptr_t)ZEGARMISTRZ_MAGIC_CALL_ADDR;
    // Compiler defense: the opaquing asm below makes the compiler forget
    // everything it knows about the call target and the arguments, so it
    // cannot discover that "request" is a bogus pointer and can neither
    // reject, elide, nor reason about the call.
    __asm__ __volatile__("" : "+r"(request), "+r"(op), "+r"(ptr),
                         "+r"(size), "+r"(flags));
    uint64_t ret = request(op, ptr, size, flags);
    // Second defense: an opaque asm consumes the result, so the compiler
    // cannot turn the call into a tail call (jmp *target). The magic
    // address is only meaningful as a *CALL* target — the emulator
    // intercepts CALL there (RIP advances past the call, no push), while a
    // jmp would just fault. The barrier must run after the call returns,
    // which forces a real call + return at every optimization level.
    __asm__ __volatile__("" : "+r"(ret) : : "memory");
    return ret;
}

// op 0: no-op/query. Returns 0.
static inline uint64_t zegarmistrz_query(void) {
    return zegarmistrz_client_request(0, NULL, 0, 0);
}

// op 1: poison each byte of [ptr, ptr+size) with the flags bitmask
// (ZEGARMISTRZ_POISON_*). Returns 0. No-op when not under zegarmistrz.
static inline uint64_t zegarmistrz_poison_range(void* ptr, size_t size,
                                                unsigned flags) {
    return zegarmistrz_client_request(1, ptr, size, (uint64_t)flags);
}

// op 2: unpoison [ptr, ptr+size). Returns 0. No-op when not under
// zegarmistrz.
static inline uint64_t zegarmistrz_unpoison_range(void* ptr, size_t size) {
    return zegarmistrz_client_request(2, ptr, size, 0);
}

#endif // ZEGARMISTRZ_H

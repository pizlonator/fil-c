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
// tests/shufpd_sdm.c -- the AVX-512 SDM-conformance battery for the
// perm AND convert AND ternlog families (masked VSHUFPD and masked
// VPSHUFD, the {1toN} embedded broadcasts including the REAL
// vpternlogq m512{1to8} form, masked+broadcast VPERMILPD, the EVEX
// VPERMILPD/PS imm forms, the widening/narrowing converts whose mask and
// broadcast granularity follow the SOURCE element, the masked
// VPTERNLOGQ/VPTERNLOGD merges whose granularity is per-mnemonic, and the
// VPERMQ/VPERMPD vector-control (variable-index) forms whose index vector
// is the vvvv operand, whose data is the rm operand, and whose low-3-bit
// index elements select GLOBALLY across the whole source -- plus the imm8
// forms and the VPERMD kind-2 pin that share the rule book).
//
// EMULATOR-ONLY: this guest is NEVER run natively by the suite (the
// run-tests case `shufpd-sdm` executes it only under ./zegarmistrz and
// requires exit 0 + the shufpd-sdm-ok line + the executed-probe count
// the guest reports on stderr).  The host class this suite must pass on
// has no AVX-512, so a native run of any EVEX instruction would SIGILL
// -- the emulator implements AVX-512 in software on ANY host, which is
// the entire point of running these probes under it.  Everything here is
// therefore gated on is_in_zegarmistrz() (a manual native run does
// nothing and exits 0); the have_avx512f()-gated EVEX.512 VSHUFPD imm
// group below keeps its CPUID gate so it still runs under the emulator
// on hosts that DO have AVX-512, exactly as it did before this file was
// split out of tests/shufpd.c.  Compile-time -mavx512f -mavx512dq are
// needed only for the {k1}/{1toN} inline-asm brace decorations; because
// this file is never natively executed, gcc auto-vectorizing any of its
// C with 512-bit instructions is harmless (see tests/shufpd.c for the
// native-leg version of that story).

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "zegarmistrz.h"
static int failures = 0;
static int sdm_probes = 0;
#define CHECK(cond, name) do { \
    if (!(cond)) { printf("FAIL %s\n", (name)); failures++; } \
} while (0)

static void cpuid(unsigned leaf, unsigned sub, unsigned out[4]) {
    asm volatile("cpuid" : "=a"(out[0]), "=b"(out[1]), "=c"(out[2]), "=d"(out[3])
                 : "a"(leaf), "c"(sub));
}
static int have_avx(void) {
    unsigned f[4];
    cpuid(1, 0, f);
    return (f[2] >> 28) & 1;
}
static int have_avx512f(void) {
    unsigned f[4];
    cpuid(7, 0, f);
    return (f[1] >> 16) & 1;
}

/* the two source vectors, with distinct qwords in both halves of each lane */
static const unsigned long long A[8] = {0xAAAAAAAAAAAAAAAAULL, 0xA1A2A3A4A5A6A7A8ULL,
                                        0xBBBBBBBBBBBBBBBBULL, 0xB1B2B3B4B5B6B7B8ULL,
                                        0xA0A0A0A0A0A0A0A0ULL, 0xA9A8A7A6A5A4A3A2ULL,
                                        0xB0B0B0B0B0B0B0B0ULL, 0xB9B8B7B6B5B4B3B2ULL};
static const unsigned long long B[8] = {0xCCCCCCCCCCCCCCCCULL, 0xC1C2C3C4C5C6C7C8ULL,
                                        0xDDDDDDDDDDDDDDDDULL, 0xD1D2D3D4D5D6D7D8ULL,
                                        0xC0C0C0C0C0C0C0C0ULL, 0xC9C8C7C6C5C4C3C2ULL,
                                        0xD0D0D0D0D0D0D0D0ULL, 0xD9D8D7D6D5D4D3D2ULL};
/* VSHUFPD expected qwords: output qword 2L (even) selects WITHIN src1=A by
 * imm bit 2L; output qword 2L+1 (odd) selects WITHIN src2=B by bit 2L+1. */
static unsigned long long exp_pd_q(unsigned imm, unsigned lane, unsigned odd) {
    const unsigned long long *src = odd ? B : A;
    unsigned sel = (imm >> (2 * lane + odd)) & 1;
    return src[2 * lane + sel];
}
/* VSHUFPS expected dword: dwords 0/1 of each lane select within src1=A by
 * imm bits [1:0]/[3:2]; dwords 2/3 select within src2=B by bits [5:4]/[7:6].
 * The bit pairs restart per lane. */
static unsigned int exp_ps_d(unsigned imm, unsigned lane, unsigned i) {
    unsigned sel = (imm >> (2 * i)) & 3;
    unsigned half = (i >= 2);
    const unsigned int *src = half ? (const unsigned int *)B : (const unsigned int *)A;
    return src[4 * lane + 2 * (sel >> 1) + (sel & 1)];
}
/* ---- AVX-512 SDM-conformance probes (emulator-only; see above) ----
 * Each probe is a noinline basic-asm function: the ABI boundary keeps the
 * clobbered zmm/k/rax state away from the compiler's register allocator
 * (zmm0-31 and k0-7 are caller-saved), and basic-asm templates pass the
 * {k1}/{1to8} brace decorations through to the assembler verbatim --
 * extended-asm templates would strip them as gcc dialect markers, so the
 * exact EVEX forms cannot be spelled there.  The memory markers are
 * referenced from basic asm by name, hence the "used" attribute (gcc would
 * otherwise drop them as unreferenced).
 * GOTCHA: a body consisting ONLY of basic asm makes GCC's -fipa-ra conclude
 * the probe clobbers NO caller-saved registers, so code GCC schedules
 * across a probe call may reuse stale PRE-call register values -- observed
 * as a rep-stos zero-fill of an expected-value array scheduled after the
 * call being filled with the probe's leftover %eax (the kmovd immediate)
 * instead of 0.  Build the expected-value arrays BEFORE the probe call:
 * the call's implicit memory clobber keeps them there. */

#define SDM_CANARY_Q0 0x1111111111111111ULL
#define SDM_CANARY_Q1 0x2222222222222222ULL
#define SDM_CANARY_Q2 0x3333333333333333ULL
#define SDM_CANARY_Q3 0x4444444444444444ULL
#define SDM_CANARY_Q4 0x5555555555555555ULL
#define SDM_CANARY_Q5 0x6666666666666666ULL
#define SDM_CANARY_Q6 0x7777777777777777ULL
#define SDM_CANARY_Q7 0x8888888888888888ULL

__attribute__((used, aligned(64)))
static const unsigned long long sdm_canary[8] = {
    SDM_CANARY_Q0, SDM_CANARY_Q1, SDM_CANARY_Q2, SDM_CANARY_Q3,
    SDM_CANARY_Q4, SDM_CANARY_Q5, SDM_CANARY_Q6, SDM_CANARY_Q7};

/* {1to8} broadcast source: eight DISTINCT marker qwords.  A correct
 * broadcast reads ONLY the first qword; a sequential full-vector read (the
 * old emulator bug) pulls all eight into the result and fails loudly. */
__attribute__((used, aligned(64)))
static const unsigned long long sdm_barr[8] = {
    0x0BAD0BAD0BAD0001ULL, 0x0BAD0BAD0BAD0002ULL, 0x0BAD0BAD0BAD0003ULL,
    0x0BAD0BAD0BAD0004ULL, 0x0BAD0BAD0BAD0005ULL, 0x0BAD0BAD0BAD0006ULL,
    0x0BAD0BAD0BAD0007ULL, 0x0BAD0BAD0BAD0008ULL};

/* {1to16} broadcast source: sixteen distinct marker dwords. */
__attribute__((used, aligned(64)))
static const unsigned int sdm_barr32[16] = {
    0x1BAD0001u, 0x1BAD0002u, 0x1BAD0003u, 0x1BAD0004u, 0x1BAD0005u,
    0x1BAD0006u, 0x1BAD0007u, 0x1BAD0008u, 0x1BAD0009u, 0x1BAD000au,
    0x1BAD000bu, 0x1BAD000cu, 0x1BAD000du, 0x1BAD000eu, 0x1BAD000fu,
    0x1BAD0010u};

/* VPERMILPD {1to8} broadcast CONTROL vector: qword 0 = 2 (bit 1 set, bit 0
 * clear).  A correct broadcast uses this one qword as the control for every
 * dest qword (sel = 1 -> odd member of each src pair); the old emulator's
 * sequential read would use per-qword controls (bit 1 clear on the odd
 * markers) and the old bit-0 rule would read sel = 0 -- both fail loudly. */
__attribute__((used, aligned(64)))
static const unsigned long long sdm_barr2[8] = {
    0x0000000000000002ULL, 0x0000000000000003ULL, 0x0000000000000002ULL,
    0x0000000000000003ULL, 0x0000000000000002ULL, 0x0000000000000003ULL,
    0x0000000000000002ULL, 0x0000000000000003ULL};

__attribute__((noinline, used)) static void
sdm_shufpd_masked(unsigned long long *out) {
    /* vshufpd $0xAA, %zmm2, %zmm1, %zmm0{%k1} with k1 = 0x0D: writes qwords
     * 0/2/3 only, selecting src1.q0 / src2.q1 / src1.q2 / src2.q3 / ... by
     * the GLOBAL imm bits.  Byte 62 f1 f5 49 c6 c2 aa. */
    asm volatile(
        "movl $0x0d, %eax\n\t"
        "kmovd %eax, %k1\n\t"
        "vmovups sdm_canary(%rip), %zmm0\n\t"
        "vmovups A(%rip), %zmm1\n\t"
        "vmovups B(%rip), %zmm2\n\t"
        "vshufpd $0xAA, %zmm2, %zmm1, %zmm0{%k1}\n\t"
        "vmovups %zmm0, (%rdi)\n\t");

    /* IPA-RA barrier: the basic-asm body above really kills the ABI
     * caller-saved GPRs (rax holds the kmovd immediate, rdi the out
     * pointer, rcx/rdx/rsi/r8-r11 the vmovups/vperm scratch) AND every
     * vector register (zmm0 is the probe's staging vector, zmm1/zmm2 the
     * sources, and the k-register loads go through eax), but basic asm
     * tells the compiler nothing, so -fipa-ra concludes this probe
     * clobbers NO caller-saved register and lets the CALLER keep values
     * in those registers across the call -- observed as an
     * expected-array store scheduled after the call being filled with
     * the probe's leftover %eax (the kmovd immediate) instead of its
     * constant, and as a (double)sdm_cvt_f32[0] value hoisted into an
     * xmm that the probe then trashed.  Declaring the clobbers ("xmmN"
     * covers the full zmm under AVX) makes every probe call a real
     * register kill, so the caller cannot reuse stale post-call
     * register contents; this is deterministic across gcc versions and
     * optimization levels (the split-file -O2 build tripped it in five
     * checks the old single-file -O3 build had dodged by layout luck). */
    asm volatile("" :::
                 "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10",
                 "r11",
                 "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
                 "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12",
                 "xmm13", "xmm14", "xmm15",
                 "memory");
}

__attribute__((noinline, used)) static void
sdm_shufpd_maskedz(unsigned long long *out) {
    /* Same, with {z}: masked-off lanes must be ZERO, not old dest. */
    asm volatile(
        "movl $0x0d, %eax\n\t"
        "kmovd %eax, %k1\n\t"
        "vmovups sdm_canary(%rip), %zmm0\n\t"
        "vmovups A(%rip), %zmm1\n\t"
        "vmovups B(%rip), %zmm2\n\t"
        "vshufpd $0xAA, %zmm2, %zmm1, %zmm0{%k1}{z}\n\t"
        "vmovups %zmm0, (%rdi)\n\t");

    /* IPA-RA barrier: the basic-asm body above really kills the ABI
     * caller-saved GPRs (rax holds the kmovd immediate, rdi the out
     * pointer, rcx/rdx/rsi/r8-r11 the vmovups/vperm scratch) AND every
     * vector register (zmm0 is the probe's staging vector, zmm1/zmm2 the
     * sources, and the k-register loads go through eax), but basic asm
     * tells the compiler nothing, so -fipa-ra concludes this probe
     * clobbers NO caller-saved register and lets the CALLER keep values
     * in those registers across the call -- observed as an
     * expected-array store scheduled after the call being filled with
     * the probe's leftover %eax (the kmovd immediate) instead of its
     * constant, and as a (double)sdm_cvt_f32[0] value hoisted into an
     * xmm that the probe then trashed.  Declaring the clobbers ("xmmN"
     * covers the full zmm under AVX) makes every probe call a real
     * register kill, so the caller cannot reuse stale post-call
     * register contents; this is deterministic across gcc versions and
     * optimization levels (the split-file -O2 build tripped it in five
     * checks the old single-file -O3 build had dodged by layout luck). */
    asm volatile("" :::
                 "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10",
                 "r11",
                 "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
                 "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12",
                 "xmm13", "xmm14", "xmm15",
                 "memory");
}

__attribute__((noinline, used)) static void
sdm_shufpd_bcast(unsigned long long *out) {
    /* vshufpd $0x00, m64{1to8}, %zmm1, %zmm0: imm 0 keeps the even qwords in
     * src1; every odd qword is the SINGLE broadcast element barr[0]. */
    asm volatile(
        "vmovups A(%rip), %zmm1\n\t"
        "vshufpd $0x00, sdm_barr(%rip){1to8}, %zmm1, %zmm0\n\t"
        "vmovups %zmm0, (%rdi)\n\t");

    /* IPA-RA barrier: the basic-asm body above really kills the ABI
     * caller-saved GPRs (rax holds the kmovd immediate, rdi the out
     * pointer, rcx/rdx/rsi/r8-r11 the vmovups/vperm scratch) AND every
     * vector register (zmm0 is the probe's staging vector, zmm1/zmm2 the
     * sources, and the k-register loads go through eax), but basic asm
     * tells the compiler nothing, so -fipa-ra concludes this probe
     * clobbers NO caller-saved register and lets the CALLER keep values
     * in those registers across the call -- observed as an
     * expected-array store scheduled after the call being filled with
     * the probe's leftover %eax (the kmovd immediate) instead of its
     * constant, and as a (double)sdm_cvt_f32[0] value hoisted into an
     * xmm that the probe then trashed.  Declaring the clobbers ("xmmN"
     * covers the full zmm under AVX) makes every probe call a real
     * register kill, so the caller cannot reuse stale post-call
     * register contents; this is deterministic across gcc versions and
     * optimization levels (the split-file -O2 build tripped it in five
     * checks the old single-file -O3 build had dodged by layout luck). */
    asm volatile("" :::
                 "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10",
                 "r11",
                 "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
                 "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12",
                 "xmm13", "xmm14", "xmm15",
                 "memory");
}

__attribute__((noinline, used)) static void
sdm_pshufd_bcast(unsigned int *out) {
    /* vpshufd $0xE4, m512{1to16}, %zmm0: identity dword shuffle; the
     * broadcast replicates barr32[0] into all sixteen dwords. */
    asm volatile(
        "vpshufd $0xE4, sdm_barr32(%rip){1to16}, %zmm0\n\t"
        "vmovups %zmm0, (%rdi)\n\t");

    /* IPA-RA barrier: the basic-asm body above really kills the ABI
     * caller-saved GPRs (rax holds the kmovd immediate, rdi the out
     * pointer, rcx/rdx/rsi/r8-r11 the vmovups/vperm scratch) AND every
     * vector register (zmm0 is the probe's staging vector, zmm1/zmm2 the
     * sources, and the k-register loads go through eax), but basic asm
     * tells the compiler nothing, so -fipa-ra concludes this probe
     * clobbers NO caller-saved register and lets the CALLER keep values
     * in those registers across the call -- observed as an
     * expected-array store scheduled after the call being filled with
     * the probe's leftover %eax (the kmovd immediate) instead of its
     * constant, and as a (double)sdm_cvt_f32[0] value hoisted into an
     * xmm that the probe then trashed.  Declaring the clobbers ("xmmN"
     * covers the full zmm under AVX) makes every probe call a real
     * register kill, so the caller cannot reuse stale post-call
     * register contents; this is deterministic across gcc versions and
     * optimization levels (the split-file -O2 build tripped it in five
     * checks the old single-file -O3 build had dodged by layout luck). */
    asm volatile("" :::
                 "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10",
                 "r11",
                 "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
                 "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12",
                 "xmm13", "xmm14", "xmm15",
                 "memory");
}

__attribute__((noinline, used)) static void
sdm_vpermilpd_bcast_masked(unsigned long long *out) {
    /* vpermilpd m64{1to8}, %zmm1, %zmm0{%k1} with k1 = 0x33: writes qwords
     * 0/1/4/5.  Control = the broadcast qword 2 (bit 1 set) -> every written
     * dest qword takes the ODD member of its src pair; the unwritten lanes
     * keep the canary (qword-granular merge). */
    asm volatile(
        "movl $0x33, %eax\n\t"
        "kmovd %eax, %k1\n\t"
        "vmovups sdm_canary(%rip), %zmm0\n\t"
        "vmovups A(%rip), %zmm1\n\t"
        "vpermilpd sdm_barr2(%rip){1to8}, %zmm1, %zmm0{%k1}\n\t"
        "vmovups %zmm0, (%rdi)\n\t");

    /* IPA-RA barrier: the basic-asm body above really kills the ABI
     * caller-saved GPRs (rax holds the kmovd immediate, rdi the out
     * pointer, rcx/rdx/rsi/r8-r11 the vmovups/vperm scratch) AND every
     * vector register (zmm0 is the probe's staging vector, zmm1/zmm2 the
     * sources, and the k-register loads go through eax), but basic asm
     * tells the compiler nothing, so -fipa-ra concludes this probe
     * clobbers NO caller-saved register and lets the CALLER keep values
     * in those registers across the call -- observed as an
     * expected-array store scheduled after the call being filled with
     * the probe's leftover %eax (the kmovd immediate) instead of its
     * constant, and as a (double)sdm_cvt_f32[0] value hoisted into an
     * xmm that the probe then trashed.  Declaring the clobbers ("xmmN"
     * covers the full zmm under AVX) makes every probe call a real
     * register kill, so the caller cannot reuse stale post-call
     * register contents; this is deterministic across gcc versions and
     * optimization levels (the split-file -O2 build tripped it in five
     * checks the old single-file -O3 build had dodged by layout luck). */
    asm volatile("" :::
                 "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10",
                 "r11",
                 "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
                 "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12",
                 "xmm13", "xmm14", "xmm15",
                 "memory");
}

/* ---- EVEX widening/narrowing convert probes ----
 * The {1to8} broadcast element of a WIDENING convert is the SOURCE element
 * (4-byte int32/float), not the dest double: these probes fail loudly on
 * the pre-fix emulator, which derived the broadcast element from the dest
 * width/count (8 bytes for the zmm {1to8} forms) -- vcvtdq2pd died on the
 * "impossible EVEX broadcast element size" tripwire, vcvtps2pd silently
 * broadcast 8-byte chunks AND ran the 16-lane convert off the end of the
 * 64-byte VecVal (a real stack OOB -- the unmasked probe doubles as the
 * canary).  The masked probes additionally pin the SOURCE mask granularity:
 * with k1 = 0xF0 hardware loads source dwords 4-7 and converts them into
 * dest doubles 4-7 (the pre-fix emulator masked 4 8-byte lanes and loaded
 * nothing). */

__attribute__((used, aligned(64)))
static const int sdm_cvt_i32[8] = {11, 22, 33, 44, 55, 66, 77, 88};

__attribute__((used, aligned(64)))
static const float sdm_cvt_f32[8] = {1.5f, 2.5f, 3.5f, 4.5f, 5.5f, 6.5f, 7.5f, 8.5f};

/* doubles for the narrowing (pd2ps) masked-source probe: 8 exact halves */
__attribute__((used, aligned(64)))
static const double sdm_cvt_f64[8] = {10.5, 11.5, 12.5, 13.5, 14.5, 15.5, 16.5, 17.5};

__attribute__((noinline, used)) static void
sdm_dq2pd_bcast(double *out) {
    /* vcvtdq2pd m256{1to8}, %zmm0: every dest double is the SINGLE broadcast
     * source dword 11 converted to double. */
    asm volatile(
        "vcvtdq2pd sdm_cvt_i32(%rip){1to8}, %zmm0\n\t"
        "vmovups %zmm0, (%rdi)\n\t");

    /* IPA-RA barrier: the basic-asm body above really kills the ABI
     * caller-saved GPRs (rax holds the kmovd immediate, rdi the out
     * pointer, rcx/rdx/rsi/r8-r11 the vmovups/vperm scratch) AND every
     * vector register (zmm0 is the probe's staging vector, zmm1/zmm2 the
     * sources, and the k-register loads go through eax), but basic asm
     * tells the compiler nothing, so -fipa-ra concludes this probe
     * clobbers NO caller-saved register and lets the CALLER keep values
     * in those registers across the call -- observed as an
     * expected-array store scheduled after the call being filled with
     * the probe's leftover %eax (the kmovd immediate) instead of its
     * constant, and as a (double)sdm_cvt_f32[0] value hoisted into an
     * xmm that the probe then trashed.  Declaring the clobbers ("xmmN"
     * covers the full zmm under AVX) makes every probe call a real
     * register kill, so the caller cannot reuse stale post-call
     * register contents; this is deterministic across gcc versions and
     * optimization levels (the split-file -O2 build tripped it in five
     * checks the old single-file -O3 build had dodged by layout luck). */
    asm volatile("" :::
                 "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10",
                 "r11",
                 "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
                 "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12",
                 "xmm13", "xmm14", "xmm15",
                 "memory");
}

__attribute__((noinline, used)) static void
sdm_dq2pd_bcast_kz(double *out) {
    /* Same, {k1}{z} with k1 = 0xF0: dest doubles 4-7 = converted broadcast
     * element, doubles 0-3 = zero ({z}). */
    asm volatile(
        "movl $0xf0, %eax\n\t"
        "kmovd %eax, %k1\n\t"
        "vcvtdq2pd sdm_cvt_i32(%rip){1to8}, %zmm0{%k1}{z}\n\t"
        "vmovups %zmm0, (%rdi)\n\t");

    /* IPA-RA barrier: the basic-asm body above really kills the ABI
     * caller-saved GPRs (rax holds the kmovd immediate, rdi the out
     * pointer, rcx/rdx/rsi/r8-r11 the vmovups/vperm scratch) AND every
     * vector register (zmm0 is the probe's staging vector, zmm1/zmm2 the
     * sources, and the k-register loads go through eax), but basic asm
     * tells the compiler nothing, so -fipa-ra concludes this probe
     * clobbers NO caller-saved register and lets the CALLER keep values
     * in those registers across the call -- observed as an
     * expected-array store scheduled after the call being filled with
     * the probe's leftover %eax (the kmovd immediate) instead of its
     * constant, and as a (double)sdm_cvt_f32[0] value hoisted into an
     * xmm that the probe then trashed.  Declaring the clobbers ("xmmN"
     * covers the full zmm under AVX) makes every probe call a real
     * register kill, so the caller cannot reuse stale post-call
     * register contents; this is deterministic across gcc versions and
     * optimization levels (the split-file -O2 build tripped it in five
     * checks the old single-file -O3 build had dodged by layout luck). */
    asm volatile("" :::
                 "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10",
                 "r11",
                 "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
                 "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12",
                 "xmm13", "xmm14", "xmm15",
                 "memory");
}

__attribute__((noinline, used)) static void
sdm_ps2pd_bcast(double *out) {
    /* vcvtps2pd m256{1to8}, %zmm0: every dest double = (double)1.5f.  The
     * pre-fix emulator wrote 16 lanes here (stack OOB). */
    asm volatile(
        "vcvtps2pd sdm_cvt_f32(%rip){1to8}, %zmm0\n\t"
        "vmovups %zmm0, (%rdi)\n\t");

    /* IPA-RA barrier: the basic-asm body above really kills the ABI
     * caller-saved GPRs (rax holds the kmovd immediate, rdi the out
     * pointer, rcx/rdx/rsi/r8-r11 the vmovups/vperm scratch) AND every
     * vector register (zmm0 is the probe's staging vector, zmm1/zmm2 the
     * sources, and the k-register loads go through eax), but basic asm
     * tells the compiler nothing, so -fipa-ra concludes this probe
     * clobbers NO caller-saved register and lets the CALLER keep values
     * in those registers across the call -- observed as an
     * expected-array store scheduled after the call being filled with
     * the probe's leftover %eax (the kmovd immediate) instead of its
     * constant, and as a (double)sdm_cvt_f32[0] value hoisted into an
     * xmm that the probe then trashed.  Declaring the clobbers ("xmmN"
     * covers the full zmm under AVX) makes every probe call a real
     * register kill, so the caller cannot reuse stale post-call
     * register contents; this is deterministic across gcc versions and
     * optimization levels (the split-file -O2 build tripped it in five
     * checks the old single-file -O3 build had dodged by layout luck). */
    asm volatile("" :::
                 "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10",
                 "r11",
                 "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
                 "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12",
                 "xmm13", "xmm14", "xmm15",
                 "memory");
}

__attribute__((noinline, used)) static void
sdm_ps2pd_bcast_kz(double *out) {
    /* vcvtps2pd m256{1to8}, %zmm0{k1}{z} with k1 = 0xF0: doubles 4-7 = the
     * broadcast element, doubles 0-3 = zero. */
    asm volatile(
        "movl $0xf0, %eax\n\t"
        "kmovd %eax, %k1\n\t"
        "vcvtps2pd sdm_cvt_f32(%rip){1to8}, %zmm0{%k1}{z}\n\t"
        "vmovups %zmm0, (%rdi)\n\t");

    /* IPA-RA barrier: the basic-asm body above really kills the ABI
     * caller-saved GPRs (rax holds the kmovd immediate, rdi the out
     * pointer, rcx/rdx/rsi/r8-r11 the vmovups/vperm scratch) AND every
     * vector register (zmm0 is the probe's staging vector, zmm1/zmm2 the
     * sources, and the k-register loads go through eax), but basic asm
     * tells the compiler nothing, so -fipa-ra concludes this probe
     * clobbers NO caller-saved register and lets the CALLER keep values
     * in those registers across the call -- observed as an
     * expected-array store scheduled after the call being filled with
     * the probe's leftover %eax (the kmovd immediate) instead of its
     * constant, and as a (double)sdm_cvt_f32[0] value hoisted into an
     * xmm that the probe then trashed.  Declaring the clobbers ("xmmN"
     * covers the full zmm under AVX) makes every probe call a real
     * register kill, so the caller cannot reuse stale post-call
     * register contents; this is deterministic across gcc versions and
     * optimization levels (the split-file -O2 build tripped it in five
     * checks the old single-file -O3 build had dodged by layout luck). */
    asm volatile("" :::
                 "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10",
                 "r11",
                 "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
                 "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12",
                 "xmm13", "xmm14", "xmm15",
                 "memory");
}

__attribute__((noinline, used)) static void
sdm_ps2pd_masked_kz(double *out) {
    /* vcvtps2pd m256, %zmm0{k1}{z} (non-broadcast) with k1 = 0xF0: hardware
     * loads SOURCE dwords 4-7 and converts them into dest doubles 4-7 (the
     * mask granularity follows the SOURCE element).  The pre-fix emulator
     * masked 4 8-byte lanes and loaded nothing (all zeros). */
    asm volatile(
        "movl $0xf0, %eax\n\t"
        "kmovd %eax, %k1\n\t"
        "vcvtps2pd sdm_cvt_f32(%rip), %zmm0{%k1}{z}\n\t"
        "vmovups %zmm0, (%rdi)\n\t");

    /* IPA-RA barrier: the basic-asm body above really kills the ABI
     * caller-saved GPRs (rax holds the kmovd immediate, rdi the out
     * pointer, rcx/rdx/rsi/r8-r11 the vmovups/vperm scratch) AND every
     * vector register (zmm0 is the probe's staging vector, zmm1/zmm2 the
     * sources, and the k-register loads go through eax), but basic asm
     * tells the compiler nothing, so -fipa-ra concludes this probe
     * clobbers NO caller-saved register and lets the CALLER keep values
     * in those registers across the call -- observed as an
     * expected-array store scheduled after the call being filled with
     * the probe's leftover %eax (the kmovd immediate) instead of its
     * constant, and as a (double)sdm_cvt_f32[0] value hoisted into an
     * xmm that the probe then trashed.  Declaring the clobbers ("xmmN"
     * covers the full zmm under AVX) makes every probe call a real
     * register kill, so the caller cannot reuse stale post-call
     * register contents; this is deterministic across gcc versions and
     * optimization levels (the split-file -O2 build tripped it in five
     * checks the old single-file -O3 build had dodged by layout luck). */
    asm volatile("" :::
                 "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10",
                 "r11",
                 "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
                 "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12",
                 "xmm13", "xmm14", "xmm15",
                 "memory");
}

__attribute__((noinline, used)) static void
sdm_dq2pd_masked_kz(double *out) {
    /* vcvtdq2pd m256, %zmm0{k1}{z} with k1 = 0xF0: dest doubles 4-7 =
     * (double)sdm_cvt_i32[4..7] = 55/66/77/88, doubles 0-3 = zero. */
    asm volatile(
        "movl $0xf0, %eax\n\t"
        "kmovd %eax, %k1\n\t"
        "vcvtdq2pd sdm_cvt_i32(%rip), %zmm0{%k1}{z}\n\t"
        "vmovups %zmm0, (%rdi)\n\t");

    /* IPA-RA barrier: the basic-asm body above really kills the ABI
     * caller-saved GPRs (rax holds the kmovd immediate, rdi the out
     * pointer, rcx/rdx/rsi/r8-r11 the vmovups/vperm scratch) AND every
     * vector register (zmm0 is the probe's staging vector, zmm1/zmm2 the
     * sources, and the k-register loads go through eax), but basic asm
     * tells the compiler nothing, so -fipa-ra concludes this probe
     * clobbers NO caller-saved register and lets the CALLER keep values
     * in those registers across the call -- observed as an
     * expected-array store scheduled after the call being filled with
     * the probe's leftover %eax (the kmovd immediate) instead of its
     * constant, and as a (double)sdm_cvt_f32[0] value hoisted into an
     * xmm that the probe then trashed.  Declaring the clobbers ("xmmN"
     * covers the full zmm under AVX) makes every probe call a real
     * register kill, so the caller cannot reuse stale post-call
     * register contents; this is deterministic across gcc versions and
     * optimization levels (the split-file -O2 build tripped it in five
     * checks the old single-file -O3 build had dodged by layout luck). */
    asm volatile("" :::
                 "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10",
                 "r11",
                 "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
                 "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12",
                 "xmm13", "xmm14", "xmm15",
                 "memory");
}

__attribute__((noinline, used)) static void
sdm_dq2pd_reg_kz(double *out) {
    /* vcvtdq2pd %ymm1, %zmm0{k1}{z} with k1 = 0xF0: the register-source
     * widening form, same source-mask rule (source dwords 4-7 -> doubles
     * 4-7). */
    asm volatile(
        "movl $0xf0, %eax\n\t"
        "kmovd %eax, %k1\n\t"
        "vmovups sdm_cvt_i32(%rip), %ymm1\n\t"
        "vcvtdq2pd %ymm1, %zmm0{%k1}{z}\n\t"
        "vmovups %zmm0, (%rdi)\n\t");

    /* IPA-RA barrier: the basic-asm body above really kills the ABI
     * caller-saved GPRs (rax holds the kmovd immediate, rdi the out
     * pointer, rcx/rdx/rsi/r8-r11 the vmovups/vperm scratch) AND every
     * vector register (zmm0 is the probe's staging vector, zmm1/zmm2 the
     * sources, and the k-register loads go through eax), but basic asm
     * tells the compiler nothing, so -fipa-ra concludes this probe
     * clobbers NO caller-saved register and lets the CALLER keep values
     * in those registers across the call -- observed as an
     * expected-array store scheduled after the call being filled with
     * the probe's leftover %eax (the kmovd immediate) instead of its
     * constant, and as a (double)sdm_cvt_f32[0] value hoisted into an
     * xmm that the probe then trashed.  Declaring the clobbers ("xmmN"
     * covers the full zmm under AVX) makes every probe call a real
     * register kill, so the caller cannot reuse stale post-call
     * register contents; this is deterministic across gcc versions and
     * optimization levels (the split-file -O2 build tripped it in five
     * checks the old single-file -O3 build had dodged by layout luck). */
    asm volatile("" :::
                 "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10",
                 "r11",
                 "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
                 "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12",
                 "xmm13", "xmm14", "xmm15",
                 "memory");
}

__attribute__((noinline, used)) static void
sdm_pd2ps_masked_kz(float *out) {
    /* vcvtpd2ps m512, %ymm0{k1}{z} (narrowing, the ymm-dest mem-only form)
     * with k1 = 0xF0: the mask is over the DEST dwords; the source read is
     * full-width with SOURCE (double) granularity, so dest floats 4-7 =
     * (float)sdm_cvt_f64[4..7], floats 0-3 zeroed by {z}. */
    asm volatile(
        "movl $0xf0, %eax\n\t"
        "kmovd %eax, %k1\n\t"
        "vcvtpd2ps sdm_cvt_f64(%rip), %ymm0{%k1}{z}\n\t"
        "vmovups %ymm0, (%rdi)\n\t");

    /* IPA-RA barrier: the basic-asm body above really kills the ABI
     * caller-saved GPRs (rax holds the kmovd immediate, rdi the out
     * pointer, rcx/rdx/rsi/r8-r11 the vmovups/vperm scratch) AND every
     * vector register (zmm0 is the probe's staging vector, zmm1/zmm2 the
     * sources, and the k-register loads go through eax), but basic asm
     * tells the compiler nothing, so -fipa-ra concludes this probe
     * clobbers NO caller-saved register and lets the CALLER keep values
     * in those registers across the call -- observed as an
     * expected-array store scheduled after the call being filled with
     * the probe's leftover %eax (the kmovd immediate) instead of its
     * constant, and as a (double)sdm_cvt_f32[0] value hoisted into an
     * xmm that the probe then trashed.  Declaring the clobbers ("xmmN"
     * covers the full zmm under AVX) makes every probe call a real
     * register kill, so the caller cannot reuse stale post-call
     * register contents; this is deterministic across gcc versions and
     * optimization levels (the split-file -O2 build tripped it in five
     * checks the old single-file -O3 build had dodged by layout luck). */
    asm volatile("" :::
                 "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10",
                 "r11",
                 "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
                 "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12",
                 "xmm13", "xmm14", "xmm15",
                 "memory");
}

/* ---- EVEX VPERMILPD/VPERMILPS imm-form probes ----
 * Real AVX-512 forms (what _mm512_permute_pd/ps emit): the pre-fix
 * emulator routed the imm8 through evex_load and died with "unsupported
 * EVEX source operand" (exit 132).  Semantics per the SDM: VPERMILPD
 * consumes ONE imm bit per dest qword GLOBALLY (dest qword j selects within
 * its 128-bit lane pair by bit j); VPERMILPS restarts its 2-bit pairs per
 * 128-bit lane. */

__attribute__((noinline, used)) static void
sdm_evex_vpermilpd_imm(unsigned long long *out) {
    /* vpermilpd $0x0F, A(%rip), %zmm0: dest.q[0..3] = A[1],A[1],A[3],A[3]
     * (imm bits 0-3 set), dest.q[4..7] = A[4],A[4],A[6],A[6] (bits 4-7
     * clear) -- the GLOBAL bit indexing. */
    asm volatile(
        "vpermilpd $0x0F, A(%rip), %zmm0\n\t"
        "vmovups %zmm0, (%rdi)\n\t");

    /* IPA-RA barrier: the basic-asm body above really kills the ABI
     * caller-saved GPRs (rax holds the kmovd immediate, rdi the out
     * pointer, rcx/rdx/rsi/r8-r11 the vmovups/vperm scratch) AND every
     * vector register (zmm0 is the probe's staging vector, zmm1/zmm2 the
     * sources, and the k-register loads go through eax), but basic asm
     * tells the compiler nothing, so -fipa-ra concludes this probe
     * clobbers NO caller-saved register and lets the CALLER keep values
     * in those registers across the call -- observed as an
     * expected-array store scheduled after the call being filled with
     * the probe's leftover %eax (the kmovd immediate) instead of its
     * constant, and as a (double)sdm_cvt_f32[0] value hoisted into an
     * xmm that the probe then trashed.  Declaring the clobbers ("xmmN"
     * covers the full zmm under AVX) makes every probe call a real
     * register kill, so the caller cannot reuse stale post-call
     * register contents; this is deterministic across gcc versions and
     * optimization levels (the split-file -O2 build tripped it in five
     * checks the old single-file -O3 build had dodged by layout luck). */
    asm volatile("" :::
                 "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10",
                 "r11",
                 "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
                 "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12",
                 "xmm13", "xmm14", "xmm15",
                 "memory");
}

__attribute__((noinline, used)) static void
sdm_evex_vpermilpd_imm2(unsigned long long *out) {
    /* vpermilpd $0x5A, A(%rip), %zmm0: bits = 0101 1010 -> dest.q[j] takes
     * the pair member by bit j: A[0],A[1],A[2],A[3],A[5],A[4],A[7],A[6]. */
    asm volatile(
        "vpermilpd $0x5A, A(%rip), %zmm0\n\t"
        "vmovups %zmm0, (%rdi)\n\t");

    /* IPA-RA barrier: the basic-asm body above really kills the ABI
     * caller-saved GPRs (rax holds the kmovd immediate, rdi the out
     * pointer, rcx/rdx/rsi/r8-r11 the vmovups/vperm scratch) AND every
     * vector register (zmm0 is the probe's staging vector, zmm1/zmm2 the
     * sources, and the k-register loads go through eax), but basic asm
     * tells the compiler nothing, so -fipa-ra concludes this probe
     * clobbers NO caller-saved register and lets the CALLER keep values
     * in those registers across the call -- observed as an
     * expected-array store scheduled after the call being filled with
     * the probe's leftover %eax (the kmovd immediate) instead of its
     * constant, and as a (double)sdm_cvt_f32[0] value hoisted into an
     * xmm that the probe then trashed.  Declaring the clobbers ("xmmN"
     * covers the full zmm under AVX) makes every probe call a real
     * register kill, so the caller cannot reuse stale post-call
     * register contents; this is deterministic across gcc versions and
     * optimization levels (the split-file -O2 build tripped it in five
     * checks the old single-file -O3 build had dodged by layout luck). */
    asm volatile("" :::
                 "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10",
                 "r11",
                 "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
                 "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12",
                 "xmm13", "xmm14", "xmm15",
                 "memory");
}

__attribute__((noinline, used)) static void
sdm_evex_vpermilps_imm(unsigned int *out) {
    /* vpermilps $0x1B, A(%rip), %zmm0: per lane the 2-bit selects restart
     * (bits [1:0]/[3:2]/[5:4]/[7:6] = 3/2/1/0): dest dwords per lane =
     * A.d[3],A.d[2],A.d[1],A.d[0]. */
    asm volatile(
        "vpermilps $0x1B, A(%rip), %zmm0\n\t"
        "vmovups %zmm0, (%rdi)\n\t");

    /* IPA-RA barrier: the basic-asm body above really kills the ABI
     * caller-saved GPRs (rax holds the kmovd immediate, rdi the out
     * pointer, rcx/rdx/rsi/r8-r11 the vmovups/vperm scratch) AND every
     * vector register (zmm0 is the probe's staging vector, zmm1/zmm2 the
     * sources, and the k-register loads go through eax), but basic asm
     * tells the compiler nothing, so -fipa-ra concludes this probe
     * clobbers NO caller-saved register and lets the CALLER keep values
     * in those registers across the call -- observed as an
     * expected-array store scheduled after the call being filled with
     * the probe's leftover %eax (the kmovd immediate) instead of its
     * constant, and as a (double)sdm_cvt_f32[0] value hoisted into an
     * xmm that the probe then trashed.  Declaring the clobbers ("xmmN"
     * covers the full zmm under AVX) makes every probe call a real
     * register kill, so the caller cannot reuse stale post-call
     * register contents; this is deterministic across gcc versions and
     * optimization levels (the split-file -O2 build tripped it in five
     * checks the old single-file -O3 build had dodged by layout luck). */
    asm volatile("" :::
                 "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10",
                 "r11",
                 "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
                 "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12",
                 "xmm13", "xmm14", "xmm15",
                 "memory");
}

__attribute__((noinline, used)) static void
sdm_evex_vpermilps_imm2(unsigned int *out) {
    /* vpermilps $0xE4, A(%rip), %zmm0: identity per lane (bits pairs
     * 00/01/10/11). */
    asm volatile(
        "vpermilps $0xE4, A(%rip), %zmm0\n\t"
        "vmovups %zmm0, (%rdi)\n\t");

    /* IPA-RA barrier: the basic-asm body above really kills the ABI
     * caller-saved GPRs (rax holds the kmovd immediate, rdi the out
     * pointer, rcx/rdx/rsi/r8-r11 the vmovups/vperm scratch) AND every
     * vector register (zmm0 is the probe's staging vector, zmm1/zmm2 the
     * sources, and the k-register loads go through eax), but basic asm
     * tells the compiler nothing, so -fipa-ra concludes this probe
     * clobbers NO caller-saved register and lets the CALLER keep values
     * in those registers across the call -- observed as an
     * expected-array store scheduled after the call being filled with
     * the probe's leftover %eax (the kmovd immediate) instead of its
     * constant, and as a (double)sdm_cvt_f32[0] value hoisted into an
     * xmm that the probe then trashed.  Declaring the clobbers ("xmmN"
     * covers the full zmm under AVX) makes every probe call a real
     * register kill, so the caller cannot reuse stale post-call
     * register contents; this is deterministic across gcc versions and
     * optimization levels (the split-file -O2 build tripped it in five
     * checks the old single-file -O3 build had dodged by layout luck). */
    asm volatile("" :::
                 "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10",
                 "r11",
                 "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
                 "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12",
                 "xmm13", "xmm14", "xmm15",
                 "memory");
}

__attribute__((noinline, used)) static void
sdm_evex_vpermilpd_imm_bcast(unsigned long long *out) {
    /* vpermilpd $0xAA, m64{1to8}, %zmm0 (imm form with embedded broadcast):
     * the broadcast qword is BOTH members of every dest pair, so every dest
     * qword is sdm_barr[0] regardless of the imm bits. */
    asm volatile(
        "vpermilpd $0xAA, sdm_barr(%rip){1to8}, %zmm0\n\t"
        "vmovups %zmm0, (%rdi)\n\t");

    /* IPA-RA barrier: the basic-asm body above really kills the ABI
     * caller-saved GPRs (rax holds the kmovd immediate, rdi the out
     * pointer, rcx/rdx/rsi/r8-r11 the vmovups/vperm scratch) AND every
     * vector register (zmm0 is the probe's staging vector, zmm1/zmm2 the
     * sources, and the k-register loads go through eax), but basic asm
     * tells the compiler nothing, so -fipa-ra concludes this probe
     * clobbers NO caller-saved register and lets the CALLER keep values
     * in those registers across the call -- observed as an
     * expected-array store scheduled after the call being filled with
     * the probe's leftover %eax (the kmovd immediate) instead of its
     * constant, and as a (double)sdm_cvt_f32[0] value hoisted into an
     * xmm that the probe then trashed.  Declaring the clobbers ("xmmN"
     * covers the full zmm under AVX) makes every probe call a real
     * register kill, so the caller cannot reuse stale post-call
     * register contents; this is deterministic across gcc versions and
     * optimization levels (the split-file -O2 build tripped it in five
     * checks the old single-file -O3 build had dodged by layout luck). */
    asm volatile("" :::
                 "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10",
                 "r11",
                 "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
                 "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12",
                 "xmm13", "xmm14", "xmm15",
                 "memory");
}

/* ---- EVEX VUNPCK family (elem granularity + L/H half selection) ----
 * The PS unpacks permute dwords (elem 4) and the H variants take the HIGH
 * half of each lane; the pre-fix handler defaulted every non-QDQ mnemonic
 * to qword elem and left the VUNPCKH PD/PS variants on the low halves. */

__attribute__((noinline, used)) static void
sdm_vunpcklpd_bcast(unsigned long long *out) {
    /* vunpcklpd m512{1to8}, %zmm1, %zmm0: dest.q[2L] = A[2L], dest.q[2L+1] =
     * the broadcast qword. */
    asm volatile(
        "vmovups A(%rip), %zmm1\n\t"
        "vunpcklpd sdm_barr(%rip){1to8}, %zmm1, %zmm0\n\t"
        "vmovups %zmm0, (%rdi)\n\t");

    /* IPA-RA barrier: the basic-asm body above really kills the ABI
     * caller-saved GPRs (rax holds the kmovd immediate, rdi the out
     * pointer, rcx/rdx/rsi/r8-r11 the vmovups/vperm scratch) AND every
     * vector register (zmm0 is the probe's staging vector, zmm1/zmm2 the
     * sources, and the k-register loads go through eax), but basic asm
     * tells the compiler nothing, so -fipa-ra concludes this probe
     * clobbers NO caller-saved register and lets the CALLER keep values
     * in those registers across the call -- observed as an
     * expected-array store scheduled after the call being filled with
     * the probe's leftover %eax (the kmovd immediate) instead of its
     * constant, and as a (double)sdm_cvt_f32[0] value hoisted into an
     * xmm that the probe then trashed.  Declaring the clobbers ("xmmN"
     * covers the full zmm under AVX) makes every probe call a real
     * register kill, so the caller cannot reuse stale post-call
     * register contents; this is deterministic across gcc versions and
     * optimization levels (the split-file -O2 build tripped it in five
     * checks the old single-file -O3 build had dodged by layout luck). */
    asm volatile("" :::
                 "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10",
                 "r11",
                 "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
                 "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12",
                 "xmm13", "xmm14", "xmm15",
                 "memory");
}

__attribute__((noinline, used)) static void
sdm_vunpckhpd_bcast(unsigned long long *out) {
    /* vunpckhpd m512{1to8}, %zmm1, %zmm0: the HIGH qword of each lane:
     * dest.q[2L] = A[2L+1], dest.q[2L+1] = the broadcast qword. */
    asm volatile(
        "vmovups A(%rip), %zmm1\n\t"
        "vunpckhpd sdm_barr(%rip){1to8}, %zmm1, %zmm0\n\t"
        "vmovups %zmm0, (%rdi)\n\t");

    /* IPA-RA barrier: the basic-asm body above really kills the ABI
     * caller-saved GPRs (rax holds the kmovd immediate, rdi the out
     * pointer, rcx/rdx/rsi/r8-r11 the vmovups/vperm scratch) AND every
     * vector register (zmm0 is the probe's staging vector, zmm1/zmm2 the
     * sources, and the k-register loads go through eax), but basic asm
     * tells the compiler nothing, so -fipa-ra concludes this probe
     * clobbers NO caller-saved register and lets the CALLER keep values
     * in those registers across the call -- observed as an
     * expected-array store scheduled after the call being filled with
     * the probe's leftover %eax (the kmovd immediate) instead of its
     * constant, and as a (double)sdm_cvt_f32[0] value hoisted into an
     * xmm that the probe then trashed.  Declaring the clobbers ("xmmN"
     * covers the full zmm under AVX) makes every probe call a real
     * register kill, so the caller cannot reuse stale post-call
     * register contents; this is deterministic across gcc versions and
     * optimization levels (the split-file -O2 build tripped it in five
     * checks the old single-file -O3 build had dodged by layout luck). */
    asm volatile("" :::
                 "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10",
                 "r11",
                 "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
                 "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12",
                 "xmm13", "xmm14", "xmm15",
                 "memory");
}

__attribute__((noinline, used)) static void
sdm_vunpcklps_bcast(unsigned int *out) {
    /* vunpcklps m512{1to16}, %zmm1, %zmm0: DWORD interleave -- dest.d per
     * lane = [a.d0, bc, a.d1, bc, a.d2, bc, a.d3, bc].  The pre-fix
     * handler's qword elem tripped the broadcast-element tripwire here on
     * legal code. */
    asm volatile(
        "vmovups A(%rip), %zmm1\n\t"
        "vunpcklps sdm_barr32(%rip){1to16}, %zmm1, %zmm0\n\t"
        "vmovups %zmm0, (%rdi)\n\t");

    /* IPA-RA barrier: the basic-asm body above really kills the ABI
     * caller-saved GPRs (rax holds the kmovd immediate, rdi the out
     * pointer, rcx/rdx/rsi/r8-r11 the vmovups/vperm scratch) AND every
     * vector register (zmm0 is the probe's staging vector, zmm1/zmm2 the
     * sources, and the k-register loads go through eax), but basic asm
     * tells the compiler nothing, so -fipa-ra concludes this probe
     * clobbers NO caller-saved register and lets the CALLER keep values
     * in those registers across the call -- observed as an
     * expected-array store scheduled after the call being filled with
     * the probe's leftover %eax (the kmovd immediate) instead of its
     * constant, and as a (double)sdm_cvt_f32[0] value hoisted into an
     * xmm that the probe then trashed.  Declaring the clobbers ("xmmN"
     * covers the full zmm under AVX) makes every probe call a real
     * register kill, so the caller cannot reuse stale post-call
     * register contents; this is deterministic across gcc versions and
     * optimization levels (the split-file -O2 build tripped it in five
     * checks the old single-file -O3 build had dodged by layout luck). */
    asm volatile("" :::
                 "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10",
                 "r11",
                 "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
                 "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12",
                 "xmm13", "xmm14", "xmm15",
                 "memory");
}

__attribute__((noinline, used)) static void
sdm_vunpckhps_bcast(unsigned int *out) {
    /* vunpckhps m512{1to16}, %zmm1, %zmm0: dest.d per lane = [a.d2, bc,
     * a.d3, bc, a.d4, bc, a.d5, bc] -- the HIGH dwords of each lane. */
    asm volatile(
        "vmovups A(%rip), %zmm1\n\t"
        "vunpckhps sdm_barr32(%rip){1to16}, %zmm1, %zmm0\n\t"
        "vmovups %zmm0, (%rdi)\n\t");

    /* IPA-RA barrier: the basic-asm body above really kills the ABI
     * caller-saved GPRs (rax holds the kmovd immediate, rdi the out
     * pointer, rcx/rdx/rsi/r8-r11 the vmovups/vperm scratch) AND every
     * vector register (zmm0 is the probe's staging vector, zmm1/zmm2 the
     * sources, and the k-register loads go through eax), but basic asm
     * tells the compiler nothing, so -fipa-ra concludes this probe
     * clobbers NO caller-saved register and lets the CALLER keep values
     * in those registers across the call -- observed as an
     * expected-array store scheduled after the call being filled with
     * the probe's leftover %eax (the kmovd immediate) instead of its
     * constant, and as a (double)sdm_cvt_f32[0] value hoisted into an
     * xmm that the probe then trashed.  Declaring the clobbers ("xmmN"
     * covers the full zmm under AVX) makes every probe call a real
     * register kill, so the caller cannot reuse stale post-call
     * register contents; this is deterministic across gcc versions and
     * optimization levels (the split-file -O2 build tripped it in five
     * checks the old single-file -O3 build had dodged by layout luck). */
    asm volatile("" :::
                 "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10",
                 "r11",
                 "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
                 "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12",
                 "xmm13", "xmm14", "xmm15",
                 "memory");
}

/* ---- EVEX masked VPSHUFD probes (kind-5 elem is DWORD-granular) ----
 * VPSHUFD has DWORD elements: k1 bit j controls dest DWORD j (what
 * _mm512_mask_shuffle_epi32/_mm512_maskz_shuffle_epi32 emit).  The shared
 * kind-4/5 tail used to apply the mask at QWORD granularity for both kinds,
 * so a masked VPSHUFD took whole qwords from the result.  The markers below
 * make that regression loud: every qword holds a (result, old) DWORD pair
 * that no correct result produces. */

__attribute__((used, aligned(64)))
static const unsigned int sdm_pshufd_src[16] = {
    0x21u, 0x22u, 0x23u, 0x24u, 0x25u, 0x26u, 0x27u, 0x28u,
    0x29u, 0x2au, 0x2bu, 0x2cu, 0x2du, 0x2eu, 0x2fu, 0x30u};

__attribute__((used, aligned(64)))
static const unsigned int sdm_pshufd_old[16] = {
    0xD000u, 0xD001u, 0xD002u, 0xD003u, 0xD004u, 0xD005u, 0xD006u, 0xD007u,
    0xD008u, 0xD009u, 0xD00au, 0xD00bu, 0xD00cu, 0xD00du, 0xD00eu, 0xD00fu};

__attribute__((noinline, used)) static void
sdm_pshufd_masked(unsigned int *out) {
    /* vpshufd $0x1B, %zmm1, %zmm0{%k1} with k1 = 0x0005: $0x1B selects
     * d0<-src.d3, d1<-src.d2, d2<-src.d1, d3<-src.d0 per lane, so the full
     * result is [24,23,22,21, 28,27,26,25, 2C,2B,2A,29, 30,2F,2E,2D]; the
     * mask writes dwords 0 and 2, merging dwords 1 and 3 with the old dest. */
    asm volatile(
        "movl $0x0005, %eax\n\t"
        "kmovd %eax, %k1\n\t"
        "vmovups sdm_pshufd_old(%rip), %zmm0\n\t"
        "vmovups sdm_pshufd_src(%rip), %zmm1\n\t"
        "vpshufd $0x1B, %zmm1, %zmm0{%k1}\n\t"
        "vmovups %zmm0, (%rdi)\n\t");

    /* IPA-RA barrier: the basic-asm body above really kills the ABI
     * caller-saved GPRs (rax holds the kmovd immediate, rdi the out
     * pointer, rcx/rdx/rsi/r8-r11 the vmovups/vperm scratch) AND every
     * vector register (zmm0 is the probe's staging vector, zmm1/zmm2 the
     * sources, and the k-register loads go through eax), but basic asm
     * tells the compiler nothing, so -fipa-ra concludes this probe
     * clobbers NO caller-saved register and lets the CALLER keep values
     * in those registers across the call -- observed as an
     * expected-array store scheduled after the call being filled with
     * the probe's leftover %eax (the kmovd immediate) instead of its
     * constant, and as a (double)sdm_cvt_f32[0] value hoisted into an
     * xmm that the probe then trashed.  Declaring the clobbers ("xmmN"
     * covers the full zmm under AVX) makes every probe call a real
     * register kill, so the caller cannot reuse stale post-call
     * register contents; this is deterministic across gcc versions and
     * optimization levels (the split-file -O2 build tripped it in five
     * checks the old single-file -O3 build had dodged by layout luck). */
    asm volatile("" :::
                 "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10",
                 "r11",
                 "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
                 "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12",
                 "xmm13", "xmm14", "xmm15",
                 "memory");
}

__attribute__((noinline, used)) static void
sdm_pshufd_maskedz(unsigned int *out) {
    /* Same, with {z}: masked-off dwords must be ZERO, not old dest. */
    asm volatile(
        "movl $0x0005, %eax\n\t"
        "kmovd %eax, %k1\n\t"
        "vmovups sdm_pshufd_old(%rip), %zmm0\n\t"
        "vmovups sdm_pshufd_src(%rip), %zmm1\n\t"
        "vpshufd $0x1B, %zmm1, %zmm0{%k1}{z}\n\t"
        "vmovups %zmm0, (%rdi)\n\t");

    /* IPA-RA barrier: the basic-asm body above really kills the ABI
     * caller-saved GPRs (rax holds the kmovd immediate, rdi the out
     * pointer, rcx/rdx/rsi/r8-r11 the vmovups/vperm scratch) AND every
     * vector register (zmm0 is the probe's staging vector, zmm1/zmm2 the
     * sources, and the k-register loads go through eax), but basic asm
     * tells the compiler nothing, so -fipa-ra concludes this probe
     * clobbers NO caller-saved register and lets the CALLER keep values
     * in those registers across the call -- observed as an
     * expected-array store scheduled after the call being filled with
     * the probe's leftover %eax (the kmovd immediate) instead of its
     * constant, and as a (double)sdm_cvt_f32[0] value hoisted into an
     * xmm that the probe then trashed.  Declaring the clobbers ("xmmN"
     * covers the full zmm under AVX) makes every probe call a real
     * register kill, so the caller cannot reuse stale post-call
     * register contents; this is deterministic across gcc versions and
     * optimization levels (the split-file -O2 build tripped it in five
     * checks the old single-file -O3 build had dodged by layout luck). */
    asm volatile("" :::
                 "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10",
                 "r11",
                 "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
                 "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12",
                 "xmm13", "xmm14", "xmm15",
                 "memory");
}

/* ---- EVEX ternlog probes (elem is per-mnemonic: D dwords, Q qwords) ----
 * Both ternlog broadcast forms are real (the SDM lists m32bcst for
 * VPTERNLOGD and m64bcst for VPTERNLOGQ; binutils assembles both), and the
 * write mask granularity follows the element: VPTERNLOGQ masked-off lanes
 * keep/zero whole QWORDS, VPTERNLOGD dwords.  The old handler hardcoded
 * elem=4 for both mnemonics, tripping the "impossible EVEX broadcast
 * element size" tripwire on the legal unmasked vpternlogq m512{1to8} form
 * and making a masked VPTERNLOGQ merge dword-granular. */

/* the old dest of the masked ternlog probes (distinct marker qwords) */
__attribute__((used, aligned(64)))
static const unsigned long long sdm_tl_old64[8] = {
    0xD100000000000001ULL, 0xD100000000000002ULL, 0xD100000000000003ULL,
    0xD100000000000004ULL, 0xD100000000000005ULL, 0xD100000000000006ULL,
    0xD100000000000007ULL, 0xD100000000000008ULL};

__attribute__((used, aligned(64)))
static const unsigned int sdm_tl_old32[16] = {
    0xD0000001u, 0xD0000002u, 0xD0000003u, 0xD0000004u, 0xD0000005u,
    0xD0000006u, 0xD0000007u, 0xD0000008u, 0xD0000009u, 0xD000000au,
    0xD000000bu, 0xD000000cu, 0xD000000du, 0xD000000eu, 0xD000000fu,
    0xD0000010u};

__attribute__((noinline, used)) static void
sdm_ternlogq_bcast(unsigned long long *out) {
    /* vpternlogq $0x96, m512{1to8}, %zmm1, %zmm0: the unmasked form that
     * died on the tripwire pre-fix.  imm 0x96 = bits 7/4/2/1 = 3-input XOR
     * (symmetric in the LUT index, so the truth table is pinned without
     * betting on which source is which index bit): every dest qword is
     * A[q] ^ barr[0] ^ canary[q] -- the broadcast element replicated; a
     * sequential full-vector read (the old bug class) would use barr[q]. */
    asm volatile(
        "vmovups sdm_canary(%rip), %zmm0\n\t"
        "vmovups A(%rip), %zmm1\n\t"
        "vpternlogq $0x96, sdm_barr(%rip){1to8}, %zmm1, %zmm0\n\t"
        "vmovups %zmm0, (%rdi)\n\t");

    /* IPA-RA barrier: the basic-asm body above really kills the ABI
     * caller-saved GPRs (rax holds the kmovd immediate, rdi the out
     * pointer, rcx/rdx/rsi/r8-r11 the vmovups/vperm scratch) AND every
     * vector register (zmm0 is the probe's staging vector, zmm1/zmm2 the
     * sources, and the k-register loads go through eax), but basic asm
     * tells the compiler nothing, so -fipa-ra concludes this probe
     * clobbers NO caller-saved register and lets the CALLER keep values
     * in those registers across the call -- observed as an
     * expected-array store scheduled after the call being filled with
     * the probe's leftover %eax (the kmovd immediate) instead of its
     * constant, and as a (double)sdm_cvt_f32[0] value hoisted into an
     * xmm that the probe then trashed.  Declaring the clobbers ("xmmN"
     * covers the full zmm under AVX) makes every probe call a real
     * register kill, so the caller cannot reuse stale post-call
     * register contents; this is deterministic across gcc versions and
     * optimization levels (the split-file -O2 build tripped it in five
     * checks the old single-file -O3 build had dodged by layout luck). */
    asm volatile("" :::
                 "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10",
                 "r11",
                 "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
                 "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12",
                 "xmm13", "xmm14", "xmm15",
                 "memory");
}

__attribute__((noinline, used)) static void
sdm_ternlogq_masked(unsigned long long *out) {
    /* vpternlogq $0xFF, %zmm1, %zmm1, %zmm2{%k1} with k1 = 0x0002: imm 0xFF
     * makes every LUT index produce 1, so the enabled lane is all ones and
     * the mask is QWORD-granular -- dest qword 1 = 0xFFFFFFFFFFFFFFFF,
     * qwords 0/2..7 keep the old dest (merge).  The pre-fix emulator wrote
     * only DWORD 1 of q0 and kept q1. */
    asm volatile(
        "movl $0x0002, %eax\n\t"
        "kmovd %eax, %k1\n\t"
        "vmovups sdm_tl_old64(%rip), %zmm2\n\t"
        "vpternlogq $0xFF, %zmm1, %zmm1, %zmm2{%k1}\n\t"
        "vmovups %zmm2, (%rdi)\n\t");
}

__attribute__((noinline, used)) static void
sdm_ternlogq_maskedz(unsigned long long *out) {
    /* Same, with {z}: masked-off qwords must be ZERO. */
    asm volatile(
        "movl $0x0002, %eax\n\t"
        "kmovd %eax, %k1\n\t"
        "vmovups sdm_tl_old64(%rip), %zmm2\n\t"
        "vpternlogq $0xFF, %zmm1, %zmm1, %zmm2{%k1}{z}\n\t"
        "vmovups %zmm2, (%rdi)\n\t");
}

__attribute__((noinline, used)) static void
sdm_ternlogd_masked(unsigned int *out) {
    /* vpternlogd $0xFF, %zmm1, %zmm1, %zmm2{%k1} with k1 = 0x0002: the D
     * twin, pinning the elem table from both sides -- the mask stays
     * DWORD-granular: dest dword 1 = 0xFFFFFFFF, dwords 0/2..15 keep the
     * old dest. */
    asm volatile(
        "movl $0x0002, %eax\n\t"
        "kmovd %eax, %k1\n\t"
        "vmovups sdm_tl_old32(%rip), %zmm2\n\t"
        "vpternlogd $0xFF, %zmm1, %zmm1, %zmm2{%k1}\n\t"
        "vmovups %zmm2, (%rdi)\n\t");
}

/* ---- EVEX VPERMQ/VPERMPD vector-control (variable-index) probes ----
 * The vector-control form shares the opcode with the imm8 form and differs
 * in operand ROLES: the INDEX vector is the EVEX.vvvv operand (the SECOND
 * Intel operand) and the DATA is the ModRM.rm operand (the THIRD), per the
 * SDM (VPERMQ/VPERMPD "EVEX - vector control forms"): "Copies quadwords
 * from the second source operand (the third operand) to the destination
 * operand (the first operand) according to the indices in the first source
 * operand (the second operand)."  The low 3 bits of each index qword select
 * GLOBALLY across the whole 512-bit source
 * (TMP_DEST[63:0] := (TMP_SRC2[511:0] >> (SRC1[2:0] * 64))[63:0]) -- the
 * same global mod-lanes rule VPERMD implements (hardware-verified on AVX2).
 * The index vectors below put lane-1 elements >= 4, so a per-half
 * ("per-lane") rule -- the WRONG reading that belongs to the imm8 form,
 * whose 2-bit selects restart per 256-bit half -- yields a different
 * vector and fails loudly.
 *
 * The pre-fix emulator dispatched the variable forms to the imm8-only
 * handler and kload()ed the third operand as the imm8: the register-index
 * form died with "unsupported register operand" (exit 132; kload cannot
 * load vector registers) and the memory third-operand form silently used
 * the first qword of the 64-byte data vector as the imm8 selects.  There
 * is NO memory-index form: EVEX.vvvv is register-only, so a memory third
 * operand is always the DATA (m512, or {1to8} m64bcst).  AT&T syntax
 * spells the form with the data FIRST (vpermq %zmm_data, %zmm_idx, %zmm0):
 * AT&T reverses Intel order, so the last AT&T source is the vvvv INDEX. */

/* distinguishing index vector: lane 1 selects cross the 256-bit half */
__attribute__((used, aligned(64)))
static const unsigned long long sdm_perm_idx[8] = {7, 6, 5, 4, 3, 2, 1, 0};

/* second index vector: also crosses the half (5/4/7/6 in lane 1) */
__attribute__((used, aligned(64)))
static const unsigned long long sdm_perm_idx2[8] = {1, 0, 3, 2, 5, 4, 7, 6};

/* data vector: qword j holds 10+j (used as raw bits and as doubles) */
__attribute__((used, aligned(64)))
static const unsigned long long sdm_perm_dat[8] = {10, 11, 12, 13, 14, 15, 16, 17};

/* VPERMD pin: the same rule at dword granularity on ymm (AVX2, kind 2) */
__attribute__((used, aligned(64)))
static const unsigned int sdm_perm_didx[8] = {7, 6, 5, 4, 3, 2, 1, 0};

__attribute__((used, aligned(64)))
static const unsigned int sdm_perm_ddat[8] = {100, 101, 102, 103,
                                              104, 105, 106, 107};

__attribute__((noinline, used)) static void
sdm_vpermq_regidx(unsigned long long *out) {
    /* vpermq %zmm2, %zmm1, %zmm0 with the index in %zmm1 (vvvv) and the
     * data in %zmm2 (rm): dest.q[i] = zmm2.q[zmm1.q[i] & 7] GLOBALLY.
     * Pre-fix this exact encoding died with exit 132. */
    asm volatile(
        "vmovups sdm_perm_idx(%rip), %zmm1\n\t"
        "vmovups sdm_perm_dat(%rip), %zmm2\n\t"
        "vpermq %zmm2, %zmm1, %zmm0\n\t"
        "vmovups %zmm0, (%rdi)\n\t");

    /* IPA-RA barrier: the basic-asm body above really kills the ABI
     * caller-saved GPRs (rax holds the kmovd immediate, rdi the out
     * pointer, rcx/rdx/rsi/r8-r11 the vmovups/vperm scratch) AND every
     * vector register (zmm0 is the probe's staging vector, zmm1/zmm2 the
     * sources, and the k-register loads go through eax), but basic asm
     * tells the compiler nothing, so -fipa-ra concludes this probe
     * clobbers NO caller-saved register and lets the CALLER keep values
     * in those registers across the call -- observed as an
     * expected-array store scheduled after the call being filled with
     * the probe's leftover %eax (the kmovd immediate) instead of its
     * constant, and as a (double)sdm_cvt_f32[0] value hoisted into an
     * xmm that the probe then trashed.  Declaring the clobbers ("xmmN"
     * covers the full zmm under AVX) makes every probe call a real
     * register kill, so the caller cannot reuse stale post-call
     * register contents; this is deterministic across gcc versions and
     * optimization levels (the split-file -O2 build tripped it in five
     * checks the old single-file -O3 build had dodged by layout luck). */
    asm volatile("" :::
                 "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10",
                 "r11",
                 "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
                 "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12",
                 "xmm13", "xmm14", "xmm15",
                 "memory");
}

__attribute__((noinline, used)) static void
sdm_vpermq_memdat(unsigned long long *out) {
    /* vpermq sdm_perm_dat(%rip), %zmm1, %zmm0: memory third operand = DATA
     * (m512), index register in vvvv.  Pre-fix the emulator kload()ed the
     * first qword of the data vector as imm8 selects and printed
     * [3,3,1,1,7,7,5,5] with exit 0. */
    asm volatile(
        "vmovups sdm_perm_idx(%rip), %zmm1\n\t"
        "vpermq sdm_perm_dat(%rip), %zmm1, %zmm0\n\t"
        "vmovups %zmm0, (%rdi)\n\t");

    /* IPA-RA barrier: the basic-asm body above really kills the ABI
     * caller-saved GPRs (rax holds the kmovd immediate, rdi the out
     * pointer, rcx/rdx/rsi/r8-r11 the vmovups/vperm scratch) AND every
     * vector register (zmm0 is the probe's staging vector, zmm1/zmm2 the
     * sources, and the k-register loads go through eax), but basic asm
     * tells the compiler nothing, so -fipa-ra concludes this probe
     * clobbers NO caller-saved register and lets the CALLER keep values
     * in those registers across the call -- observed as an
     * expected-array store scheduled after the call being filled with
     * the probe's leftover %eax (the kmovd immediate) instead of its
     * constant, and as a (double)sdm_cvt_f32[0] value hoisted into an
     * xmm that the probe then trashed.  Declaring the clobbers ("xmmN"
     * covers the full zmm under AVX) makes every probe call a real
     * register kill, so the caller cannot reuse stale post-call
     * register contents; this is deterministic across gcc versions and
     * optimization levels (the split-file -O2 build tripped it in five
     * checks the old single-file -O3 build had dodged by layout luck). */
    asm volatile("" :::
                 "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10",
                 "r11",
                 "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
                 "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12",
                 "xmm13", "xmm14", "xmm15",
                 "memory");
}

__attribute__((noinline, used)) static void
sdm_vpermq_memdat2(unsigned long long *out) {
    /* Same form with sdm_perm_idx2: dest = data[idx2] =
     * [11,10,13,12,15,14,17,16].  A per-half rule would give
     * [11,10,13,12,11,10,13,12]. */
    asm volatile(
        "vmovups sdm_perm_idx2(%rip), %zmm1\n\t"
        "vpermq sdm_perm_dat(%rip), %zmm1, %zmm0\n\t"
        "vmovups %zmm0, (%rdi)\n\t");

    /* IPA-RA barrier: the basic-asm body above really kills the ABI
     * caller-saved GPRs (rax holds the kmovd immediate, rdi the out
     * pointer, rcx/rdx/rsi/r8-r11 the vmovups/vperm scratch) AND every
     * vector register (zmm0 is the probe's staging vector, zmm1/zmm2 the
     * sources, and the k-register loads go through eax), but basic asm
     * tells the compiler nothing, so -fipa-ra concludes this probe
     * clobbers NO caller-saved register and lets the CALLER keep values
     * in those registers across the call -- observed as an
     * expected-array store scheduled after the call being filled with
     * the probe's leftover %eax (the kmovd immediate) instead of its
     * constant, and as a (double)sdm_cvt_f32[0] value hoisted into an
     * xmm that the probe then trashed.  Declaring the clobbers ("xmmN"
     * covers the full zmm under AVX) makes every probe call a real
     * register kill, so the caller cannot reuse stale post-call
     * register contents; this is deterministic across gcc versions and
     * optimization levels (the split-file -O2 build tripped it in five
     * checks the old single-file -O3 build had dodged by layout luck). */
    asm volatile("" :::
                 "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10",
                 "r11",
                 "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
                 "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12",
                 "xmm13", "xmm14", "xmm15",
                 "memory");
}

__attribute__((noinline, used)) static void
sdm_vpermq_memdat_bcast(unsigned long long *out) {
    /* vpermq sdm_barr(%rip){1to8}, %zmm1, %zmm0: the {1to8} m64bcst lives
     * on the DATA (rm) operand -- the SDM lists m64bcst for the third
     * operand only, and Zydis reports a REAL 1_TO_8 mode there (op.size=64;
     * nothing fabricated).  The broadcast IS the whole data vector, so
     * every dest qword is sdm_barr[0] regardless of the index values. */
    asm volatile(
        "vmovups sdm_perm_idx(%rip), %zmm1\n\t"
        "vpermq sdm_barr(%rip){1to8}, %zmm1, %zmm0\n\t"
        "vmovups %zmm0, (%rdi)\n\t");

    /* IPA-RA barrier: the basic-asm body above really kills the ABI
     * caller-saved GPRs (rax holds the kmovd immediate, rdi the out
     * pointer, rcx/rdx/rsi/r8-r11 the vmovups/vperm scratch) AND every
     * vector register (zmm0 is the probe's staging vector, zmm1/zmm2 the
     * sources, and the k-register loads go through eax), but basic asm
     * tells the compiler nothing, so -fipa-ra concludes this probe
     * clobbers NO caller-saved register and lets the CALLER keep values
     * in those registers across the call -- observed as an
     * expected-array store scheduled after the call being filled with
     * the probe's leftover %eax (the kmovd immediate) instead of its
     * constant, and as a (double)sdm_cvt_f32[0] value hoisted into an
     * xmm that the probe then trashed.  Declaring the clobbers ("xmmN"
     * covers the full zmm under AVX) makes every probe call a real
     * register kill, so the caller cannot reuse stale post-call
     * register contents; this is deterministic across gcc versions and
     * optimization levels (the split-file -O2 build tripped it in five
     * checks the old single-file -O3 build had dodged by layout luck). */
    asm volatile("" :::
                 "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10",
                 "r11",
                 "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
                 "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12",
                 "xmm13", "xmm14", "xmm15",
                 "memory");
}

__attribute__((noinline, used)) static void
sdm_vpermq_regidx_masked(unsigned long long *out) {
    /* vpermq %zmm2, %zmm1, %zmm0{%k1} with k1 = 0xA5 (what a kmovw of
     * 0xA5A5 loads, opmask registers are 8 bits): written qwords 0/2/5/7
     * take the indexed value, the others keep the canary -- the mask is
     * QWORD-granular, same as the imm8 form. */
    asm volatile(
        "movl $0xa5, %eax\n\t"
        "kmovd %eax, %k1\n\t"
        "vmovups sdm_canary(%rip), %zmm0\n\t"
        "vmovups sdm_perm_idx(%rip), %zmm1\n\t"
        "vmovups sdm_perm_dat(%rip), %zmm2\n\t"
        "vpermq %zmm2, %zmm1, %zmm0{%k1}\n\t"
        "vmovups %zmm0, (%rdi)\n\t");

    /* IPA-RA barrier: the basic-asm body above really kills the ABI
     * caller-saved GPRs (rax holds the kmovd immediate, rdi the out
     * pointer, rcx/rdx/rsi/r8-r11 the vmovups/vperm scratch) AND every
     * vector register (zmm0 is the probe's staging vector, zmm1/zmm2 the
     * sources, and the k-register loads go through eax), but basic asm
     * tells the compiler nothing, so -fipa-ra concludes this probe
     * clobbers NO caller-saved register and lets the CALLER keep values
     * in those registers across the call -- observed as an
     * expected-array store scheduled after the call being filled with
     * the probe's leftover %eax (the kmovd immediate) instead of its
     * constant, and as a (double)sdm_cvt_f32[0] value hoisted into an
     * xmm that the probe then trashed.  Declaring the clobbers ("xmmN"
     * covers the full zmm under AVX) makes every probe call a real
     * register kill, so the caller cannot reuse stale post-call
     * register contents; this is deterministic across gcc versions and
     * optimization levels (the split-file -O2 build tripped it in five
     * checks the old single-file -O3 build had dodged by layout luck). */
    asm volatile("" :::
                 "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10",
                 "r11",
                 "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
                 "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12",
                 "xmm13", "xmm14", "xmm15",
                 "memory");
}

__attribute__((noinline, used)) static void
sdm_vpermpd_regidx(unsigned long long *out) {
    /* vpermpd %zmm2, %zmm1, %zmm0: what _mm512_permutexvar_pd emits (clang
     * canonicalizes the epi64 twin to the PD opcode).  Same qword
     * granularity, same global rule. */
    asm volatile(
        "vmovups sdm_perm_idx(%rip), %zmm1\n\t"
        "vmovups sdm_perm_dat(%rip), %zmm2\n\t"
        "vpermpd %zmm2, %zmm1, %zmm0\n\t"
        "vmovups %zmm0, (%rdi)\n\t");

    /* IPA-RA barrier: the basic-asm body above really kills the ABI
     * caller-saved GPRs (rax holds the kmovd immediate, rdi the out
     * pointer, rcx/rdx/rsi/r8-r11 the vmovups/vperm scratch) AND every
     * vector register (zmm0 is the probe's staging vector, zmm1/zmm2 the
     * sources, and the k-register loads go through eax), but basic asm
     * tells the compiler nothing, so -fipa-ra concludes this probe
     * clobbers NO caller-saved register and lets the CALLER keep values
     * in those registers across the call -- observed as an
     * expected-array store scheduled after the call being filled with
     * the probe's leftover %eax (the kmovd immediate) instead of its
     * constant, and as a (double)sdm_cvt_f32[0] value hoisted into an
     * xmm that the probe then trashed.  Declaring the clobbers ("xmmN"
     * covers the full zmm under AVX) makes every probe call a real
     * register kill, so the caller cannot reuse stale post-call
     * register contents; this is deterministic across gcc versions and
     * optimization levels (the split-file -O2 build tripped it in five
     * checks the old single-file -O3 build had dodged by layout luck). */
    asm volatile("" :::
                 "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10",
                 "r11",
                 "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
                 "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12",
                 "xmm13", "xmm14", "xmm15",
                 "memory");
}

__attribute__((noinline, used)) static void
sdm_vpermpd_memdat(unsigned long long *out) {
    /* vpermpd sdm_perm_dat(%rip), %zmm1, %zmm0: the PD memory-data twin. */
    asm volatile(
        "vmovups sdm_perm_idx(%rip), %zmm1\n\t"
        "vpermpd sdm_perm_dat(%rip), %zmm1, %zmm0\n\t"
        "vmovups %zmm0, (%rdi)\n\t");

    /* IPA-RA barrier: the basic-asm body above really kills the ABI
     * caller-saved GPRs (rax holds the kmovd immediate, rdi the out
     * pointer, rcx/rdx/rsi/r8-r11 the vmovups/vperm scratch) AND every
     * vector register (zmm0 is the probe's staging vector, zmm1/zmm2 the
     * sources, and the k-register loads go through eax), but basic asm
     * tells the compiler nothing, so -fipa-ra concludes this probe
     * clobbers NO caller-saved register and lets the CALLER keep values
     * in those registers across the call -- observed as an
     * expected-array store scheduled after the call being filled with
     * the probe's leftover %eax (the kmovd immediate) instead of its
     * constant, and as a (double)sdm_cvt_f32[0] value hoisted into an
     * xmm that the probe then trashed.  Declaring the clobbers ("xmmN"
     * covers the full zmm under AVX) makes every probe call a real
     * register kill, so the caller cannot reuse stale post-call
     * register contents; this is deterministic across gcc versions and
     * optimization levels (the split-file -O2 build tripped it in five
     * checks the old single-file -O3 build had dodged by layout luck). */
    asm volatile("" :::
                 "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10",
                 "r11",
                 "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
                 "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12",
                 "xmm13", "xmm14", "xmm15",
                 "memory");
}

__attribute__((noinline, used)) static void
sdm_vpermpd_regidx_masked(unsigned long long *out) {
    /* vpermpd %zmm2, %zmm1, %zmm0{%k1} with k1 = 0xA5: the PD masked twin. */
    asm volatile(
        "movl $0xa5, %eax\n\t"
        "kmovd %eax, %k1\n\t"
        "vmovups sdm_canary(%rip), %zmm0\n\t"
        "vmovups sdm_perm_idx(%rip), %zmm1\n\t"
        "vmovups sdm_perm_dat(%rip), %zmm2\n\t"
        "vpermpd %zmm2, %zmm1, %zmm0{%k1}\n\t"
        "vmovups %zmm0, (%rdi)\n\t");

    /* IPA-RA barrier: the basic-asm body above really kills the ABI
     * caller-saved GPRs (rax holds the kmovd immediate, rdi the out
     * pointer, rcx/rdx/rsi/r8-r11 the vmovups/vperm scratch) AND every
     * vector register (zmm0 is the probe's staging vector, zmm1/zmm2 the
     * sources, and the k-register loads go through eax), but basic asm
     * tells the compiler nothing, so -fipa-ra concludes this probe
     * clobbers NO caller-saved register and lets the CALLER keep values
     * in those registers across the call -- observed as an
     * expected-array store scheduled after the call being filled with
     * the probe's leftover %eax (the kmovd immediate) instead of its
     * constant, and as a (double)sdm_cvt_f32[0] value hoisted into an
     * xmm that the probe then trashed.  Declaring the clobbers ("xmmN"
     * covers the full zmm under AVX) makes every probe call a real
     * register kill, so the caller cannot reuse stale post-call
     * register contents; this is deterministic across gcc versions and
     * optimization levels (the split-file -O2 build tripped it in five
     * checks the old single-file -O3 build had dodged by layout luck). */
    asm volatile("" :::
                 "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10",
                 "r11",
                 "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
                 "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12",
                 "xmm13", "xmm14", "xmm15",
                 "memory");
}

__attribute__((noinline, used)) static void
sdm_vpermd_pin(unsigned int *out) {
    /* vpermd %ymm2, %ymm1, %ymm0: the kind-2 rule the Q/PD variable form
     * shares (index in vvvv, data in rm, GLOBAL mod-lanes over the whole
     * source; hardware-verified on AVX2).  Pin it so a change to the shared
     * loop cannot silently diverge across encodings. */
    asm volatile(
        "vmovups sdm_perm_didx(%rip), %ymm1\n\t"
        "vmovups sdm_perm_ddat(%rip), %ymm2\n\t"
        "vpermd %ymm2, %ymm1, %ymm0\n\t"
        "vmovups %ymm0, (%rdi)\n\t");

    /* IPA-RA barrier: the basic-asm body above really kills the ABI
     * caller-saved GPRs (rax holds the kmovd immediate, rdi the out
     * pointer, rcx/rdx/rsi/r8-r11 the vmovups/vperm scratch) AND every
     * vector register (zmm0 is the probe's staging vector, zmm1/zmm2 the
     * sources, and the k-register loads go through eax), but basic asm
     * tells the compiler nothing, so -fipa-ra concludes this probe
     * clobbers NO caller-saved register and lets the CALLER keep values
     * in those registers across the call -- observed as an
     * expected-array store scheduled after the call being filled with
     * the probe's leftover %eax (the kmovd immediate) instead of its
     * constant, and as a (double)sdm_cvt_f32[0] value hoisted into an
     * xmm that the probe then trashed.  Declaring the clobbers ("xmmN"
     * covers the full zmm under AVX) makes every probe call a real
     * register kill, so the caller cannot reuse stale post-call
     * register contents; this is deterministic across gcc versions and
     * optimization levels (the split-file -O2 build tripped it in five
     * checks the old single-file -O3 build had dodged by layout luck). */
    asm volatile("" :::
                 "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10",
                 "r11",
                 "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
                 "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12",
                 "xmm13", "xmm14", "xmm15",
                 "memory");
}

__attribute__((noinline, used)) static void
sdm_vpermq_imm(unsigned long long *out) {
    /* vpermq $0x1B, %zmm1, %zmm0: the IMM8 form (ex[1] = data in rm, ex[2]
     * = imm8).  The 2-bit selects index WITHIN each 256-bit half and apply
     * to BOTH halves: for data 10..17, imm 0x1B gives
     * [13,12,11,10,17,16,15,14].  Pins the imm8 path against the vector-
     * control dispatch change. */
    asm volatile(
        "vmovups sdm_perm_dat(%rip), %zmm1\n\t"
        "vpermq $0x1B, %zmm1, %zmm0\n\t"
        "vmovups %zmm0, (%rdi)\n\t");

    /* IPA-RA barrier: the basic-asm body above really kills the ABI
     * caller-saved GPRs (rax holds the kmovd immediate, rdi the out
     * pointer, rcx/rdx/rsi/r8-r11 the vmovups/vperm scratch) AND every
     * vector register (zmm0 is the probe's staging vector, zmm1/zmm2 the
     * sources, and the k-register loads go through eax), but basic asm
     * tells the compiler nothing, so -fipa-ra concludes this probe
     * clobbers NO caller-saved register and lets the CALLER keep values
     * in those registers across the call -- observed as an
     * expected-array store scheduled after the call being filled with
     * the probe's leftover %eax (the kmovd immediate) instead of its
     * constant, and as a (double)sdm_cvt_f32[0] value hoisted into an
     * xmm that the probe then trashed.  Declaring the clobbers ("xmmN"
     * covers the full zmm under AVX) makes every probe call a real
     * register kill, so the caller cannot reuse stale post-call
     * register contents; this is deterministic across gcc versions and
     * optimization levels (the split-file -O2 build tripped it in five
     * checks the old single-file -O3 build had dodged by layout luck). */
    asm volatile("" :::
                 "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10",
                 "r11",
                 "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
                 "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12",
                 "xmm13", "xmm14", "xmm15",
                 "memory");
}

__attribute__((noinline, used)) static void
sdm_vpermpd_imm(unsigned long long *out) {
    /* vpermpd $0x1B, %zmm1, %zmm0: the PD imm8 twin. */
    asm volatile(
        "vmovups sdm_perm_dat(%rip), %zmm1\n\t"
        "vpermpd $0x1B, %zmm1, %zmm0\n\t"
        "vmovups %zmm0, (%rdi)\n\t");

    /* IPA-RA barrier: the basic-asm body above really kills the ABI
     * caller-saved GPRs (rax holds the kmovd immediate, rdi the out
     * pointer, rcx/rdx/rsi/r8-r11 the vmovups/vperm scratch) AND every
     * vector register (zmm0 is the probe's staging vector, zmm1/zmm2 the
     * sources, and the k-register loads go through eax), but basic asm
     * tells the compiler nothing, so -fipa-ra concludes this probe
     * clobbers NO caller-saved register and lets the CALLER keep values
     * in those registers across the call -- observed as an
     * expected-array store scheduled after the call being filled with
     * the probe's leftover %eax (the kmovd immediate) instead of its
     * constant, and as a (double)sdm_cvt_f32[0] value hoisted into an
     * xmm that the probe then trashed.  Declaring the clobbers ("xmmN"
     * covers the full zmm under AVX) makes every probe call a real
     * register kill, so the caller cannot reuse stale post-call
     * register contents; this is deterministic across gcc versions and
     * optimization levels (the split-file -O2 build tripped it in five
     * checks the old single-file -O3 build had dodged by layout luck). */
    asm volatile("" :::
                 "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10",
                 "r11",
                 "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
                 "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12",
                 "xmm13", "xmm14", "xmm15",
                 "memory");
}

int main(void) {
    if (have_avx512f()) {
        /* ---- EVEX.512 VSHUFPD unmasked: global imm bits across all four
         * lanes (SDM-derived; skipped on hosts without AVX512F) ---- */
        {
#define TEST_EVEX512_PD(imm) do { \
    unsigned long long got[8]; \
    asm volatile("vmovups %1, %%zmm1\n\t" \
                 "vmovups %2, %%zmm2\n\t" \
                 "vshufpd $" #imm ", %%zmm2, %%zmm1, %%zmm0\n\t" \
                 "vmovups %%zmm0, %0" \
                 : "=m"(got) : "m"(A), "m"(B) : "%zmm0", "%zmm1", "%zmm2"); \
    unsigned long long want[8]; \
    for (unsigned lane = 0; lane < 4; lane++) { \
        want[2 * lane] = exp_pd_q(imm, lane, 0); \
        want[2 * lane + 1] = exp_pd_q(imm, lane, 1); \
    } \
    CHECK(memcmp(got, want, 64) == 0, "evex512 vshufpd imm=" #imm); \
    printf("evex512 vshufpd imm=" #imm " done\n"); \
} while (0)
            TEST_EVEX512_PD(1);
            TEST_EVEX512_PD(2);
            TEST_EVEX512_PD(5);
            TEST_EVEX512_PD(0x55);
            TEST_EVEX512_PD(0xAA);
#undef TEST_EVEX512_PD
        }
    } else {
        printf("skip avx512 groups\n");
    }
    if (is_in_zegarmistrz()) {
        /* ---- AVX-512 SDM-conformance battery: EMULATOR-ONLY (see the
         * header comment).  Silent on success; every mismatch prints a BAD
         * marker and exits nonzero. ---- */

        /* {k1}-masked EVEX.512 VSHUFPD, merging mask k1=0x0D (qwords 0/2/3
         * written): the write mask is QWORD-granular.  The old emulator
         * merged dwords, so q1 came out as a mix of result/old dwords. */
        {
            unsigned long long got[8];
            sdm_probes++;
            sdm_shufpd_masked(got);
            unsigned long long want[8] = {A[0],        SDM_CANARY_Q1, A[2], B[3],
                                          SDM_CANARY_Q4, SDM_CANARY_Q5,
                                          SDM_CANARY_Q6, SDM_CANARY_Q7};
            CHECK(memcmp(got, want, 64) == 0, "sdm evex512 vshufpd {k1} qword merge");
            /* (silent on success: stdout must stay identical to the native run) */
        }

        /* Same form with {z}: masked-off lanes must be ZERO. */
        {
            unsigned long long got[8];
            sdm_probes++;
            sdm_shufpd_maskedz(got);
            unsigned long long want[8] = {A[0], 0, A[2], B[3], 0, 0, 0, 0};
            CHECK(memcmp(got, want, 64) == 0, "sdm evex512 vshufpd {k1}{z} zeroing");
            /* (silent on success: stdout must stay identical to the native run) */
        }

        /* EVEX.512 VSHUFPD {1to8} embedded broadcast: the memory operand is
         * ONE qword replicated across the vector.  imm=0x00 keeps the even
         * qwords in src1 (A[0]/A[2]/A[4]/A[6]) and every odd qword is the
         * broadcast element sdm_barr[0].  The old emulator read the full 64
         * bytes sequentially, so the odd qwords came out barr[1]/[3]/[5]/[7].
         * This also pins the broadcast DETECTION: the bundled Zydis 4.1
         * never sets ZYDIS_ATTRIB_HAS_EVEX_B on decode, so detection keys off
         * avx.broadcast.mode instead. */
        {
            unsigned long long got[8];
            sdm_probes++;
            sdm_shufpd_bcast(got);
            unsigned long long want[8] = {A[0], sdm_barr[0], A[2], sdm_barr[0],
                                          A[4], sdm_barr[0], A[6], sdm_barr[0]};
            CHECK(memcmp(got, want, 64) == 0, "sdm evex512 vshufpd {1to8} broadcast");
            /* (silent on success: stdout must stay identical to the native run) */
        }

        /* EVEX.512 VPSHUFD {1to16} embedded broadcast: one dword replicated
         * into all sixteen lanes ($0xE4 is the identity shuffle, so each
         * lane's dword is exactly the broadcast element).  Same detection
         * pin as above, for the kind-5 (elem=4) handler. */
        {
            unsigned int got[16];
            sdm_probes++;
            sdm_pshufd_bcast(got);
            unsigned int want[16];
            for (unsigned i = 0; i < 16; i++) want[i] = sdm_barr32[0];
            CHECK(memcmp(got, want, 64) == 0, "sdm evex512 vpshufd {1to16} broadcast");
            /* (silent on success: stdout must stay identical to the native run) */
        }

        /* EVEX.512 VPERMILPD {1to8} broadcast control + {k1} mask (k1=0x33,
         * qwords 0/1/4/5 written): the broadcast control qword (bit 1 set)
         * makes every WRITTEN dest qword take the odd member of its src
         * pair, and the merge is qword-granular. */
        {
            unsigned long long got[8];
            sdm_probes++;
            sdm_vpermilpd_bcast_masked(got);
            unsigned long long want[8] = {A[1], A[1], SDM_CANARY_Q2, SDM_CANARY_Q3,
                                          A[5], A[5], SDM_CANARY_Q6, SDM_CANARY_Q7};
            CHECK(memcmp(got, want, 64) == 0,
                  "sdm evex512 vpermilpd {1to8} bcst + {k1} merge");
            /* (silent on success: stdout must stay identical to the native run) */
        }

        /* EVEX widening converts, {1to8} embedded broadcast (SOURCE element
         * granularity).  The pre-fix emulator died on vcvtdq2pd (tripwire)
         * and mis-broadcast + OOB-wrote on vcvtps2pd. */
        {
            double got[8];
            sdm_probes++;
            sdm_dq2pd_bcast(got);
            double want[8];
            for (unsigned i = 0; i < 8; i++) want[i] = (double)sdm_cvt_i32[0];
            CHECK(memcmp(got, want, 64) == 0, "sdm evex512 vcvtdq2pd {1to8} bcst");
            sdm_probes++;
            sdm_ps2pd_bcast(got);
            for (unsigned i = 0; i < 8; i++) want[i] = (double)sdm_cvt_f32[0];
            CHECK(memcmp(got, want, 64) == 0, "sdm evex512 vcvtps2pd {1to8} bcst");
            /* (silent on success: stdout must stay identical to the native run) */
        }

        /* Same broadcast converts with {k1}{z}, k1 = 0xF0: the mask is over
         * the DEST doubles (8 lanes), and every enabled lane takes the
         * single broadcast element; masked-off lanes are zero. */
        {
            double got[8];
            sdm_probes++;
            sdm_dq2pd_bcast_kz(got);
            double want[8] = {0, 0, 0, 0, (double)sdm_cvt_i32[0], (double)sdm_cvt_i32[0],
                              (double)sdm_cvt_i32[0], (double)sdm_cvt_i32[0]};
            CHECK(memcmp(got, want, 64) == 0, "sdm evex512 vcvtdq2pd {1to8} bcst {k1}{z}");
            sdm_probes++;
            sdm_ps2pd_bcast_kz(got);
            for (unsigned i = 0; i < 4; i++) want[i] = 0;
            for (unsigned i = 4; i < 8; i++) want[i] = (double)sdm_cvt_f32[0];
            CHECK(memcmp(got, want, 64) == 0, "sdm evex512 vcvtps2pd {1to8} bcst {k1}{z}");
            /* (silent on success: stdout must stay identical to the native run) */
        }

        /* NON-broadcast widening converts with {k1}{z}, k1 = 0xF0: the mask
         * granularity follows the SOURCE element -- hardware loads source
         * dwords 4-7 and converts them into dest doubles 4-7; doubles 0-3
         * are zero.  The pre-fix emulator masked 4 8-byte lanes, loaded
         * nothing and produced all zeros. */
        {
            double got[8];
            sdm_probes++;
            sdm_ps2pd_masked_kz(got);
            double want[8] = {0, 0, 0, 0, (double)sdm_cvt_f32[4], (double)sdm_cvt_f32[5],
                              (double)sdm_cvt_f32[6], (double)sdm_cvt_f32[7]};
            CHECK(memcmp(got, want, 64) == 0, "sdm evex512 vcvtps2pd m256 {k1}{z} src mask");
            sdm_probes++;
            sdm_dq2pd_masked_kz(got);
            double want2[8] = {0, 0, 0, 0, (double)sdm_cvt_i32[4], (double)sdm_cvt_i32[5],
                               (double)sdm_cvt_i32[6], (double)sdm_cvt_i32[7]};
            CHECK(memcmp(got, want2, 64) == 0, "sdm evex512 vcvtdq2pd m256 {k1}{z} src mask");
            /* (silent on success: stdout must stay identical to the native run) */
        }

        /* Register-source widening convert with {k1}{z} (k1 = 0xF0): same
         * source-mask rule -- dest doubles 4-7 come from the ymm's dwords
         * 4-7. */
        {
            double got[8];
            sdm_probes++;
            sdm_dq2pd_reg_kz(got);
            double want[8] = {0, 0, 0, 0, (double)sdm_cvt_i32[4], (double)sdm_cvt_i32[5],
                              (double)sdm_cvt_i32[6], (double)sdm_cvt_i32[7]};
            CHECK(memcmp(got, want, 64) == 0, "sdm evex512 vcvtdq2pd ymm {k1}{z} src mask");
            /* (silent on success: stdout must stay identical to the native run) */
        }

        /* Narrowing convert with {k1}{z} (k1 = 0xF0): the mask is over the
         * DEST dwords (the source doubles are read full-width at double
         * granularity), dest floats 4-7 = (float)src[4..7], floats 0-3
         * zeroed by {z}. */
        {
            float got[8];
            sdm_probes++;
            sdm_pd2ps_masked_kz(got);
            float want[8] = {0, 0, 0, 0, (float)sdm_cvt_f64[4], (float)sdm_cvt_f64[5],
                             (float)sdm_cvt_f64[6], (float)sdm_cvt_f64[7]};
            CHECK(memcmp(got, want, 32) == 0, "sdm evex512 vcvtpd2ps {k1}{z} dest-dword mask");
            /* (silent on success: stdout must stay identical to the native run) */
        }

        /* EVEX VPERMILPD imm form ($0x0F / $0x5A): one imm bit per dest
         * qword, consumed GLOBALLY (dest.q[j] selects within its pair by
         * bit j).  The pre-fix emulator died with "unsupported EVEX source
         * operand" (exit 132) on these legal _mm512_permute_pd forms. */
        {
            unsigned long long got[8];
            sdm_probes++;
            sdm_evex_vpermilpd_imm(got);
            unsigned long long want[8] = {A[1], A[1], A[3], A[3], A[4], A[4], A[6], A[6]};
            CHECK(memcmp(got, want, 64) == 0, "sdm evex512 vpermilpd $0x0F global bits");
            sdm_probes++;
            sdm_evex_vpermilpd_imm2(got);
            unsigned long long want2[8] = {A[0], A[1], A[2], A[3], A[5], A[4], A[7], A[6]};
            CHECK(memcmp(got, want2, 64) == 0, "sdm evex512 vpermilpd $0x5A global bits");
            /* (silent on success: stdout must stay identical to the native run) */
        }

        /* EVEX VPERMILPS imm form ($0x1B / $0xE4): the 2-bit selects restart
         * per 128-bit lane (PS rule). */
        {
            unsigned int got[16];
            const unsigned int *ad = (const unsigned int *)A;
            sdm_probes++;
            sdm_evex_vpermilps_imm(got);
            unsigned int want[16];
            for (unsigned lane = 0; lane < 4; lane++)
                for (unsigned i = 0; i < 4; i++)
                    want[4 * lane + i] = ad[4 * lane + (3 - i)];
            CHECK(memcmp(got, want, 64) == 0, "sdm evex512 vpermilps $0x1B per-lane pairs");
            sdm_probes++;
            sdm_evex_vpermilps_imm2(got);
            for (unsigned lane = 0; lane < 4; lane++)
                for (unsigned i = 0; i < 4; i++)
                    want[4 * lane + i] = ad[4 * lane + i];
            CHECK(memcmp(got, want, 64) == 0, "sdm evex512 vpermilps $0xE4 identity");
            /* (silent on success: stdout must stay identical to the native run) */
        }

        /* EVEX VPERMILPD imm form with {1to8} embedded broadcast: both
         * members of every dest pair are the broadcast qword, so the result
         * is sdm_barr[0] replicated regardless of the imm bits. */
        {
            unsigned long long got[8];
            sdm_probes++;
            sdm_evex_vpermilpd_imm_bcast(got);
            unsigned long long want[8];
            for (unsigned i = 0; i < 8; i++) want[i] = sdm_barr[0];
            CHECK(memcmp(got, want, 64) == 0, "sdm evex512 vpermilpd imm {1to8} bcst");
            /* (silent on success: stdout must stay identical to the native run) */
        }

        /* EVEX VUNPCK family: PD dwords/qwords L and H half selection plus
         * the PS dword elem (the pre-fix handler tripped the broadcast
         * tripwire on vunpcklps {1to16} and took the LOW half for the H
         * variants). */
        {
            unsigned long long got[8];
            unsigned int got32[16];
            const unsigned int *ad = (const unsigned int *)A;
            sdm_probes++;
            sdm_vunpcklpd_bcast(got);
            unsigned long long want[8] = {A[0], sdm_barr[0], A[2], sdm_barr[0],
                                          A[4], sdm_barr[0], A[6], sdm_barr[0]};
            CHECK(memcmp(got, want, 64) == 0, "sdm evex512 vunpcklpd {1to8} low half");
            sdm_probes++;
            sdm_vunpckhpd_bcast(got);
            unsigned long long want2[8] = {A[1], sdm_barr[0], A[3], sdm_barr[0],
                                           A[5], sdm_barr[0], A[7], sdm_barr[0]};
            CHECK(memcmp(got, want2, 64) == 0, "sdm evex512 vunpckhpd {1to8} high half");
            sdm_probes++;
            sdm_vunpcklps_bcast(got32);
            unsigned int want3[16];
            for (unsigned lane = 0; lane < 4; lane++) {
                want3[4 * lane + 0] = ad[4 * lane + 0];
                want3[4 * lane + 1] = sdm_barr32[0];
                want3[4 * lane + 2] = ad[4 * lane + 1];
                want3[4 * lane + 3] = sdm_barr32[0];
            }
            CHECK(memcmp(got32, want3, 64) == 0, "sdm evex512 vunpcklps {1to16} dword elem");
            sdm_probes++;
            sdm_vunpckhps_bcast(got32);
            unsigned int want4[16];
            for (unsigned lane = 0; lane < 4; lane++) {
                want4[4 * lane + 0] = ad[4 * lane + 2];
                want4[4 * lane + 1] = sdm_barr32[0];
                want4[4 * lane + 2] = ad[4 * lane + 3];
                want4[4 * lane + 3] = sdm_barr32[0];
            }
            CHECK(memcmp(got32, want4, 64) == 0, "sdm evex512 vunpckhps {1to16} high dwords");
            /* (silent on success: stdout must stay identical to the native run) */
        }

        /* EVEX masked VPSHUFD, merging mask k1=0x0005 (dwords 0 and 2 of
         * each lane written): the write mask is DWORD-granular for VPSHUFD
         * (k1 bit j = dest dword j).  The old emulator shared the kind-4
         * qword elem, so it took whole qwords from the result: q0 came out
         * [24,23] and q2 [28,27] with old dwords elsewhere -- every pinned
         * dword below differs somewhere in the pre-fix output.
         * The expected arrays are built BEFORE the probe calls: the probes
         * are basic-asm bodies, which tell GCC (via -fipa-ra) that they
         * clobber NO caller-saved registers, so a zero-fill scheduled after
         * a call would otherwise be filled with the probe's leftover %eax
         * (the kmovd immediate) instead of 0 -- observed as want arrays
         * holding the mask value. */
        {
            unsigned int got[16];
            unsigned int want[16] = {0x24u, 0xD001u, 0x22u, 0xD003u,
                                     0xD004u, 0xD005u, 0xD006u, 0xD007u,
                                     0xD008u, 0xD009u, 0xD00au, 0xD00bu,
                                     0xD00cu, 0xD00du, 0xD00eu, 0xD00fu};
            unsigned int wantz[16] = {0x24u, 0, 0x22u, 0,
                                      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
            sdm_probes++;
            sdm_pshufd_masked(got);
            CHECK(memcmp(got, want, 64) == 0, "sdm evex512 vpshufd {k1} dword merge");
            sdm_probes++;
            sdm_pshufd_maskedz(got);
            CHECK(memcmp(got, wantz, 64) == 0, "sdm evex512 vpshufd {k1}{z} dword zeroing");
            /* (silent on success: stdout must stay identical to the native run) */
        }

        /* EVEX VPTERNLOGQ {1to8} embedded broadcast, unmasked: the form the
         * pre-fix emulator killed with "impossible EVEX broadcast element
         * size" (exit 134) on legal code.  imm 0x96 = 3-input XOR, so every
         * dest qword is A[q] ^ barr[0] ^ canary[q] with the broadcast
         * element replicated across all eight qwords. */
        {
            unsigned long long got[8];
            unsigned long long want[8];
            for (unsigned q = 0; q < 8; q++)
                want[q] = A[q] ^ sdm_barr[0] ^ sdm_canary[q];
            sdm_probes++;
            sdm_ternlogq_bcast(got);
            CHECK(memcmp(got, want, 64) == 0, "sdm evex512 vpternlogq {1to8} bcast");
            /* (silent on success: stdout must stay identical to the native run) */
        }

        /* EVEX masked VPTERNLOGQ: the mask is QWORD-granular (k1 bit j =
         * dest qword j).  imm 0xFF produces all ones for the enabled lane;
         * the old dest is distinct from both sources.  The pre-fix emulator
         * merged dwords: only dest dword 1 was written (q0 = a mix of old
         * dword 0 and the result), and q1 wrongly kept the old dest. */
        {
            unsigned long long got[8];
            unsigned long long want[8] = {sdm_tl_old64[0], 0xFFFFFFFFFFFFFFFFULL,
                                          sdm_tl_old64[2], sdm_tl_old64[3],
                                          sdm_tl_old64[4], sdm_tl_old64[5],
                                          sdm_tl_old64[6], sdm_tl_old64[7]};
            unsigned long long wantz[8] = {0, 0xFFFFFFFFFFFFFFFFULL,
                                           0, 0, 0, 0, 0, 0};
            sdm_probes++;
            sdm_ternlogq_masked(got);
            CHECK(memcmp(got, want, 64) == 0, "sdm evex512 vpternlogq {k1} qword merge");
            sdm_probes++;
            sdm_ternlogq_maskedz(got);
            CHECK(memcmp(got, wantz, 64) == 0, "sdm evex512 vpternlogq {k1}{z} qword zeroing");
            /* (silent on success: stdout must stay identical to the native run) */
        }

        /* EVEX masked VPTERNLOGD: the D twin stays DWORD-granular (k1 bit j
         * = dest dword j over the 16 dword lanes) -- pins the elem table
         * from both sides so the Q fix cannot leak into D. */
        {
            unsigned int got[16];
            unsigned int want[16] = {sdm_tl_old32[0], 0xFFFFFFFFu, sdm_tl_old32[2],
                                     sdm_tl_old32[3], sdm_tl_old32[4], sdm_tl_old32[5],
                                     sdm_tl_old32[6], sdm_tl_old32[7], sdm_tl_old32[8],
                                     sdm_tl_old32[9], sdm_tl_old32[10], sdm_tl_old32[11],
                                     sdm_tl_old32[12], sdm_tl_old32[13], sdm_tl_old32[14],
                                     sdm_tl_old32[15]};
            sdm_probes++;
            sdm_ternlogd_masked(got);
            CHECK(memcmp(got, want, 64) == 0, "sdm evex512 vpternlogd {k1} dword merge");
            /* (silent on success: stdout must stay identical to the native run) */
        }

        /* EVEX VPERMQ/VPERMPD vector-control (variable-index) forms: the
         * index vector is the vvvv operand and the data is the rm operand
         * (see the probe-function comment block above), with GLOBAL
         * mod-8 indexing.  The register-index probes died with exit 132
         * pre-fix and the memory-data probes were silently wrong. */
        {
            unsigned long long got[8];
            sdm_probes++;
            sdm_vpermq_regidx(got);
            unsigned long long want[8] = {17, 16, 15, 14, 13, 12, 11, 10};
            CHECK(memcmp(got, want, 64) == 0, "sdm evex512 vpermq vecctl global");
            sdm_probes++;
            sdm_vpermq_memdat(got);
            CHECK(memcmp(got, want, 64) == 0, "sdm evex512 vpermq memdata global");
            unsigned long long want2[8] = {11, 10, 13, 12, 15, 14, 17, 16};
            sdm_probes++;
            sdm_vpermq_memdat2(got);
            CHECK(memcmp(got, want2, 64) == 0, "sdm evex512 vpermq memdata idx2");
            unsigned long long wantb[8];
            for (unsigned i = 0; i < 8; i++) wantb[i] = sdm_barr[0];
            sdm_probes++;
            sdm_vpermq_memdat_bcast(got);
            CHECK(memcmp(got, wantb, 64) == 0, "sdm evex512 vpermq memdata {1to8}");
            unsigned long long wantm[8] = {17, SDM_CANARY_Q1, 15, SDM_CANARY_Q3,
                                           SDM_CANARY_Q4, 12, SDM_CANARY_Q6, 10};
            sdm_probes++;
            sdm_vpermq_regidx_masked(got);
            CHECK(memcmp(got, wantm, 64) == 0, "sdm evex512 vpermq vecctl {k1} qword merge");
            sdm_probes++;
            sdm_vpermpd_regidx(got);
            CHECK(memcmp(got, want, 64) == 0, "sdm evex512 vpermpd vecctl global");
            sdm_probes++;
            sdm_vpermpd_memdat(got);
            CHECK(memcmp(got, want, 64) == 0, "sdm evex512 vpermpd memdata global");
            sdm_probes++;
            sdm_vpermpd_regidx_masked(got);
            CHECK(memcmp(got, wantm, 64) == 0, "sdm evex512 vpermpd vecctl {k1} qword merge");
            /* (silent on success: stdout must stay identical to the native run) */
        }

        /* VPERMD kind-2 pin (register-index, dword granularity, ymm): the
         * Q/PD variable form implements the SAME global mod-lanes rule as
         * this hardware-verified path -- dest[i] = data[idx[i] & 7]. */
        {
            unsigned int got[8];
            unsigned int want[8] = {107, 106, 105, 104, 103, 102, 101, 100};
            sdm_probes++;
            sdm_vpermd_pin(got);
            CHECK(memcmp(got, want, 32) == 0, "sdm vpermd regidx global pin");
            /* (silent on success: stdout must stay identical to the native run) */
        }

        /* EVEX VPERMQ/VPERMPD imm8 forms: the imm path must keep working
         * after the vector-control dispatch split (the battery had no
         * kind-4 imm checks before).  imm 0x1B over data 10..17 selects
         * within each 256-bit half, same selects both halves:
         * [13,12,11,10,17,16,15,14]. */
        {
            unsigned long long got[8];
            unsigned long long want[8] = {13, 12, 11, 10, 17, 16, 15, 14};
            sdm_probes++;
            sdm_vpermq_imm(got);
            CHECK(memcmp(got, want, 64) == 0, "sdm evex512 vpermq imm8 per-half");
            sdm_probes++;
            sdm_vpermpd_imm(got);
            CHECK(memcmp(got, want, 64) == 0, "sdm evex512 vpermpd imm8 per-half");
            /* (silent on success: stdout must stay identical to the native run) */
        }

        /* Report the executed-probe count on STDERR: the shufpd case in
         * run-tests compares stdout byte-for-byte between the native and the
         * emulated runs, so stdout must stay identical; stderr is visible in
         * a manual ./zegarmistrz run and discarded by the case. */
        fprintf(stderr, "sdm avx512 probes executed: %d\n", sdm_probes);
    }
    if (failures == 0) {
        printf("shufpd-sdm-ok\n");
        return 0;
    }
    printf("shufpd-sdm FAILED: %d\n", failures);
    return 1;
}

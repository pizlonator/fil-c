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
// Instruction-semantics regression guest for the SHUFPD family -- legacy
// SHUFPD (66 0F C6), VSHUFPD (VEX.128/VEX.256/EVEX.512), the SHUFPS
// companions whose roles the PD form must NOT borrow, the VPERMIL imm-form
// control rules (PD global vs PS per-lane), and the AVX-512 SDM-conformance
// battery for the perm AND convert AND ternlog families (masked VSHUFPD and
// masked VPSHUFD, the {1toN} embedded broadcasts including the REAL
// vpternlogq m512{1to8} form, masked+broadcast VPERMILPD, the EVEX
// VPERMILPD/PS imm forms, the widening/narrowing converts whose mask and
// broadcast granularity follow the SOURCE element, the masked
// VPTERNLOGQ/VPTERNLOGD merges whose granularity is per-mnemonic, and the
// VPERMQ/VPERMPD vector-control (variable-index) forms whose index vector
// is the vvvv operand, whose data is the rm operand, and whose low-3-bit
// index elements select GLOBALLY across the whole source -- plus the imm8
// forms and the VPERMD kind-2 pin that share the rule book).
//
// The serious bug this pins: gcc -O3 builds of djb supercop crypto_core/
// keccakf160064bits/reference1600 emit eight shufpd $1 sites per Keccak
// round (the theta column D values are built with punpcklqdq + shufpd $1),
// and the emulator computed [imm0?SRC.q0:DEST.q0, imm1?SRC.q1:DEST.q1] --
// picking the SOURCE by the imm bit -- instead of the hardware rule
//
//   out.q[0] = imm[0] ? DEST.q[1] : DEST.q[0]    (selects WITHIN DEST)
//   out.q[1] = imm[1] ? SRC.q[1]  : SRC.q[0]    (selects WITHIN SRC)
//
// which swapped a pair of theta columns every round and silently permuted
// the whole 1600-bit state differently from bare metal (no fault, wrong
// output: fnv fcfa0f4eb454b7d4 native vs 8496bcb3d78151b4 emulated).
// Hardware-verified on i9-14900HX: with DEST=[A,A] and SRC=[B,B] every
// imm8 yields [A.lo, B.lo]; gcc -O0 folds _mm_shuffle_pd imm0 to UNPCKLPD
// and imm3 to UNPCKHPD, pinning DEST.lo / SRC.hi as the imm=0 endpoints.
// The VEX.256 imm bits index GLOBAL qwords (the upper lane uses bits 2/3,
// unlike VSHUFPS which restarts its bit pairs per lane); EVEX.512 follows
// the same global rule per the SDM pseudo-code (not hardware-checkable on
// this host, which lacks AVX512 -- the group skips consistently natively
// and under the emulator there).
// The SHUFPS groups pin the PS rule the PD code shares a handler with:
// output dwords 0/1 select WITHIN DEST by imm bits [1:0]/[3:2], dwords 2/3
// select WITHIN SRC by imm bits [5:4]/[7:6], and the bit pairs restart per
// lane at 256-bit (verified: imm=0x11 gives lane1 = [src1.d1, src1.d0,
// src2.d1, src2.d0]).
// Also runs the full 24-round Keccak-f[1600] permutation of the djb
// reference core over a deterministic state and checks the FNV-1a hash of
// the result end to end (the workload that reported the bug).
// Gating: the legacy/VEX groups are gated on runtime CPUID (AVX) so they
// run everywhere, natively AND under the emulator (the native-vs-emulated
// mirror).  The AVX-512 SDM-conformance battery is gated on
// is_in_zegarmistrz() instead: the emulator implements AVX-512 in software
// on ANY host, but this host has no AVX-512, so executing EVEX natively
// would SIGILL -- the battery therefore runs ONLY under the emulator and is
// silent on success (no stdout output), keeping the native and emulated
// stdout byte-identical for the mirror case; a failure prints a BAD marker
// and exits nonzero.  A manual run under ./zegarmistrz reports the number
// of SDM probes executed on stderr.  Prints shufpd-ok.
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
}

__attribute__((noinline, used)) static void
sdm_shufpd_bcast(unsigned long long *out) {
    /* vshufpd $0x00, m64{1to8}, %zmm1, %zmm0: imm 0 keeps the even qwords in
     * src1; every odd qword is the SINGLE broadcast element barr[0]. */
    asm volatile(
        "vmovups A(%rip), %zmm1\n\t"
        "vshufpd $0x00, sdm_barr(%rip){1to8}, %zmm1, %zmm0\n\t"
        "vmovups %zmm0, (%rdi)\n\t");
}

__attribute__((noinline, used)) static void
sdm_pshufd_bcast(unsigned int *out) {
    /* vpshufd $0xE4, m512{1to16}, %zmm0: identity dword shuffle; the
     * broadcast replicates barr32[0] into all sixteen dwords. */
    asm volatile(
        "vpshufd $0xE4, sdm_barr32(%rip){1to16}, %zmm0\n\t"
        "vmovups %zmm0, (%rdi)\n\t");
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
}

__attribute__((noinline, used)) static void
sdm_ps2pd_bcast(double *out) {
    /* vcvtps2pd m256{1to8}, %zmm0: every dest double = (double)1.5f.  The
     * pre-fix emulator wrote 16 lanes here (stack OOB). */
    asm volatile(
        "vcvtps2pd sdm_cvt_f32(%rip){1to8}, %zmm0\n\t"
        "vmovups %zmm0, (%rdi)\n\t");
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
}

__attribute__((noinline, used)) static void
sdm_evex_vpermilpd_imm2(unsigned long long *out) {
    /* vpermilpd $0x5A, A(%rip), %zmm0: bits = 0101 1010 -> dest.q[j] takes
     * the pair member by bit j: A[0],A[1],A[2],A[3],A[5],A[4],A[7],A[6]. */
    asm volatile(
        "vpermilpd $0x5A, A(%rip), %zmm0\n\t"
        "vmovups %zmm0, (%rdi)\n\t");
}

__attribute__((noinline, used)) static void
sdm_evex_vpermilps_imm(unsigned int *out) {
    /* vpermilps $0x1B, A(%rip), %zmm0: per lane the 2-bit selects restart
     * (bits [1:0]/[3:2]/[5:4]/[7:6] = 3/2/1/0): dest dwords per lane =
     * A.d[3],A.d[2],A.d[1],A.d[0]. */
    asm volatile(
        "vpermilps $0x1B, A(%rip), %zmm0\n\t"
        "vmovups %zmm0, (%rdi)\n\t");
}

__attribute__((noinline, used)) static void
sdm_evex_vpermilps_imm2(unsigned int *out) {
    /* vpermilps $0xE4, A(%rip), %zmm0: identity per lane (bits pairs
     * 00/01/10/11). */
    asm volatile(
        "vpermilps $0xE4, A(%rip), %zmm0\n\t"
        "vmovups %zmm0, (%rdi)\n\t");
}

__attribute__((noinline, used)) static void
sdm_evex_vpermilpd_imm_bcast(unsigned long long *out) {
    /* vpermilpd $0xAA, m64{1to8}, %zmm0 (imm form with embedded broadcast):
     * the broadcast qword is BOTH members of every dest pair, so every dest
     * qword is sdm_barr[0] regardless of the imm bits. */
    asm volatile(
        "vpermilpd $0xAA, sdm_barr(%rip){1to8}, %zmm0\n\t"
        "vmovups %zmm0, (%rdi)\n\t");
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
}

__attribute__((noinline, used)) static void
sdm_vunpckhpd_bcast(unsigned long long *out) {
    /* vunpckhpd m512{1to8}, %zmm1, %zmm0: the HIGH qword of each lane:
     * dest.q[2L] = A[2L+1], dest.q[2L+1] = the broadcast qword. */
    asm volatile(
        "vmovups A(%rip), %zmm1\n\t"
        "vunpckhpd sdm_barr(%rip){1to8}, %zmm1, %zmm0\n\t"
        "vmovups %zmm0, (%rdi)\n\t");
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
}

__attribute__((noinline, used)) static void
sdm_vunpckhps_bcast(unsigned int *out) {
    /* vunpckhps m512{1to16}, %zmm1, %zmm0: dest.d per lane = [a.d2, bc,
     * a.d3, bc, a.d4, bc, a.d5, bc] -- the HIGH dwords of each lane. */
    asm volatile(
        "vmovups A(%rip), %zmm1\n\t"
        "vunpckhps sdm_barr32(%rip){1to16}, %zmm1, %zmm0\n\t"
        "vmovups %zmm0, (%rdi)\n\t");
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
}

__attribute__((noinline, used)) static void
sdm_vpermpd_memdat(unsigned long long *out) {
    /* vpermpd sdm_perm_dat(%rip), %zmm1, %zmm0: the PD memory-data twin. */
    asm volatile(
        "vmovups sdm_perm_idx(%rip), %zmm1\n\t"
        "vpermpd sdm_perm_dat(%rip), %zmm1, %zmm0\n\t"
        "vmovups %zmm0, (%rdi)\n\t");
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
}

__attribute__((noinline, used)) static void
sdm_vpermpd_imm(unsigned long long *out) {
    /* vpermpd $0x1B, %zmm1, %zmm0: the PD imm8 twin. */
    asm volatile(
        "vmovups sdm_perm_dat(%rip), %zmm1\n\t"
        "vpermpd $0x1B, %zmm1, %zmm0\n\t"
        "vmovups %zmm0, (%rdi)\n\t");
}

int main(void) {
    /* ---- legacy SHUFPD reg-reg, imm 0..3 (SSE2, always present) ---- */
    {
        unsigned long long dst[2] = {A[0], A[1]};
        unsigned long long src[2] = {B[0], B[1]};
#define TEST_LEGACY_PD(imm) do { \
    unsigned long long got[4]; \
    asm volatile("movups %1, %%xmm0\n\t" \
                 "movups %2, %%xmm1\n\t" \
                 "shufpd $" #imm ", %%xmm1, %%xmm0\n\t" \
                 "movups %%xmm0, %0" \
                 : "=m"(got) : "m"(dst), "m"(src) : "%xmm0", "%xmm1"); \
    unsigned long long want[4] = {exp_pd_q(imm, 0, 0), exp_pd_q(imm, 0, 1), 0, 0}; \
    CHECK(memcmp(got, want, 16) == 0, "legacy shufpd reg-reg imm=" #imm); \
    printf("legacy shufpd imm=" #imm " done\n"); \
} while (0)
        TEST_LEGACY_PD(0);
        TEST_LEGACY_PD(1);
        TEST_LEGACY_PD(2);
        TEST_LEGACY_PD(3);
#undef TEST_LEGACY_PD
    }
    /* ---- legacy SHUFPD with the m128 source form ---- */
    {
        unsigned long long dst[2] = {A[0], A[1]};
#define TEST_LEGACY_PD_MEM(imm) do { \
    unsigned long long got[4]; \
    asm volatile("movups %1, %%xmm0\n\t" \
                 "shufpd $" #imm ", %2, %%xmm0\n\t" \
                 "movups %%xmm0, %0" \
                 : "=m"(got) : "m"(dst), "m"(B) : "%xmm0"); \
    unsigned long long want[4] = {exp_pd_q(imm, 0, 0), exp_pd_q(imm, 0, 1), 0, 0}; \
    CHECK(memcmp(got, want, 16) == 0, "legacy shufpd m128 imm=" #imm); \
    printf("legacy shufpd-mem imm=" #imm " done\n"); \
} while (0)
        TEST_LEGACY_PD_MEM(0);
        TEST_LEGACY_PD_MEM(1);
        TEST_LEGACY_PD_MEM(2);
        TEST_LEGACY_PD_MEM(3);
#undef TEST_LEGACY_PD_MEM
    }
    /* ---- legacy SHUFPS reg-reg (pin the PS roles the PD code shares a
     * handler with) ---- */
    {
        const unsigned int Ad[4] = {0xA0A0A0A0u, 0xA1A1A1A1u, 0xA2A2A2A2u, 0xA3A3A3A3u};
        const unsigned int Bd[4] = {0xB0B0B0B0u, 0xB1B1B1B1u, 0xB2B2B2B2u, 0xB3B3B3B3u};
#define TEST_LEGACY_PS(imm) do { \
    unsigned int got[4]; \
    asm volatile("movups %1, %%xmm0\n\t" \
                 "movups %2, %%xmm1\n\t" \
                 "shufps $" #imm ", %%xmm1, %%xmm0\n\t" \
                 "movups %%xmm0, %0" \
                 : "=m"(got) : "m"(Ad), "m"(Bd) : "%xmm0", "%xmm1"); \
    unsigned int want[4]; \
    for (unsigned i = 0; i < 4; i++) { \
        unsigned sel = ((imm) >> (2 * i)) & 3; \
        const unsigned int *s = (i < 2) ? Ad : Bd; \
        want[i] = s[sel]; \
    } \
    CHECK(memcmp(got, want, 16) == 0, "legacy shufps reg-reg imm=" #imm); \
    printf("legacy shufps imm=" #imm " done\n"); \
} while (0)
        TEST_LEGACY_PS(0x00);
        TEST_LEGACY_PS(0x44);
        TEST_LEGACY_PS(0xE4);
        TEST_LEGACY_PS(0x1B);
        TEST_LEGACY_PS(0xFF);
#undef TEST_LEGACY_PS
    }
    if (have_avx()) {
        /* ---- VEX.128 VSHUFPD: dst,src1,src2,imm; upper ymm stays zeroed ---- */
        {
#define TEST_VEX128_PD(imm) do { \
    unsigned long long got[4]; \
    asm volatile("vmovups %1, %%ymm1\n\t" \
                 "vmovups %2, %%ymm2\n\t" \
                 "vshufpd $" #imm ", %%xmm2, %%xmm1, %%xmm0\n\t" \
                 "vmovups %%ymm0, %0" \
                 : "=m"(got) : "m"(A), "m"(B) : "%ymm0", "%ymm1", "%ymm2"); \
    unsigned long long want[4] = {exp_pd_q(imm, 0, 0), exp_pd_q(imm, 0, 1), 0, 0}; \
    CHECK(memcmp(got, want, 16) == 0, "vex128 vshufpd imm=" #imm); \
    printf("vex128 vshufpd imm=" #imm " done\n"); \
} while (0)
            TEST_VEX128_PD(0);
            TEST_VEX128_PD(1);
            TEST_VEX128_PD(2);
            TEST_VEX128_PD(3);
            TEST_VEX128_PD(0x55);
#undef TEST_VEX128_PD
        }
        /* ---- VEX.256 VSHUFPD: global imm bits, upper lane uses bits 2/3 ---- */
        {
#define TEST_VEX256_PD(imm) do { \
    unsigned long long got[4]; \
    asm volatile("vmovups %1, %%ymm1\n\t" \
                 "vmovups %2, %%ymm2\n\t" \
                 "vshufpd $" #imm ", %%ymm2, %%ymm1, %%ymm0\n\t" \
                 "vmovups %%ymm0, %0" \
                 : "=m"(got) : "m"(A), "m"(B) : "%ymm0", "%ymm1", "%ymm2"); \
    unsigned long long want[4] = {exp_pd_q(imm, 0, 0), exp_pd_q(imm, 0, 1), \
                                  exp_pd_q(imm, 1, 0), exp_pd_q(imm, 1, 1)}; \
    CHECK(memcmp(got, want, 32) == 0, "vex256 vshufpd imm=" #imm); \
    printf("vex256 vshufpd imm=" #imm " done\n"); \
} while (0)
            TEST_VEX256_PD(0);
            TEST_VEX256_PD(1);
            TEST_VEX256_PD(2);
            TEST_VEX256_PD(3);
            TEST_VEX256_PD(5);
            TEST_VEX256_PD(0x0A);
            TEST_VEX256_PD(0x0F);
            TEST_VEX256_PD(0x55);
            TEST_VEX256_PD(0xAA);
#undef TEST_VEX256_PD
        }
        /* ---- VEX.256 VSHUFPS: bit pairs restart per lane ---- */
        {
#define TEST_VEX256_PS(imm) do { \
    unsigned int got[8]; \
    asm volatile("vmovups %1, %%ymm1\n\t" \
                 "vmovups %2, %%ymm2\n\t" \
                 "vshufps $" #imm ", %%ymm2, %%ymm1, %%ymm0\n\t" \
                 "vmovups %%ymm0, %0" \
                 : "=m"(got) : "m"(A), "m"(B) : "%ymm0", "%ymm1", "%ymm2"); \
    unsigned int want[8]; \
    for (unsigned lane = 0; lane < 2; lane++) \
        for (unsigned i = 0; i < 4; i++) want[4 * lane + i] = exp_ps_d(imm, lane, i); \
    CHECK(memcmp(got, want, 32) == 0, "vex256 vshufps per-lane bits imm=" #imm); \
    printf("vex256 vshufps imm=" #imm " done\n"); \
} while (0)
            TEST_VEX256_PS(0x11);
            TEST_VEX256_PS(0x44);
            TEST_VEX256_PS(0xE4);
            TEST_VEX256_PS(0x1B);
            TEST_VEX256_PS(0xFF);
#undef TEST_VEX256_PS
        }

        /* ---- VPERMILPD variable-form control rule (VEX.256, hardware-
         * verified differential on this AVX2 host): the control bit is BIT 1
         * of each control qword, read per DEST qword, selecting within that
         * 128-bit lane's qword pair.  Bit 0 and bit 2 must do nothing (the
         * old code read bit 0, so controls with only bit 1 set selected the
         * wrong qword). ---- */
        {
            static const unsigned long long P[4] = {0xA0A0A0A0A0A0A0A0ULL,
                                                    0xA1A1A1A1A1A1A1A1ULL,
                                                    0xA2A2A2A2A2A2A2A2ULL,
                                                    0xA3A3A3A3A3A3A3A3ULL};
#define TEST_VPERMILPD_VAR(c0, c1, c2, c3, want0, want1, want2, want3, name) do { \
    unsigned long long got[4]; \
    const unsigned long long ctl[4] = {(c0), (c1), (c2), (c3)}; \
    asm volatile("vmovups %1, %%ymm0\n\t" \
                 "vmovups %2, %%ymm1\n\t" \
                 "vpermilpd %%ymm1, %%ymm0, %%ymm2\n\t" \
                 "vmovups %%ymm2, %0" \
                 : "=m"(got) : "m"(P), "m"(ctl) : "%ymm0", "%ymm1", "%ymm2"); \
    unsigned long long want[4] = {(want0), (want1), (want2), (want3)}; \
    CHECK(memcmp(got, want, 32) == 0, name); \
    printf(name " done\n"); \
} while (0)
            /* bit 1 of q0 set -> dest.q0 = P1, dest.q1 = P0; bit 1 of q2 set
             * -> dest.q2 = P3, dest.q3 = P2. */
            TEST_VPERMILPD_VAR(0x2, 0x0, 0x2, 0x0,
                               P[1], P[0], P[3], P[2],
                               "vpermilpd var bit1 controls");
            /* Bits 0 and 2 set everywhere must NOT change the selection:
             * sel stays 0, so each dest qword keeps its pair's LOW member
             * (dest = [P0, P0, P2, P2] -- the variable form always selects
             * within the dest qword's own pair, unlike the imm form whose
             * dest.q1 selects by its own imm bit; hardware-verified). */
            TEST_VPERMILPD_VAR(0x1, 0x1, 0x1, 0x1,
                               P[0], P[0], P[2], P[2],
                               "vpermilpd var bit0 ignored");
            TEST_VPERMILPD_VAR(0x4, 0x4, 0x4, 0x4,
                               P[0], P[0], P[2], P[2],
                               "vpermilpd var bit2 ignored");
#undef TEST_VPERMILPD_VAR
        }

        /* ---- VPERMILPD imm-form global imm indexing (VEX.256, hardware-
         * verified differential sweep over ALL 16 immediates): dest.q[j]
         * selects within its 128-bit lane's qword pair by imm bit j with j
         * the GLOBAL dest qword number -- the upper lane uses bits 2/3, it
         * does NOT restart at bit 0.  The old emulator restarted per lane,
         * so 12 of 16 immediates diverged from bare metal (imm=$3 gave
         * [a1,a1,a3,a3] instead of [a1,a1,a2,a2]; imm=$0xC gave
         * [a0,a0,a2,a2] instead of [a0,a0,a3,a3]).  VPERMILPS restarts its
         * 2-bit pairs per lane (imm8 holds only four pairs) -- pinned here
         * so the PD fix does not leak into the PS rule the same handler
         * shares. ---- */
        {
            static const unsigned long long V[4] = {0x1111111111111111ULL,
                                                    0x2222222222222222ULL,
                                                    0x3333333333333333ULL,
                                                    0x4444444444444444ULL};
#define TEST_VPERMILPD_IMM256(imm) do { \
    unsigned long long got[4]; \
    asm volatile("vmovups %1, %%ymm1\n\t" \
                 "vpermilpd $" #imm ", %%ymm1, %%ymm0\n\t" \
                 "vmovups %%ymm0, %0" \
                 : "=m"(got) : "m"(V) : "%ymm0", "%ymm1"); \
    unsigned long long want[4]; \
    for (unsigned j = 0; j < 4; j++) \
        want[j] = V[(j & ~1u) + (((imm) >> j) & 1)]; \
    CHECK(memcmp(got, want, 32) == 0, "vpermilpd imm256 global bits imm=" #imm); \
    printf("vpermilpd imm256 imm=" #imm " done\n"); \
} while (0)
            TEST_VPERMILPD_IMM256(0x0);
            TEST_VPERMILPD_IMM256(0x1);
            TEST_VPERMILPD_IMM256(0x2);
            TEST_VPERMILPD_IMM256(0x3);
            TEST_VPERMILPD_IMM256(0x4);
            TEST_VPERMILPD_IMM256(0x5);
            TEST_VPERMILPD_IMM256(0x6);
            TEST_VPERMILPD_IMM256(0x7);
            TEST_VPERMILPD_IMM256(0x8);
            TEST_VPERMILPD_IMM256(0x9);
            TEST_VPERMILPD_IMM256(0xA);
            TEST_VPERMILPD_IMM256(0xB);
            TEST_VPERMILPD_IMM256(0xC);
            TEST_VPERMILPD_IMM256(0xD);
            TEST_VPERMILPD_IMM256(0xE);
            TEST_VPERMILPD_IMM256(0xF);
#undef TEST_VPERMILPD_IMM256
        }
        {
            static const unsigned int W[8] = {0x11111111u, 0x22222222u, 0x33333333u,
                                              0x44444444u, 0x55555555u, 0x66666666u,
                                              0x77777777u, 0x88888888u};
#define TEST_VPERMILPS_IMM256(imm) do { \
    unsigned int got[8]; \
    asm volatile("vmovups %1, %%ymm1\n\t" \
                 "vpermilps $" #imm ", %%ymm1, %%ymm0\n\t" \
                 "vmovups %%ymm0, %0" \
                 : "=m"(got) : "m"(W) : "%ymm0", "%ymm1"); \
    unsigned int want[8]; \
    for (unsigned lane = 0; lane < 2; lane++) \
        for (unsigned i = 0; i < 4; i++) \
            want[4 * lane + i] = W[4 * lane + (((imm) >> (2 * i)) & 3)]; \
    CHECK(memcmp(got, want, 32) == 0, "vpermilps imm256 per-lane pairs imm=" #imm); \
    printf("vpermilps imm256 imm=" #imm " done\n"); \
} while (0)
            TEST_VPERMILPS_IMM256(0x3);
            TEST_VPERMILPS_IMM256(0xC);
            TEST_VPERMILPS_IMM256(0x1B);
            TEST_VPERMILPS_IMM256(0xE4);
            TEST_VPERMILPS_IMM256(0xFF);
#undef TEST_VPERMILPS_IMM256
        }
    } else {
        printf("skip avx groups\n");
    }
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
    /* ---- end to end: the djb reference Keccak-f[1600] core (the workload
     * that reported the bug).  Same LCG input and same FNV-1a hash as the
     * differential driver: every correct run of any implementation prints
     * fcfa0f4eb454b7d4. ---- */
    {
        typedef unsigned long long u64;
        u64 st[25];
        unsigned char inbuf[200];
        unsigned x = 0x12345678u;
        for (unsigned i = 0; i < 200; i++) {
            x = x * 1664525u + 1013904223u;
            inbuf[i] = (unsigned char)(x >> 24);
        }
        memcpy(st, inbuf, 200);
#define ROL64(a, offset) \
    ((offset != 0) ? ((((u64)a) << offset) ^ (((u64)a) >> (64 - offset))) : a)
#define KIDX(x, y) (((x) % 5) + 5 * ((y) % 5))
        static const unsigned int rho[25] = {0,  1,  62, 28, 27, 36, 44,  6,  55, 20,
                                             3,  10, 43, 25, 39, 41, 45, 15, 21, 8,
                                             18, 2,  61, 56, 14};
        static const u64 rc[24] = {
            0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL,
            0x8000000080008000ULL, 0x000000000000808bULL, 0x0000000080000001ULL,
            0x8000000080008081ULL, 0x8000000000008009ULL, 0x000000000000008aULL,
            0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000aULL,
            0x000000008000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL,
            0x8000000000008003ULL, 0x8000000000008002ULL, 0x8000000000000080ULL,
            0x000000000000800aULL, 0x800000008000000aULL, 0x8000000080008081ULL,
            0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL};
        for (unsigned r = 0; r < 24; r++) {
            u64 C[5], D[5];
            for (unsigned xx = 0; xx < 5; xx++) {
                C[xx] = 0;
                for (unsigned yy = 0; yy < 5; yy++) C[xx] ^= st[KIDX(xx, yy)];
            }
            for (unsigned xx = 0; xx < 5; xx++)
                D[xx] = ROL64(C[(xx + 1) % 5], 1) ^ C[(xx + 4) % 5];
            for (unsigned xx = 0; xx < 5; xx++)
                for (unsigned yy = 0; yy < 5; yy++) st[KIDX(xx, yy)] ^= D[xx];
            for (unsigned xx = 0; xx < 5; xx++)
                for (unsigned yy = 0; yy < 5; yy++)
                    st[KIDX(xx, yy)] = ROL64(st[KIDX(xx, yy)], rho[KIDX(xx, yy)]);
            u64 tmp[25];
            for (unsigned xx = 0; xx < 5; xx++)
                for (unsigned yy = 0; yy < 5; yy++) tmp[KIDX(xx, yy)] = st[KIDX(xx, yy)];
            for (unsigned xx = 0; xx < 5; xx++)
                for (unsigned yy = 0; yy < 5; yy++)
                    st[KIDX(0 * xx + 1 * yy, 2 * xx + 3 * yy)] = tmp[KIDX(xx, yy)];
            for (unsigned yy = 0; yy < 5; yy++) {
                for (unsigned xx = 0; xx < 5; xx++)
                    C[xx] = st[KIDX(xx, yy)] ^
                            ((~st[KIDX(xx + 1, yy)]) & st[KIDX(xx + 2, yy)]);
                for (unsigned xx = 0; xx < 5; xx++) st[KIDX(xx, yy)] = C[xx];
            }
            st[KIDX(0, 0)] ^= rc[r];
        }
        unsigned long long h = 1469598103934665603ULL;
        const unsigned char *p = (const unsigned char *)st;
        for (unsigned i = 0; i < 200; i++) {
            h ^= p[i];
            h *= 1099511628211ULL;
        }
        CHECK(h == 0xfcfa0f4eb454b7d4ULL, "keccakf1600 reference1600 fnv");
        printf("keccak fnv=%016llx\n", h);
    }
    if (failures == 0) {
        printf("shufpd-ok\n");
        return 0;
    }
    printf("shufpd FAILED: %d\n", failures);
    return 1;
}

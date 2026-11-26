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
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL FILIP PIZLO OR CONTRIBUTORS
// BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
// BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
// IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// native-trace: the x86sem-suite hardware oracle. Runs a freestanding
// _start guest on the REAL CPU and writes the whole architectural state
// after every guest instruction, in exactly the same line format as
// `zegarmistrz --state-trace=FILE` (see the emulator side). The resulting
// native trace is the oracle that x86sem-tests/compare-traces.py checks
// the emulator's trace against.
//
// How it steps without ptrace: this container (and some hardened hosts)
// block ptrace, so gdb single-stepping is unavailable here. Instead the
// dumper loads the guest into its own address space, arms the x86 trap
// flag (TF), and jumps to the guest's entry: every guest instruction then
// raises #DB, delivered as SIGTRAP, and the SIGTRAP handler dumps the
// state and re-arms TF for the next instruction. No ptrace, no
// privileges; one kernel round trip per instruction, which is also much
// faster than gdb single-stepping.
//
// The state after instruction k is captured from the signal frame the
// kernel built at trap delivery: GPRs + EFLAGS from ucontext
// uc_mcontext.gregs, and the vector state from the frame's xsave image
// (uc_mcontext.fpregs): the FXSAVE-format legacy area holds the LOW 128
// bits of every ymm in xmm_space (offset 160 + 16*N), and the AVX state
// component (offset from CPUID leaf 0xD, 576 here) holds the UPPER 128
// bits of every ymm (16 bytes per register), present iff XSTATE_BV has
// the AVX bit. Reading the LIVE vector registers inside the handler does
// NOT work: the kernel deactivates the FPU when delivering a signal, so
// the first FP instruction in the handler lazily restores a stale image,
// and glibc helpers (sprintf etc) clobber ymm0-15 with their own
// internals. The signal frame is the kernel's authoritative copy of the
// guest state at delivery; it is what a ptrace-based dumper would see.
//
// Trace format (identical on both sides; see compare-traces.py and the
// suite README):
//   S rip=<16hex> rax=<16hex> ... r15=<16hex> fl=<8hex> ymm0=<128hex> ... ymm15=<128hex>
// GPRs/rip are natural hex (MSB first), fl is the raw 32-bit EFLAGS, and
// each ymmN is the 512-bit register written as its eight qwords q7..q0,
// each qword big-endian, so the register's LOW qword is the LAST 16 hex
// chars. The upper 256 bits (bits 256..511 of the zmm) are always
// written as zero: the kernel zeroes them at execve, no corpus
// instruction writes above 256 bits (no EVEX), and the emulator side
// keeps them zero too, so both sides agree on zeros by construction.
//
// Termination: the imported corpus programs end with an appended
// exit-group epilogue (movq $60, %rax; xorq %rdi, %rdi; syscall). The
// handler detects the syscall instruction before it executes (the only
// syscall in any corpus program is that epilogue) and exits cleanly
// with status 0, so the trace ends exactly like the emulator's: after
// the last completed instruction, without a record for the syscall
// itself. A guest fault (SIGSEGV/SIGILL/...) flushes what was traced and
// exits 3; the 10000-step cap (matching the kframework harness) flushes
// and exits 0.
//
// Initial guest state matches what a Linux execve of the guest binary
// would establish: all GPRs zero, fresh deterministic stack with a
// minimal initial stack image (argc/argv/envp/auxv), EFLAGS = IF|TF|1
// (the comparator ignores all but CF/PF/AF/ZF/SF/OF), and the
// execve-initial FP state (x87 init, MXCSR 0x1f80, all vector registers
// zero). The FP state is fixed up at the first trap (rip == entry,
// before the guest's first instruction): the handler rewrites the
// signal frame's FP image to the execve-init values and hard-fails if
// the integer state is not clean, so glibc residue from this dumper's
// own startup can never leak into the guest.
//
// FP-extraction self-test: immediately before the guest is entered, all
// 16 ymm registers are loaded with DISTINCT known 256-bit probe
// patterns (both the xmm half and the ymm-upper half). The pre-entry
// trap then verifies the signal-frame extraction returns every pattern
// bit-exactly and dies loudly naming the mismatched register otherwise
// (see validate_entry_fp). This is the guard that makes a
// host/kernel-specific extraction failure (a different xsave layout, a
// kernel that does not save the AVX component into the frame, a
// different CPUID(0xD,2) offset) diagnose ITSELF instead of silently
// producing wrong ymm slots whose divergences masquerade as emulator
// bugs on some other machine. NT_NO_FIXUP and NT_DIRTY_FP (debug hooks)
// skip the planting and the self-test.

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <elf.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/personality.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ucontext.h>
#include <unistd.h>

// The guest code range (from the executable PT_LOADs) and entry point.
static uint64_t g_lo, g_hi, g_entry;
static int g_fd = -1;
static int g_started = 0;      // set once the pre-entry trap is seen
static long g_steps = 0;
static long g_cap = 10000;
static const char* g_guest_path = "";

// CPUID(0xD, 2).EBX: offset of the AVX state component in an
// uncompacted xsave area (576 on every x86-64 Linux: 512 legacy + 64
// header). Validated against the frame's magic at startup.
static unsigned g_avx_off = 576;

// FP-extraction self-test (see validate_entry_fp below): 16 DISTINCT
// 256-bit patterns, one per ymm register, written immediately before the
// guest is entered. The pre-entry trap then verifies the signal frame
// returned every pattern bit-exactly -- the low 128 bits from the frame's
// FXSAVE legacy xmm_space AND the upper 128 bits from the AVX xsave
// component (the extraction whose offset/gating is host- and
// kernel-dependent). A mismatch means this host/kernel's frame layout
// differs from what the dumper assumes, so every vector slot it traces
// would be garbage; the oracle then dies loudly instead of producing a
// native trace whose divergences look like emulator bugs. This is what
// makes a host-reportable divergence (e.g. an unknown kernel's xsave
// layout) diagnose itself on first run.
static unsigned long long g_fp_probe[16][4];
static int g_validate_fp = 1;

static void die(int code) {
    if (g_fd >= 0)
        close(g_fd);
    _exit(code);
}

static void cpuid(unsigned leaf, unsigned sub, unsigned* a, unsigned* b,
                  unsigned* c, unsigned* d) {
    __asm__ volatile("cpuid"
                     : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
                     : "a"(leaf), "c"(sub));
}

// Sanity-check the AVX feature set and the xsave component offset.
static void init_xsave_offsets(void) {
    unsigned a, b, c, d;
    cpuid(1, 0, &a, &b, &c, &d);
    if (!((c >> 26) & 1) || !((c >> 27) & 1)) {
        fprintf(stderr, "native-trace: CPU lacks xsave/OSXSAVE\n");
        exit(2);
    }
    unsigned xcr0_lo, xcr0_hi;
    __asm__ volatile("xgetbv" : "=a"(xcr0_lo), "=d"(xcr0_hi) : "c"(0));
    uint64_t xcr0 = xcr0_lo | ((uint64_t)xcr0_hi << 32);
    if (!(xcr0 & (1ULL << 2))) {
        fprintf(stderr, "native-trace: AVX not enabled in XCR0\n");
        exit(2);
    }
    cpuid(0xD, 2, &a, &b, &c, &d);
    if (b < 576 || a < 256) {
        fprintf(stderr,
                "native-trace: unexpected AVX xsave layout (off=%u size=%u)\n",
                b, a);
        exit(2);
    }
    g_avx_off = b;
}

// zsnap is not used for tracing (the signal frame is the source of
// truth) but only by the FP-state fix-up at guest entry, which must
// zero the registers without glibc involvement.
__asm__(
    ".globl zsnap\n"
    "zsnap:\n"
    "  vmovups %ymm0, 0x000(%rdi)\n"
    "  vmovups %ymm1, 0x020(%rdi)\n"
    "  vmovups %ymm2, 0x040(%rdi)\n"
    "  vmovups %ymm3, 0x060(%rdi)\n"
    "  vmovups %ymm4, 0x080(%rdi)\n"
    "  vmovups %ymm5, 0x0a0(%rdi)\n"
    "  vmovups %ymm6, 0x0c0(%rdi)\n"
    "  vmovups %ymm7, 0x0e0(%rdi)\n"
    "  vmovups %ymm8, 0x100(%rdi)\n"
    "  vmovups %ymm9, 0x120(%rdi)\n"
    "  vmovups %ymm10, 0x140(%rdi)\n"
    "  vmovups %ymm11, 0x160(%rdi)\n"
    "  vmovups %ymm12, 0x180(%rdi)\n"
    "  vmovups %ymm13, 0x1a0(%rdi)\n"
    "  vmovups %ymm14, 0x1c0(%rdi)\n"
    "  vmovups %ymm15, 0x1e0(%rdi)\n"
    "  ret\n");
extern void zsnap(void* buf);

// Zero all 16 ymm registers and set MXCSR to the execve-init value,
// without any glibc code running around it.
static void zero_fp_state(void) {
    unsigned mxcsr = 0x1f80;
    __asm__ volatile("fninit\n\t"
                     "fnclex\n\t"
                     "ldmxcsr %0\n\t"
                     "vpxor %%ymm0, %%ymm0, %%ymm0\n\t"
                     "vpxor %%ymm1, %%ymm1, %%ymm1\n\t"
                     "vpxor %%ymm2, %%ymm2, %%ymm2\n\t"
                     "vpxor %%ymm3, %%ymm3, %%ymm3\n\t"
                     "vpxor %%ymm4, %%ymm4, %%ymm4\n\t"
                     "vpxor %%ymm5, %%ymm5, %%ymm5\n\t"
                     "vpxor %%ymm6, %%ymm6, %%ymm6\n\t"
                     "vpxor %%ymm7, %%ymm7, %%ymm7\n\t"
                     "vpxor %%ymm8, %%ymm8, %%ymm8\n\t"
                     "vpxor %%ymm9, %%ymm9, %%ymm9\n\t"
                     "vpxor %%ymm10, %%ymm10, %%ymm10\n\t"
                     "vpxor %%ymm11, %%ymm11, %%ymm11\n\t"
                     "vpxor %%ymm12, %%ymm12, %%ymm12\n\t"
                     "vpxor %%ymm13, %%ymm13, %%ymm13\n\t"
                     "vpxor %%ymm14, %%ymm14, %%ymm14\n\t"
                     "vpxor %%ymm15, %%ymm15, %%ymm15\n\t"
                     :: "m"(mxcsr)
                     : "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
                       "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12",
                       "xmm13", "xmm14", "xmm15");
}

static char g_line[4096];
static uint64_t g_prev_rip = 0;

// Extract the 512-bit state of ymm0..15 into dst (32 bytes per register,
// low 256 bits in the low 32 bytes; upper 256 bits pinned zero) from the
// signal frame's xsave image. See the file comment for the layout and
// why the live registers cannot be read here.
static void frame_ymm(ucontext_t* uc, unsigned char* dst) {
    const unsigned char* fp = (const unsigned char*)uc->uc_mcontext.fpregs;
    // Each register gets a 64-byte slice holding the register image in
    // LITTLE-ENDIAN qword order: [low 128 bits (16 bytes), ymm upper 128
    // bits (16 bytes), 32 zero bytes]. The printer emits q[7]..q[0]
    // (descending), rendering the 512-bit value MSW-first with the
    // register's LOW qword as the last 16 hex chars. The upper 256 bits
    // are pinned zero (see the file comment).
    for (int r = 0; r < 16; r++) {
        memset(dst + 64 * r, 0, 64);
        // Low 128 bits: the FXSAVE legacy area's xmm_space at 160.
        if (fp)
            memcpy(dst + 64 * r, fp + 160 + 16 * r, 16);
    }
    if (!fp)
        return;
    // The frame's xsave header (magic "XSAV" = 0x46505853) sits at 464
    // with XSTATE_BV at +8. The AVX component holds only the upper 128
    // bits of each ymm, 16 bytes per register, at the CPUID(0xD,2).EBX
    // offset (576). If the AVX bit is clear the uppers are in init state
    // (zero). Fallback header location 512 is checked too in case a
    // different kernel writes the standard 512-legacy + 64-header area.
    unsigned hdr = 464;
    uint32_t magic = 0;
    memcpy(&magic, fp + hdr, 4);
    if (magic != 0x46505853u) {
        hdr = 512;
        memcpy(&magic, fp + hdr, 4);
    }
    if (magic != 0x46505853u) {
        fprintf(stderr,
                "native-trace: no xsave header in signal frame (magic=%08x)\n",
                magic);
        die(2);
    }
    uint64_t bv = 0;
    memcpy(&bv, fp + hdr + 8, 8);
    if (bv & (1ULL << 2)) {
        for (int r = 0; r < 16; r++)
            memcpy(dst + 64 * r + 16, fp + g_avx_off + 16 * r, 16);
    }
}

// Verify, at the pre-entry trap (before any guest instruction), that the
// signal-frame extraction returns the exact probe patterns planted in
// ymm0..15 before gogo. Both halves of every register must round-trip:
// the xmm (low 128) half via the FXSAVE legacy area and the ymm-upper
// (high 128) half via the AVX xsave component gated on XSTATE_BV.
static void validate_entry_fp(ucontext_t* uc) {
    unsigned char ybuf[16 * 64];
    frame_ymm(uc, ybuf); // dies loudly if the xsave header is missing
    for (int r = 0; r < 16; r++) {
        const unsigned char* got = ybuf + 64 * r;
        if (memcmp(got, g_fp_probe[r], 32) == 0)
            continue;
        unsigned long long* want = g_fp_probe[r];
        unsigned long long gl, gu;
        memcpy(&gl, got, 8);
        memcpy(&gu, got + 16, 8);
        fprintf(stderr,
                "native-trace: FP EXTRACTION SELF-TEST FAILED: ymm%d "
                "(low half expected %016llx%016llx, upper %016llx%016llx; "
                "frame gave low %016llx... upper %016llx...): the "
                "signal-frame vector-state extraction is wrong on this "
                "host/kernel (check the xsave layout: XSTATE_BV AVX gating "
                "and the CPUID(0xD,2) component offset); every ymm slot "
                "this oracle traces would be garbage\n",
                r, want[0], want[1], want[2], want[3], gl, gu);
        die(2);
    }
}

static void write_one(ucontext_t* uc) {
    unsigned char ybuf[16 * 64];
    frame_ymm(uc, ybuf);
    greg_t* g = uc->uc_mcontext.gregs;
    char* p = g_line;
    // gregs indices: R8..R15 = 0..7, RDI=8, RSI=9, RBP=10, RBX=11, RDX=12,
    // RAX=13, RCX=14, RSP=15, RIP=16, EFL=17.
    p += sprintf(p,
                 "S rip=%016llx rax=%016llx rbx=%016llx rcx=%016llx "
                 "rdx=%016llx rsi=%016llx rdi=%016llx rbp=%016llx rsp=%016llx",
                 (unsigned long long)g[16], (unsigned long long)g[13],
                 (unsigned long long)g[11], (unsigned long long)g[14],
                 (unsigned long long)g[12], (unsigned long long)g[9],
                 (unsigned long long)g[8], (unsigned long long)g[10],
                 (unsigned long long)g[15]);
    for (int i = 0; i < 8; i++)
        p += sprintf(p, " r%d=%016llx", 8 + i, (unsigned long long)g[i]);
    p += sprintf(p, " fl=%08llx", (unsigned long long)(g[17] & 0xffffffffULL));
    for (int r = 0; r < 16; r++) {
        unsigned long long* q = (unsigned long long*)(ybuf + 64 * r);
        p += sprintf(p, " ymm%d=", r);
        for (int k = 7; k >= 0; k--)
            p += sprintf(p, "%016llx", q[k]);
    }
    *p++ = '\n';
    if (write(g_fd, g_line, p - g_line) < 0) {
        perror("native-trace: write");
        die(9);
    }
}

// The trap after the pre-entry jump (rip == entry, no guest instruction
// executed yet): rewrite the signal frame's FP image to the execve-init
// state so the sigreturn installs it before the guest's first
// instruction, and verify the integer state is clean.
static void fixup_entry_frame(ucontext_t* uc) {
    unsigned char* fp = (unsigned char*)uc->uc_mcontext.fpregs;
    if (!fp) {
        fprintf(stderr, "native-trace: no fpregs in signal frame\n");
        die(2);
    }
    unsigned hdr = 464;
    uint32_t magic = 0;
    memcpy(&magic, fp + hdr, 4);
    if (magic != 0x46505853u) {
        hdr = 512;
        memcpy(&magic, fp + hdr, 4);
    }
    if (magic != 0x46505853u) {
        fprintf(stderr,
                "native-trace: no xsave header at guest entry (magic=%08x)\n",
                magic);
        die(2);
    }
    // execve-init FP image: FCW 0x37f, everything else zero, MXCSR
    // 0x1f80 (+ mask 0xffff), all st/xmm zero. Then mark x87|SSE in use
    // and AVX in init (uppers zero) in the xsave header (XSTATE_BV at
    // frame offset 512, the kernel's xsave_hdr_struct; the
    // fpx_sw_bytes copy at 472 is updated too) and zero the AVX
    // component bytes for good measure.
    const char* phase = getenv("NT_FIXUP_PHASE");
    int upto = phase ? atoi(phase) : 9;
    if (upto >= 1) {
        memset(fp, 0, 416);
        uint16_t fcw = 0x37f;
        memcpy(fp, &fcw, 2);
        uint32_t mxcsr = 0x1f80, mxcsr_mask = 0xffff;
        memcpy(fp + 24, &mxcsr, 4);
        memcpy(fp + 28, &mxcsr_mask, 4);
    }
    uint64_t bv = 0x203; // x87 | SSE | PKRU; AVX bit clear = uppers init
    if (upto >= 2)
        memcpy(fp + 512, &bv, 8);   // xsave_hdr_struct.xstate_bv
    if (upto >= 3)
        memcpy(fp + hdr + 8, &bv, 8); // fpx_sw_bytes.xstate_bv mirror
    if (upto >= 4)
        memset(fp + g_avx_off, 0, 256);
    // Integer state must already be clean (gogo zeroes it); verify.
    static const int greg_zero_check[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
                                          11, 12, 13, 14};
    greg_t* g = uc->uc_mcontext.gregs;
    for (unsigned i = 0; i < sizeof(greg_zero_check) / sizeof(int); i++) {
        if (g[greg_zero_check[i]] != 0) {
            fprintf(stderr,
                    "native-trace: guest entry greg #%d is nonzero (%llx); "
                    "the dumper is broken\n",
                    greg_zero_check[i], (unsigned long long)g[greg_zero_check[i]]);
            die(2);
        }
    }
    unsigned char ybuf[16 * 64];
    frame_ymm(uc, ybuf);
    for (unsigned i = 0; i < sizeof(ybuf); i++) {
        if (ybuf[i]) {
            fprintf(stderr,
                    "native-trace: guest entry vector state is nonzero "
                    "(byte %u = %02x); the dumper is broken\n",
                    i, ybuf[i]);
            die(2);
        }
    }
}

// True iff the bytes at rip start with a REP/REPNZ-prefixed string
// instruction (MOVS/CMPS/SCAS/LODS/STOS): the only instructions whose
// trap-flag single-stepping reports per-iteration.
static int is_rep_string(uint64_t rip) {
    unsigned char bytes[16];
    memcpy(bytes, (const void*)(uintptr_t)rip, sizeof(bytes));
    size_t i = 0;
    int rep = 0;
    while (i < sizeof(bytes)) {
        unsigned char b = bytes[i];
        if (b == 0x66 || b == 0x67 || b == 0xf0 || b == 0x2e || b == 0x36 ||
            b == 0x3e || b == 0x26 || b == 0x64 || b == 0x65) {
            i++;
        } else if (b == 0xf2 || b == 0xf3) {
            rep = 1;
            i++;
        } else if (b >= 0x40 && b <= 0x4f) {
            i++; // REX
        } else {
            break;
        }
    }
    if (!rep || i >= sizeof(bytes))
        return 0;
    unsigned char op = bytes[i];
    if (op == 0x0f && i + 1 < sizeof(bytes))
        op = bytes[i + 1]; // two-byte opcode forms (none are rep-string)
    switch (op) {
    case 0xa4: case 0xa5: // movs
    case 0xa6: case 0xa7: // cmps
    case 0xaa: case 0xab: // stos
    case 0xac: case 0xad: // lods
    case 0xae: case 0xaf: // scas
        return 1;
    }
    return 0;
}

static void trap(int sig, siginfo_t* si, void* vuc) {
    (void)sig;
    (void)si;
    ucontext_t* uc = (ucontext_t*)vuc;
    greg_t* g = uc->uc_mcontext.gregs;
    unsigned long long rip = g[16];
    // Keep the single-stepping armed for the resumed instruction.
    g[17] |= 0x100ULL;
    if (!g_started) {
        // The only pre-guest trap: the one right after gogo's ret landed
        // on the entry point. Fix the frame's FP state to execve-init
        // (sigreturn applies it before the guest's first instruction).
        if (rip != g_entry) {
            fprintf(stderr,
                    "native-trace: first trap rip=%#llx is not the guest "
                    "entry (%#llx)\n",
                    rip, (unsigned long long)g_entry);
            die(2);
        }
        if (getenv("NT_NO_FIXUP")) { g_started = 1; return; }
        // Extraction self-test FIRST (the patterns are still live in the
        // frame here; the fixup below zeroes them out afterwards).
        if (g_validate_fp)
            validate_entry_fp(uc);
        fixup_entry_frame(uc);
        // gogo's pushq $0x302 / popfq / ret run on the guest stack and leave
        // residue just below the initial guest rsp: the push spills 0x302 at
        // [rsp-16, rsp-8) and the popped entry word stays at [rsp-8, rsp). A
        // guest that scans below rsp (the corpus's `leaq -8(%rsp); scasw`
        // reads exactly this region) would then compare against those bytes,
        // which exist only in this TF-stepping oracle -- a ptrace-stepped
        // execve (the upstream kframework oracle) sees a clean zero stack
        // there, and so does the emulator. Zero the region once here, before
        // the first guest instruction, so the below-rsp state matches the
        // execve environment on every oracle. (Later deliveries do not touch
        // the guest stack: frames go to the sigaltstack.)
        {
            unsigned long long rsp = g[REG_RSP];
            if (rsp >= 512)
                memset((void*)(uintptr_t)(rsp - 512), 0, 512);
        }
        g_started = 1;
        return;
    }
    if (rip < g_lo || rip >= g_hi)
        return; // still stepping dumper code (should not happen)
    // REP string instructions single-step PER ITERATION when the trap
    // flag is set (documented x86 behavior: #DB fires after each
    // iteration, with RIP still pointing at the rep instruction). The
    // emulator executes a rep as ONE instruction (one record), and the
    // architectural instruction boundary is the whole rep, so coalesce:
    // a trap whose rip equals the last recorded rip AND whose
    // instruction at rip is a REP-prefixed string op is an intermediate
    // iteration -- skip it. (The rip==prev test alone would also swallow
    // genuine one-instruction loops like "loop $-2"; hence the rep
    // prefix check.) The record written when the rep completes is the
    // architectural post-instruction state.
    if (g_prev_rip == rip && is_rep_string(rip)) {
        // Intermediate REP iteration: not a record, but it IS executed
        // work -- count it toward the step cap so an infinite REP loop
        // with a huge RCX cannot bypass the 10000-step bound by
        // trapping here forever.
        g_steps++;
        if (g_steps > g_cap)
            die(0); // capped, like the kframework harness's nstep 10000
        return;
    }
    g_steps++;
    if (g_steps > g_cap)
        die(0); // capped, like the kframework harness's nstep 10000
    write_one(uc);
    g_prev_rip = rip;
    // End of the corpus program: the next instruction is the appended
    // epilogue's syscall (exit or exit_group). Stop now, having just
    // recorded the state after the last completed instruction -- the
    // emulator trace ends the same way (its syscall handler _exits
    // before any record), so the traces align record for record; the
    // exit code is 0 on both sides. The syscall itself is never executed
    // here.
    unsigned short next = 0;
    memcpy(&next, (const void*)(uintptr_t)rip, sizeof(next));
    if (next == 0x050f && (g[13] == 60 || g[13] == 231))
        die(0);
}

static void fault(int sig, siginfo_t* si, void* vuc) {
    ucontext_t* uc = (ucontext_t*)vuc;
    fprintf(stderr,
            "native-trace: guest fault sig=%d rip=%#llx addr=%#llx after %ld "
            "steps\n",
            sig, (unsigned long long)uc->uc_mcontext.gregs[16],
            (unsigned long long)si->si_addr, g_steps);
    die(3);
}

// gogo: switch to the guest stack and ret into [rsp] (the entry point).
// Every integer GPR is zeroed so the guest starts exactly like after
// execve. EFLAGS = TF|IF|1 via popfq arms single-stepping; the ret into
// the guest then traps at rip == entry (the pre-state trap handled
// above).
__asm__(
    ".globl gogo\n"
    "gogo:\n"
    "  movq %rdi, %rsp\n"
    "  xorq %rbx, %rbx\n"
    "  xorq %rax, %rax\n"
    "  xorq %rcx, %rcx\n"
    "  xorq %rdx, %rdx\n"
    "  xorq %rsi, %rsi\n"
    "  xorq %rdi, %rdi\n"
    "  xorq %rbp, %rbp\n"
    "  xorq %r8, %r8\n"
    "  xorq %r9, %r9\n"
    "  xorq %r10, %r10\n"
    "  xorq %r11, %r11\n"
    "  xorq %r12, %r12\n"
    "  xorq %r13, %r13\n"
    "  xorq %r14, %r14\n"
    "  xorq %r15, %r15\n"
    "  pushq $0x302\n"
    "  popfq\n"
    "  ret\n");
extern void gogo(uint64_t rsp);

static void load_guest(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        perror("native-trace: open guest");
        exit(2);
    }
    Elf64_Ehdr eh;
    if (fread(&eh, sizeof(eh), 1, f) != 1 ||
        memcmp(eh.e_ident, ELFMAG, SELFMAG) || eh.e_ident[EI_CLASS] != ELFCLASS64) {
        fprintf(stderr, "native-trace: %s is not an ELF64 image\n", path);
        exit(2);
    }
    Elf64_Phdr* ph = malloc(eh.e_phnum * sizeof(*ph));
    if (fseek(f, (long)eh.e_phoff, SEEK_SET) ||
        fread(ph, sizeof(*ph), eh.e_phnum, f) != eh.e_phnum) {
        fprintf(stderr, "native-trace: bad program headers\n");
        exit(2);
    }
    for (int i = 0; i < eh.e_phnum; i++) {
        if (ph[i].p_type != PT_LOAD)
            continue;
        uint64_t va0 = ph[i].p_vaddr & ~0xfffULL;
        uint64_t va1 = (ph[i].p_vaddr + ph[i].p_memsz + 0xfff) & ~0xfffULL;
        void* m = mmap((void*)va0, va1 - va0,
                       PROT_READ | PROT_WRITE | PROT_EXEC,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        if (m == MAP_FAILED) {
            perror("native-trace: mmap guest segment");
            exit(2);
        }
        if (ph[i].p_filesz) {
            if (fseek(f, (long)ph[i].p_offset, SEEK_SET) ||
                fread((void*)(uintptr_t)ph[i].p_vaddr, 1, ph[i].p_filesz, f) !=
                    ph[i].p_filesz) {
                fprintf(stderr, "native-trace: short read of segment\n");
                exit(2);
            }
        }
        int prot = 0;
        if (ph[i].p_flags & PF_R)
            prot |= PROT_READ;
        if (ph[i].p_flags & PF_W)
            prot |= PROT_WRITE;
        if (ph[i].p_flags & PF_X)
            prot |= PROT_EXEC;
        mprotect((void*)va0, va1 - va0, prot);
        if (prot & PROT_EXEC) {
            if (!g_lo)
                g_lo = va0;
            if (va1 > g_hi)
                g_hi = va1;
        }
    }
    fclose(f);
    g_entry = eh.e_entry;
    if (!g_entry || !g_lo) {
        fprintf(stderr, "native-trace: no entry point / no executable "
                        "segment\n");
        exit(2);
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: native-trace <guest> <trace-out> [cap]\n");
        return 2;
    }
    g_guest_path = argv[1];
    // Deterministic mappings: trace content must not depend on ASLR.
    personality(ADDR_NO_RANDOMIZE);
    init_xsave_offsets();
    g_fd = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (g_fd < 0) {
        perror("native-trace: open trace");
        return 2;
    }
    if (argc > 3)
        g_cap = atol(argv[3]);

    load_guest(g_guest_path);

    // Fresh deterministic guest stack at a fixed address (nothing else
    // lives there in a PIE process; the dumper's own image is PIE and
    // sits far above 4GB).
    const uint64_t stk_base = 0x700000000000ULL, stk_size = 8ULL << 20;
    if (mmap((void*)stk_base, stk_size, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1,
             0) == MAP_FAILED) {
        perror("native-trace: mmap guest stack");
        return 2;
    }
    uint64_t rsp = stk_base + stk_size;
    const char* argv0 = "x86sem";
    size_t nlen = strlen(argv0) + 1;
    rsp -= nlen;
    memcpy((void*)rsp, argv0, nlen);
    uint64_t name_addr = rsp;
    rsp &= ~15ULL;
    // Minimal initial stack image, laid out like the kernel's: entry
    // word consumed by gogo's ret, then argc, argv0, NULL argv, NULL
    // envp, AT_PAGESZ, AT_NULL. The corpus never reads past argv, but a
    // faithful image costs nothing.
    rsp -= 9 * 8;
    uint64_t* w = (uint64_t*)rsp;
    w[0] = g_entry;
    w[1] = 1;
    w[2] = name_addr;
    w[3] = 0;
    w[4] = 0;
    w[5] = 6; // AT_PAGESZ
    w[6] = 4096;
    w[7] = 0; // AT_NULL
    w[8] = 0;

    // Handler stack: keeps signal frames off the guest stack entirely.
    stack_t ss;
    ss.ss_sp = mmap(NULL, 1 << 20, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ss.ss_size = 1 << 20;
    ss.ss_flags = 0;
    sigaltstack(&ss, NULL);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = trap;
    sa.sa_flags = SA_SIGINFO | SA_NODEFER | SA_ONSTACK;
    sigaction(SIGTRAP, &sa, NULL);
    sa.sa_sigaction = fault;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);

    // The guest must start with execve-like FP state. glibc above used
    // the vector unit, so zero it as the very last thing before gogo;
    // the entry-trap fixup then double-checks (and force-fixes) the
    // signal frame regardless.
    zero_fp_state();
    if (getenv("NT_NO_FIXUP")) {
        // Debug mode: the guest starts with whatever the dumper left in
        // the vector registers, so the probe patterns must NOT be planted
        // (the self-test at entry would trip on them) and are skipped.
        g_validate_fp = 0;
    } else if (getenv("NT_DIRTY_FP")) {
        // Deliberately dirty the vector state after the zeroing (debug
        // hook, mutually exclusive with the probe planting): the
        // entry-frame fixup must still start the guest clean. No
        // self-test in this mode (the junk would trip it).
        g_validate_fp = 0;
        static unsigned long long junk[4] = {0xdeadbeefcafebabeULL,
                                             0x1122334455667788ULL,
                                             0xfeedface0c0ffeeULL,
                                             0x0123456789abcdefULL};
        __asm__ volatile("vmovups %0, %%ymm0\n\t"
                         "vmovups %0, %%ymm15\n\t"
                         "vaddps %%ymm0, %%ymm0, %%ymm1\n\t"
                         :: "m"(junk) : "xmm0", "xmm1", "xmm15");
    } else {
        // Plant 16 distinct 256-bit probe patterns (register number
        // encoded in every qword) as the last vector writes before the
        // guest runs; the pre-entry trap verifies the signal frame
        // returned them bit-exactly (see validate_entry_fp). Ordered
        // after zero_fp_state by the volatile asm boundaries.
        for (int r = 0; r < 16; r++) {
            g_fp_probe[r][0] = 0xa5a5a5a5a5a5a500ULL + (unsigned)r;
            g_fp_probe[r][1] = 0x5a5a5a5a5a5a5a00ULL + (unsigned)r;
            g_fp_probe[r][2] = 0xc0dec0dec0de0000ULL + (unsigned)r;
            g_fp_probe[r][3] = 0xfeedfeedfeed0000ULL + (unsigned)r;
        }
        __asm__ volatile(
            "vmovups %0, %%ymm0\n\t"
            "vmovups %1, %%ymm1\n\t"
            "vmovups %2, %%ymm2\n\t"
            "vmovups %3, %%ymm3\n\t"
            "vmovups %4, %%ymm4\n\t"
            "vmovups %5, %%ymm5\n\t"
            "vmovups %6, %%ymm6\n\t"
            "vmovups %7, %%ymm7\n\t"
            "vmovups %8, %%ymm8\n\t"
            "vmovups %9, %%ymm9\n\t"
            "vmovups %10, %%ymm10\n\t"
            "vmovups %11, %%ymm11\n\t"
            "vmovups %12, %%ymm12\n\t"
            "vmovups %13, %%ymm13\n\t"
            "vmovups %14, %%ymm14\n\t"
            "vmovups %15, %%ymm15\n\t"
            :: "m"(g_fp_probe[0][0]), "m"(g_fp_probe[1][0]),
               "m"(g_fp_probe[2][0]), "m"(g_fp_probe[3][0]),
               "m"(g_fp_probe[4][0]), "m"(g_fp_probe[5][0]),
               "m"(g_fp_probe[6][0]), "m"(g_fp_probe[7][0]),
               "m"(g_fp_probe[8][0]), "m"(g_fp_probe[9][0]),
               "m"(g_fp_probe[10][0]), "m"(g_fp_probe[11][0]),
               "m"(g_fp_probe[12][0]), "m"(g_fp_probe[13][0]),
               "m"(g_fp_probe[14][0]), "m"(g_fp_probe[15][0])
            : "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
              "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12",
              "xmm13", "xmm14", "xmm15");
    }
    gogo(rsp);
    return 0; // unreachable
}

// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test: the cannot-index check battery. One guest, one argv subtest per
// cannot-index check site in the emulator; every subtest MUST fault (exit
// 139 + core dump) with the expected reason in stderr. The taint is built
// by poisoning the SLOT holding a value and volatile-loading it (the taint
// rides the value); every tainted value points at real mapped memory, so a
// regressed check shows up as exit 0, a wrong-reason stderr, or a wild
// crash whose stderr lacks the expected reason -- in every case the
// subtest fails.
//
// Subtests (see run-tests for the reason grep per subtest):
//   load-base      tainted pointer as a mov-load base        (resolve_mem base)
//   store-base     tainted pointer as a mov-store base       (resolve_mem base)
//   load-index     clean base + tainted index*4 load         (resolve_mem index)
//   store-index    clean base + tainted index*4 store        (resolve_mem index)
//   alu-mem        add from a tainted base                   (resolve_mem base)
//   movzx-mem      movzx from a tainted base                 (resolve_mem base)
//   div-mem        div by a memory word at a tainted base    (resolve_mem base)
//   vector-load    vmovdqu from a tainted base               (resolve_mem base)
//   rsp-mem        mov (rsp) with a tainted RSP              (resolve_mem base)
//   push           push with a tainted (valid) RSP           ("used by push")
//   pop            pop with a tainted (valid) RSP            ("used by pop")
//   ret-rsp        ret with a tainted (valid) RSP            ("used by ret")
//   ret-addr       push a tainted value, then ret            (return address)
//   indirect-call  call through a tainted function pointer   (branch target)
//   indirect-jmp   jmp through a tainted target              (branch target)
//   string-movs    movsb with a tainted RSI                  (RSI/RDI in string op)
//   string-stos    stosb with a tainted RDI                  (RSI/RDI in string op)
//   rep-movs       rep movsb with a tainted RCX count        (RCX in rep string op)
//   loop           loop with a tainted RCX                   (RCX in loop)
//   jcxz           jrcxz with a tainted RCX                  (RCX in jcxz)
//   bt-mem         bt with a tainted bit-offset register     (BT bit offset)
//   xlat           xlatb with a tainted RBX table base       (XLAT table index/base)
//   prefetch       prefetcht0 with a tainted base            (resolve_mem base)
//   clflush        clflush with a tainted base               (resolve_mem base)
//   gather         vpgatherdd with a tainted VSIB index      (gather/scatter index)
//   scatter        vpscatterdd with a tainted VSIB index     (gather/scatter index)
//   gather-base    vpgatherdd with a tainted scalar base     (gather/scatter base)
//   scatter-base   vpscatterdd with a tainted scalar base    (gather/scatter base)
//   enter-frame    enter $16, $2 with a tainted RBP          ("used by enter")
//
// Guests run only under the emulator, so no runtime CPUID gates are needed;
// compile-time -mavx/-mavx512f/-mavx512vl is for the inline asm/intrinsics.

#include <immintrin.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

static uint64_t magic(uint64_t op, void* ptr, size_t size, uint64_t flags) {
    return ((uint64_t (*)(uint64_t, void*, size_t, uint64_t))0x1410141014101410ULL)(
        op, ptr, size, flags);
}

// Real memory every tainted operand points at (in-bounds if a check
// regresses).
static char buf[256];
static char fake_stack[4096];
static char src_buf[64];
static char dst_buf[64];
static uint32_t div_word = 7; // nonzero: a regressed div must not #DE
static void ok_fn(void) {} // real no-op function: a regressed call/jmp survives
static void (*volatile fslot)(void) = ok_fn;
static volatile uint64_t slot;

static uint64_t tainted_u64(uint64_t v) {
    slot = v;
    magic(1, (void*)&slot, sizeof(slot), 8u); // op=poison, cannot-index
    return slot; // volatile load: the value acquires the taint
}

static void* tainted_ptr(void* p) {
    return (void*)tainted_u64((uint64_t)p);
}

// The gather/scatter VSIB index VECTOR must carry the taint: volatile stores
// + an opaque magic() call prevent constant folding, the integer copy
// propagates the taint into tmp's bytes, and the vector load picks it up.
static volatile int32_t idx_words[8];

static __m256i tainted_vec(void) {
    for (int k = 0; k < 8; k++)
        idx_words[k] = k;                              // volatile stores: real
    magic(1, (void*)idx_words, sizeof(idx_words), 8u); // cannot-index
    int32_t tmp[8];
    for (int k = 0; k < 8; k++)
        tmp[k] = idx_words[k];                         // volatile loads: tainted
    return _mm256_loadu_si256((const __m256i*)tmp);
}

// Same shape as tainted_vec() but WITHOUT the poison call: the VSIB index
// vector stays clean while the scalar base pointer (via tainted_ptr) is
// tainted, so the gather/scatter BASE check site is the one under test.
static __m256i clean_vec(void) {
    for (int k = 0; k < 8; k++)
        idx_words[k] = k; // volatile stores: real, no poison
    int32_t tmp[8];
    for (int k = 0; k < 8; k++)
        tmp[k] = idx_words[k]; // volatile loads: clean
    return _mm256_loadu_si256((const __m256i*)tmp);
}

int main(int argc, char** argv) {
    const char* which = argc > 1 ? argv[1] : "";
    uint64_t out;
    if (strcmp(which, "load-base") == 0) {
        uint64_t* tp = (uint64_t*)tainted_ptr(buf);
        asm volatile("movq (%1), %0" : "=r"(out) : "r"(tp) : "memory");
    } else if (strcmp(which, "store-base") == 0) {
        uint64_t* tp = (uint64_t*)tainted_ptr(buf);
        asm volatile("movq %0, (%1)" :: "r"(0x1122334455667788ULL), "r"(tp) : "memory");
    } else if (strcmp(which, "load-index") == 0) {
        uint64_t ti = tainted_u64(24); // *4 -> buf+96, in-bounds
        uint32_t out32;
        asm volatile("movl (%1,%2,4), %0"
                     : "=r"(out32)
                     : "r"((uint64_t)buf), "r"(ti)
                     : "memory");
        out = out32;
    } else if (strcmp(which, "store-index") == 0) {
        uint64_t ti = tainted_u64(24); // *4 -> buf+96, in-bounds
        asm volatile("movl %0, (%1,%2,4)"
                     :
                     : "r"(42u), "r"((uint64_t)buf), "r"(ti)
                     : "memory");
    } else if (strcmp(which, "alu-mem") == 0) {
        uint32_t* tp = (uint32_t*)tainted_ptr(buf);
        uint32_t acc = 1;
        asm volatile("addl (%1), %0" : "+r"(acc) : "r"(tp) : "cc", "memory");
        out = acc;
    } else if (strcmp(which, "movzx-mem") == 0) {
        uint8_t* tp = (uint8_t*)tainted_ptr(buf);
        uint32_t out32;
        asm volatile("movzbl (%1), %0" : "=r"(out32) : "r"(tp) : "memory");
        out = out32;
    } else if (strcmp(which, "div-mem") == 0) {
        uint32_t* tp = (uint32_t*)tainted_ptr(&div_word);
        // The "a"/"d"/"r" constraint split keeps GCC from clobbering the
        // dividend (EDX:EAX = 100:0) with the pointer register.
        asm volatile("divl (%2)" :: "a"(100u), "d"(0u), "r"(tp) : "cc", "memory");
        out = 0;
    } else if (strcmp(which, "vector-load") == 0) {
        void* tp = tainted_ptr(buf);
        __m256i vout;
        asm volatile("vmovdqu (%1), %0" : "=x"(vout) : "r"(tp) : "memory");
        out = (uint64_t)_mm_cvtsi128_si64(_mm256_castsi256_si128(vout));
    } else if (strcmp(which, "rsp-mem") == 0) {
        void* tsp = tainted_ptr(fake_stack + 2048); // mid-buffer: valid stack
        asm volatile("movq %1, %%rsp\n\tmovq (%%rsp), %0"
                     : "=r"(out)
                     : "r"(tsp)
                     : "memory");
    } else if (strcmp(which, "push") == 0) {
        void* tsp = tainted_ptr(fake_stack + 2048); // mid-buffer: valid stack
        asm volatile("movq %0, %%rsp\n\tpush $0" :: "r"(tsp) : "memory");
        out = 0;
    } else if (strcmp(which, "pop") == 0) {
        void* tsp = tainted_ptr(fake_stack + 2048); // mid-buffer: valid stack
        asm volatile("movq %0, %%rsp\n\tpopq %%rcx" :: "r"(tsp) : "rcx", "memory");
        out = 0;
    } else if (strcmp(which, "ret-rsp") == 0) {
        void* tsp = tainted_ptr(fake_stack + 2048); // mid-buffer: valid stack
        asm volatile("movq %0, %%rsp\n\tret" :: "r"(tsp) : "memory");
        out = 0;
    } else if (strcmp(which, "ret-addr") == 0) {
        // push propagates the value taint onto the stack slot; the ret then
        // faults on the tainted return address (never actually returns).
        uint64_t fp = tainted_u64((uint64_t)(uintptr_t)ok_fn);
        asm volatile("pushq %0\n\tret" :: "r"(fp) : "memory");
        out = 0;
    } else if (strcmp(which, "enter-frame") == 0) {
        // ENTER with nesting level >= 2 reads [RBP - i*8] to copy the
        // nesting frames: a real memory access keyed on RBP, so a
        // cannot-index-tainted RBP must fault ("used by enter"). Mid-buffer
        // base keeps a regressed run in-bounds (level 2 reads RBP-8).
        uint64_t rs = (uint64_t)tainted_ptr(fake_stack + 2048);
        asm volatile("movq %0, %%rbp\n\tenter $16, $2" :: "r"(rs) : "memory");
        out = 0;
    } else if (strcmp(which, "indirect-call") == 0) {
        magic(1, (void*)&fslot, sizeof(fslot), 8u); // cannot-index
        void (*g)(void) = fslot; // volatile load: the pointer is tainted
        g();
        out = 0;
    } else if (strcmp(which, "indirect-jmp") == 0) {
        uint64_t jt = tainted_u64((uint64_t)(uintptr_t)ok_fn);
        asm volatile("jmpq *%0" :: "r"(jt) : "memory");
        out = 0;
    } else if (strcmp(which, "string-movs") == 0) {
        const char* s = (const char*)tainted_ptr(src_buf);
        char* d = dst_buf; // clean
        asm volatile("movsb" : "+S"(s), "+D"(d) : : "memory");
        out = 0;
    } else if (strcmp(which, "string-stos") == 0) {
        char* d = (char*)tainted_ptr(dst_buf);
        char al = 0;
        asm volatile("stosb" : "+D"(d), "+a"(al) : : "memory");
        out = 0;
    } else if (strcmp(which, "rep-movs") == 0) {
        const char* s = src_buf; // clean
        char* d = dst_buf;       // clean
        size_t n = (size_t)tainted_u64(4);
        asm volatile("rep movsb" : "+S"(s), "+D"(d), "+c"(n) : : "memory");
        out = 0;
    } else if (strcmp(which, "loop") == 0) {
        // Tainted count 1: a regressed check decrements 1 -> 0 and falls
        // through (safe).
        uint64_t n = tainted_u64(1);
        asm volatile("1: loop 1b" :: "c"(n) : "cc", "memory");
        out = 0;
    } else if (strcmp(which, "jcxz") == 0) {
        // jcxz is not encodable in 64-bit mode; jrcxz is its 64-bit form and
        // takes the same RCX taint path. Tainted count 1: a regressed check
        // sees CX != 0 and falls through (safe).
        uint64_t n = tainted_u64(1);
        asm volatile("jrcxz 1f\n1:" :: "c"(n) : "cc", "memory");
        out = 0;
    } else if (strcmp(which, "bt-mem") == 0) {
        // AT&T: mem first operand in (%0), tainted reg %1 is the bit offset.
        uint64_t ti = tainted_u64(3); // bit 3 of buf[0]: in-bounds
        asm volatile("bt %1, (%0)"
                     :
                     : "r"((uint64_t)buf), "r"(ti)
                     : "cc", "memory");
        out = 0;
    } else if (strcmp(which, "xlat") == 0) {
        // AL is always <= 255, so a regressed run stays in-bounds of buf.
        asm volatile("xlatb" :: "b"(tainted_ptr(buf)) : "memory");
        out = 0;
    } else if (strcmp(which, "prefetch") == 0) {
        void* tp = tainted_ptr(buf);
        asm volatile("prefetcht0 (%0)" :: "r"(tp) : "memory");
        out = 0;
    } else if (strcmp(which, "clflush") == 0) {
        void* tp = tainted_ptr(buf);
        asm volatile("clflush (%0)" :: "r"(tp) : "memory");
        out = 0;
    } else if (strcmp(which, "gather") == 0) {
        // Indices 0..7, scale 4 -> buf[0..31]: in-bounds if a check regresses.
        __m256i r = _mm256_i32gather_epi32((const int*)buf, tainted_vec(), 4);
        // Volatile-asm consumer: keeps the gather live (GCC would otherwise
        // DCE the whole instruction, since the result is never used).
        asm volatile("" :: "x"(r) : "memory");
        out = (uint64_t)_mm_cvtsi128_si64(_mm256_castsi256_si128(r));
    } else if (strcmp(which, "scatter") == 0) {
        // Indices 0..7, scale 4 -> buf[0..31]: in-bounds if a check regresses.
        _mm256_i32scatter_epi32((void*)buf, tainted_vec(), _mm256_set1_epi32(7), 4);
        out = 0;
    } else if (strcmp(which, "gather-base") == 0) {
        // Clean VSIB index vector, tainted scalar BASE: exercises the other
        // vsib_base check site. Indices 0..7, scale 4 -> buf[0..31]:
        // in-bounds if the base check regresses.
        __m256i r = _mm256_i32gather_epi32((const int*)tainted_ptr(buf), clean_vec(), 4);
        // Volatile-asm consumer: keeps the gather live (GCC would otherwise
        // DCE the whole instruction, since the result is never used).
        asm volatile("" :: "x"(r) : "memory");
        out = (uint64_t)_mm_cvtsi128_si64(_mm256_castsi256_si128(r));
    } else if (strcmp(which, "scatter-base") == 0) {
        // Clean VSIB index vector, tainted scalar BASE: indices 0..7,
        // scale 4 -> buf[0..31]: in-bounds if the base check regresses.
        _mm256_i32scatter_epi32((void*)tainted_ptr(buf), clean_vec(), _mm256_set1_epi32(7), 4);
        out = 0;
    } else {
        return 2;
    }
    (void)out;
    // Unreachable when the emulator is correct: every subtest above faults.
    write(1, "NOTREACHED\n", 11);
    return 0;
}

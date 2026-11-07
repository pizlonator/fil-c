// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Poly1305 chunked-update KAT + VEX VMOVQ zeroing regression guest for the
// zegarmistrz emulator.
//
// The trigger: EVP_MAC POLY1305 with update sizes [12, 65536, 100].  The
// first update (12 bytes) leaves an odd unaligned tail in openssl's
// poly1305 tail buffer; the next Poly1305_Update merges it into a 16-byte
// block processed by poly1305_blocks_vpmadd52 (the AVX512-IFMA path), which
// reloads the key powers with VEX-encoded `vmovq m64, %xmmN` before
// multiplying all four lanes.  The emulator's exec_movd cleared only the
// 128-bit XMM image and left the upper 128 bits of the YMM register stale
// (VEX.128 semantics require zeroing them), so the IFMA multiplies consumed
// garbage in lanes 1-3 and the final tag diverged from hardware.  The
// expected tag below was derived from native execution.
//
// Self-checking: verifies the KAT digest, then instruction-level checks for
// the fixed VMOVD/VMOVQ semantics (VEX.128 m64/r64 -> xmm must zero the
// upper YMM lanes; legacy MOVQ must not).  Instruction groups are gated on
// runtime CPUID.  Prints "poly1305-kat-ok" and exits 0 on success.
#include <immintrin.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <openssl/evp.h>
#include <openssl/core_names.h>

static int failures = 0;

#define CHECK(cond, name)                                          \
    do {                                                           \
        if (!(cond)) {                                             \
            printf("FAIL %s\n", (name));                           \
            failures++;                                            \
        }                                                          \
    } while (0)

static void cpuid(unsigned leaf, unsigned* a, unsigned* b, unsigned* c,
                  unsigned* d) {
    asm volatile("cpuid"
                 : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
                 : "a"(leaf), "c"(0));
}

// The KAT: key[i] = 0x85 + i, msg[i] = (i * 7 + 3) & 0xff, updates of
// 12 / 65536 / 100 bytes.  Expected tag computed on hardware (matches the
// one-shot digest of the same 65648 bytes, as required by the MAC).
static int kat(void) {
    static unsigned char key[32], msg[65660], out[16];
    static const unsigned char expect[16] = {
        0x95, 0xf9, 0xb6, 0x84, 0x55, 0xdb, 0x12, 0xdc,
        0xe9, 0xc6, 0x5c, 0x34, 0x35, 0x1c, 0xaf, 0x9f
    };
    for (int i = 0; i < 32; i++) key[i] = (unsigned char)(0x85 + i);
    for (int i = 0; i < 65660; i++) msg[i] = (unsigned char)(i * 7 + 3);
    EVP_MAC* mac = EVP_MAC_fetch(NULL, "POLY1305", NULL);
    if (!mac) {
        printf("FAIL poly1305 fetch\n");
        failures++;
        return 1;
    }
    EVP_MAC_CTX* ctx = EVP_MAC_CTX_new(mac);
    OSSL_PARAM params[2];
    params[0] = OSSL_PARAM_construct_octet_string(OSSL_MAC_PARAM_KEY, key, 32);
    params[1] = OSSL_PARAM_construct_end();
    int ok = EVP_MAC_init(ctx, NULL, 0, params) &&
             EVP_MAC_update(ctx, msg, 12) &&
             EVP_MAC_update(ctx, msg + 12, 65536) &&
             EVP_MAC_update(ctx, msg + 12 + 65536, 100);
    size_t l = 16;
    if (ok) ok = EVP_MAC_final(ctx, out, &l, 16) && l == 16;
    EVP_MAC_CTX_free(ctx);
    EVP_MAC_free(mac);
    if (!ok) {
        printf("FAIL poly1305 kat compute\n");
        failures++;
        return 1;
    }
    CHECK(memcmp(out, expect, 16) == 0, "poly1305 [12,65536,100] tag");
    // The same total bytes in one update must agree (chunk-invariance).
    mac = EVP_MAC_fetch(NULL, "POLY1305", NULL);
    ctx = EVP_MAC_CTX_new(mac);
    params[0] = OSSL_PARAM_construct_octet_string(OSSL_MAC_PARAM_KEY, key, 32);
    params[1] = OSSL_PARAM_construct_end();
    ok = EVP_MAC_init(ctx, NULL, 0, params) &&
         EVP_MAC_update(ctx, msg, 65648);
    l = 16;
    if (ok) ok = EVP_MAC_final(ctx, out, &l, 16) && l == 16;
    EVP_MAC_CTX_free(ctx);
    EVP_MAC_free(mac);
    if (!ok) {
        printf("FAIL poly1305 oneshot compute\n");
        failures++;
        return 1;
    }
    CHECK(memcmp(out, expect, 16) == 0, "poly1305 one-shot tag agrees");
    return 0;
}

// VEX.128 VMOVQ m64 -> xmm: the loaded qword lands in bits 63:0 and every
// other bit of the containing YMM register is cleared.
static void vex_vmovq_mem_checks(void) {
    unsigned long long garbage = 0xddddddddddddddddULL, val = 0x8899aabbccddeeffULL;
    unsigned long long q[8];
    __asm__ volatile(
        "vbroadcastsd %1, %%ymm3\n"     /* fill the whole ymm3 with garbage */
        "vmovq %2, %%xmm3\n"            /* VEX.128: 64-bit load, zero rest */
        "vmovdqu %%ymm3, %0\n"
        : "=m"(q)
        : "m"(garbage), "m"(val)
        : );
    CHECK(q[0] == 0x8899aabbccddeeffULL, "vex vmovq m64 loads qword");
    CHECK(q[1] == 0 && q[2] == 0 && q[3] == 0,
          "vex vmovq m64 zeroes upper ymm");
    printf("vex-vmovq-mem done\n");
}

// VEX.128 VMOVQ r64 -> xmm: same zeroing rule for the GPR source form.
static void vex_vmovq_gpr_checks(void) {
    unsigned long long garbage = 0xeeeeeeeeeeeeeeeeULL;
    unsigned long long val = 0x0123456789abcdefULL;
    unsigned long long q[8];
    __asm__ volatile(
        "vbroadcastsd %1, %%ymm4\n"
        "movq %2, %%rax\n"
        "vmovq %%rax, %%xmm4\n"
        "vmovdqu %%ymm4, %0\n"
        : "=m"(q)
        : "m"(garbage), "m"(val)
        : );
    CHECK(q[0] == 0x0123456789abcdefULL, "vex vmovq r64 loads qword");
    CHECK(q[1] == 0 && q[2] == 0 && q[3] == 0,
          "vex vmovq r64 zeroes upper ymm");
    printf("vex-vmovq-gpr done\n");
}

// VEX.128 VMOVD r32 -> xmm: bits 63:32 and the upper 128 are cleared.
static void vex_vmovd_gpr_checks(void) {
    unsigned long long garbage = 0xccccccccccccccccULL;
    unsigned int val = 0xcafebabeU;
    unsigned long long q[8];
    __asm__ volatile(
        "vbroadcastsd %1, %%ymm5\n"
        "movl %2, %%eax\n"
        "vmovd %%eax, %%xmm5\n"
        "vmovdqu %%ymm5, %0\n"
        : "=m"(q)
        : "m"(garbage), "m"(val)
        : );
    CHECK(q[0] == 0xcafebabeULL, "vex vmovd r32 loads dword");
    CHECK(q[1] == 0 && q[2] == 0 && q[3] == 0,
          "vex vmovd r32 zeroes upper ymm");
    printf("vex-vmovd-gpr done\n");
}

// Legacy (non-VEX) MOVQ m64 -> xmm: bits 127:64 are cleared, but the upper
// 128 bits of the YMM register must be PRESERVED (no VEX zeroing rule).
static void legacy_movq_checks(void) {
    unsigned long long garbage = 0xdfdfdfdfdfdfdfdfULL;
    unsigned long long val = 0x5566778899aabbccULL;
    unsigned long long q[8];
    __asm__ volatile(
        "vbroadcastsd %1, %%ymm6\n"
        "movq %2, %%xmm6\n"             /* legacy SSE load (F3 0F 7E): no VEX zeroing rule */
        "vmovdqu %%ymm6, %0\n"
        : "=m"(q)
        : "m"(garbage), "m"(val)
        : );
    CHECK(q[0] == 0x5566778899aabbccULL, "legacy movq m64 loads qword");
    CHECK(q[1] == 0, "legacy movq m64 clears bits 127:64");
    CHECK(q[2] == garbage && q[3] == garbage,
          "legacy movq m64 preserves upper ymm");
    printf("legacy-movq done\n");
}

int main(void) {
    kat();

    unsigned a, b, c, d;
    cpuid(1, &a, &b, &c, &d);
    if (c & (1u << 28)) {  // AVX: VEX encodings available
        vex_vmovq_mem_checks();
        vex_vmovq_gpr_checks();
        vex_vmovd_gpr_checks();
        legacy_movq_checks();
    } else {
        printf("skip vex vmovq/vmovd groups (no host AVX)\n");
    }

    if (failures == 0) {
        printf("poly1305-kat-ok\n");
        return 0;
    }
    printf("poly1305-kat FAILED: %d\n", failures);
    return 1;
}

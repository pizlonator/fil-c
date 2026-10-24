#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>

/*
 * test_digest_shake -- SHAKE128/256 XOFs and KECCAK-KMAC-128/256.
 *
 * Known-answer vectors, all verified with pizfix/bin/openssl, e.g.
 *
 *   printf 'abc' | pizfix/bin/openssl dgst -shake128 -xoflen 32
 *   printf 'abc' | pizfix/bin/openssl dgst -keccak-kmac-128 -xoflen 32
 *
 * Coverage:
 *   - output length control through OSSL_DIGEST_PARAM_XOFLEN ("xoflen")
 *     via EVP_MD_CTX_set_params BEFORE final, and through
 *     EVP_DigestFinalXOF; both routes must agree exactly;
 *   - the prefix property: squeezing more bytes extends, never changes,
 *     a shorter squeeze of the same input;
 *   - EVP_DigestSqueeze (OpenSSL 3.5+; absent from 3.0.x, so that subtest
 *     compiles in only on newer headers and otherwise prints SKIP): two
 *     squeeze calls (10 then 100 bytes) must equal one 110-byte squeeze.
 *     The implementation buffers leftover state between squeezes
 *     (crypto/sha/sha3.c);
 *   - KECCAK-KMAC-128/256 with xoflen (the digest form of KMAC).
 *
 * Version gating: only EVP_DigestSqueeze is version-gated (3.5+).  On
 * OpenSSL 3.0.x the program prints SKIP and exits 0 after running all the
 * other checks.
 */

/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */
static void *ALLOCATE_INPUT(size_t n)  { return poison_alloc(POISON_INPUT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }
static void *ALLOCATE_OTHER(size_t n)  { return poison_alloc(POISON_OTHER, n); }

static unsigned g_checks = 0;

/* Set when EVP_DigestSqueeze is not available in this OpenSSL build. */
static int g_no_squeeze = 0;

static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_digest_shake: %s\n", msg);
    exit(1);
}

#define CHECK(cond, msg)                      \
    do {                                      \
        if (!(cond))                          \
            test_failed(msg);                 \
        g_checks++;                           \
    } while (0)

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Copies a hex constant into a buffer allocated by an ALLOCATE_* function
 * (house rule: constants live in the source as hex strings, but are copied
 * into labeled working buffers before use).  Returns the byte length. */
static size_t from_hex(const char *hex, unsigned char *dst, size_t dstcap)
{
    size_t n = strlen(hex) / 2;
    size_t i;
    if (strlen(hex) % 2 != 0)
        test_failed("odd-length hex constant");
    if (n > dstcap)
        test_failed("hex constant larger than its destination buffer");
    for (i = 0; i < n; i++) {
        int hi = hexval(hex[2 * i]);
        int lo = hexval(hex[2 * i + 1]);
        if (hi < 0 || lo < 0)
            test_failed("bad hex digit in constant");
        dst[i] = (unsigned char)((hi << 4) | lo);
    }
    return n;
}

/* XOF output via OSSL_DIGEST_PARAM_XOFLEN + EVP_DigestFinal_ex.  The
 * xoflen parameter must be set before the final call; `out` must have room
 * for xoflen bytes (house rule 6: SHAKE out-buffers are sized by the
 * xoflen we set, since EVP_MD_get_size is 0 for XOFs on newer OpenSSL). */
static void xof_digest_param(const EVP_MD *md, const unsigned char *in, size_t inlen,
                             size_t xoflen, unsigned char *out)
{
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    OSSL_PARAM params[2];
    unsigned int outlen = 0;
    size_t len = xoflen;
    CHECK(ctx != NULL, "EVP_MD_CTX_new failed");
    CHECK(EVP_DigestInit_ex(ctx, md, NULL) == 1, "EVP_DigestInit_ex failed");
    params[0] = OSSL_PARAM_construct_size_t(OSSL_DIGEST_PARAM_XOFLEN, &len);
    params[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_MD_CTX_set_params(ctx, params) == 1,
          "EVP_MD_CTX_set_params(xoflen) failed");
    if (inlen > 0)
        CHECK(EVP_DigestUpdate(ctx, in, inlen) == 1, "EVP_DigestUpdate failed");
    CHECK(EVP_DigestFinal_ex(ctx, out, &outlen) == 1,
          "EVP_DigestFinal_ex failed");
    CHECK((size_t)outlen == xoflen, "xoflen output length mismatch");
    EVP_MD_CTX_free(ctx);
}

/* XOF output via EVP_DigestFinalXOF.  Must agree with xof_digest_param. */
static void xof_digest_finalxof(const EVP_MD *md, const unsigned char *in,
                                size_t inlen, size_t xoflen, unsigned char *out)
{
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    CHECK(ctx != NULL, "EVP_MD_CTX_new failed");
    CHECK(EVP_DigestInit_ex(ctx, md, NULL) == 1, "EVP_DigestInit_ex failed");
    if (inlen > 0)
        CHECK(EVP_DigestUpdate(ctx, in, inlen) == 1, "EVP_DigestUpdate failed");
    CHECK(EVP_DigestFinalXOF(ctx, out, xoflen) == 1,
          "EVP_DigestFinalXOF failed");
    EVP_MD_CTX_free(ctx);
}

static const char *const MSG_ABC = "abc";

static const struct shake_kat {
    const char *mdname;
    const char *msg;        /* NULL means the empty message */
    size_t xoflen;
    const char *outhex;
} shake_kats[] = {
    { "SHAKE128", "abc", 32,
      "5881092dd818bf5cf8a3ddb793fbcba74097d5c526a6d35f97b83351940f2cc8" },
    { "SHAKE256", "abc", 64,
      "483366601360a8771c6863080cc4114d8db44530f8f1e1ee4f94ea37e78b5739d5a15bef186a5386c75744c0527e1faa9f8726e462a12a4feb06bd8801e751e4" },
    { "SHAKE128", NULL, 16,
      "7f9c2ba4e88f827d616045507605853e" },
    { "SHAKE256", NULL, 32,
      "46b9dd2b0ba88d13233b3feb743eeb243fcd52ea62b81b82b50c27646ed5762f" },
};

/* (1) KATs through both output-length routes; the two routes must agree

 * byte for byte. */

static void test_kats(void)
{
    size_t k;
    for (k = 0; k < sizeof(shake_kats) / sizeof(shake_kats[0]); k++) {
        const struct shake_kat *kat = &shake_kats[k];
        EVP_MD *md = EVP_MD_fetch(NULL, kat->mdname, NULL);
        unsigned char *in = ALLOCATE_INPUT(3);
        unsigned char *want = ALLOCATE_OTHER(64);
        unsigned char *got_param = ALLOCATE_OUTPUT(64);
        unsigned char *got_xof = ALLOCATE_OUTPUT(64);
        size_t inlen = 0, wantlen;
        char msg[128];

        CHECK(md != NULL, "EVP_MD_fetch failed for a core SHAKE XOF");
        if (kat->msg != NULL) {
            inlen = strlen(kat->msg);
            memcpy(in, kat->msg, inlen);
        }
        wantlen = from_hex(kat->outhex, want, 64);
        CHECK(wantlen == kat->xoflen, "KAT length disagrees with its xoflen");

        xof_digest_param(md, in, inlen, kat->xoflen, got_param);
        snprintf(msg, sizeof(msg), "%s xoflen=%zu digest does not match the KAT",
                 kat->mdname, kat->xoflen);
        CHECK(memcmp(got_param, want, wantlen) == 0, msg);

        xof_digest_finalxof(md, in, inlen, kat->xoflen, got_xof);
        snprintf(msg, sizeof(msg),
                 "EVP_DigestFinalXOF disagrees with the xoflen route for %s",
                 kat->mdname);
        CHECK(memcmp(got_param, got_xof, wantlen) == 0, msg);
        EVP_MD_free(md);
    }
}

/* (2) Prefix property: the first xoflen bytes of a longer squeeze are the

 * shorter squeeze, for a deterministic 137-byte input. */

static void test_prefix_property(void)
{
    EVP_MD *mds[2];
    const char *names[2] = { "SHAKE128", "SHAKE256" };
    unsigned char *in = ALLOCATE_INPUT(137);
    unsigned char *short_out = ALLOCATE_OUTPUT(64);
    unsigned char *long_out = ALLOCATE_OUTPUT(200);
    size_t m, i;
    char msg[128];

    for (i = 0; i < 137; i++)
        in[i] = (unsigned char)(i * 7 + 3);
    mds[0] = EVP_MD_fetch(NULL, names[0], NULL);
    mds[1] = EVP_MD_fetch(NULL, names[1], NULL);
    CHECK(mds[0] != NULL && mds[1] != NULL, "EVP_MD_fetch failed for a core SHAKE XOF");

    for (m = 0; m < 2; m++) {
        size_t short_len = (m == 0) ? 32 : 64;
        xof_digest_param(mds[m], in, 137, short_len, short_out);
        xof_digest_param(mds[m], in, 137, 200, long_out);
        snprintf(msg, sizeof(msg),
                 "%s: longer squeeze changed the first %zu bytes", names[m],
                 short_len);
        CHECK(memcmp(short_out, long_out, short_len) == 0, msg);
        EVP_MD_free(mds[m]);
    }
}

#if OPENSSL_VERSION_NUMBER >= 0x30500000L
/* (3) EVP_DigestSqueeze (OpenSSL 3.5+): squeezing 10 bytes and then 100

 * bytes must equal a single 110-byte squeeze.  The provider keeps the
 * leftover squeezed state in ctx->buf between calls
 * (crypto/sha/sha3.c), so a two-call squeeze is not the same as two
 * independent squeezes -- it is one continuous output. */

static void test_squeeze(void)
{
    EVP_MD *md = EVP_MD_fetch(NULL, "SHAKE256", NULL);
    unsigned char *in = ALLOCATE_INPUT(137);
    unsigned char *twocall = ALLOCATE_OUTPUT(110);
    unsigned char *onecall = ALLOCATE_OUTPUT(110);
    EVP_MD_CTX *c1 = EVP_MD_CTX_new(), *c2 = EVP_MD_CTX_new();
    size_t i;

    CHECK(md != NULL, "EVP_MD_fetch(\"SHAKE256\") failed");
    for (i = 0; i < 137; i++)
        in[i] = (unsigned char)(i * 7 + 3);
    CHECK(c1 != NULL && c2 != NULL, "EVP_MD_CTX_new failed");

    CHECK(EVP_DigestInit_ex(c1, md, NULL) == 1, "EVP_DigestInit_ex failed");
    CHECK(EVP_DigestUpdate(c1, in, 137) == 1, "EVP_DigestUpdate failed");
    CHECK(EVP_DigestSqueeze(c1, twocall, 10) == 1,
          "EVP_DigestSqueeze (10 bytes) failed");
    CHECK(EVP_DigestSqueeze(c1, twocall + 10, 100) == 1,
          "EVP_DigestSqueeze (100 bytes) failed");

    CHECK(EVP_DigestInit_ex(c2, md, NULL) == 1, "EVP_DigestInit_ex failed");
    CHECK(EVP_DigestUpdate(c2, in, 137) == 1, "EVP_DigestUpdate failed");
    CHECK(EVP_DigestSqueeze(c2, onecall, 110) == 1,
          "EVP_DigestSqueeze (110 bytes) failed");

    CHECK(memcmp(twocall, onecall, 110) == 0,
          "two-call squeeze differs from the single 110-byte squeeze");
    EVP_MD_CTX_free(c1);
    EVP_MD_CTX_free(c2);
    EVP_MD_free(md);
}
#endif /* OPENSSL_VERSION_NUMBER >= 0x30500000L */

/* (4) KECCAK-KMAC-128/256: KMAC-style XOF digests (SHAKE with the KMAC

 * key/custom encoding).  Present in every OpenSSL 3.x build tested here

 * (3.0.13 and 3.6.5). */

static const struct kkmac_kat {
    const char *mdname;
    size_t xoflen;
    const char *outhex;
} kkmac_kats[] = {
    { "KECCAK-KMAC-128", 32,
      "3bcfe6e0471a2168f61c444843e32aea0a09ec15bd9155f169189147f98c11fc" },
    { "KECCAK-KMAC-256", 64,
      "f4e4a2d747910716f38c8ec58a5a50f6b0ea4ebd1e4c92a19e9b36ae640580f1fda41b8b534dfc57a1a719528dadc28e3e6181daba9dc9595e459e249b2bcd95" },
};

static void test_keccak_kmac(void)
{
    size_t k;
    for (k = 0; k < sizeof(kkmac_kats) / sizeof(kkmac_kats[0]); k++) {
        const struct kkmac_kat *kat = &kkmac_kats[k];
        EVP_MD *md = EVP_MD_fetch(NULL, kat->mdname, NULL);
        unsigned char *want = ALLOCATE_OTHER(64);
        unsigned char *got_param = ALLOCATE_OUTPUT(64);
        unsigned char *got_xof = ALLOCATE_OUTPUT(64);
        size_t wantlen;
        char msg[128];

        if (md == NULL) {
            printf("SKIP test_digest_shake: %s is unavailable\n", kat->mdname);
            exit(0);
        }
        CHECK(EVP_MD_get_size(md) == (int)kat->xoflen,
              "KECCAK-KMAC EVP_MD_get_size disagrees with its xoflen");
        wantlen = from_hex(kat->outhex, want, 64);

        xof_digest_param(md, (const unsigned char *)MSG_ABC, 3, kat->xoflen,
                         got_param);
        snprintf(msg, sizeof(msg), "%s(\"abc\") digest does not match the KAT",
                 kat->mdname);
        CHECK(memcmp(got_param, want, wantlen) == 0, msg);

        xof_digest_finalxof(md, (const unsigned char *)MSG_ABC, 3, kat->xoflen,
                            got_xof);
        snprintf(msg, sizeof(msg), "%s: FinalXOF route disagrees with xoflen route",
                 kat->mdname);
        CHECK(memcmp(got_param, got_xof, wantlen) == 0, msg);
        EVP_MD_free(md);
    }
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_kats();
    test_prefix_property();
#if OPENSSL_VERSION_NUMBER >= 0x30500000L
    test_squeeze();
#else
    g_no_squeeze = 1;
#endif

    test_keccak_kmac();

    if (g_no_squeeze) {
        printf("SKIP test_digest_shake: EVP_DigestSqueeze is unavailable\n");
        return 0;
    }
    printf("test_digest_shake: all %u checks passed\n", g_checks);
    return 0;
}

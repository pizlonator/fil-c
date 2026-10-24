#include "poisoning.h"

#include <openssl/evp.h>
#include <openssl/provider.h>

/*
 * test_digest_mdc2_whirlpool -- MDC2 and WHIRLPOOL (legacy provider).
 *
 * Known-answer vectors, all verified with pizfix/bin/openssl:
 *
 *   printf 'abc' | pizfix/bin/openssl dgst -provider legacy \
 *       -provider default -mdc2
 *   printf 'message digest' | pizfix/bin/openssl dgst -provider legacy \
 *       -provider default -whirlpool
 *
 * (plus "abc" for both and the empty message for WHIRLPOOL).  Both
 * algorithms live in the legacy provider, which is loaded explicitly with
 * OSSL_PROVIDER_load(NULL, "legacy").
 *
 * Coverage: legacy provider load; one-shot and chunked equality; out-size
 * checks (MDC2 16 bytes, WHIRLPOOL 64); the WHIRLPOOL byname route.
 *
 * Version/build gating: MDC2 is absent from some distributions' OpenSSL
 * 3.0 builds (e.g. Ubuntu's, which drop it along with single-DES), so a
 * failed MDC2 fetch prints SKIP at the end of main() and exits 0 after
 * the WHIRLPOOL checks have run.
 */

/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */
static void *ALLOCATE_INPUT(size_t n)  { return poison_alloc(POISON_INPUT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }
static void *ALLOCATE_OTHER(size_t n)  { return poison_alloc(POISON_OTHER, n); }

static unsigned g_checks = 0;

/* Set when a genuinely unavailable algorithm was skipped; main() then
 * prints the SKIP line and exits 0. */
static const char *g_skipmsg = NULL;

static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_digest_mdc2_whirlpool: %s\n", msg);
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

/* One-shot digest via EVP_Digest.  Returns the digest length. */
static size_t digest_one_shot(const EVP_MD *md, const unsigned char *in, size_t inlen,
                              unsigned char *out, size_t outcap)
{
    unsigned int outlen = 0;
    CHECK((size_t)EVP_MD_get_size(md) <= outcap,
          "digest output buffer smaller than EVP_MD_get_size");
    CHECK(EVP_Digest(in, inlen, out, &outlen, md, NULL) == 1,
          "EVP_Digest failed");
    return outlen;
}

/* Digest via init/update/final: one byte, then 63 bytes, then the tail. */
static size_t digest_chunked(const EVP_MD *md, const unsigned char *in, size_t inlen,
                             unsigned char *out, size_t outcap)
{
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    unsigned int outlen = 0;
    size_t off = 0;
    CHECK(ctx != NULL, "EVP_MD_CTX_new failed");
    CHECK(EVP_DigestInit_ex(ctx, md, NULL) == 1, "EVP_DigestInit_ex failed");
    if (inlen > 0) {
        CHECK(EVP_DigestUpdate(ctx, in, 1) == 1,
              "EVP_DigestUpdate failed (1-byte chunk)");
        off = 1;
    }
    if (inlen > 1) {
        size_t n = (inlen - off < 63) ? inlen - off : 63;
        CHECK(EVP_DigestUpdate(ctx, in + off, n) == 1,
              "EVP_DigestUpdate failed (63-byte chunk)");
        off += n;
    }
    if (off < inlen) {
        CHECK(EVP_DigestUpdate(ctx, in + off, inlen - off) == 1,
              "EVP_DigestUpdate failed (tail chunk)");
    }
    CHECK((size_t)EVP_MD_get_size(md) <= outcap,
          "digest output buffer smaller than EVP_MD_get_size");
    CHECK(EVP_DigestFinal_ex(ctx, out, &outlen) == 1, "EVP_DigestFinal_ex failed");
    EVP_MD_CTX_free(ctx);
    return outlen;
}

/* (0) The legacy provider hosts MDC2 and WHIRLPOOL in OpenSSL 3.x. */
static void test_legacy_load(void)
{
    /* Loading a provider explicitly disables the auto-load of the

     * default provider, so both must be loaded (legacy hosts MD4/MDC2/

     * WHIRLPOOL; everything else comes from default). */
    OSSL_PROVIDER *legacy = OSSL_PROVIDER_load(NULL, "legacy");
    OSSL_PROVIDER *dflt = OSSL_PROVIDER_load(NULL, "default");
    CHECK(legacy != NULL, "OSSL_PROVIDER_load(NULL, \"legacy\") failed");
    CHECK(dflt != NULL, "OSSL_PROVIDER_load(NULL, \"default\") failed");
}

/* (1) MDC2: the ISO/IEC 10118-2 style KATs (default padding option). */

static void test_mdc2(void)
{
    EVP_MD *md = EVP_MD_fetch(NULL, "MDC2", NULL);
    unsigned char *in = ALLOCATE_INPUT(14);
    unsigned char *want = ALLOCATE_OTHER(16);
    unsigned char *got = ALLOCATE_OUTPUT(16);
    size_t wantlen, gotlen;

    if (md == NULL) {
        /* Absent from some OpenSSL 3.0 distributions (build config, not

         * a core 3.x property). */
        g_skipmsg = "MDC2";
        return;
    }
    CHECK(EVP_MD_get_size(md) == 16, "EVP_MD_get_size(MDC2) is not 16");

    memcpy(in, "abc", 3);
    wantlen = from_hex("3ff42120ee863f5d910cf2ee5064f82f", want, 16);
    gotlen = digest_one_shot(md, in, 3, got, 16);
    CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0,
          "MDC2(\"abc\") one-shot digest does not match the KAT");

    memset(got, 0, 16);
    gotlen = digest_chunked(md, in, 3, got, 16);
    CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0,
          "MDC2(\"abc\") chunked digest does not match the KAT");

    memcpy(in, "message digest", 14);
    wantlen = from_hex("fa13444f04050d012f209890b1c235ab", want, 16);
    gotlen = digest_one_shot(md, in, 14, got, 16);
    CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0,
          "MDC2(\"message digest\") one-shot digest does not match the KAT");
    EVP_MD_free(md);
}

/* (2) WHIRLPOOL: KATs for "abc", "message digest" and the empty message,

 * through the fetch route (one-shot and chunked) and the byname route. */

static void test_whirlpool(void)
{
    EVP_MD *md = EVP_MD_fetch(NULL, "WHIRLPOOL", NULL);
    const EVP_MD *byname_md;
    unsigned char *in = ALLOCATE_INPUT(14);
    unsigned char *want = ALLOCATE_OTHER(64);
    unsigned char *got = ALLOCATE_OUTPUT(64);
    size_t wantlen, gotlen;
    char msg[128];

    CHECK(md != NULL, "EVP_MD_fetch(\"WHIRLPOOL\") failed");
    CHECK(EVP_MD_get_size(md) == 64, "EVP_MD_get_size(WHIRLPOOL) is not 64");

    memcpy(in, "abc", 3);
    wantlen = from_hex("4e2448a4c6f486bb16b6562c73b4020bf3043e3a731bce721ae1b303d97e6d4c7181eebdb6c57e277d0e34957114cbd6c797fc9d95d8b582d225292076d4eef5", want, 64);
    gotlen = digest_one_shot(md, in, 3, got, 64);
    CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0,
          "WHIRLPOOL(\"abc\") one-shot digest does not match the KAT");

    memset(got, 0, 64);
    gotlen = digest_chunked(md, in, 3, got, 64);
    CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0,
          "WHIRLPOOL(\"abc\") chunked digest does not match the KAT");

    memcpy(in, "message digest", 14);
    wantlen = from_hex("378c84a4126e2dc6e56dcc7458377aac838d00032230f53ce1f5700c0ffb4d3b8421557659ef55c106b4b52ac5a4aaa692ed920052838f3362e86dbd37a8903e", want, 64);
    gotlen = digest_one_shot(md, in, 14, got, 64);
    CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0,
          "WHIRLPOOL(\"message digest\") one-shot digest does not match the KAT");

    wantlen = from_hex("19fa61d75522a4669b44e39c1d2e1726c530232130d407f89afee0964997f7a73e83be698b288febcf88e3e03c4f0757ea8964e59b63d93708b138cc42a66eb3", want, 64);
    gotlen = digest_one_shot(md, in, 0, got, 64);
    CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0,
          "WHIRLPOOL(\"\") one-shot digest does not match the KAT");

    byname_md = EVP_get_digestbyname("whirlpool");
    CHECK(byname_md != NULL, "EVP_get_digestbyname(\"whirlpool\") returned NULL");
    memcpy(in, "abc", 3);
    memset(got, 0, 64);
    gotlen = digest_chunked(byname_md, in, 3, got, 64);
    wantlen = from_hex("4e2448a4c6f486bb16b6562c73b4020bf3043e3a731bce721ae1b303d97e6d4c7181eebdb6c57e277d0e34957114cbd6c797fc9d95d8b582d225292076d4eef5", want, 64);
    snprintf(msg, sizeof(msg),
             "EVP_get_digestbyname(\"whirlpool\") digest does not match the KAT");
    CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0, msg);
    EVP_MD_free(md);
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_legacy_load();
    test_mdc2();
    test_whirlpool();

    if (g_skipmsg != NULL) {
        printf("SKIP test_digest_mdc2_whirlpool: %s is unavailable\n", g_skipmsg);
        return 0;
    }
    printf("test_digest_mdc2_whirlpool: all %u checks passed\n", g_checks);
    return 0;
}

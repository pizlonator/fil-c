#include "poisoning.h"

#include <openssl/evp.h>
#include <openssl/provider.h>

/*
 * test_digest_md5 -- MD5, MD5-SHA1 and MD4 (legacy provider).
 *
 * Known-answer vectors, all verified with pizfix/bin/openssl, e.g.
 *
 *   printf 'abc' | pizfix/bin/openssl dgst -md5
 *   printf 'abc' | pizfix/bin/openssl dgst -md5-sha1
 *   printf 'abc' | pizfix/bin/openssl dgst -provider legacy \
 *       -provider default -md4
 *
 *   MD5("")    = d41d8cd9...   MD5("abc")   = 90015098...
 *   MD5-SHA1("abc") = MD5("abc") || SHA1("abc")   (36 bytes)
 *   MD4("abc") = a448017a...
 *
 * MD4 lives in the legacy provider, which is loaded explicitly with
 * OSSL_PROVIDER_load(NULL, "legacy") (the default provider is loaded
 * implicitly).  Coverage: fetch routes for all three digests, the static
 * EVP_md5() route, EVP_get_digestbyname routes, chunked-vs-one-shot
 * equality around the 64-byte block boundary, MD5-SHA1 streaming updates
 * split across the two halves, and the concatenation property of
 * MD5-SHA1.
 */

/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */
static void *ALLOCATE_INPUT(size_t n)  { return poison_alloc(POISON_INPUT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }
static void *ALLOCATE_OTHER(size_t n)  { return poison_alloc(POISON_OTHER, n); }

static unsigned g_checks = 0;

static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_digest_md5: %s\n", msg);
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

/* Digest via init/update/final: one byte, then 63 bytes, then the tail.
 * Must equal the one-shot digest for every input length. */
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

/* Digest via init/update/final with explicit chunk lengths {a, b, rest}. */
static size_t digest_chunked_ab(const EVP_MD *md, const unsigned char *in, size_t inlen,
                                size_t a, size_t b, unsigned char *out, size_t outcap)
{
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    unsigned int outlen = 0;
    size_t off = 0;
    CHECK(ctx != NULL, "EVP_MD_CTX_new failed");
    CHECK(EVP_DigestInit_ex(ctx, md, NULL) == 1, "EVP_DigestInit_ex failed");
    if (a > 0 && inlen > 0) {
        size_t n = a < inlen ? a : inlen;
        CHECK(EVP_DigestUpdate(ctx, in, n) == 1,
              "EVP_DigestUpdate failed (first chunk)");
        off = n;
    }
    if (b > 0 && off < inlen) {
        size_t n = b < inlen - off ? b : inlen - off;
        CHECK(EVP_DigestUpdate(ctx, in + off, n) == 1,
              "EVP_DigestUpdate failed (second chunk)");
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

static const char *const MSG_ABC = "abc";

static const struct md5_kat {
    const char *mdname;
    const char *msg;
    const char *mdhex;
} md5_kats[] = {
    { "MD5", NULL, /* the empty message */
      "d41d8cd98f00b204e9800998ecf8427e" },
    { "MD5", "abc",
      "900150983cd24fb0d6963f7d28e17f72" },
    { "MD4", "abc",
      "a448017aaf21d8525fc10ae87aa6729d" },
};

/* (0) The legacy provider is required for MD4 (MDC2-style legacy
 * algorithms live there in every OpenSSL 3.x). */
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

/* (1) One-shot EVP_Digest with EVP_MD_fetch == KAT for every vector
 * (MD4's fetch requires the legacy provider loaded above). */
static void test_kats(void)
{
    size_t k;
    for (k = 0; k < sizeof(md5_kats) / sizeof(md5_kats[0]); k++) {
        const struct md5_kat *kat = &md5_kats[k];
        EVP_MD *md = EVP_MD_fetch(NULL, kat->mdname, NULL);
        unsigned char *in = ALLOCATE_INPUT(3);
        unsigned char *want = ALLOCATE_OTHER(16);
        unsigned char *got = ALLOCATE_OUTPUT(16);
        size_t inlen = 0, wantlen, gotlen;
        char msg[128];

        CHECK(md != NULL, "EVP_MD_fetch failed (is the legacy provider loaded?)");
        CHECK(EVP_MD_get_size(md) == (int)(strlen(kat->mdhex) / 2),
              "EVP_MD_get_size disagrees with the KAT digest length");
        if (kat->msg != NULL) {
            inlen = strlen(kat->msg);
            memcpy(in, kat->msg, inlen);
        }
        wantlen = from_hex(kat->mdhex, want, 16);
        gotlen = digest_one_shot(md, in, inlen, got, 16);
        snprintf(msg, sizeof(msg), "%s one-shot digest does not match the KAT",
                 kat->mdname);
        CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0, msg);
        EVP_MD_free(md);
    }
}

/* (2) The "abc" KATs through the alternate routes: static EVP_md5() and

 * EVP_get_digestbyname("md5") / ("md4"). */

static void test_routes(void)
{
    const EVP_MD *static_md;
    const EVP_MD *byname_md;
    unsigned char *in = ALLOCATE_INPUT(3);
    unsigned char *want_md5 = ALLOCATE_OTHER(16);
    unsigned char *want_md4 = ALLOCATE_OTHER(16);
    unsigned char *got = ALLOCATE_OUTPUT(16);
    size_t wantlen, gotlen;

    memcpy(in, MSG_ABC, 3);
    wantlen = from_hex(md5_kats[1].mdhex, want_md5, 16);
    from_hex(md5_kats[2].mdhex, want_md4, 16);

    static_md = EVP_md5();
    CHECK(static_md != NULL, "EVP_md5() returned NULL");
    gotlen = digest_one_shot(static_md, in, 3, got, 16);
    CHECK(gotlen == wantlen && memcmp(got, want_md5, wantlen) == 0,
          "EVP_md5() digest does not match the KAT");

    byname_md = EVP_get_digestbyname("md5");
    CHECK(byname_md != NULL, "EVP_get_digestbyname(\"md5\") returned NULL");
    memset(got, 0, 16);
    gotlen = digest_chunked(byname_md, in, 3, got, 16);
    CHECK(gotlen == wantlen && memcmp(got, want_md5, wantlen) == 0,
          "EVP_get_digestbyname(\"md5\") digest does not match the KAT");

    byname_md = EVP_get_digestbyname("md4");
    CHECK(byname_md != NULL, "EVP_get_digestbyname(\"md4\") returned NULL");
    memset(got, 0, 16);
    gotlen = digest_chunked(byname_md, in, 3, got, 16);
    CHECK(gotlen == 16 && memcmp(got, want_md4, 16) == 0,
          "EVP_get_digestbyname(\"md4\") digest does not match the KAT");
}

/* (3) Chunked-vs-one-shot equality for MD5 around the 64-byte block

 * boundary (deterministic byte pattern i*7+3). */

static void test_md5_chunked_equality(void)
{
    static const size_t sizes[] = { 0, 1, 55, 56, 63, 64, 65, 1000 };
    EVP_MD *md = EVP_MD_fetch(NULL, "MD5", NULL);
    unsigned char *in = ALLOCATE_INPUT(1000);
    unsigned char *one = ALLOCATE_OUTPUT(16);
    unsigned char *many = ALLOCATE_OUTPUT(16);
    size_t s, i;
    char msg[128];

    CHECK(md != NULL, "EVP_MD_fetch(\"MD5\") failed");
    for (i = 0; i < 1000; i++)
        in[i] = (unsigned char)(i * 7 + 3);
    for (s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
        size_t n = sizes[s];
        size_t one_len = digest_one_shot(md, in, n, one, 16);
        size_t many_len = digest_chunked(md, in, n, many, 16);
        snprintf(msg, sizeof(msg),
                 "MD5 chunked digest differs from one-shot at input length %zu", n);
        CHECK(one_len == many_len && memcmp(one, many, one_len) == 0, msg);
    }
    EVP_MD_free(md);
}

/* (4) MD5-SHA1: the 36-byte concatenation of MD5("abc") and SHA1("abc").

 * Verified as a KAT, split across the two halves with streaming updates

 * ({2, 20, rest} bytes), and by re-deriving it from the two component

 * digests. */

static void test_md5_sha1(void)
{
    EVP_MD *md = EVP_MD_fetch(NULL, "MD5-SHA1", NULL);
    EVP_MD *md5 = EVP_MD_fetch(NULL, "MD5", NULL);
    EVP_MD *sha1 = EVP_MD_fetch(NULL, "SHA1", NULL);
    unsigned char *in = ALLOCATE_INPUT(3);
    unsigned char *want = ALLOCATE_OTHER(36);
    unsigned char *got = ALLOCATE_OUTPUT(36);
    unsigned char *halves = ALLOCATE_OTHER(36);
    unsigned char *md5_out = ALLOCATE_OUTPUT(16);
    unsigned char *sha1_out = ALLOCATE_OUTPUT(20);
    size_t wantlen, gotlen, md5len, sha1len;

    CHECK(md != NULL, "EVP_MD_fetch(\"MD5-SHA1\") failed");
    CHECK(md5 != NULL && sha1 != NULL, "EVP_MD_fetch failed for MD5 or SHA1");
    CHECK(EVP_MD_get_size(md) == 36, "EVP_MD_get_size(MD5-SHA1) is not 36");

    memcpy(in, MSG_ABC, 3);
    wantlen = from_hex("900150983cd24fb0d6963f7d28e17f72a9993e364706816aba3e25717850c26c9cd0d89d", want, 36);

    gotlen = digest_one_shot(md, in, 3, got, 36);
    CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0,
          "MD5-SHA1(\"abc\") one-shot digest does not match the KAT");

    memset(got, 0, 36);
    gotlen = digest_chunked_ab(md, in, 3, 2, 20, got, 36);
    CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0,
          "MD5-SHA1 streaming updates ({2,20,rest}) do not match the KAT");

    md5len = digest_one_shot(md5, in, 3, md5_out, 16);
    sha1len = digest_one_shot(sha1, in, 3, sha1_out, 20);
    CHECK(md5len == 16 && sha1len == 20, "component digest lengths are wrong");
    memcpy(halves, md5_out, 16);
    memcpy(halves + 16, sha1_out, 20);
    CHECK(memcmp(halves, want, 36) == 0,
          "MD5-SHA1 is not the concatenation MD5(\"abc\") || SHA1(\"abc\")");
    EVP_MD_free(md);
    EVP_MD_free(md5);
    EVP_MD_free(sha1);
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_legacy_load();
    test_kats();
    test_routes();
    test_md5_chunked_equality();
    test_md5_sha1();

    printf("test_digest_md5: all %u checks passed\n", g_checks);
    return 0;
}

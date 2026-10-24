#include "poisoning.h"

#include <openssl/evp.h>

/*
 * test_digest_ripemd_sm3 -- RIPEMD-160 and SM3 known-answer and behavior
 * tests.
 *
 * Known-answer vectors, all verified with pizfix/bin/openssl, e.g.
 *
 *   printf 'abc' | pizfix/bin/openssl dgst -ripemd160
 *   printf 'abc' | pizfix/bin/openssl dgst -sm3
 *
 *   RIPEMD-160("") = 9c1185a5...   RIPEMD-160("abc") = 8eb208f7...
 *   SM3("abc")     = 66c7f0f4...   (the GB/T 32905-2016 sample vector)
 *
 * Both digests are in the default provider in the OpenSSL 3.x builds
 * exercised here (no legacy load needed).  Coverage: fetch and
 * EVP_get_digestbyname routes, EVP_MD_get_size checks (20 and 32), and
 * chunked-vs-one-shot equality including exact 64-byte block multiples
 * (the underlying compression functions are 64-byte Merkle-Damgard).
 */

/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */
static void *ALLOCATE_INPUT(size_t n)  { return poison_alloc(POISON_INPUT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }
static void *ALLOCATE_OTHER(size_t n)  { return poison_alloc(POISON_OTHER, n); }

static unsigned g_checks = 0;

static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_digest_ripemd_sm3: %s\n", msg);
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

/* Digest with all-chunks-equal-block-size updates: feeds inlen bytes as
 * successive `block`-byte updates, so the final update lands exactly on a
 * block boundary when inlen is a multiple of block. */
static size_t digest_chunked_block(const EVP_MD *md, const unsigned char *in,
                                   size_t inlen, size_t block,
                                   unsigned char *out, size_t outcap)
{
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    unsigned int outlen = 0;
    size_t off = 0;
    CHECK(ctx != NULL, "EVP_MD_CTX_new failed");
    CHECK(EVP_DigestInit_ex(ctx, md, NULL) == 1, "EVP_DigestInit_ex failed");
    while (off < inlen) {
        size_t n = block < inlen - off ? block : inlen - off;
        CHECK(EVP_DigestUpdate(ctx, in + off, n) == 1,
              "EVP_DigestUpdate failed (block-size chunk)");
        off += n;
    }
    CHECK((size_t)EVP_MD_get_size(md) <= outcap,
          "digest output buffer smaller than EVP_MD_get_size");
    CHECK(EVP_DigestFinal_ex(ctx, out, &outlen) == 1, "EVP_DigestFinal_ex failed");
    EVP_MD_CTX_free(ctx);
    return outlen;
}

static const char *const MSG_ABC = "abc";

/* (1) KATs through EVP_MD_fetch, plus EVP_MD_get_size checks. */
static void test_kats(void)
{
    static const struct {
        const char *mdname;
        int mdsize;
        const char *emptyhex;
        const char *abchex;
    } kats[] = {
        { "RIPEMD160", 20,
          "9c1185a5c5e9fc54612808977ee8f548b2258d31",
          "8eb208f7e05d987a9b044a8e98c6b087f15a0bfc" },
        { "SM3", 32,
          NULL,
          "66c7f0f462eeedd9d1f2d46bdc10e4e24167c4875cf2f7a2297da02b8f4ba8e0" },
    };
    size_t k;
    for (k = 0; k < sizeof(kats) / sizeof(kats[0]); k++) {
        EVP_MD *md = EVP_MD_fetch(NULL, kats[k].mdname, NULL);
        unsigned char *in = ALLOCATE_INPUT(3);
        unsigned char *want = ALLOCATE_OTHER(32);
        unsigned char *got = ALLOCATE_OUTPUT(32);
        size_t wantlen, gotlen;
        char msg[128];

        CHECK(md != NULL, "EVP_MD_fetch failed for a core digest");
        snprintf(msg, sizeof(msg), "%s: EVP_MD_get_size is wrong", kats[k].mdname);
        CHECK(EVP_MD_get_size(md) == kats[k].mdsize, msg);

        memcpy(in, MSG_ABC, 3);
        wantlen = from_hex(kats[k].abchex, want, 32);
        gotlen = digest_one_shot(md, in, 3, got, 32);
        snprintf(msg, sizeof(msg), "%s(\"abc\") one-shot digest does not match the KAT",
                 kats[k].mdname);
        CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0, msg);

        if (kats[k].emptyhex != NULL) {
            wantlen = from_hex(kats[k].emptyhex, want, 32);
            gotlen = digest_one_shot(md, in, 0, got, 32);
            snprintf(msg, sizeof(msg), "%s(\"\") one-shot digest does not match the KAT",
                     kats[k].mdname);
            CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0, msg);
        }
        EVP_MD_free(md);
    }
}

/* (2) The "abc" KATs via EVP_get_digestbyname("ripemd160") and ("sm3"). */
static void test_byname_routes(void)
{
    const EVP_MD *byname_md;
    unsigned char *in = ALLOCATE_INPUT(3);
    unsigned char *want = ALLOCATE_OTHER(32);
    unsigned char *got = ALLOCATE_OUTPUT(32);
    size_t wantlen, gotlen;

    memcpy(in, MSG_ABC, 3);

    byname_md = EVP_get_digestbyname("ripemd160");
    CHECK(byname_md != NULL, "EVP_get_digestbyname(\"ripemd160\") returned NULL");
    wantlen = from_hex("8eb208f7e05d987a9b044a8e98c6b087f15a0bfc", want, 32);
    memset(got, 0, 32);
    gotlen = digest_chunked(byname_md, in, 3, got, 32);
    CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0,
          "EVP_get_digestbyname(\"ripemd160\") digest does not match the KAT");

    byname_md = EVP_get_digestbyname("sm3");
    CHECK(byname_md != NULL, "EVP_get_digestbyname(\"sm3\") returned NULL");
    wantlen = from_hex("66c7f0f462eeedd9d1f2d46bdc10e4e24167c4875cf2f7a2297da02b8f4ba8e0", want, 32);
    memset(got, 0, 32);
    digest_chunked(byname_md, in, 3, got, 32);
    CHECK(memcmp(got, want, wantlen) == 0,
          "EVP_get_digestbyname(\"sm3\") digest does not match the KAT");
}

/* (3) Chunked-vs-one-shot equality around the 64-byte block boundary and

 * at exact block multiples (block-stashed-at-final behavior must not

 * change the result). */

static void test_chunked_equality(void)
{
    static const size_t sizes[] = { 0, 1, 55, 56, 63, 64, 65, 128, 129, 1000 };
    static const char *const names[] = { "RIPEMD160", "SM3" };
    unsigned char *in = ALLOCATE_INPUT(1000);
    unsigned char *one = ALLOCATE_OUTPUT(32);
    unsigned char *many = ALLOCATE_OUTPUT(32);
    size_t m, s, i;
    char msg[128];

    for (i = 0; i < 1000; i++)
        in[i] = (unsigned char)(i * 7 + 3);

    for (m = 0; m < sizeof(names) / sizeof(names[0]); m++) {
        EVP_MD *md = EVP_MD_fetch(NULL, names[m], NULL);
        size_t mdsize = (size_t)EVP_MD_get_size(md);
        CHECK(md != NULL, "EVP_MD_fetch failed for a core digest");
        for (s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
            size_t n = sizes[s];
            size_t one_len = digest_one_shot(md, in, n, one, 32);
            size_t many_len = digest_chunked(md, in, n, many, 32);
            snprintf(msg, sizeof(msg),
                     "%s chunked digest differs from one-shot at input length %zu",
                     names[m], n);
            CHECK(one_len == many_len && memcmp(one, many, one_len) == 0, msg);
        }
        /* 256 bytes fed as exact 64-byte updates. */
        {
            size_t one_len = digest_one_shot(md, in, 256, one, 32);
            size_t many_len = digest_chunked_block(md, in, 256, 64, many, 32);
            CHECK(one_len == mdsize && many_len == mdsize,
                  "block-chunked digest has the wrong length");
            CHECK(memcmp(one, many, mdsize) == 0,
                  "all-64-byte-chunk digest differs from one-shot");
        }
        EVP_MD_free(md);
    }
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_kats();
    test_byname_routes();
    test_chunked_equality();

    printf("test_digest_ripemd_sm3: all %u checks passed\n", g_checks);
    return 0;
}

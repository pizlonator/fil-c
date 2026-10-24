#include "poisoning.h"

#include <openssl/evp.h>

/*
 * test_digest_sha2 -- SHA-2 family digest tests: SHA2-224/256/384/512 and
 * the SHA2-512/224 and SHA2-512/256 truncations, plus the SKIP path for
 * SHA2-256/192 (not offered by any current OpenSSL 3.x release).
 *
 * Known-answer vectors (all verified against the authoritative build with:
 *
 *   printf 'abc' | pizfix/bin/openssl dgst -sha256
 *
 * and likewise for -sha224/-sha384/-sha512/-sha512-224/-sha512-256):
 *
 *   "abc"   -- the classic one-block FIPS vector, all six variants
 *   56-byte "abcdbcde...nopq" -- the two-block-boundary FIPS vector
 *   112-byte "abcdefgh...nopqrstu" -- the two-block SHA-512 vector
 */

/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */
static void *ALLOCATE_INPUT(size_t n)  { return poison_alloc(POISON_INPUT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }
static void *ALLOCATE_OTHER(size_t n)  { return poison_alloc(POISON_OTHER, n); }

static unsigned g_checks = 0;

static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_digest_sha2: %s\n", msg);
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

/* The classic FIPS messages. */
static const char *const MSG_ABC =
    "abc";
static const char *const MSG_56 =
    "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
static const char *const MSG_112 =
    "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmno"
    "ijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu";

static const struct sha_kat {
    const char *mdname;
    const char *msg;
    const char *mdhex;
} sha_kats[] = {
    { "SHA2-224", MSG_ABC,
      "23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7" },
    { "SHA2-256", MSG_ABC,
      "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" },
    { "SHA2-384", MSG_ABC,
      "cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed"
      "8086072ba1e7cc2358baeca134c825a7" },
    { "SHA2-512", MSG_ABC,
      "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
      "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f" },
    { "SHA2-512/224", MSG_ABC,
      "4634270f707b6a54daae7530460842e20e37ed265ceee9a43e8924aa" },
    { "SHA2-512/256", MSG_ABC,
      "53048e2681941ef99b2e29b76b4c7dabe4c2d0c634fc6d46e0e2f13107e7af23" },
    { "SHA2-256", MSG_56,
      "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1" },
    { "SHA2-512", MSG_56,
      "204a8fc6dda82f0a0ced7beb8e08a41657c16ef468b228a8279be331a703c335"
      "96fd15c13b1b07f9aa1d3bea57789ca031ad85c7a71dd70354ec631238ca3445" },
    { "SHA2-512", MSG_112,
      "8e959b75dae313da8cf4f72814fc143f8f7779c6eb9f7fa17299aeadb6889018"
      "501d289e4900f7e4331b99dec4b5433ac7d329eeb6dd26545e96e55b874be909" },
    { "SHA2-256", MSG_112,
      "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1" },
};

/* One-shot digest via EVP_Digest.  `out` must have room for
 * EVP_MD_get_size(md) bytes.  Returns the digest length. */
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

/* Digest via init/update/final with a deliberately awkward chunking: one
 * byte, then 63 bytes, then everything else in a single final update.  The
 * result must equal the one-shot digest for every input length, including
 * lengths around the 64- and 128-byte block boundaries. */
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

/* (1) One-shot EVP_Digest with EVP_MD_fetch == KAT for every vector; also
 * checks EVP_MD_get_size against the KAT length. */
static void test_kats(void)
{
    size_t k;
    for (k = 0; k < sizeof(sha_kats) / sizeof(sha_kats[0]); k++) {
        const struct sha_kat *kat = &sha_kats[k];
        EVP_MD *md = EVP_MD_fetch(NULL, kat->mdname, NULL);
        unsigned char *in = ALLOCATE_INPUT(112);
        unsigned char *want = ALLOCATE_OTHER(64);
        unsigned char *got = ALLOCATE_OUTPUT(64);
        size_t inlen, wantlen, gotlen;
        char msg[128];

        CHECK(md != NULL, "EVP_MD_fetch failed for a core SHA-2 digest");
        CHECK(EVP_MD_get_size(md) == (int)(strlen(kat->mdhex) / 2),
              "EVP_MD_get_size disagrees with the KAT digest length");

        inlen = strlen(kat->msg);
        memcpy(in, kat->msg, inlen);
        wantlen = from_hex(kat->mdhex, want, 64);

        gotlen = digest_one_shot(md, in, inlen, got, 64);
        snprintf(msg, sizeof(msg), "%s(\"%s\") one-shot digest does not match the KAT",
                 kat->mdname, inlen == 3 ? "abc" : "...");
        CHECK(gotlen == wantlen, msg);
        CHECK(memcmp(got, want, wantlen) == 0, msg);
        EVP_MD_free(md);
    }
}

/* (2) + (3) The same KAT via the legacy static EVP_sha256() route and via
 * EVP_get_digestbyname("sha256"). */
static void test_routes(void)
{
    const EVP_MD *static_md;
    const EVP_MD *byname_md;
    unsigned char *in = ALLOCATE_INPUT(3);
    unsigned char *want = ALLOCATE_OTHER(32);
    unsigned char *got = ALLOCATE_OUTPUT(32);
    size_t wantlen, gotlen;

    memcpy(in, MSG_ABC, 3);
    wantlen = from_hex(sha_kats[1].mdhex, want, 32);

    static_md = EVP_sha256();
    CHECK(static_md != NULL, "EVP_sha256() returned NULL");
    gotlen = digest_one_shot(static_md, in, 3, got, 32);
    CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0,
          "EVP_sha256() digest does not match the KAT");

    byname_md = EVP_get_digestbyname("sha256");
    CHECK(byname_md != NULL, "EVP_get_digestbyname(\"sha256\") returned NULL");
    memset(got, 0, 32);
    gotlen = digest_chunked(byname_md, in, 3, got, 32);
    CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0,
          "EVP_get_digestbyname() digest does not match the KAT");
}

/* (4) Chunked digesting must equal one-shot digesting for input lengths
 * around every block boundary, for SHA2-256 and SHA2-512. */
static void test_chunked_equality(void)
{
    static const size_t sizes[] = {
        0, 1, 55, 56, 63, 64, 65, 111, 112, 127, 128, 129, 1000
    };
    static const char *const names[] = { "SHA2-256", "SHA2-512" };
    unsigned char *in = ALLOCATE_INPUT(1000);
    unsigned char *one = ALLOCATE_OUTPUT(64);
    unsigned char *many = ALLOCATE_OUTPUT(64);
    size_t s, m, i;
    char msg[128];

    for (i = 0; i < 1000; i++)
        in[i] = (unsigned char)(i * 7 + 3);

    for (m = 0; m < sizeof(names) / sizeof(names[0]); m++) {
        EVP_MD *md = EVP_MD_fetch(NULL, names[m], NULL);
        CHECK(md != NULL, "EVP_MD_fetch failed for a core SHA-2 digest");
        for (s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
            size_t n = sizes[s];
            size_t one_len = digest_one_shot(md, in, n, one, 64);
            size_t many_len = digest_chunked(md, in, n, many, 64);
            snprintf(msg, sizeof(msg),
                     "%s chunked digest differs from one-shot at input length %zu",
                     names[m], n);
            CHECK(one_len == many_len && memcmp(one, many, one_len) == 0, msg);
        }
        EVP_MD_free(md);
    }
}

/* (5) EVP_MD_get_size for all six SHA-2 variants. */
static void test_digest_sizes(void)
{
    static const struct {
        const char *name;
        int size;
    } expect[] = {
        { "SHA2-224", 28 }, { "SHA2-256", 32 }, { "SHA2-384", 48 },
        { "SHA2-512", 64 }, { "SHA2-512/224", 28 }, { "SHA2-512/256", 32 },
    };
    size_t e;
    for (e = 0; e < sizeof(expect) / sizeof(expect[0]); e++) {
        EVP_MD *md = EVP_MD_fetch(NULL, expect[e].name, NULL);
        CHECK(md != NULL, "EVP_MD_fetch failed for a core SHA-2 digest");
        CHECK(EVP_MD_get_size(md) == expect[e].size,
              "EVP_MD_get_size returned an unexpected digest size");
        EVP_MD_free(md);
    }
}

/* (6) SHA2-256/192 is genuinely version-gated (no OpenSSL 3.x ships it), so
 * its absence takes the SKIP path; if a future build offers it, verify what
 * we can without an unverified KAT: the digest size and chunked/one-shot
 * self-consistency. */
static void test_sha2_256_192(void)
{
    EVP_MD *md = EVP_MD_fetch(NULL, "SHA2-256/192", NULL);
    unsigned char *in = ALLOCATE_INPUT(3);
    unsigned char *one = ALLOCATE_OUTPUT(64);
    unsigned char *many = ALLOCATE_OUTPUT(64);

    if (md == NULL) {
        printf("SKIP test_digest_sha2: SHA2-256/192 is unavailable\n");
        exit(0);
    }
    memcpy(in, MSG_ABC, 3);
    CHECK(EVP_MD_get_size(md) == 24,
          "SHA2-256/192 digest size is not 24 bytes");
    CHECK(digest_one_shot(md, in, 3, one, 64) == 24,
          "SHA2-256/192 one-shot digest has the wrong length");
    CHECK(digest_chunked(md, in, 3, many, 64) == 24 &&
          memcmp(one, many, 24) == 0,
          "SHA2-256/192 chunked digest differs from one-shot");
    EVP_MD_free(md);
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_kats();
    test_routes();
    test_chunked_equality();
    test_digest_sizes();
    test_sha2_256_192();

    printf("test_digest_sha2: all %u checks passed\n", g_checks);
    return 0;
}

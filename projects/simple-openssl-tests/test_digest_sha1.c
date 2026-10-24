#include "poisoning.h"

#include <openssl/evp.h>

/*
 * test_digest_sha1 -- SHA-1 known-answer and behavior tests.
 *
 * Known-answer vectors (verified with pizfix/bin/openssl, e.g.
 *
 *   printf 'abc' | pizfix/bin/openssl dgst -sha1
 * ):
 *
 *   ""                 -- the empty-message vector (da39a3ee...)
 *   "abc"              -- the classic one-block FIPS vector
 *   56-byte "abcdbcde...nopq" -- the two-block-boundary FIPS vector
 *
 * Coverage: one-shot EVP_Digest with EVP_MD_fetch("SHA1"); the static
 * EVP_sha1() route; EVP_get_digestbyname("sha1"); chunked-vs-one-shot
 * equality for input lengths around the 64-byte block boundary; and
 * EVP_MD_get_size() == 20.
 */

/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */

static void *ALLOCATE_INPUT(size_t n)  { return poison_alloc(POISON_INPUT, n); }

static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }

static void *ALLOCATE_OTHER(size_t n)  { return poison_alloc(POISON_OTHER, n); }

static unsigned g_checks = 0;

static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_digest_sha1: %s\n", msg);
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
 * lengths around the 64-byte block boundary. */

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

/* The classic FIPS messages. */

static const char *const MSG_ABC = "abc";
static const char *const MSG_56 =
    "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";

static const struct sha1_kat {
    const char *msg;
    const char *mdhex;
} sha1_kats[] = {
    { NULL, /* the empty message */
      "da39a3ee5e6b4b0d3255bfef95601890afd80709" },
    { "abc",
      "a9993e364706816aba3e25717850c26c9cd0d89d" },
    /* 56 bytes: two-block-boundary FIPS vector */
    { "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
      "84983e441c3bd26ebaae4aa1f95129e5e54670f1" },
};

/* (1) One-shot EVP_Digest with EVP_MD_fetch("SHA1") == KAT for every

 * vector; also checks EVP_MD_get_size against the KAT digest length. */

static void test_kats(void)
{
    size_t k;
    for (k = 0; k < sizeof(sha1_kats) / sizeof(sha1_kats[0]); k++) {
        const struct sha1_kat *kat = &sha1_kats[k];
        EVP_MD *md = EVP_MD_fetch(NULL, "SHA1", NULL);
        unsigned char *in = ALLOCATE_INPUT(56);
        unsigned char *want = ALLOCATE_OTHER(20);
        unsigned char *got = ALLOCATE_OUTPUT(20);
        size_t inlen, wantlen, gotlen;
        char msg[128];

        CHECK(md != NULL, "EVP_MD_fetch(\"SHA1\") failed (SHA1 is core)");
        CHECK(EVP_MD_get_size(md) == 20, "EVP_MD_get_size(SHA1) is not 20");

        if (kat->msg == NULL) {
            inlen = 0;
            memcpy(msg, "SHA1(\"\")", sizeof("SHA1(\"\")"));
        } else {
            inlen = strlen(kat->msg);
            memcpy(in, kat->msg, inlen);
            snprintf(msg, sizeof(msg), "SHA1(\"%.3s...\")", kat->msg);
        }
        wantlen = from_hex(kat->mdhex, want, 20);

        gotlen = digest_one_shot(md, in, inlen, got, 20);
        strncat(msg, " one-shot digest does not match the KAT",
                sizeof(msg) - strlen(msg) - 1);
        CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0, msg);
        EVP_MD_free(md);
    }
}

/* (2) + (3) The "abc" KAT via the static EVP_sha1() route and via

 * EVP_get_digestbyname("sha1"). */

static void test_routes(void)
{
    const EVP_MD *static_md;
    const EVP_MD *byname_md;
    unsigned char *in = ALLOCATE_INPUT(3);
    unsigned char *want = ALLOCATE_OTHER(20);
    unsigned char *got = ALLOCATE_OUTPUT(20);
    size_t wantlen, gotlen;

    memcpy(in, MSG_ABC, 3);
    wantlen = from_hex(sha1_kats[1].mdhex, want, 20);

    static_md = EVP_sha1();
    CHECK(static_md != NULL, "EVP_sha1() returned NULL");
    gotlen = digest_one_shot(static_md, in, 3, got, 20);
    CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0,
          "EVP_sha1() digest does not match the KAT");

    byname_md = EVP_get_digestbyname("sha1");
    CHECK(byname_md != NULL, "EVP_get_digestbyname(\"sha1\") returned NULL");
    memset(got, 0, 20);
    gotlen = digest_chunked(byname_md, in, 3, got, 20);
    CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0,
          "EVP_get_digestbyname(\"sha1\") digest does not match the KAT");
}

/* (4) Chunked digesting must equal one-shot digesting for input lengths

 * around the 64-byte block boundary (and one long input), using the
 * deterministic byte pattern i*7+3. */

static void test_chunked_equality(void)
{
    static const size_t sizes[] = { 0, 1, 55, 56, 63, 64, 65, 1000 };
    EVP_MD *md = EVP_MD_fetch(NULL, "SHA1", NULL);
    unsigned char *in = ALLOCATE_INPUT(1000);
    unsigned char *one = ALLOCATE_OUTPUT(20);
    unsigned char *many = ALLOCATE_OUTPUT(20);
    size_t s, i;
    char msg[128];

    CHECK(md != NULL, "EVP_MD_fetch(\"SHA1\") failed");
    for (i = 0; i < 1000; i++)
        in[i] = (unsigned char)(i * 7 + 3);

    for (s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
        size_t n = sizes[s];
        size_t one_len = digest_one_shot(md, in, n, one, 20);
        size_t many_len = digest_chunked(md, in, n, many, 20);
        snprintf(msg, sizeof(msg),
                 "SHA1 chunked digest differs from one-shot at input length %zu",
                 n);
        CHECK(one_len == many_len && memcmp(one, many, one_len) == 0, msg);
    }
    EVP_MD_free(md);
}

/* (5) The 56-byte classic message must also digest correctly through the

 * chunked route (it straddles the two-block padding boundary). */

static void test_msg56_chunked(void)
{
    EVP_MD *md = EVP_MD_fetch(NULL, "SHA1", NULL);
    unsigned char *in = ALLOCATE_INPUT(56);
    unsigned char *want = ALLOCATE_OTHER(20);
    unsigned char *got = ALLOCATE_OUTPUT(20);
    size_t wantlen, gotlen;

    CHECK(md != NULL, "EVP_MD_fetch(\"SHA1\") failed");
    memcpy(in, MSG_56, 56);
    wantlen = from_hex(sha1_kats[2].mdhex, want, 20);
    gotlen = digest_chunked(md, in, 56, got, 20);
    CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0,
          "chunked SHA1 of the 56-byte message does not match the KAT");
    EVP_MD_free(md);
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_kats();
    test_routes();
    test_chunked_equality();
    test_msg56_chunked();

    printf("test_digest_sha1: all %u checks passed\n", g_checks);
    return 0;
}


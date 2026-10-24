#include "poisoning.h"

#include <openssl/evp.h>

/*
 * test_digest_blake2 -- BLAKE2b-512 and BLAKE2s-256 known-answer and
 * behavior tests.
 *
 * Known-answer vectors (the official RFC 7693-style vectors for the
 * unkeyed hashes), all verified with pizfix/bin/openssl, e.g.
 *
 *   printf 'abc' | pizfix/bin/openssl dgst -blake2b512
 *
 *   BLAKE2b-512("") = 786a02f7...   BLAKE2s-256("") = 69217a30...
 *   BLAKE2b-512("abc") = ba80a53f... BLAKE2s-256("abc") = 508c5e8c...
 *
 * Coverage: fetch and EVP_get_digestbyname routes; EVP_MD_get_size checks
 * (64 and 32); chunked-vs-one-shot equality INCLUDING updates that are
 * exact multiples of the block size (128 for BLAKE2b, 64 for BLAKE2s) --
 * the implementations stash a complete final block away for the final
 * call, so those splits must still agree; mixed odd chunks.
 */

/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */
static void *ALLOCATE_INPUT(size_t n)  { return poison_alloc(POISON_INPUT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }
static void *ALLOCATE_OTHER(size_t n)  { return poison_alloc(POISON_OTHER, n); }

static unsigned g_checks = 0;

static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_digest_blake2: %s\n", msg);
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

/* Digest via init/update/final with updates cycling through `chunks`
 * (each truncated to what remains).  Must equal the one-shot digest. */
static size_t digest_chunked(const EVP_MD *md, const unsigned char *in, size_t inlen,
                             const size_t *chunks, size_t nchunks,
                             unsigned char *out, size_t outcap)
{
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    unsigned int outlen = 0;
    size_t off = 0, ci = 0;
    CHECK(ctx != NULL, "EVP_MD_CTX_new failed");
    CHECK(EVP_DigestInit_ex(ctx, md, NULL) == 1, "EVP_DigestInit_ex failed");
    while (off < inlen) {
        size_t n = chunks[ci++ % nchunks];
        if (n > inlen - off)
            n = inlen - off;
        CHECK(EVP_DigestUpdate(ctx, in + off, n) == 1,
              "EVP_DigestUpdate failed in the chunked test");
        off += n;
    }
    CHECK((size_t)EVP_MD_get_size(md) <= outcap,
          "digest output buffer smaller than EVP_MD_get_size");
    CHECK(EVP_DigestFinal_ex(ctx, out, &outlen) == 1, "EVP_DigestFinal_ex failed");
    EVP_MD_CTX_free(ctx);
    return outlen;
}

static const char *const MSG_ABC = "abc";

static const struct blake2_kat {
    const char *mdname;
    int mdsize;
    const char *emptyhex;
    const char *abchex;
} blake2_kats[] = {
    { "BLAKE2B-512", 64,
      "786a02f742015903c6c6fd852552d272912f4740e15847618a86e217f71f5419d25e1031afee585313896444934eb04b903a685b1448b755d56f701afe9be2ce",
      "ba80a53f981c4d0d6a2797b69f12f6e94c212f14685ac4b74b12bb6fdbffa2d17d87c5392aab792dc252d5de4533cc9518d38aa8dbf1925ab92386edd4009923" },
    { "BLAKE2S-256", 32,
      "69217a3079908094e11121d042354a7c1f55b6482ca1a51e1b250dfd1ed0eef9",
      "508c5e8c327c14e2e1a72ba34eeb452f37458b209ed63a294d999b4c86675982" },
};

/* (1) KATs through EVP_MD_fetch (one-shot), plus EVP_MD_get_size checks. */
static void test_kats(void)
{
    size_t k;
    for (k = 0; k < sizeof(blake2_kats) / sizeof(blake2_kats[0]); k++) {
        const struct blake2_kat *kat = &blake2_kats[k];
        EVP_MD *md = EVP_MD_fetch(NULL, kat->mdname, NULL);
        unsigned char *in = ALLOCATE_INPUT(3);
        unsigned char *want = ALLOCATE_OTHER(64);
        unsigned char *got = ALLOCATE_OUTPUT(64);
        size_t wantlen, gotlen;
        char msg[128];

        CHECK(md != NULL, "EVP_MD_fetch failed for a core BLAKE2 digest");
        snprintf(msg, sizeof(msg), "%s: EVP_MD_get_size is wrong", kat->mdname);
        CHECK(EVP_MD_get_size(md) == kat->mdsize, msg);

        wantlen = from_hex(kat->emptyhex, want, 64);
        gotlen = digest_one_shot(md, in, 0, got, 64);
        snprintf(msg, sizeof(msg), "%s(\"\") one-shot digest does not match the KAT",
                 kat->mdname);
        CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0, msg);

        memcpy(in, MSG_ABC, 3);
        wantlen = from_hex(kat->abchex, want, 64);
        gotlen = digest_one_shot(md, in, 3, got, 64);
        snprintf(msg, sizeof(msg), "%s(\"abc\") one-shot digest does not match the KAT",
                 kat->mdname);
        CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0, msg);
        EVP_MD_free(md);
    }
}

/* (2) The "abc" KATs through EVP_get_digestbyname("blake2b512") and

 * ("blake2s256"). */

static void test_byname_routes(void)
{
    const EVP_MD *byname_md;
    unsigned char *in = ALLOCATE_INPUT(3);
    unsigned char *want = ALLOCATE_OTHER(64);
    unsigned char *got = ALLOCATE_OUTPUT(64);
    size_t wantlen, gotlen;
    char msg[128];

    memcpy(in, MSG_ABC, 3);

    byname_md = EVP_get_digestbyname("blake2b512");
    CHECK(byname_md != NULL, "EVP_get_digestbyname(\"blake2b512\") returned NULL");
    wantlen = from_hex(blake2_kats[0].abchex, want, 64);
    memset(got, 0, 64);
    {
        static const size_t chunks[] = { 1, 2 };
        gotlen = digest_chunked(byname_md, in, 3, chunks, 2, got, 64);
    }
    snprintf(msg, sizeof(msg),
             "EVP_get_digestbyname(\"blake2b512\") digest does not match the KAT");
    CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0, msg);

    byname_md = EVP_get_digestbyname("blake2s256");
    CHECK(byname_md != NULL, "EVP_get_digestbyname(\"blake2s256\") returned NULL");
    wantlen = from_hex(blake2_kats[1].abchex, want, 32);
    memset(got, 0, 64);
    {
        static const size_t chunks[] = { 2, 1 };
        gotlen = digest_chunked(byname_md, in, 3, chunks, 2, got, 64);
    }
    snprintf(msg, sizeof(msg),
             "EVP_get_digestbyname(\"blake2s256\") digest does not match the KAT");
    CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0, msg);
}

/* (3) Chunked-vs-one-shot equality with splits that hit the block size

 * (128 for BLAKE2b, 64 for BLAKE2s) exactly, plus mixed odd chunks.  A

 * 512-byte deterministic input is fed as: exact block multiples, block

 * +/- 1, and byte-at-a-time.  The implementations keep one complete block

 * buffered for the final call, so the last full block must survive the

 * update sequence unchanged. */

static void test_block_boundary_chunks(void)
{
    static const char *const names[2] = { "BLAKE2B-512", "BLAKE2S-256" };
    static const size_t blocks[2] = { 128, 64 };
    unsigned char *in = ALLOCATE_INPUT(512);
    unsigned char *one = ALLOCATE_OUTPUT(64);
    unsigned char *many = ALLOCATE_OUTPUT(64);
    size_t v, i, nchunks;
    size_t *chunks = ALLOCATE_OTHER(8 * sizeof(size_t));
    char msg[128];

    for (i = 0; i < 512; i++)
        in[i] = (unsigned char)(i * 13 + 9);

    for (v = 0; v < 2; v++) {
        EVP_MD *md = EVP_MD_fetch(NULL, names[v], NULL);
        size_t b = blocks[v];
        size_t one_len;

        CHECK(md != NULL, "EVP_MD_fetch failed for a core BLAKE2 digest");
        one_len = digest_one_shot(md, in, 512, one, 64);
        CHECK(one_len == (size_t)EVP_MD_get_size(md), "one-shot length mismatch");

        /* exact block multiples: 512 / b updates of b bytes each */
        chunks[0] = b;
        nchunks = 1;
        {
            size_t many_len = digest_chunked(md, in, 512, chunks, nchunks, many, 64);
            snprintf(msg, sizeof(msg),
                     "%s: all-%zu-byte-chunk digest differs from one-shot",
                     names[v], b);
            CHECK(many_len == one_len && memcmp(one, many, one_len) == 0, msg);
        }

        /* block-1 then block then block+1 cycling */
        chunks[0] = b - 1;
        chunks[1] = b;
        chunks[2] = b + 1;
        nchunks = 3;
        {
            size_t many_len = digest_chunked(md, in, 512, chunks, nchunks, many, 64);
            snprintf(msg, sizeof(msg),
                     "%s: block-boundary chunk digest differs from one-shot",
                     names[v]);
            CHECK(many_len == one_len && memcmp(one, many, one_len) == 0, msg);
        }

        /* mixed odd chunks */
        chunks[0] = 1;
        chunks[1] = 7;
        chunks[2] = 33;
        chunks[3] = b + 5;
        nchunks = 4;
        {
            size_t many_len = digest_chunked(md, in, 512, chunks, nchunks, many, 64);
            snprintf(msg, sizeof(msg),
                     "%s: mixed odd chunk digest differs from one-shot", names[v]);
            CHECK(many_len == one_len && memcmp(one, many, one_len) == 0, msg);
        }

        /* byte-at-a-time */
        chunks[0] = 1;
        nchunks = 1;
        {
            size_t many_len = digest_chunked(md, in, 512, chunks, nchunks, many, 64);
            CHECK(many_len == one_len && memcmp(one, many, one_len) == 0,
                  "byte-at-a-time BLAKE2 digest differs from one-shot");
        }
        EVP_MD_free(md);
    }
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_kats();
    test_byname_routes();
    test_block_boundary_chunks();

    printf("test_digest_blake2: all %u checks passed\n", g_checks);
    return 0;
}

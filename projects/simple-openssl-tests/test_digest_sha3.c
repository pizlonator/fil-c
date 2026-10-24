#include "poisoning.h"

#include <openssl/evp.h>

/*
 * test_digest_sha3 -- SHA3-224/256/384/512 and KECCAK-224/256/384/512
 * known-answer and behavior tests.
 *
 * Known-answer vectors, all verified with pizfix/bin/openssl, e.g.
 *
 *   printf 'abc' | pizfix/bin/openssl dgst -sha3-256
 *   printf 'abc' | pizfix/bin/openssl dgst -keccak-224
 *
 *   "abc" and the empty message for all eight variants.  SHA3-* and
 *   KECCAK-* differ only in the domain-separation padding byte (0x06 vs
 *   0x01), so every vector is distinct.
 *
 * Coverage: one-shot KATs; EVP_MD_get_size checks (28/32/48/64); the
 * byname route for sha3-256; chunked-vs-one-shot equality around each
 * rate boundary (the rates are 144/136/104/72 bytes for 224/256/384/512);
 * and the same treatment for the KECCAK-* variants.
 *
 * Version gating: the KECCAK-* digests are absent from OpenSSL 3.0.x's
 * default provider (EVP_MD_fetch fails there) and present in newer
 * releases, so a failed fetch takes the SKIP path at the end of main().
 * SHA3-* itself is core OpenSSL 3 functionality and never SKIPs.
 */

/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */
static void *ALLOCATE_INPUT(size_t n)  { return poison_alloc(POISON_INPUT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }
static void *ALLOCATE_OTHER(size_t n)  { return poison_alloc(POISON_OTHER, n); }

static unsigned g_checks = 0;

/* Set when a genuinely version-gated algorithm is unavailable in this
 * OpenSSL build; main() then prints the SKIP line and exits 0. */
static const char *g_skipmsg = NULL;

static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_digest_sha3: %s\n", msg);
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

/* Digest via init/update/final with updates of {a, b, rest} bytes (each

 * truncated to what remains).  Used for the rate-boundary splits, where
 * the individual update lengths matter more than the total. */
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

/* Digest with one-byte updates throughout (worst-case chunking). */
static size_t digest_chunked_bytes(const EVP_MD *md, const unsigned char *in,
                                   size_t inlen, unsigned char *out, size_t outcap)
{
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    unsigned int outlen = 0;
    size_t i;
    CHECK(ctx != NULL, "EVP_MD_CTX_new failed");
    CHECK(EVP_DigestInit_ex(ctx, md, NULL) == 1, "EVP_DigestInit_ex failed");
    for (i = 0; i < inlen; i++) {
        CHECK(EVP_DigestUpdate(ctx, in + i, 1) == 1,
              "EVP_DigestUpdate failed (1-byte chunk)");
    }
    CHECK((size_t)EVP_MD_get_size(md) <= outcap,
          "digest output buffer smaller than EVP_MD_get_size");
    CHECK(EVP_DigestFinal_ex(ctx, out, &outlen) == 1, "EVP_DigestFinal_ex failed");
    EVP_MD_CTX_free(ctx);
    return outlen;
}

static const char *const MSG_ABC = "abc";

static const struct sha3_kat {
    const char *mdname;
    int mdsize;
    const char *abchex;
    const char *emptyhex;
} sha3_kats[] = {
    { "SHA3-224", 28,
      "e642824c3f8cf24ad09234ee7d3c766fc9a3a5168d0c94ad73b46fdf",
      "6b4e03423667dbb73b6e15454f0eb1abd4597f9a1b078e3f5b5a6bc7" },
    { "SHA3-256", 32,
      "3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532",
      "a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a" },
    { "SHA3-384", 48,
      "ec01498288516fc926459f58e2c6ad8df9b473cb0fc08c2596da7cf0e49be4b298d88cea927ac7f539f1edf228376d25",
      "0c63a75b845e4f7d01107d852e4c2485c51a50aaaa94fc61995e71bbee983a2ac3713831264adb47fb6bd1e058d5f004" },
    { "SHA3-512", 64,
      "b751850b1a57168a5693cd924b6b096e08f621827444f70d884f5d0240d2712e10e116e9192af3c91a7ec57647e3934057340b4cf408d5a56592f8274eec53f0",
      "a69f73cca23a9ac5c8b567dc185a756e97c982164fe25859e0d1dcc1475c80a615b2123af1f5f94c11e3e9402c3ac558f500199d95b6d3e301758586281dcd26" },
    { "KECCAK-224", 28,
      "c30411768506ebe1c2871b1ee2e87d38df342317300a9b97a95ec6a8",
      "f71837502ba8e10837bdd8d365adb85591895602fc552b48b7390abd" },
    { "KECCAK-256", 32,
      "4e03657aea45a94fc7d47ba826c8d667c0d1e6e33a64a036ec44f58fa12d6c45",
      "c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470" },
    { "KECCAK-384", 48,
      "f7df1165f033337be098e7d288ad6a2f74409d7a60b49c36642218de161b1f99f8c681e4afaf31a34db29fb763e3c28e",
      "2c23146a63a29acf99e73b88f8c24eaa7dc60aa771780ccc006afbfa8fe2479b2dd2b21362337441ac12b515911957ff" },
    { "KECCAK-512", 64,
      "18587dc2ea106b9a1563e32b3312421ca164c7f1f07bc922a9c83d77cea3a1e5d0c69910739025372dc14ac9642629379540c17e2a65b19d77aa511a9d00bb96",
      "0eab42de4c3ceb9235fc91acffe746b29c29a8c366b7c60e4e67c466f36a4304c00fa9caf9d87976ba469bcbe06713b435f091ef2769fb160cdab33d3670680e" },
};

/* (1) One-shot EVP_Digest with EVP_MD_fetch == KAT for every variant and

 * both messages; also checks EVP_MD_get_size against the KAT length. */

static void test_kats(void)
{
    size_t k;
    for (k = 0; k < sizeof(sha3_kats) / sizeof(sha3_kats[0]); k++) {
        const struct sha3_kat *kat = &sha3_kats[k];
        EVP_MD *md = EVP_MD_fetch(NULL, kat->mdname, NULL);
        unsigned char *in = ALLOCATE_INPUT(3);
        unsigned char *want = ALLOCATE_OTHER(64);
        unsigned char *got = ALLOCATE_OUTPUT(64);
        size_t wantlen, gotlen;
        char msg[128];

        if (md == NULL) {
            /* Only the KECCAK-* variants may be missing (OpenSSL 3.0.x).

             * SHA3-* is core OpenSSL 3 functionality: never SKIP, only
             * FAIL. */
            if (k < 4)
                test_failed("EVP_MD_fetch failed for a core SHA3 digest");
            g_skipmsg = kat->mdname;
            return;
        }
        snprintf(msg, sizeof(msg), "%s: EVP_MD_get_size is wrong", kat->mdname);
        CHECK(EVP_MD_get_size(md) == kat->mdsize, msg);

        wantlen = from_hex(kat->abchex, want, 64);
        memcpy(in, MSG_ABC, 3);
        gotlen = digest_one_shot(md, in, 3, got, 64);
        snprintf(msg, sizeof(msg), "%s(\"abc\") one-shot digest does not match the KAT",
                 kat->mdname);
        CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0, msg);

        wantlen = from_hex(kat->emptyhex, want, 64);
        gotlen = digest_one_shot(md, in, 0, got, 64);
        snprintf(msg, sizeof(msg), "%s(\"\") one-shot digest does not match the KAT",
                 kat->mdname);
        CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0, msg);
        EVP_MD_free(md);
    }
}

/* (2) The SHA3-256 KAT via the EVP_get_digestbyname("sha3-256") route

 * (chunked with one-byte updates for good measure). */

static void test_byname_route(void)
{
    const EVP_MD *byname_md;
    unsigned char *in = ALLOCATE_INPUT(3);
    unsigned char *want = ALLOCATE_OTHER(32);
    unsigned char *got = ALLOCATE_OUTPUT(32);
    size_t wantlen, gotlen;

    byname_md = EVP_get_digestbyname("sha3-256");
    CHECK(byname_md != NULL, "EVP_get_digestbyname(\"sha3-256\") returned NULL");
    memcpy(in, MSG_ABC, 3);
    wantlen = from_hex(sha3_kats[1].abchex, want, 32);
    gotlen = digest_chunked_bytes(byname_md, in, 3, got, 32);
    CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0,
          "EVP_get_digestbyname(\"sha3-256\") digest does not match the KAT");
}

/* (3) Rate-boundary chunking: for every available variant, updates of

 * {rate-1, rate, rate+1} bytes (plus a byte-at-a-time pass) must reproduce

 * the one-shot digest of a 2*rate+2-byte deterministic input.  The KECCAK

 * rates equal the SHA3 rates at each output size. */

static void test_rate_boundaries(void)
{
    static const struct {
        const char *mdname;
        size_t rate;
    } variants[] = {
        { "SHA3-224", 144 }, { "SHA3-256", 136 }, { "SHA3-384", 104 },
        { "SHA3-512", 72 },  { "KECCAK-224", 144 }, { "KECCAK-256", 136 },
        { "KECCAK-384", 104 }, { "KECCAK-512", 72 },
    };
    /* The largest boundary input is 2*144+2 = 290 bytes (SHA3-224). */
    unsigned char *in = ALLOCATE_INPUT(290);
    unsigned char *one = ALLOCATE_OUTPUT(64);
    unsigned char *many = ALLOCATE_OUTPUT(64);
    size_t v, i;
    char msg[128];

    for (i = 0; i < 290; i++)
        in[i] = (unsigned char)(i * 11 + 5);

    for (v = 0; v < sizeof(variants) / sizeof(variants[0]); v++) {
        EVP_MD *md = EVP_MD_fetch(NULL, variants[v].mdname, NULL);
        size_t r = variants[v].rate;
        if (md == NULL) {
            /* Already reported by test_kats(); keep the loop going. */
            continue;
        }
        {
            size_t one_len = digest_one_shot(md, in, 2 * r + 2, one, 64);
            size_t many_len;

            many_len = digest_chunked_ab(md, in, 2 * r + 2, r - 1, r, many, 64);
            snprintf(msg, sizeof(msg),
                     "%s chunked digest (rate-1/rate split) differs from one-shot",
                     variants[v].mdname);
            CHECK(one_len == many_len && memcmp(one, many, one_len) == 0, msg);

            many_len = digest_chunked_ab(md, in, 2 * r + 2, r, r + 1, many, 64);
            snprintf(msg, sizeof(msg),
                     "%s chunked digest (rate/rate+1 split) differs from one-shot",
                     variants[v].mdname);
            CHECK(one_len == many_len && memcmp(one, many, one_len) == 0, msg);

            many_len = digest_chunked_bytes(md, in, 2 * r + 2, many, 64);
            snprintf(msg, sizeof(msg),
                     "%s byte-at-a-time digest differs from one-shot",
                     variants[v].mdname);
            CHECK(one_len == many_len && memcmp(one, many, one_len) == 0, msg);
        }
        EVP_MD_free(md);
    }
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_kats();
    test_byname_route();
    test_rate_boundaries();

    if (g_skipmsg != NULL) {
        printf("SKIP test_digest_sha3: %s is unavailable\n", g_skipmsg);
        return 0;
    }
    printf("test_digest_sha3: all %u checks passed\n", g_checks);
    return 0;
}

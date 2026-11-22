/*
 * test_digest_sha3_common.c -- shared implementation of the SHA-3 and
 * KECCAK digest family tests (API in test_digest_sha3_common.h; one binary
 * per digest, one struct sha3_kat row per binary: SHA3-224/256/384/512 and
 * KECCAK-224/256/384/512).
 *
 * Known-answer vectors, all verified with pizfix/bin/openssl, e.g.
 *
 *   printf 'abc' | pizfix/bin/openssl dgst -sha3-256
 *   printf 'abc' | pizfix/bin/openssl dgst -keccak-224
 *
 *   "abc" and the empty message for all eight variants.  SHA3-* and
 *   KECCAK-* differ only in the domain-separation padding byte (0x06 vs
 *   0x01), so every vector is distinct.  Each per-digest binary carries its
 *   own digest's row.
 *
 * Coverage (per binary, for the binary's own digest): one-shot KATs for
 * "abc" and the empty message; the EVP_MD_get_size check (28/32/48/64);
 * chunked-vs-one-shot equality around the digest's rate boundary (the
 * rates are 144/136/104/72 bytes for 224/256/384/512, and the KECCAK rates
 * equal the SHA3 rates at each output size) -- {rate-1, rate, rate+1}
 * splits plus a byte-at-a-time pass.  The byname route ("sha3-256") is a
 * SHA3-256-only section and belongs to test_digest_sha3_256.c.
 *
 * Version gates: the KECCAK-* digests are absent from OpenSSL 3.0.x's
 * default provider (EVP_MD_fetch fails there) and present in newer
 * releases, so each KECCAK binary gates its whole program on the fetch
 * (sha3_probe_or_skip right after test_parse_args()).  SHA3-* itself is
 * core OpenSSL 3 functionality and never skips; a failed fetch is a FAIL.
 */

#include "test_digest_sha3_common.h"

#include <openssl/evp.h>

/* The classic one-block FIPS message; the other KAT message is the empty
 * message. */
static const char *const MSG_ABC = "abc";

/* Whole-program version gate: SKIP with the documented reason when the
 * build lacks the digest (the KECCAK-* binaries on OpenSSL 3.0.x), release
 * the probe handle otherwise. */
void sha3_probe_or_skip(const char *mdname)
{
    char msg[128];
    EVP_MD *probe = EVP_MD_fetch(NULL, mdname, NULL);

    if (probe == NULL) {
        snprintf(msg, sizeof(msg), "%s is unavailable", mdname);
        test_skip(msg);
    }
    EVP_MD_free(probe);
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

/* (1) One-shot EVP_Digest with EVP_MD_fetch == KAT for the row's two
 * messages ("abc" and the empty message); also checks EVP_MD_get_size
 * against the row's mdsize. */
void sha3_test_kat(const struct sha3_kat *kat)
{
    EVP_MD *md;
    unsigned char *in = ALLOCATE_INPUT(3);
    unsigned char *want = ALLOCATE_OTHER(64);
    unsigned char *got = ALLOCATE_OUTPUT(64);
    size_t wantlen, gotlen;
    char msg[128];

    snprintf(msg, sizeof(msg), "EVP_MD_fetch(\"%s\") failed", kat->mdname);
    md = EVP_MD_fetch(NULL, kat->mdname, NULL);
    CHECK(md != NULL, msg);

    snprintf(msg, sizeof(msg), "%s: EVP_MD_get_size is wrong", kat->mdname);
    CHECK(EVP_MD_get_size(md) == kat->mdsize, msg);

    wantlen = from_hex(kat->abchex, want, 64);
    memcpy(in, MSG_ABC, 3);
    gotlen = digest_one_shot(md, in, 3, got, 64);
    snprintf(msg, sizeof(msg), "%s(\"abc\") one-shot digest does not match the KAT",
             kat->mdname);
    unpoison_all();
    CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0, msg);
    repoison_all();

    wantlen = from_hex(kat->emptyhex, want, 64);
    gotlen = digest_one_shot(md, in, 0, got, 64);
    snprintf(msg, sizeof(msg), "%s(\"\") one-shot digest does not match the KAT",
             kat->mdname);
    unpoison_all();
    CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0, msg);
    repoison_all();
    EVP_MD_free(md);

    free_poison_allocations();
}

/* (2) The KAT (abchex) via the EVP_get_digestbyname(byname) route (chunked
 * with one-byte updates for good measure). */
void sha3_test_byname_route(const char *byname, const char *abchex)
{
    const EVP_MD *byname_md;
    unsigned char *in = ALLOCATE_INPUT(3);
    unsigned char *want = ALLOCATE_OTHER(32);
    unsigned char *got = ALLOCATE_OUTPUT(32);
    size_t wantlen, gotlen;
    char msg[128];

    byname_md = EVP_get_digestbyname(byname);
    snprintf(msg, sizeof(msg), "EVP_get_digestbyname(\"%s\") returned NULL",
             byname);
    CHECK(byname_md != NULL, msg);
    memcpy(in, MSG_ABC, 3);
    wantlen = from_hex(abchex, want, 32);
    gotlen = digest_chunked_bytes(byname_md, in, 3, got, 32);
    snprintf(msg, sizeof(msg),
             "EVP_get_digestbyname(\"%s\") digest does not match the KAT", byname);
    unpoison_all();
    CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0, msg);
    repoison_all();

    free_poison_allocations();
}

/* (3) Rate-boundary chunking: updates of {rate-1, rate, rate+1} bytes (plus
 * a byte-at-a-time pass) must reproduce the one-shot digest of a
 * 2*rate+2-byte deterministic input (the byte pattern i*11+5).  The
 * largest boundary input is 2*144+2 = 290 bytes (rate 144). */
void sha3_test_rate_boundaries(const char *mdname, size_t rate)
{
    EVP_MD *md;
    unsigned char *in = ALLOCATE_INPUT(290);
    unsigned char *one = ALLOCATE_OUTPUT(64);
    unsigned char *many = ALLOCATE_OUTPUT(64);
    size_t i, one_len, many_len;
    char msg[128];

    for (i = 0; i < 290; i++)
        in[i] = (unsigned char)(i * 11 + 5);

    snprintf(msg, sizeof(msg), "EVP_MD_fetch(\"%s\") failed", mdname);
    md = EVP_MD_fetch(NULL, mdname, NULL);
    CHECK(md != NULL, msg);

    one_len = digest_one_shot(md, in, 2 * rate + 2, one, 64);

    many_len = digest_chunked_ab(md, in, 2 * rate + 2, rate - 1, rate, many, 64);
    snprintf(msg, sizeof(msg),
             "%s chunked digest (rate-1/rate split) differs from one-shot",
             mdname);
    unpoison_all();
    CHECK(one_len == many_len && memcmp(one, many, one_len) == 0, msg);
    repoison_all();

    many_len = digest_chunked_ab(md, in, 2 * rate + 2, rate, rate + 1, many, 64);
    snprintf(msg, sizeof(msg),
             "%s chunked digest (rate/rate+1 split) differs from one-shot",
             mdname);
    unpoison_all();
    CHECK(one_len == many_len && memcmp(one, many, one_len) == 0, msg);
    repoison_all();

    many_len = digest_chunked_bytes(md, in, 2 * rate + 2, many, 64);
    snprintf(msg, sizeof(msg),
             "%s byte-at-a-time digest differs from one-shot", mdname);
    unpoison_all();
    CHECK(one_len == many_len && memcmp(one, many, one_len) == 0, msg);
    repoison_all();
    EVP_MD_free(md);

    free_poison_allocations();
}

/*

 * test_digest_ripemd_sm3_common.c -- shared implementation of the

 * RIPEMD-160 and SM3 digest tests (API in test_digest_ripemd_sm3_common.h;

 * one binary per digest, one KAT row set per binary:

 * test_digest_ripemd160.c and test_digest_sm3.c).

 *

 * Known-answer vectors, all verified with pizfix/bin/openssl, e.g.

 *

 *   printf 'abc' | pizfix/bin/openssl dgst -ripemd160

 *   printf 'abc' | pizfix/bin/openssl dgst -sm3

 *

 *   RIPEMD-160("") = 9c1185a5...   RIPEMD-160("abc") = 8eb208f7...

 *   SM3("abc")     = 66c7f0f4...   (the GB/T 32905-2016 sample vector)

 *

 * Each per-digest binary carries its own digest's rows of that table and

 * runs the byname route and the chunked-vs-one-shot equality for its own

 * digest (the monolith ran the same sections for both digests, so the

 * split preserves coverage exactly, once per binary).

 *

 * Both digests are in the default provider in the OpenSSL 3.x builds

 * exercised here (no legacy load needed).  The underlying compression

 * functions are 64-byte Merkle-Damgard, so the chunked equality includes

 * exact 64-byte block multiples.

 */

#include "test_digest_ripemd_sm3_common.h"

#include <openssl/evp.h>

/* The shared "abc" message (the KAT message of both digests).  The

 * constant is an array (not a pointer) so that it is an address constant

 * usable where the per-digest binaries need it. */

const char ripemd_sm3_msg_abc[] = "abc";

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

/* (1) KATs through EVP_MD_fetch, plus EVP_MD_get_size checks. */

void ripemd_sm3_test_kats(const struct ripemd_sm3_kat *kats, size_t nkats)

{

    size_t k;

    for (k = 0; k < nkats; k++) {

        const struct ripemd_sm3_kat *kat = &kats[k];

        EVP_MD *md;

        unsigned char *in = ALLOCATE_INPUT(3);

        unsigned char *want = ALLOCATE_OTHER(32);

        unsigned char *got = ALLOCATE_OUTPUT(32);

        size_t wantlen, gotlen;

        char msg[128];

        snprintf(msg, sizeof(msg),

                 "EVP_MD_fetch(\"%s\") failed for a core digest", kat->mdname);

        md = EVP_MD_fetch(NULL, kat->mdname, NULL);

        CHECK(md != NULL, msg);

        snprintf(msg, sizeof(msg), "%s: EVP_MD_get_size is wrong", kat->mdname);

        CHECK(EVP_MD_get_size(md) == kat->mdsize, msg);

        memcpy(in, ripemd_sm3_msg_abc, strlen(ripemd_sm3_msg_abc));

        wantlen = from_hex(kat->abchex, want, 32);

        gotlen = digest_one_shot(md, in, 3, got, 32);

        snprintf(msg, sizeof(msg),

                 "%s(\"abc\") one-shot digest does not match the KAT",

                 kat->mdname);

        unpoison_all();

        CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0, msg);

        repoison_all();

        if (kat->emptyhex != NULL) {

            wantlen = from_hex(kat->emptyhex, want, 32);

            gotlen = digest_one_shot(md, in, 0, got, 32);

            snprintf(msg, sizeof(msg),

                     "%s(\"\") one-shot digest does not match the KAT",

                     kat->mdname);

            unpoison_all();

            CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0, msg);

            repoison_all();

        }

        EVP_MD_free(md);

        free_poison_allocations();

    }

}

/* (2) The "abc" KAT (abchex) via the EVP_get_digestbyname(byname) route,

 * chunked. */

void ripemd_sm3_test_byname_route(const char *byname, const char *abchex)

{

    const EVP_MD *byname_md;

    unsigned char *in = ALLOCATE_INPUT(3);

    unsigned char *want = ALLOCATE_OTHER(32);

    unsigned char *got = ALLOCATE_OUTPUT(32);

    size_t wantlen, gotlen;

    char msg[128];

    memcpy(in, ripemd_sm3_msg_abc, strlen(ripemd_sm3_msg_abc));

    snprintf(msg, sizeof(msg),

             "EVP_get_digestbyname(\"%s\") returned NULL", byname);

    byname_md = EVP_get_digestbyname(byname);

    CHECK(byname_md != NULL, msg);

    wantlen = from_hex(abchex, want, 32);

    memset(got, 0, 32);

    gotlen = digest_chunked(byname_md, in, 3, got, 32);

    snprintf(msg, sizeof(msg),

             "EVP_get_digestbyname(\"%s\") digest does not match the KAT",

             byname);

    unpoison_all();

    CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0, msg);

    repoison_all();

    free_poison_allocations();

}

/* (3) Chunked-vs-one-shot equality around the 64-byte block boundary and

 * at exact block multiples (block-stashed-at-final behavior must not

 * change the result). */

void ripemd_sm3_test_chunked_equality(const char *mdname)

{

    static const size_t sizes[] = { 0, 1, 55, 56, 63, 64, 65, 128, 129, 1000 };

    EVP_MD *md;

    unsigned char *in = ALLOCATE_INPUT(1000);

    unsigned char *one = ALLOCATE_OUTPUT(32);

    unsigned char *many = ALLOCATE_OUTPUT(32);

    size_t mdsize, s, i;

    char msg[128];

    snprintf(msg, sizeof(msg),

             "EVP_MD_fetch(\"%s\") failed for a core digest", mdname);

    md = EVP_MD_fetch(NULL, mdname, NULL);

    CHECK(md != NULL, msg);

    mdsize = (size_t)EVP_MD_get_size(md);

    for (i = 0; i < 1000; i++)

        in[i] = (unsigned char)(i * 7 + 3);

    for (s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {

        size_t n = sizes[s];

        size_t one_len = digest_one_shot(md, in, n, one, 32);

        size_t many_len = digest_chunked(md, in, n, many, 32);

        snprintf(msg, sizeof(msg),

                 "%s chunked digest differs from one-shot at input length %zu",

                 mdname, n);

        unpoison_all();

        CHECK(one_len == many_len && memcmp(one, many, one_len) == 0, msg);

        repoison_all();

    }

    /* 256 bytes fed as exact 64-byte updates. */

    {

        size_t one_len = digest_one_shot(md, in, 256, one, 32);

        size_t many_len = digest_chunked_block(md, in, 256, 64, many, 32);

        CHECK(one_len == mdsize && many_len == mdsize,

              "block-chunked digest has the wrong length");

        unpoison_all();

        CHECK(memcmp(one, many, mdsize) == 0,

              "all-64-byte-chunk digest differs from one-shot");

        repoison_all();

    }

    EVP_MD_free(md);

    free_poison_allocations();

}


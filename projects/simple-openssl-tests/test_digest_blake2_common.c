/*

 * test_digest_blake2_common.c -- shared implementation of the BLAKE2b-512

 * and BLAKE2s-256 digest tests (API in test_digest_blake2_common.h; one

 * binary per digest, one KAT row set per binary: test_digest_blake2b.c and

 * test_digest_blake2s.c).

 *

 * Known-answer vectors (the official RFC 7693-style vectors for the

 * unkeyed hashes), all verified with pizfix/bin/openssl, e.g.

 *

 *   printf 'abc' | pizfix/bin/openssl dgst -blake2b512

 *

 *   BLAKE2b-512("") = 786a02f7...   BLAKE2s-256("") = 69217a30...

 *   BLAKE2b-512("abc") = ba80a53f... BLAKE2s-256("abc") = 508c5e8c...

 *

 * Each per-digest binary carries its own digest's rows of that table and

 * runs the byname route and the block-boundary chunking for its own digest

 * (the monolith ran the same sections for both digests, so the split

 * preserves coverage exactly, once per binary).

 *

 * Both digests are in the default provider in the OpenSSL 3.x builds

 * exercised here (no legacy load needed).  The implementations stash a

 * complete final block away for the final call, so the chunked equality

 * includes updates that are exact multiples of the block size (128 for

 * BLAKE2b, 64 for BLAKE2s) and mixed odd chunks.

 */

#include "test_digest_blake2_common.h"

#include <openssl/evp.h>

/* The shared "abc" message (the KAT message of both digests).  The

 * constant is an array (not a pointer) so that it is an address constant

 * usable where the per-digest binaries need it. */

const char blake2_msg_abc[] = "abc";

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

/* (1) KATs through EVP_MD_fetch (one-shot), plus EVP_MD_get_size checks. */

void blake2_test_kats(const struct blake2_kat *kats, size_t nkats)

{

    size_t k;

    for (k = 0; k < nkats; k++) {

        const struct blake2_kat *kat = &kats[k];

        EVP_MD *md;

        unsigned char *in = ALLOCATE_INPUT(3);

        unsigned char *want = ALLOCATE_OTHER(64);

        unsigned char *got = ALLOCATE_OUTPUT(64);

        size_t wantlen, gotlen;

        char msg[128];

        snprintf(msg, sizeof(msg),

                 "EVP_MD_fetch(\"%s\") failed for a core BLAKE2 digest",

                 kat->mdname);

        md = EVP_MD_fetch(NULL, kat->mdname, NULL);

        CHECK(md != NULL, msg);

        snprintf(msg, sizeof(msg), "%s: EVP_MD_get_size is wrong", kat->mdname);

        CHECK(EVP_MD_get_size(md) == kat->mdsize, msg);

        wantlen = from_hex(kat->emptyhex, want, 64);

        gotlen = digest_one_shot(md, in, 0, got, 64);

        snprintf(msg, sizeof(msg),

                 "%s(\"\") one-shot digest does not match the KAT", kat->mdname);

        unpoison_all();

        CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0, msg);

        repoison_all();

        memcpy(in, blake2_msg_abc, strlen(blake2_msg_abc));

        wantlen = from_hex(kat->abchex, want, 64);

        gotlen = digest_one_shot(md, in, 3, got, 64);

        snprintf(msg, sizeof(msg),

                 "%s(\"abc\") one-shot digest does not match the KAT",

                 kat->mdname);

        unpoison_all();

        CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0, msg);

        repoison_all();

        EVP_MD_free(md);

        free_poison_allocations();

    }

}

/* (2) The "abc" KAT (abchex) via the EVP_get_digestbyname(byname) route,

 * chunked with the `nchunks` `chunks` update lengths. */

void blake2_test_byname_route(const char *byname, const char *abchex,

                              const size_t *chunks, size_t nchunks)

{

    const EVP_MD *byname_md;

    unsigned char *in = ALLOCATE_INPUT(3);

    unsigned char *want = ALLOCATE_OTHER(64);

    unsigned char *got = ALLOCATE_OUTPUT(64);

    size_t wantlen, gotlen;

    char msg[128];

    memcpy(in, blake2_msg_abc, strlen(blake2_msg_abc));

    snprintf(msg, sizeof(msg),

             "EVP_get_digestbyname(\"%s\") returned NULL", byname);

    byname_md = EVP_get_digestbyname(byname);

    CHECK(byname_md != NULL, msg);

    wantlen = from_hex(abchex, want, 64);

    memset(got, 0, 64);

    gotlen = digest_chunked(byname_md, in, 3, chunks, nchunks, got, 64);

    snprintf(msg, sizeof(msg),

             "EVP_get_digestbyname(\"%s\") digest does not match the KAT",

             byname);

    unpoison_all();

    CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0, msg);

    repoison_all();

    free_poison_allocations();

}

/* (3) Block-boundary chunking for mdname with block size `block`: a

 * 512-byte deterministic input is fed as exact block multiples, block

 * +/- 1, mixed odd chunks, and byte-at-a-time.  The implementations keep

 * one complete block buffered for the final call, so the last full block

 * must survive the update sequence unchanged. */

void blake2_test_block_boundary_chunks(const char *mdname, size_t block)

{

    EVP_MD *md;

    unsigned char *in = ALLOCATE_INPUT(512);

    unsigned char *one = ALLOCATE_OUTPUT(64);

    unsigned char *many = ALLOCATE_OUTPUT(64);

    size_t i, nchunks, one_len;

    size_t *chunks = ALLOCATE_OTHER(8 * sizeof(size_t));

    char msg[128];

    snprintf(msg, sizeof(msg),

             "EVP_MD_fetch(\"%s\") failed for a core BLAKE2 digest", mdname);

    md = EVP_MD_fetch(NULL, mdname, NULL);

    CHECK(md != NULL, msg);

    for (i = 0; i < 512; i++)

        in[i] = (unsigned char)(i * 13 + 9);

    one_len = digest_one_shot(md, in, 512, one, 64);

    CHECK(one_len == (size_t)EVP_MD_get_size(md), "one-shot length mismatch");

    /* exact block multiples: 512 / block updates of block bytes each */

    chunks[0] = block;

    nchunks = 1;

    {

        size_t many_len = digest_chunked(md, in, 512, chunks, nchunks, many, 64);

        snprintf(msg, sizeof(msg),

                 "%s: all-%zu-byte-chunk digest differs from one-shot",

                 mdname, block);

        unpoison_all();

        CHECK(many_len == one_len && memcmp(one, many, one_len) == 0, msg);

        repoison_all();

    }

    /* block-1 then block then block+1 cycling */

    chunks[0] = block - 1;

    chunks[1] = block;

    chunks[2] = block + 1;

    nchunks = 3;

    {

        size_t many_len = digest_chunked(md, in, 512, chunks, nchunks, many, 64);

        snprintf(msg, sizeof(msg),

                 "%s: block-boundary chunk digest differs from one-shot",

                 mdname);

        unpoison_all();

        CHECK(many_len == one_len && memcmp(one, many, one_len) == 0, msg);

        repoison_all();

    }

    /* mixed odd chunks */

    chunks[0] = 1;

    chunks[1] = 7;

    chunks[2] = 33;

    chunks[3] = block + 5;

    nchunks = 4;

    {

        size_t many_len = digest_chunked(md, in, 512, chunks, nchunks, many, 64);

        snprintf(msg, sizeof(msg),

                 "%s: mixed odd chunk digest differs from one-shot", mdname);

        unpoison_all();

        CHECK(many_len == one_len && memcmp(one, many, one_len) == 0, msg);

        repoison_all();

    }

    /* byte-at-a-time */

    chunks[0] = 1;

    nchunks = 1;

    {

        size_t many_len = digest_chunked(md, in, 512, chunks, nchunks, many, 64);

        snprintf(msg, sizeof(msg),

                 "%s: byte-at-a-time digest differs from one-shot", mdname);

        unpoison_all();

        CHECK(many_len == one_len && memcmp(one, many, one_len) == 0, msg);

        repoison_all();

    }

    EVP_MD_free(md);

    free_poison_allocations();

}


/*

 * test_digest_md5_common.c -- shared implementation of the MD5 / MD4 /

 * MD5-SHA1 digest family tests (API in test_digest_md5_common.h; one

 * binary per digest, one KAT row set per binary: test_digest_md5.c,

 * test_digest_md4.c and test_digest_md5_sha1.c).

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

 * Each per-digest binary carries its own digest's rows of that table and

 * runs the routes and the chunked-vs-one-shot equality for its own digest


 *

 * Provider situation: MD4 lives in the legacy provider, which is loaded

 * explicitly with OSSL_PROVIDER_load(NULL, "legacy") (the default provider

 * hosts MD5 and MD5-SHA1).  Loading a provider explicitly disables the

 * auto-load of the default provider, so both must be loaded.

 */

#include "test_digest_md5_common.h"

#include <openssl/evp.h>

/* The shared "abc" message (the KAT message of MD4, MD5 and MD5-SHA1).

 * The constant is an array (not a pointer) so that it is an address

 * constant usable in the static struct md5_kat initializers of the

 * per-digest binaries. */

const char md5_msg_abc[] = "abc";

/* The legacy provider is required for MD4; the default provider hosts MD5

 * and MD5-SHA1.  Loading a provider explicitly disables default-provider

 * auto-load, so both are loaded (each CHECK()ing success). */

void md5_load_providers(void)

{

    test_load_provider("legacy");

    test_load_provider("default");

}

/* One-shot digest via EVP_Digest.  `out` must have room for

 * EVP_MD_get_size(md) bytes.  Returns the digest length. */

size_t md5_digest_one_shot(const EVP_MD *md, const unsigned char *in, size_t inlen,

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

/* Digest via init/update/final with explicit chunk lengths {a, b, rest}

 * (the MD5-SHA1 streaming split across the two halves). */

size_t md5_digest_chunked_ab(const EVP_MD *md, const unsigned char *in, size_t inlen,

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

/* (1) One-shot EVP_Digest with EVP_MD_fetch == KAT for every row; also

 * checks EVP_MD_get_size against the KAT digest length.  (The rows carry

 * the 16-byte MD5/MD4 vectors; MD5-SHA1's 36-byte KAT is checked by its

 * own binary.) */

void md5_test_kats(const struct md5_kat *kats, size_t nkats)

{

    size_t k;

    for (k = 0; k < nkats; k++) {

        const struct md5_kat *kat = &kats[k];

        EVP_MD *md;

        unsigned char *in = ALLOCATE_INPUT(3);

        unsigned char *want = ALLOCATE_OTHER(16);

        unsigned char *got = ALLOCATE_OUTPUT(16);

        size_t inlen = 0, wantlen, gotlen;

        char msg[128];

        snprintf(msg, sizeof(msg),

                 "EVP_MD_fetch(\"%s\") failed (is the legacy provider loaded?)",

                 kat->mdname);

        md = EVP_MD_fetch(NULL, kat->mdname, NULL);

        CHECK(md != NULL, msg);

        CHECK(EVP_MD_get_size(md) == (int)(strlen(kat->mdhex) / 2),

              "EVP_MD_get_size disagrees with the KAT digest length");

        if (kat->msg != NULL) {

            inlen = strlen(kat->msg);

            memcpy(in, kat->msg, inlen);

        }

        wantlen = from_hex(kat->mdhex, want, 16);

        gotlen = md5_digest_one_shot(md, in, inlen, got, 16);

        snprintf(msg, sizeof(msg), "%s one-shot digest does not match the KAT",

                 kat->mdname);

        unpoison_all();

        CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0, msg);

        repoison_all();

        EVP_MD_free(md);

        free_poison_allocations();

    }

}

/* (2) The "abc" KAT (abchex) through the static route (`static_md`, spelled

 * static_name in the CHECK messages) and through

 * EVP_get_digestbyname(byname); the static route digests one-shot, the

 * byname route chunked.  The working buffers are sized for the largest

 * family digest (MD5-SHA1's 36-byte output). */

void md5_test_routes(const char *abchex, const EVP_MD *static_md,

                     const char *static_name, const char *byname)

{

    unsigned char *in = ALLOCATE_INPUT(3);

    unsigned char *want = ALLOCATE_OTHER(64);

    unsigned char *got = ALLOCATE_OUTPUT(64);

    size_t wantlen, gotlen;

    char msg[128];

    const EVP_MD *byname_md;

    memcpy(in, md5_msg_abc, strlen(md5_msg_abc));

    wantlen = from_hex(abchex, want, 64);

    snprintf(msg, sizeof(msg), "%s returned NULL", static_name);

    CHECK(static_md != NULL, msg);

    gotlen = md5_digest_one_shot(static_md, in, 3, got, 64);

    snprintf(msg, sizeof(msg), "%s digest does not match the KAT", static_name);

    unpoison_all();

    CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0, msg);

    repoison_all();

    snprintf(msg, sizeof(msg), "EVP_get_digestbyname(\"%s\") returned NULL",

             byname);

    byname_md = EVP_get_digestbyname(byname);

    CHECK(byname_md != NULL, msg);

    memset(got, 0, 64);

    gotlen = digest_chunked(byname_md, in, 3, got, 64);

    snprintf(msg, sizeof(msg),

             "EVP_get_digestbyname(\"%s\") digest does not match the KAT", byname);

    unpoison_all();

    CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0, msg);

    repoison_all();

    free_poison_allocations();

}

/* (3) Chunked-vs-one-shot equality for mdname around the 64-byte block

 * boundary (deterministic byte pattern i*7+3).  The working buffers are

 * sized for the largest family digest (MD5-SHA1's 36-byte output). */

void md5_test_chunked_equality(const char *mdname)

{

    static const size_t sizes[] = { 0, 1, 55, 56, 63, 64, 65, 1000 };

    EVP_MD *md;

    unsigned char *in = ALLOCATE_INPUT(1000);

    unsigned char *one = ALLOCATE_OUTPUT(64);

    unsigned char *many = ALLOCATE_OUTPUT(64);

    size_t s, i;

    char msg[128];

    snprintf(msg, sizeof(msg), "EVP_MD_fetch(\"%s\") failed", mdname);

    md = EVP_MD_fetch(NULL, mdname, NULL);

    CHECK(md != NULL, msg);

    for (i = 0; i < 1000; i++)

        in[i] = (unsigned char)(i * 7 + 3);

    for (s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {

        size_t n = sizes[s];

        size_t one_len = md5_digest_one_shot(md, in, n, one, 64);

        size_t many_len = digest_chunked(md, in, n, many, 64);

        snprintf(msg, sizeof(msg),

                 "%s chunked digest differs from one-shot at input length %zu",

                 mdname, n);

        unpoison_all();

        CHECK(one_len == many_len && memcmp(one, many, one_len) == 0, msg);

        repoison_all();

    }

    EVP_MD_free(md);

    free_poison_allocations();

}


/*

 * test_digest_mdc2_whirlpool_common.c -- shared implementation of the

 * MDC2 and WHIRLPOOL (legacy provider) digest tests (API in

 * test_digest_mdc2_whirlpool_common.h; one binary per digest, one KAT row

 * set per binary: test_digest_mdc2.c and test_digest_whirlpool.c).

 *

 * Known-answer vectors, all verified with pizfix/bin/openssl:

 *

 *   printf 'abc' | pizfix/bin/openssl dgst -provider legacy \

 *       -provider default -mdc2

 *   printf 'message digest' | pizfix/bin/openssl dgst -provider legacy \

 *       -provider default -whirlpool

 *

 * (plus "abc" for both and the empty message for WHIRLPOOL).  Each

 * per-digest binary carries its own digest's rows of that table and runs

 * the fetch-route KATs for its own digest; WHIRLPOOL also carries the

 * byname route.

 *

 * Both algorithms live in the legacy provider, which is loaded explicitly

 * with OSSL_PROVIDER_load(NULL, "legacy") (loading a provider explicitly

 * disables default-provider auto-load, so the default provider is loaded

 * too).

 *

 * Version/build gating: MDC2 is absent from some distributions' OpenSSL

 * 3.0 builds (e.g. Ubuntu's, which drop it along with single-DES), so

 * test_digest_mdc2.c takes the whole-program SKIP path via

 * mdc2_whirlpool_probe_or_skip() right after its provider load; on a build

 * that does offer MDC2 the sections run and a NULL fetch there is a broken

 * build, not a version gate.

 */

#include "test_digest_mdc2_whirlpool_common.h"

#include <openssl/evp.h>

/* The two KAT messages of this family.  The constants are arrays (not

 * pointers) so that they are address constants usable in the static

 * struct mdc2_whirlpool_kat initializers and the byname-route calls of

 * the per-digest binaries. */

const char mdc2_whirlpool_msg_abc[] = "abc";

const char mdc2_whirlpool_msg_message_digest[] = "message digest";

/* The legacy provider hosts MDC2 and WHIRLPOOL in OpenSSL 3.x; loading a

 * provider explicitly disables default-provider auto-load, so both are

 * loaded (each CHECK()ing success). */

void mdc2_whirlpool_load_providers(void)

{

    test_load_provider("legacy");

    test_load_provider("default");

}

/* Whole-program version gate: SKIP with the documented reason when the

 * build lacks the digest, release the probe handle otherwise. */

void mdc2_whirlpool_probe_or_skip(const char *mdname)

{

    char msg[128];

    EVP_MD *probe = EVP_MD_fetch(NULL, mdname, NULL);

    if (probe == NULL) {

        snprintf(msg, sizeof(msg), "%s is unavailable", mdname);

        test_skip(msg);

    }

    EVP_MD_free(probe);

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

/* (1) The rows' KATs for mdname through EVP_MD_fetch: one-shot for every

 * row, plus the awkward {1,63,rest} chunked digest for the rows with

 * chunked != 0.  Also checks EVP_MD_get_size against `size`.  (The

 * working buffers are sized for the largest family digest, WHIRLPOOL's

 * 64-byte output.) */

void mdc2_whirlpool_test_kats(const char *mdname, int size,

                              const struct mdc2_whirlpool_kat *kats,

                              size_t nkats)

{

    EVP_MD *md;

    unsigned char *in = ALLOCATE_INPUT(14);

    unsigned char *want = ALLOCATE_OTHER(64);

    unsigned char *got = ALLOCATE_OUTPUT(64);

    size_t k, wantlen, gotlen;

    char msg[128];

    snprintf(msg, sizeof(msg), "EVP_MD_fetch(\"%s\") failed", mdname);

    md = EVP_MD_fetch(NULL, mdname, NULL);

    CHECK(md != NULL, msg);

    snprintf(msg, sizeof(msg), "EVP_MD_get_size(%s) is not %d", mdname, size);

    CHECK(EVP_MD_get_size(md) == size, msg);

    for (k = 0; k < nkats; k++) {

        const struct mdc2_whirlpool_kat *kat = &kats[k];

        size_t inlen = kat->msg != NULL ? strlen(kat->msg) : 0;

        if (kat->msg != NULL)

            memcpy(in, kat->msg, inlen);

        wantlen = from_hex(kat->mdhex, want, 64);

        gotlen = digest_one_shot(md, in, inlen, got, 64);

        snprintf(msg, sizeof(msg),

                 "%s(\"%s\") one-shot digest does not match the KAT",

                 mdname, kat->msg != NULL ? kat->msg : "");

        unpoison_all();

        CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0, msg);

        repoison_all();

        if (kat->chunked) {

            memset(got, 0, 64);

            gotlen = digest_chunked(md, in, inlen, got, 64);

            snprintf(msg, sizeof(msg),

                     "%s(\"%s\") chunked digest does not match the KAT",

                     mdname, kat->msg != NULL ? kat->msg : "");

            unpoison_all();

            CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0, msg);

            repoison_all();

        }

    }

    EVP_MD_free(md);

    free_poison_allocations();

}

/* (2) The KAT (kathex) for the msglen-byte message `msg` via the

 * EVP_get_digestbyname(byname) route, chunked.  (The working buffers are

 * sized for the largest family digest, WHIRLPOOL's 64-byte output.) */

void mdc2_whirlpool_test_byname_route(const char *byname, const char *msg,

                                      size_t msglen, const char *kathex)

{

    const EVP_MD *byname_md;

    unsigned char *in = ALLOCATE_INPUT(14);

    unsigned char *want = ALLOCATE_OTHER(64);

    unsigned char *got = ALLOCATE_OUTPUT(64);

    size_t wantlen, gotlen;

    char msgtext[128];

    snprintf(msgtext, sizeof(msgtext),

             "EVP_get_digestbyname(\"%s\") returned NULL", byname);

    byname_md = EVP_get_digestbyname(byname);

    CHECK(byname_md != NULL, msgtext);

    memcpy(in, msg, msglen);

    memset(got, 0, 64);

    gotlen = digest_chunked(byname_md, in, msglen, got, 64);

    wantlen = from_hex(kathex, want, 64);

    snprintf(msgtext, sizeof(msgtext),

             "EVP_get_digestbyname(\"%s\") digest does not match the KAT",

             byname);

    unpoison_all();

    CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0, msgtext);

    repoison_all();

    free_poison_allocations();

}


/*
 * test_digest_shake_common.c -- shared implementation of the SHAKE128/256
 * XOF and KECCAK-KMAC-128/256 digest tests (API in
 * test_digest_shake_common.h; one binary per digest, one struct shake_kat
 * row set per binary).
 *
 * Known-answer vectors, all verified with pizfix/bin/openssl, e.g.
 *
 *   printf 'abc' | pizfix/bin/openssl dgst -shake128 -xoflen 32
 *   printf 'abc' | pizfix/bin/openssl dgst -keccak-kmac-128 -xoflen 32
 *
 *   "abc" and the empty message for SHAKE128/SHAKE256 at their documented
 *   output lengths, and "abc" for KECCAK-KMAC-128/256.  Each per-digest
 *   binary carries its own rows.
 *
 * Coverage (per binary, for the binary's own digest):
 *   - output length control through OSSL_DIGEST_PARAM_XOFLEN ("xoflen")
 *     via EVP_MD_CTX_set_params BEFORE final, and through
 *     EVP_DigestFinalXOF; both routes must agree exactly;
 *   - the prefix property: squeezing more bytes extends, never changes,
 *     a shorter squeeze of the same input;
 *   - EVP_DigestSqueeze (OpenSSL 3.5+; SHAKE binaries only -- the x86
 *     provider gives the KMAC digest implementations no squeeze method, so
 *     the KMAC binaries never carry this section): two squeeze calls
 *     (10 then 100 bytes) must equal one 110-byte squeeze.  The
 *     implementation buffers leftover state between squeezes
 *     (crypto/sha/sha3.c);
 *   - KECCAK-KMAC-128/256 with xoflen (the digest form of KMAC) plus the
 *     EVP_MD_get_size == xoflen check.
 *
 * House rule 6: SHAKE out-buffers are sized by the xoflen we set, since
 * EVP_MD_get_size is 0 for XOFs on newer OpenSSL.
 *
 * Version gates: EVP_DigestSqueeze is version-gated (3.5+).  On OpenSSL
 * 3.0.x the SHAKE binaries print the documented mid-run SKIP line and exit
 * 0 after running all their other checks (the monolith's pattern, kept per
 * binary).  KECCAK-KMAC-128/256 are present in every OpenSSL 3.x build
 * tested here (3.0.13 and 3.6.5); their gate is whole-program
 * (shake_probe_or_skip right after test_parse_args()).
 */

#include "test_digest_shake_common.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>

/* The classic one-block FIPS message (the KMAC rows and the "abc" SHAKE
 * rows digest exactly these 3 bytes). */
static const char *const MSG_ABC = "abc";

/* Whole-program version gate: SKIP with the documented reason when the
 * build lacks the digest, release the probe handle otherwise. */
void shake_probe_or_skip(const char *mdname)
{
    char msg[128];
    EVP_MD *probe = EVP_MD_fetch(NULL, mdname, NULL);

    if (probe == NULL) {
        snprintf(msg, sizeof(msg), "%s is unavailable", mdname);
        test_skip(msg);
    }
    EVP_MD_free(probe);
}

/* Fetches a digest by name, CHECK()ing success.  SHAKE is core OpenSSL 3
 * functionality, and the KECCAK-KMAC-* version gate ran in main(), so a
 * NULL fetch here is a broken build, not a gate. */
static EVP_MD *shake_fetch(const char *mdname)
{
    char msg[128];
    EVP_MD *md;

    snprintf(msg, sizeof(msg), "EVP_MD_fetch(\"%s\") failed", mdname);
    md = EVP_MD_fetch(NULL, mdname, NULL);
    CHECK(md != NULL, msg);
    return md;
}

/* XOF output via OSSL_DIGEST_PARAM_XOFLEN + EVP_DigestFinal_ex.  The
 * xoflen parameter must be set before the final call; `out` must have room
 * for xoflen bytes (house rule 6: SHAKE out-buffers are sized by the
 * xoflen we set, since EVP_MD_get_size is 0 for XOFs on newer OpenSSL). */
static void xof_digest_param(const EVP_MD *md, const unsigned char *in, size_t inlen,
                             size_t xoflen, unsigned char *out)
{
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    OSSL_PARAM params[2];
    unsigned int outlen = 0;
    size_t len = xoflen;
    CHECK(ctx != NULL, "EVP_MD_CTX_new failed");
    CHECK(EVP_DigestInit_ex(ctx, md, NULL) == 1, "EVP_DigestInit_ex failed");
    params[0] = OSSL_PARAM_construct_size_t(OSSL_DIGEST_PARAM_XOFLEN, &len);
    params[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_MD_CTX_set_params(ctx, params) == 1,
          "EVP_MD_CTX_set_params(xoflen) failed");
    if (inlen > 0)
        CHECK(EVP_DigestUpdate(ctx, in, inlen) == 1, "EVP_DigestUpdate failed");
    CHECK(EVP_DigestFinal_ex(ctx, out, &outlen) == 1,
          "EVP_DigestFinal_ex failed");
    CHECK((size_t)outlen == xoflen, "xoflen output length mismatch");
    EVP_MD_CTX_free(ctx);
}

/* XOF output via EVP_DigestFinalXOF.  Must agree with xof_digest_param. */
static void xof_digest_finalxof(const EVP_MD *md, const unsigned char *in,
                                size_t inlen, size_t xoflen, unsigned char *out)
{
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    CHECK(ctx != NULL, "EVP_MD_CTX_new failed");
    CHECK(EVP_DigestInit_ex(ctx, md, NULL) == 1, "EVP_DigestInit_ex failed");
    if (inlen > 0)
        CHECK(EVP_DigestUpdate(ctx, in, inlen) == 1, "EVP_DigestUpdate failed");
    CHECK(EVP_DigestFinalXOF(ctx, out, xoflen) == 1,
          "EVP_DigestFinalXOF failed");
    EVP_MD_CTX_free(ctx);
}

/* (1) One KAT row through both output-length routes; the two routes must
 * agree byte for byte. */
void shake_test_kat(const struct shake_kat *kat)
{
    EVP_MD *md = shake_fetch(kat->mdname);
    unsigned char *in = ALLOCATE_INPUT(3);
    unsigned char *want = ALLOCATE_OTHER(64);
    unsigned char *got_param = ALLOCATE_OUTPUT(64);
    unsigned char *got_xof = ALLOCATE_OUTPUT(64);
    size_t inlen = 0, wantlen;
    char msg[128];

    if (kat->msg != NULL) {
        inlen = strlen(kat->msg);
        memcpy(in, kat->msg, inlen);
    }
    wantlen = from_hex(kat->outhex, want, 64);
    CHECK(wantlen == kat->xoflen, "KAT length disagrees with its xoflen");

    xof_digest_param(md, in, inlen, kat->xoflen, got_param);
    snprintf(msg, sizeof(msg), "%s xoflen=%zu digest does not match the KAT",
             kat->mdname, kat->xoflen);
    unpoison_all();
    CHECK(memcmp(got_param, want, wantlen) == 0, msg);
    repoison_all();

    xof_digest_finalxof(md, in, inlen, kat->xoflen, got_xof);
    snprintf(msg, sizeof(msg),
             "EVP_DigestFinalXOF disagrees with the xoflen route for %s",
             kat->mdname);
    unpoison_all();
    CHECK(memcmp(got_param, got_xof, wantlen) == 0, msg);
    repoison_all();
    EVP_MD_free(md);

    free_poison_allocations();
}

/* (4) One KECCAK-KMAC-128/256 row: like shake_test_kat, plus the
 * EVP_MD_get_size == xoflen check (EVP_MD_get_size is meaningful for the
 * KMAC digests, where it is unused for SHAKE). */
void shake_test_kmac_kat(const struct shake_kat *kat)
{
    EVP_MD *md = shake_fetch(kat->mdname);
    unsigned char *want = ALLOCATE_OTHER(64);
    unsigned char *got_param = ALLOCATE_OUTPUT(64);
    unsigned char *got_xof = ALLOCATE_OUTPUT(64);
    size_t wantlen;
    char msg[128];

    CHECK(EVP_MD_get_size(md) == (int)kat->xoflen,
          "KECCAK-KMAC EVP_MD_get_size disagrees with its xoflen");
    wantlen = from_hex(kat->outhex, want, 64);

    xof_digest_param(md, (const unsigned char *)MSG_ABC, 3, kat->xoflen,
                     got_param);
    snprintf(msg, sizeof(msg), "%s(\"abc\") digest does not match the KAT",
             kat->mdname);
    unpoison_all();
    CHECK(memcmp(got_param, want, wantlen) == 0, msg);
    repoison_all();

    xof_digest_finalxof(md, (const unsigned char *)MSG_ABC, 3, kat->xoflen,
                        got_xof);
    snprintf(msg, sizeof(msg), "%s: FinalXOF route disagrees with xoflen route",
             kat->mdname);
    unpoison_all();
    CHECK(memcmp(got_param, got_xof, wantlen) == 0, msg);
    repoison_all();
    EVP_MD_free(md);

    free_poison_allocations();
}

/* (2) Prefix property: the first short_len bytes of a 200-byte squeeze are
 * the short_len-byte squeeze, for the deterministic 137-byte input. */
void shake_test_prefix_property(const char *mdname, size_t short_len)
{
    EVP_MD *md = shake_fetch(mdname);
    unsigned char *in = ALLOCATE_INPUT(137);
    unsigned char *short_out = ALLOCATE_OUTPUT(64);
    unsigned char *long_out = ALLOCATE_OUTPUT(200);
    size_t i;
    char msg[128];

    for (i = 0; i < 137; i++)
        in[i] = (unsigned char)(i * 7 + 3);

    xof_digest_param(md, in, 137, short_len, short_out);
    xof_digest_param(md, in, 137, 200, long_out);
    snprintf(msg, sizeof(msg),
             "%s: longer squeeze changed the first %zu bytes", mdname,
             short_len);
    unpoison_all();
    CHECK(memcmp(short_out, long_out, short_len) == 0, msg);
    repoison_all();
    EVP_MD_free(md);

    free_poison_allocations();
}

#if OPENSSL_VERSION_NUMBER >= 0x30500000L
/* (3) EVP_DigestSqueeze (OpenSSL 3.5+): squeezing 10 bytes and then 100
 * bytes must equal a single 110-byte squeeze.  The provider keeps the
 * leftover squeezed state in ctx->buf between calls
 * (crypto/sha/sha3.c), so a two-call squeeze is not the same as two
 * independent squeezes -- it is one continuous output. */
void shake_test_squeeze(const char *mdname)
{
    EVP_MD *md = shake_fetch(mdname);
    unsigned char *in = ALLOCATE_INPUT(137);
    unsigned char *twocall = ALLOCATE_OUTPUT(110);
    unsigned char *onecall = ALLOCATE_OUTPUT(110);
    EVP_MD_CTX *c1 = EVP_MD_CTX_new(), *c2 = EVP_MD_CTX_new();
    size_t i;

    for (i = 0; i < 137; i++)
        in[i] = (unsigned char)(i * 7 + 3);
    CHECK(c1 != NULL && c2 != NULL, "EVP_MD_CTX_new failed");

    CHECK(EVP_DigestInit_ex(c1, md, NULL) == 1, "EVP_DigestInit_ex failed");
    CHECK(EVP_DigestUpdate(c1, in, 137) == 1, "EVP_DigestUpdate failed");
    CHECK(EVP_DigestSqueeze(c1, twocall, 10) == 1,
          "EVP_DigestSqueeze (10 bytes) failed");
    CHECK(EVP_DigestSqueeze(c1, twocall + 10, 100) == 1,
          "EVP_DigestSqueeze (100 bytes) failed");

    CHECK(EVP_DigestInit_ex(c2, md, NULL) == 1, "EVP_DigestInit_ex failed");
    CHECK(EVP_DigestUpdate(c2, in, 137) == 1, "EVP_DigestUpdate failed");
    CHECK(EVP_DigestSqueeze(c2, onecall, 110) == 1,
          "EVP_DigestSqueeze (110 bytes) failed");

    unpoison_all();
    CHECK(memcmp(twocall, onecall, 110) == 0,
          "two-call squeeze differs from the single 110-byte squeeze");
    repoison_all();
    EVP_MD_CTX_free(c1);
    EVP_MD_CTX_free(c2);
    EVP_MD_free(md);

    free_poison_allocations();
}
#endif /* OPENSSL_VERSION_NUMBER >= 0x30500000L */

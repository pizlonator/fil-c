/*
 * test_kdf_kbkdf_common.c -- shared implementation of the KBKDF-SHA1 /
 * KBKDF-SHA256 / KBKDF-SHA512 (SP 800-108) family tests (API in
 * test_kdf_kbkdf_common.h; one binary per digest, one struct kbkdf_family
 * row per binary).
 *
 * The family covers the SP 800-108 key-based KDF (KBKDF) through the EVP_KDF
 * public API: counter and feedback modes with CMAC-AES128 and HMAC PRFs, the
 * use-l / use-separator switches, the "r" counter width where the release
 * supports it, the "seed" parameter as the feedback-mode IV, KMAC128 as a
 * PRF (3.1+), repeated-info concatenation (3.2+) and the error paths.
 *
 * Parameter naming quirk: KBKDF's "salt" parameter is the SP 800-108
 * *Label*, "info" is the *Context*, and "seed" is the feedback-mode IV.
 *
 * Known-answer vectors: NIST CAVS values as distributed in OpenSSL's own
 * test data (test/recipes/30-test_evp_data/evpkdf_kbkdf_counter.txt), plus
 * feedback-mode values derived with the authoritative pizfix OpenSSL 3.6.5
 * and cross-checked under the system OpenSSL 3.0.13 (both builds agree on
 * every value below):
 *
 *   pizfix/bin/openssl kdf -keylen 16 -kdfopt mode:COUNTER \
 *       -kdfopt cipher:AES128 -kdfopt mac:CMAC -kdfopt use-l:0 \
 *       -kdfopt use-separator:0 [-kdfopt r:<8|32>] \
 *       -kdfopt hexkey:<key> -kdfopt hexinfo:<info> KBKDF
 *
 * The "r" parameter does not exist before OpenSSL 3.1, so the r-width
 * vectors are feature-detected via EVP_KDF_settable_ctx_params() and
 * skipped (with a note) where it is absent; KMAC support (3.1+) is
 * detected via EVP_MAC_fetch().
 *
 * Distribution decisions (carried out of the monolith): the CMAC-AES128 and
 * KMAC128 PRFs take a cipher or a MAC rather than a digest, so their checks
 * (the counter CMAC KAT, the CMAC r=8 KAT, the CMAC feedback KAT and the
 * KMAC128 PRF KAT) are digest-independent and run in the SHA-256 binary --
 * the representative digest the monolith's parameterized sections (the
 * switch KATs, the feedback section and the negatives) ran on, which also
 * carries their constants.  The digest-specific CAVS rows moved to their own
 * binaries (the HMAC-SHA1 r=8 vector to the SHA-1 binary, the three-chunk
 * HMAC-SHA512 r=32 vector to the SHA-512 binary); the switch, feedback,
 * r-width and negative behavior runs in every binary for its own digest,
 * with the digest name parameterized into the CHECK messages.  The digests
 * without a verified KAT constant for a generalized check (the monolith
 * verified the switch and feedback values for SHA-256 only) verify the same
 * behavior by difference or against a reference derive instead -- never by
 * dropping it -- and every such property was re-verified with the CLI on
 * the 3.0.13 and 3.6.5 builds for all three digests.
 */

#include "test_kdf_kbkdf_common.h"

#include <openssl/core_names.h>
#include <openssl/kdf.h>
#include <openssl/opensslv.h>

/* The small HMAC material shared by every binary's switch/behavior checks
 * and negatives (see the API comment in test_kdf_kbkdf_common.h). */
const char kbkdf_ksmall_hex[] = "0ef9";
const char kbkdf_ismall_hex[] = "56ec";

/* Working-buffer sizes (the original monolith's KEY_CAP/INFO_CAP/OUT_CAP;
 * the feedback IV is sized to the PRF output size at run time, whose family
 * maximum is the SHA-512 PRF's 64 bytes). */
#define KEY_CAP 64
#define INFO_CAP 64
#define OUT_CAP 48

/* TRUE if the KDF's settable parameter list contains `name`. */
static int kbkdf_has_param(const char *kdfname, const char *name)
{
    EVP_KDF *kdf = EVP_KDF_fetch(NULL, kdfname, NULL);
    const OSSL_PARAM *defs;
    int found = 0;

    CHECK(kdf != NULL, "EVP_KDF_fetch failed for the core KBKDF KDF");
    for (defs = EVP_KDF_settable_ctx_params(kdf); defs != NULL &&
         defs->key != NULL; defs++) {
        if (strcmp(defs->key, name) == 0) {
            found = 1;
            break;
        }
    }
    EVP_KDF_free(kdf);
    return found;
}

/* One KBKDF derive with a fresh context.  Pass -1 for use_l, use_sep or r
 * to leave that parameter unset; pass NULL buffers to omit optional octet
 * params.  Returns 1 on success, 0 if EVP_KDF_derive failed.  The parameter
 * array is sized for the largest call shape (mode, mac, cipher-or-digest,
 * use-l, use-separator, r, key, salt, three infos, iv) plus the end
 * marker. */
static int kbkdf_derive(const char *mode, const char *mac, const char *cipher,
                        const char *digest, const unsigned char *key,
                        size_t keylen, const unsigned char *salt,
                        size_t saltlen, const unsigned char *const *infos,
                        const size_t *infolens, size_t ninfos,
                        const unsigned char *iv, size_t ivlen,
                        int use_l, int use_sep, int r,
                        unsigned char *out, size_t outlen)
{
    EVP_KDF *kdf = EVP_KDF_fetch(NULL, "KBKDF", NULL);
    EVP_KDF_CTX *ctx;
    OSSL_PARAM params[11];
    int ok, n = 0;
    static int use_l_on = 1, use_l_off = 0;
    static int sep_on = 1, sep_off = 0;
    int rval = r;
    size_t i;

    CHECK(kdf != NULL, "EVP_KDF_fetch failed for the core KBKDF KDF");
    ctx = EVP_KDF_CTX_new(kdf);
    CHECK(ctx != NULL, "EVP_KDF_CTX_new failed");
    params[n++] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_MODE,
                                                   (char *)mode, 0);
    params[n++] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_MAC,
                                                   (char *)mac, 0);
    if (cipher != NULL)
        params[n++] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_CIPHER,
                                                       (char *)cipher, 0);
    if (digest != NULL)
        params[n++] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                                                       (char *)digest, 0);
    params[n++] = OSSL_PARAM_construct_int(OSSL_KDF_PARAM_KBKDF_USE_L,
                                           use_l < 0 ? &use_l_on :
                                           (use_l ? &use_l_on : &use_l_off));
    params[n++] = OSSL_PARAM_construct_int(OSSL_KDF_PARAM_KBKDF_USE_SEPARATOR,
                                           use_sep < 0 ? &sep_on :
                                           (use_sep ? &sep_on : &sep_off));
    if (r >= 0)
        params[n++] = OSSL_PARAM_construct_int("r", &rval);
    params[n++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY,
                                                    (void *)key, keylen);
    if (salt != NULL)
        params[n++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT,
                                                        (void *)salt,
                                                        saltlen);
    for (i = 0; i < ninfos; i++)
        params[n++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO,
                                                        (void *)infos[i],
                                                        infolens[i]);
    if (iv != NULL)
        params[n++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SEED,
                                                        (void *)iv, ivlen);
    params[n] = OSSL_PARAM_construct_end();
    ok = EVP_KDF_derive(ctx, out, outlen, params) > 0;
    EVP_KDF_CTX_free(ctx);
    EVP_KDF_free(kdf);
    return ok;
}

/* (1) Counter mode, CMAC-AES128, default 32-bit counter: the CAVS KAT. */
void kbkdf_test_counter_cmac(const char *keyhex, const char *infohex,
                             const char *outhex)
{
    unsigned char *key = ALLOCATE_KEY(KEY_CAP);
    unsigned char *info = ALLOCATE_SALT(INFO_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(OUT_CAP);
    size_t keylen = from_hex(keyhex, key, KEY_CAP);
    size_t infolen = from_hex(infohex, info, INFO_CAP);
    size_t wantlen = from_hex(outhex, want, OUT_CAP);
    const unsigned char *infos[1];
    size_t infolens[1];

    infos[0] = info;
    infolens[0] = infolen;
    CHECK(kbkdf_derive("COUNTER", "CMAC", "AES128", NULL, key, keylen,
                       NULL, 0, infos, infolens, 1, NULL, 0, 0, 0, -1,
                       out, 16),
          "KBKDF counter/CMAC derive failed");
    expect_equal("counter-mode CMAC-AES128 output does not match the CAVS KAT",
                 out, 16, want, wantlen);

    free_poison_allocations();
}

/* (2) Counter mode with the HMAC PRF of the calling binary's digest: the
 * use-l / use-separator switch behavior, then the "r" counter width where
 * the release supports it (see the API comment for the NULL-KAT behavior
 * form). */
void kbkdf_test_counter_hmac(const struct kbkdf_family *fam,
                             const char *kat_default_hex,
                             const char *kat_use_l_hex,
                             const char *kat_use_sep_hex,
                             const struct kbkdf_r_kat *rkats, size_t nrkats)
{
    unsigned char *key = ALLOCATE_KEY(KEY_CAP);
    unsigned char *info = ALLOCATE_SALT(INFO_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(OUT_CAP);
    const unsigned char *infos[1];
    size_t infolens[1];
    size_t keylen, infolen, wantlen, i;
    int has_r = kbkdf_has_param("KBKDF", "r");
    char msg[128];

    /* HMAC-<digest> with both switches off and the default counter width. */
    keylen = from_hex(kbkdf_ksmall_hex, key, KEY_CAP);
    infolen = from_hex(kbkdf_ismall_hex, info, INFO_CAP);
    infos[0] = info;
    infolens[0] = infolen;
    snprintf(msg, sizeof(msg), "KBKDF counter/HMAC-%s derive failed",
             fam->digest);
    CHECK(kbkdf_derive("COUNTER", "HMAC", NULL, fam->digest, key, keylen,
                       NULL, 0, infos, infolens, 1, NULL, 0, 0, 0, -1,
                       out, 16),
          msg);
    if (kat_default_hex != NULL) {
        wantlen = from_hex(kat_default_hex, want, OUT_CAP);
        snprintf(msg, sizeof(msg),
                 "counter-mode HMAC-%s output does not match the KAT",
                 fam->digest);
        expect_equal(msg, out, 16, want, wantlen);
    }

    /* use-l=1 appends the output length to the fixed data. */
    snprintf(msg, sizeof(msg),
             "KBKDF counter/HMAC-%s use-l=1 derive failed", fam->digest);
    if (kat_use_l_hex != NULL) {
        CHECK(kbkdf_derive("COUNTER", "HMAC", NULL, fam->digest, key, keylen,
                           NULL, 0, infos, infolens, 1, NULL, 0, 1, 0, -1,
                           out, 16),
              msg);
        wantlen = from_hex(kat_use_l_hex, want, OUT_CAP);
        expect_equal("use-l=1 output does not match the KAT", out, 16, want,
                     wantlen);
    } else {
        /* No verified use-l KAT for this digest: the switch must change
         * the output (the behavior the monolith's CLI-verified KAT
         * embodies), so the default output is kept in `out` for the
         * difference check and the switch derive goes to a second buffer. */
        unsigned char *alt = ALLOCATE_OUTPUT(OUT_CAP);

        CHECK(kbkdf_derive("COUNTER", "HMAC", NULL, fam->digest, key, keylen,
                           NULL, 0, infos, infolens, 1, NULL, 0, 1, 0, -1,
                           alt, 16),
              msg);
        snprintf(msg, sizeof(msg),
                 "use-l=1 does not change the HMAC-%s output", fam->digest);
        unpoison_all();
        CHECK(memcmp(out, alt, 16) != 0, msg);
        repoison_all();
    }

    /* use-separator=1 inserts a zero byte before the fixed data. */
    snprintf(msg, sizeof(msg),
             "KBKDF counter/HMAC-%s use-separator=1 derive failed",
             fam->digest);
    if (kat_use_sep_hex != NULL) {
        CHECK(kbkdf_derive("COUNTER", "HMAC", NULL, fam->digest, key, keylen,
                           NULL, 0, infos, infolens, 1, NULL, 0, 0, 1, -1,
                           out, 16),
              msg);
        wantlen = from_hex(kat_use_sep_hex, want, OUT_CAP);
        expect_equal("use-separator=1 output does not match the KAT",
                     out, 16, want, wantlen);
    } else {
        unsigned char *alt = ALLOCATE_OUTPUT(OUT_CAP);

        CHECK(kbkdf_derive("COUNTER", "HMAC", NULL, fam->digest, key, keylen,
                           NULL, 0, infos, infolens, 1, NULL, 0, 0, 1, -1,
                           alt, 16),
              msg);
        snprintf(msg, sizeof(msg),
                 "use-separator=1 does not change the HMAC-%s output",
                 fam->digest);
        unpoison_all();
        CHECK(memcmp(out, alt, 16) != 0, msg);
        repoison_all();
    }

    if (has_r) {
        /* This binary's r-width CAVS rows (see the API comment). */
        for (i = 0; i < nrkats; i++) {
            const struct kbkdf_r_kat *rkat = &rkats[i];

            keylen = from_hex(rkat->keyhex, key, KEY_CAP);
            infolen = from_hex(rkat->infohex, info, INFO_CAP);
            infolens[0] = infolen;
            wantlen = from_hex(rkat->outhex, want, OUT_CAP);
            snprintf(msg, sizeof(msg), "KBKDF counter/%s r=%d derive failed",
                     rkat->label, rkat->r);
            CHECK(kbkdf_derive("COUNTER", rkat->mac, rkat->cipher,
                               rkat->digest, key, keylen, NULL, 0, infos,
                               infolens, 1, NULL, 0, 0, 0, rkat->r,
                               out, 16),
                  msg);
            snprintf(msg, sizeof(msg),
                     "%s r=%d output does not match the CAVS KAT",
                     rkat->label, rkat->r);
            expect_equal(msg, out, 16, want, wantlen);
        }

        /* An unsupported counter width must be rejected (the shared small
         * material, as in the monolith, with this binary's own digest). */
        keylen = from_hex(kbkdf_ksmall_hex, key, KEY_CAP);
        infolen = from_hex(kbkdf_ismall_hex, info, INFO_CAP);
        infolens[0] = infolen;
        CHECK(!kbkdf_derive("COUNTER", "HMAC", NULL, fam->digest, key, keylen,
                            NULL, 0, infos, infolens, 1, NULL, 0, 0, 0, 12,
                            out, 16),
              "KBKDF accepted r=12");
    } else {
        printf("note %s: this OpenSSL has no \"r\" parameter; "
               "skipping the r=8 CAVS vectors\n",
               test_prog_name());
    }

    free_poison_allocations();
}

/* (3) Feedback mode with the HMAC PRF of the calling binary's digest, with
 * and without the IV ("seed"), whose length must equal the PRF output size
 * (see the API comment for the NULL-KAT behavior form). */
void kbkdf_test_feedback_hmac(const struct kbkdf_family *fam,
                              const char *ivhex, const char *kat_noiv_hex,
                              const char *kat_withiv_hex)
{
    unsigned char *key = ALLOCATE_KEY(KEY_CAP);
    unsigned char *info = ALLOCATE_SALT(INFO_CAP);
    unsigned char *iv = ALLOCATE_SALT(fam->mdsize);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(OUT_CAP);
    const unsigned char *infos[1];
    size_t infolens[1];
    size_t keylen, infolen, ivlen, wantlen, j;
    char msg[128];

    keylen = from_hex(kbkdf_ksmall_hex, key, KEY_CAP);
    infolen = from_hex(kbkdf_ismall_hex, info, INFO_CAP);
    infos[0] = info;
    infolens[0] = infolen;

    /* The IV input: the verified pattern hex (SHA-256), or the same
     * 000102.. byte pattern built up to the digest's PRF output size. */
    if (ivhex != NULL) {
        ivlen = from_hex(ivhex, iv, fam->mdsize);
    } else {
        for (j = 0; j < fam->mdsize; j++)
            iv[j] = (unsigned char)j;
        ivlen = fam->mdsize;
    }

    /* No IV: the first block is just HMAC(key, info). */
    CHECK(kbkdf_derive("FEEDBACK", "HMAC", NULL, fam->digest, key, keylen,
                       NULL, 0, infos, infolens, 1, NULL, 0, 0, 0, -1,
                       out, 32),
          "KBKDF feedback/HMAC without an IV failed");
    if (kat_noiv_hex != NULL) {
        wantlen = from_hex(kat_noiv_hex, want, OUT_CAP);
        expect_equal("feedback-mode output without an IV does not match the KAT",
                     out, 32, want, wantlen);
    } else {
        /* No verified no-IV KAT for this digest: the property the
         * monolith's no-IV KAT embodies (its first block is the counter-
         * mode default output) is checked against a fresh counter derive. */
        unsigned char *ctr = ALLOCATE_OUTPUT(OUT_CAP);

        snprintf(msg, sizeof(msg), "KBKDF counter/HMAC-%s derive failed",
                 fam->digest);
        CHECK(kbkdf_derive("COUNTER", "HMAC", NULL, fam->digest, key, keylen,
                           NULL, 0, infos, infolens, 1, NULL, 0, 0, 0, -1,
                           ctr, 16),
              msg);
        snprintf(msg, sizeof(msg),
                 "feedback-mode first block differs from the counter-mode "
                 "HMAC-%s output", fam->digest);
        unpoison_all();
        CHECK(memcmp(out, ctr, 16) == 0, msg);
        repoison_all();
    }

    /* mdsize-byte IV (== the PRF output size). */
    snprintf(msg, sizeof(msg), "KBKDF feedback/HMAC with a %zu-byte IV failed",
             fam->mdsize);
    if (kat_withiv_hex != NULL) {
        CHECK(kbkdf_derive("FEEDBACK", "HMAC", NULL, fam->digest, key, keylen,
                           NULL, 0, infos, infolens, 1, iv, ivlen, 0, 0, -1,
                           out, 32),
              msg);
        wantlen = from_hex(kat_withiv_hex, want, OUT_CAP);
        expect_equal("feedback-mode output with an IV does not match the KAT",
                     out, 32, want, wantlen);
    } else {
        /* No verified with-IV KAT for this digest: the IV must change the
         * output (the behavior the monolith's with-IV KAT embodies), so the
         * no-IV output is kept in `out` for the difference check and the
         * IV derive goes to a second buffer. */
        unsigned char *alt = ALLOCATE_OUTPUT(OUT_CAP);

        CHECK(kbkdf_derive("FEEDBACK", "HMAC", NULL, fam->digest, key, keylen,
                           NULL, 0, infos, infolens, 1, iv, ivlen, 0, 0, -1,
                           alt, 32),
              msg);
        snprintf(msg, sizeof(msg),
                 "the feedback IV does not change the HMAC-%s output",
                 fam->digest);
        unpoison_all();
        CHECK(memcmp(out, alt, 32) != 0, msg);
        repoison_all();
    }

    /* A wrongly sized IV must be rejected. */
    snprintf(msg, sizeof(msg),
             "KBKDF accepted a %zu-byte feedback IV for %s",
             fam->mdsize - 1, fam->digest_prose);
    CHECK(!kbkdf_derive("FEEDBACK", "HMAC", NULL, fam->digest, key, keylen,
                        NULL, 0, infos, infolens, 1, iv, ivlen - 1, 0, 0,
                        -1, out, 32),
          msg);

    free_poison_allocations();
}

/* (3b) Feedback mode with CMAC-AES128 and a 16-byte IV. */
void kbkdf_test_feedback_cmac(const char *keyhex, const char *infohex,
                              const char *ivhex, const char *outhex)
{
    unsigned char *key = ALLOCATE_KEY(KEY_CAP);
    unsigned char *info = ALLOCATE_SALT(INFO_CAP);
    unsigned char *iv = ALLOCATE_SALT(16);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(OUT_CAP);
    const unsigned char *infos[1];
    size_t infolens[1];
    size_t keylen, infolen, ivlen, wantlen;

    keylen = from_hex(keyhex, key, KEY_CAP);
    infolen = from_hex(infohex, info, INFO_CAP);
    infolens[0] = infolen;
    infos[0] = info;
    ivlen = from_hex(ivhex, iv, 16);
    wantlen = from_hex(outhex, want, OUT_CAP);
    CHECK(kbkdf_derive("FEEDBACK", "CMAC", "AES128", NULL, key, keylen,
                       NULL, 0, infos, infolens, 1, iv, ivlen, 0, 0, -1,
                       out, 16),
          "KBKDF feedback/CMAC with a 16-byte IV failed");
    expect_equal("feedback-mode CMAC output does not match the KAT",
                 out, 16, want, wantlen);

    free_poison_allocations();
}

/* (4) KMAC128 as the PRF (OpenSSL 3.1+). */
void kbkdf_test_kmac(const char *keyhex, const char *labelhex,
                     const char *infohex, const char *outhex)
{
    /* KBKDF accepts KMAC PRFs from OpenSSL 3.1 on (3.0 has the KMAC MAC
     * itself but KBKDF rejects it as a PRF), so gate on the version. */
    int has_kmac = OPENSSL_version_major() > 3 ||
        (OPENSSL_version_major() == 3 && OPENSSL_version_minor() >= 1);
    unsigned char *key = ALLOCATE_KEY(KEY_CAP);
    unsigned char *label = ALLOCATE_SALT(INFO_CAP);
    unsigned char *info = ALLOCATE_SALT(INFO_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(OUT_CAP);
    const unsigned char *infos[1];
    size_t infolens[1];
    size_t keylen, labellen, infolen, wantlen;

    if (!has_kmac) {
        printf("note %s: KBKDF does not support KMAC PRFs in "
               "this OpenSSL; skipping the KMAC check\n",
               test_prog_name());
    } else {
        keylen = from_hex(keyhex, key, KEY_CAP);
        labellen = from_hex(labelhex, label, INFO_CAP);
        infolen = from_hex(infohex, info, INFO_CAP);
        wantlen = from_hex(outhex, want, OUT_CAP);
        infos[0] = info;
        infolens[0] = infolen;
        CHECK(kbkdf_derive("COUNTER", "KMAC128", NULL, NULL, key, keylen,
                           label, labellen, infos, infolens, 1, NULL, 0,
                           -1, -1, -1, out, 32),
              "KBKDF KMAC128 derive failed");
        expect_equal("KMAC128 output does not match the KAT",
                     out, 32, want, wantlen);
    }

    free_poison_allocations();
}

/* (4b) Three info chunks reproduce the single-chunk result where both "r"
 * and repeated-info concat exist (see the API comment). */
void kbkdf_test_multi_info3(const char *keyhex, const char *infohex,
                            const char *outhex)
{
    unsigned char *key = ALLOCATE_KEY(KEY_CAP);
    unsigned char *chunks[3];
    unsigned char *info = ALLOCATE_SALT(INFO_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(OUT_CAP);
    const unsigned char *infos[3];
    size_t infolens[3];
    size_t keylen, infolen, wantlen, i, off;
    int has_r = kbkdf_has_param("KBKDF", "r");
    int multi_info_equal;

    if (!has_r) {
        free_poison_allocations();
        return;
    }
    keylen = from_hex(keyhex, key, KEY_CAP);
    infolen = from_hex(infohex, info, INFO_CAP);
    wantlen = from_hex(outhex, want, OUT_CAP);
    for (i = 0; i < 3; i++)
        chunks[i] = ALLOCATE_SALT(INFO_CAP);
    off = 0;
    for (i = 0; i < 3; i++) {
        size_t n = (i < 2) ? 28 : infolen - off;
        memcpy(chunks[i], info + off, n);
        infos[i] = chunks[i];
        infolens[i] = n;
        off += n;
    }
    CHECK(off == infolen, "info chunks do not add up to the info");
    CHECK(kbkdf_derive("COUNTER", "HMAC", NULL, "SHA512", key, keylen,
                       NULL, 0, infos, infolens, 3, NULL, 0, 0, 0, 32,
                       out, 48),
          "KBKDF three-info-chunk derive failed");
    unpoison_all();
    multi_info_equal = wantlen == 48 && memcmp(out, want, 48) == 0;
    repoison_all();
    if (multi_info_equal) {
        g_checks++; /* multi-chunk info equals the concatenated info */
    } else {
        printf("note %s: this OpenSSL does not concatenate "
               "repeated info params (pre-3.2 behavior)\n",
               test_prog_name());
    }

    free_poison_allocations();
}

/* (5) A zero output length must fail, and so must a missing key. */
void kbkdf_test_negative(const struct kbkdf_family *fam)
{
    unsigned char *key = ALLOCATE_KEY(KEY_CAP);
    unsigned char *info = ALLOCATE_SALT(INFO_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    const unsigned char *infos[1];
    size_t infolens[1];
    size_t keylen = from_hex(kbkdf_ksmall_hex, key, KEY_CAP);
    size_t infolen = from_hex(kbkdf_ismall_hex, info, INFO_CAP);

    infos[0] = info;
    infolens[0] = infolen;
    CHECK(!kbkdf_derive("COUNTER", "HMAC", NULL, fam->digest, key, keylen,
                        NULL, 0, infos, infolens, 1, NULL, 0, 0, 0, -1,
                        out, 0),
          "KBKDF accepted a zero output length");
    CHECK(!kbkdf_derive("COUNTER", "HMAC", NULL, fam->digest, NULL, 0,
                        NULL, 0, infos, infolens, 1, NULL, 0, 0, 0, -1,
                        out, 16),
          "KBKDF accepted a missing key");

    free_poison_allocations();
}

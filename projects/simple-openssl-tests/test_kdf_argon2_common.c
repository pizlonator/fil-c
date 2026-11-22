/*
 * test_kdf_argon2_common.c -- shared implementation of the Argon2d /
 * Argon2i / Argon2id (RFC 9106) family tests (API in
 * test_kdf_argon2_common.h; one binary per variant, one struct
 * argon2_variant row per binary).
 *
 * The family covers the RFC 9106 section 5 test vectors (which include the
 * 8-byte secret and 12 bytes of associated data), the distinctness of the
 * three variants, the version parameter (the Argon2id draft-vector and
 * version 1.0 KATs live in test_kdf_argon2id.c), and the
 * parameter-validation error paths.
 *
 * OpenSSL 3.2+ only: on older releases (e.g. the system OpenSSL 3.0.13)
 * EVP_KDF_fetch("ARGON2ID") fails and every binary takes the SKIP path.
 *
 * KAT correction vs the task brief: the Argon2id tag
 * 0d640df58d78766c...e6b01e659 is the RFC 9106 section 5.3 Argon2id vector,
 * whose inputs are P = 01 x 32, S = 02 x 16, secret = 03 x 8, associated
 * data = 04 x 12 (t=3 passes, m=32 KiB, p=4 lanes, v=0x13) -- NOT the ASCII
 * strings "password"/"somesalt" (those inputs yield bb0cc80a3e671149...
 * a6ab05c under the same cost parameters; that value is embedded as an
 * extra KAT in test_kdf_argon2id.c).
 *
 * Every hex value below was verified with the authoritative pizfix
 * OpenSSL 3.6.5 CLI:
 *
 *   pizfix/bin/openssl kdf -keylen 32 -kdfopt hexpass:<P> \
 *       -kdfopt hexsalt:<S> -kdfopt hexsecret:<K> -kdfopt hexad:<X> \
 *       -kdfopt lanes:4 -kdfopt threads:1 -kdfopt memcost:32 \
 *       -kdfopt iter:3 ARGON2ID
 *
 * (threads:1 because the CLI's library context has no thread pool; threads
 * only controls OS parallelism, never the derivation result).  The
 * Argon2-specific parameter names are passed as string literals because
 * the OpenSSL 3.0 headers (which still compile this file for the SKIP
 * path) do not define the OSSL_KDF_PARAM_ARGON2_* / _THREADS macros.
 */

#include "test_kdf_argon2_common.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>

/* The RFC 9106 section 5 vector inputs.  The constants are arrays (not
 * pointers) so that they are address constants usable where the
 * per-variant binaries need them. */
const char argon2_rfc_pass_hex[] =
    "0101010101010101010101010101010101010101010101010101010101010101";
const char argon2_rfc_salt_hex[] = "02020202020202020202020202020202";
const char argon2_rfc_secret_hex[] = "0303030303030303";
const char argon2_rfc_ad_hex[] = "040404040404040404040404";

/* Working-buffer sizes for the shared section 5 inputs and outputs (the
 * original monolith's PASS_CAP/SALT_CAP/SECRET_CAP/AD_CAP/OUT_CAP). */
#define PASS_CAP 32
#define SALT_CAP 16
#define SECRET_CAP 8
#define AD_CAP 12
#define OUT_CAP 64

/* The whole-program availability gate: Argon2 arrived in OpenSSL 3.2, so a
 * failed fetch takes the SKIP path (the original reason text; the original
 * monolith probed ARGON2ID, each binary probes its own algorithm name). */
void argon2_test_gate(const struct argon2_variant *v)
{
    EVP_KDF *probe = EVP_KDF_fetch(NULL, v->algname, NULL);

    if (probe == NULL)
        test_skip("ARGON2 is unavailable");
    EVP_KDF_free(probe);
}

/* One Argon2 derive with a fresh context fetched by algorithm name
 * (ARGON2D/ARGON2I/ARGON2ID are separate fetchable algorithms; the
 * variant is not a settable parameter).  Returns 1 on success, 0 if
 * EVP_KDF_derive failed.  The SKIP decision has already been made. */
int argon2_derive(const char *algname, const unsigned char *pass,
                  size_t passlen, const unsigned char *salt,
                  size_t saltlen, const unsigned char *secret,
                  size_t secretlen, const unsigned char *ad,
                  size_t adlen, unsigned long lanes, unsigned long threads,
                  unsigned long memcost, unsigned long iter, long version,
                  unsigned char *out, size_t outlen)
{
    EVP_KDF *kdf = EVP_KDF_fetch(NULL, algname, NULL);
    EVP_KDF_CTX *ctx;
    OSSL_PARAM params[11];
    unsigned int lanes32 = (unsigned int)lanes;
    unsigned int threads32 = (unsigned int)threads;
    unsigned int memcost32 = (unsigned int)memcost;
    unsigned int iter32 = (unsigned int)iter;
    unsigned int version32 = (unsigned int)version;
    int ok, n = 0;
    size_t i;

    if (kdf == NULL)
        return 0;
    for (i = 0; i < outlen; i++)
        out[i] = 0;
    params[n++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_PASSWORD,
                                                    (void *)pass, passlen);
    params[n++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT,
                                                    (void *)salt, saltlen);
    if (secret != NULL)
        params[n++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SECRET,
                                                        (void *)secret,
                                                        secretlen);
    if (ad != NULL)
        params[n++] = OSSL_PARAM_construct_octet_string("ad", (void *)ad,
                                                        adlen);
    params[n++] = OSSL_PARAM_construct_uint32("lanes", &lanes32);
    params[n++] = OSSL_PARAM_construct_uint32("threads", &threads32);
    params[n++] = OSSL_PARAM_construct_uint32("memcost", &memcost32);
    params[n++] = OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ITER, &iter32);
    if (version >= 0)
        params[n++] = OSSL_PARAM_construct_uint32("version", &version32);
    params[n] = OSSL_PARAM_construct_end();
    ctx = EVP_KDF_CTX_new(kdf);
    if (ctx == NULL) {
        EVP_KDF_free(kdf);
        return 0;
    }
    ok = EVP_KDF_derive(ctx, out, outlen, params) > 0;
    EVP_KDF_CTX_free(ctx);
    EVP_KDF_free(kdf);
    return ok;
}

/* (1) The RFC 9106 section 5 tag for v's variant with t=3, m=32, p=4. */
void argon2_test_rfc_kat(const struct argon2_variant *v)
{
    unsigned char *pass = ALLOCATE_KEY(PASS_CAP);
    unsigned char *salt = ALLOCATE_SALT(SALT_CAP);
    unsigned char *secret = ALLOCATE_KEY(SECRET_CAP);
    unsigned char *ad = ALLOCATE_SALT(AD_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(OUT_CAP);
    size_t passlen, saltlen, secretlen, adlen;
    char msg[128];

    passlen = from_hex(argon2_rfc_pass_hex, pass, PASS_CAP);
    saltlen = from_hex(argon2_rfc_salt_hex, salt, SALT_CAP);
    secretlen = from_hex(argon2_rfc_secret_hex, secret, SECRET_CAP);
    adlen = from_hex(argon2_rfc_ad_hex, ad, AD_CAP);
    from_hex(v->rfctaghex, want, OUT_CAP);

    snprintf(msg, sizeof(msg), "%s RFC 9106 derive failed", v->algname);
    CHECK(argon2_derive(v->algname, pass, passlen, salt, saltlen, secret,
                        secretlen, ad, adlen, 4, 1, 32, 3, -1, out, 32),
          msg);
    snprintf(msg, sizeof(msg), "%s RFC 9106 section 5 tag does not match",
             v->algname);
    unpoison_all();
    CHECK(memcmp(out, want, 32) == 0, msg);
    repoison_all();

    free_poison_allocations();
}

/* (2) Without the secret and associated data the three variants still
 * produce three different tags. */
void argon2_test_variants_distinct(const struct argon2_variant *v)
{
    unsigned char *pass = ALLOCATE_KEY(PASS_CAP);
    unsigned char *salt = ALLOCATE_SALT(SALT_CAP);
    unsigned char *d = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *it = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *idt = ALLOCATE_OUTPUT(OUT_CAP);
    size_t passlen, saltlen;
    char msg[128];

    passlen = from_hex(argon2_rfc_pass_hex, pass, PASS_CAP);
    saltlen = from_hex(argon2_rfc_salt_hex, salt, SALT_CAP);
    CHECK(argon2_derive("ARGON2D", pass, passlen, salt, saltlen, NULL, 0,
                        NULL, 0, 4, 1, 32, 3, -1, d, 32),
          "plain Argon2d derive failed");
    CHECK(argon2_derive("ARGON2I", pass, passlen, salt, saltlen, NULL, 0,
                        NULL, 0, 4, 1, 32, 3, -1, it, 32),
          "plain Argon2i derive failed");
    CHECK(argon2_derive("ARGON2ID", pass, passlen, salt, saltlen, NULL, 0,
                        NULL, 0, 4, 1, 32, 3, -1, idt, 32),
          "plain Argon2id derive failed");
    snprintf(msg, sizeof(msg),
             "the three Argon2 variants produced identical output "
             "(%s is one of them)", v->algname);
    unpoison_all();
    CHECK(memcmp(d, it, 32) != 0 && memcmp(d, idt, 32) != 0 &&
          memcmp(it, idt, 32) != 0,
          msg);
    repoison_all();

    free_poison_allocations();
}

/* (4) Validation: a missing salt, lanes=0, memcost<8, iter=0, a short salt
 * and a too-short output must all fail. */
void argon2_test_validation(const struct argon2_variant *v)
{
    unsigned char *pass = ALLOCATE_KEY(PASS_CAP);
    unsigned char *salt = ALLOCATE_SALT(SALT_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    size_t passlen, saltlen;
    char msg[128];

    passlen = from_hex(argon2_rfc_pass_hex, pass, PASS_CAP);
    saltlen = from_hex(argon2_rfc_salt_hex, salt, SALT_CAP);

    snprintf(msg, sizeof(msg), "%s accepted a missing salt", v->algname);
    CHECK(!argon2_derive(v->algname, pass, passlen, salt, 0, NULL, 0, NULL, 0,
                         4, 1, 32, 3, -1, out, 32),
          msg);
    snprintf(msg, sizeof(msg), "%s accepted lanes=0", v->algname);
    CHECK(!argon2_derive(v->algname, pass, passlen, salt, saltlen, NULL, 0,
                         NULL, 0, 0, 1, 32, 3, -1, out, 32),
          msg);
    snprintf(msg, sizeof(msg), "%s accepted memcost=7", v->algname);
    CHECK(!argon2_derive(v->algname, pass, passlen, salt, saltlen, NULL, 0,
                         NULL, 0, 4, 1, 7, 3, -1, out, 32),
          msg);
    snprintf(msg, sizeof(msg), "%s accepted iter=0", v->algname);
    CHECK(!argon2_derive(v->algname, pass, passlen, salt, saltlen, NULL, 0,
                         NULL, 0, 4, 1, 32, 0, -1, out, 32),
          msg);
    snprintf(msg, sizeof(msg), "%s accepted a 2-byte salt", v->algname);
    CHECK(!argon2_derive(v->algname, pass, passlen, salt, 2, NULL, 0, NULL, 0,
                         4, 1, 32, 3, -1, out, 32),
          msg);
    snprintf(msg, sizeof(msg), "%s accepted a 3-byte output", v->algname);
    CHECK(!argon2_derive(v->algname, pass, passlen, salt, saltlen, NULL, 0,
                         NULL, 0, 4, 1, 32, 3, -1, out, 3),
          msg);

    free_poison_allocations();
}

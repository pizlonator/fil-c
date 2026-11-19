/*
 * test_rand_drbg_common.c -- shared implementation of the EVP_RAND family
 * tests (API in test_rand_drbg_common.h; one binary per RAND: CTR-DRBG,
 * HASH-DRBG, HMAC-DRBG and TEST-RAND).
 *
 * The EVP_RAND deterministic random bit generators that every OpenSSL 3.x
 * ships: CTR-DRBG (AES-256-CTR), HASH-DRBG (SHA2-256), HMAC-DRBG (SHA2-256,
 * built on the HMAC MAC), and TEST-RAND.
 *
 * Covered behavior, identical on OpenSSL 3.0 through 3.6:
 *
 *   - EVP_RAND_fetch for each DRBG and EVP_RAND_CTX_new on top of it.
 *   - Instantiation with algorithm parameters: "cipher" = AES-256-CTR for
 *     CTR-DRBG, "digest" = SHA2-256 for HASH-DRBG, and "mac" = HMAC plus
 *     "digest" = SHA2-256 in a single EVP_RAND_CTX_set_params call for
 *     HMAC-DRBG (splitting mac and digest across two calls fails there).
 *   - EVP_RAND_generate of 1, 32, and 1024 bytes; successive outputs
 *     differ (the shared rand_drbg_generate_flow() below; the per-RAND
 *     extras live in their own binaries).
 *   - Seeding knobs: setting OSSL_DRBG_PARAM_RESEED_REQUESTS to 1 makes
 *     every generate trigger a reseed first (SP 800-90Ar1: "the DRBG
 *     shall be reseeded after no more than reseed_requests generate
 *     calls"; see drbg.c's generate_counter >= reseed_interval check) and
 *     both generates still succeed with reseed_counter increasing; the
 *     per-generate prediction_resistance flag reseeds as well and its
 *     outputs differ.  (CTR-DRBG binary.)
 *   - State reporting via the "state" parameter (READY after
 *     instantiation, UNINITIALISED after uninstantiation).
 *   - TEST-RAND determinism: two fresh TEST-RAND contexts fed the same
 *     "test_entropy" bytes produce identical outputs (the entropy itself).
 *     (TEST-RAND binary.)
 *   - The personalization string is accepted at instantiation time.
 *     (HASH-DRBG binary.)
 *
 * Version-gated extra (TEST-RAND binary): OpenSSL 3.6 added TEST-RAND's
 * "generate" mode (a fixed-seed xorshift stream, OSSL_RAND_PARAM_GENERATE),
 * which produces a reproducible stream from a freshly instantiated context.
 * That section (and its embedded known-answer bytes, verified with a
 * scratch probe against the authoritative pizfix OpenSSL 3.6.5 per README
 * house rule 7) is compiled only against 3.6+ headers.
 *
 * The DRBGs and TEST-RAND are core OpenSSL 3 functionality: the family
 * binaries never SKIP, only FAIL.
 */

#include "test_rand_drbg_common.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>

/* Reads an unsigned integer context parameter ("state", "strength",
 * "reseed_counter", ...).  Returns 1 on success. */
int rand_get_uint_param(EVP_RAND_CTX *ctx, const char *name, unsigned int *out)
{
    OSSL_PARAM params[2];

    params[0] = OSSL_PARAM_construct_uint(name, out);
    params[1] = OSSL_PARAM_construct_end();
    return EVP_RAND_CTX_get_params(ctx, params);
}

/* Reads a size_t context parameter ("max_request").  Returns 1 on success. */
int rand_get_size_param(EVP_RAND_CTX *ctx, const char *name, size_t *out)
{
    OSSL_PARAM params[2];

    params[0] = OSSL_PARAM_construct_size_t(name, out);
    params[1] = OSSL_PARAM_construct_end();
    return EVP_RAND_CTX_get_params(ctx, params);
}

/* Sets a single UTF8 string parameter (a static copy is made, as the
 * OSSL_PARAM utf8 constructor requests). */
void rand_set_string_param(EVP_RAND_CTX *ctx, const char *name,
                           const char *value)
{
    OSSL_PARAM params[2];
    char buf[64];

    CHECK(strlen(value) < sizeof(buf), "string parameter too long for the helper");
    strncpy(buf, value, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    params[0] = OSSL_PARAM_construct_utf8_string(name, buf, 0);
    params[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_RAND_CTX_set_params(ctx, params) == 1,
          "setting a string RAND parameter failed");
}

/* Sets a single unsigned integer parameter. */
void rand_set_uint_param(EVP_RAND_CTX *ctx, const char *name,
                         unsigned int value)
{
    OSSL_PARAM params[2];

    params[0] = OSSL_PARAM_construct_uint(name, &value);
    params[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_RAND_CTX_set_params(ctx, params) == 1,
          "setting an integer RAND parameter failed");
}

/* The generate-and-compare flow the three DRBG binaries share (see the
 * header comment for the buffer contract). */
void rand_drbg_generate_flow(EVP_RAND_CTX *ctx, const char *name,
                             unsigned char *buf1, unsigned char *buf32,
                             unsigned char *buf32b, unsigned char *out1024)
{
    char msg[128];

    /* Generate sizes 1, 32, 1024. */
    snprintf(msg, sizeof(msg), "%s generate of 1 byte failed", name);
    CHECK(EVP_RAND_generate(ctx, buf1, 1, 256, 0, NULL, 0) == 1, msg);
    snprintf(msg, sizeof(msg), "%s generate of 32 bytes failed", name);
    CHECK(EVP_RAND_generate(ctx, buf32, 32, 256, 0, NULL, 0) == 1, msg);
    snprintf(msg, sizeof(msg), "%s generate of 1024 bytes failed", name);
    CHECK(EVP_RAND_generate(ctx, out1024, 1024, 256, 0, NULL, 0) == 1, msg);

    /* Successive outputs differ. */
    snprintf(msg, sizeof(msg), "%s second generate failed", name);
    CHECK(EVP_RAND_generate(ctx, buf32b, 32, 256, 0, NULL, 0) == 1, msg);
    unpoison_all();
    snprintf(msg, sizeof(msg), "%s generated the same 32 bytes twice", name);
    CHECK(memcmp(buf32, buf32b, 32) != 0, msg);
    repoison_all();
}
#include "test_common.h"
#include "test_rand_drbg_common.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

/*
 * test_rand_hmac_drbg -- HMAC-DRBG (EVP_RAND) built on the HMAC MAC with
 * the SHA2-256 digest, one binary per RAND (the shared param helpers, the
 * generate-and-compare flow and the family explanatory comment live in
 * test_rand_drbg_common.c).
 *
 * Covered behavior: the "mac" and "digest" parameters must be delivered in
 * a single EVP_RAND_CTX_set_params call (verified experimentally; splitting
 * them fails to instantiate on 3.6), the READY state after instantiation,
 * the 256-bit strength, the shared generate sizes (1, 32, 1024 bytes;
 * successive outputs differ), and uninstantiation.
 */

/* (3) HMAC-DRBG: the "mac" and "digest" parameters must be delivered in a
 * single EVP_RAND_CTX_set_params call (verified experimentally; splitting
 * them fails to instantiate on 3.6), then the usual generate flow. */
static void test_hmac_drbg(void)
{
    EVP_RAND *rand = EVP_RAND_fetch(NULL, "HMAC-DRBG", NULL);
    EVP_RAND_CTX *ctx;
    OSSL_PARAM params[3];
    char mac[] = "HMAC";
    char digest[] = "SHA2-256";
    unsigned char *out1024 = ALLOCATE_OUTPUT(1024);
    unsigned char *buf32 = ALLOCATE_OUTPUT(32);
    unsigned char *buf32b = ALLOCATE_OUTPUT(32);
    unsigned char *buf1 = ALLOCATE_OUTPUT(1);
    unsigned int state = 0xbeef, strength = 0xbeef;

    CHECK(rand != NULL, "EVP_RAND_fetch(\"HMAC-DRBG\") failed");
    ctx = EVP_RAND_CTX_new(rand, NULL);
    CHECK(ctx != NULL, "EVP_RAND_CTX_new failed");
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_DRBG_PARAM_MAC, mac, 0);
    params[1] = OSSL_PARAM_construct_utf8_string(OSSL_DRBG_PARAM_DIGEST, digest, 0);
    params[2] = OSSL_PARAM_construct_end();
    CHECK(EVP_RAND_CTX_set_params(ctx, params) == 1,
          "setting \"mac\" and \"digest\" together failed for HMAC-DRBG");

    CHECK(EVP_RAND_instantiate(ctx, 256, 0, NULL, 0, NULL) == 1,
          "EVP_RAND_instantiate failed for HMAC-DRBG");
    CHECK(rand_get_uint_param(ctx, OSSL_RAND_PARAM_STATE, &state) == 1 &&
          state == EVP_RAND_STATE_READY,
          "HMAC-DRBG state is not READY after instantiation");
    CHECK(rand_get_uint_param(ctx, OSSL_RAND_PARAM_STRENGTH, &strength) == 1 &&
          strength == 256,
          "HMAC-DRBG (SHA2-256) strength is not 256");

    /* Generate sizes 1, 32, 1024; successive outputs differ. */
    rand_drbg_generate_flow(ctx, "HMAC-DRBG", buf1, buf32, buf32b, out1024);

    CHECK(EVP_RAND_uninstantiate(ctx) == 1, "EVP_RAND_uninstantiate failed");
    EVP_RAND_CTX_free(ctx);
    EVP_RAND_free(rand);

    free_poison_allocations();
}

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    /* The DRBGs and TEST-RAND are core OpenSSL 3 functionality: never
     * SKIP, only FAIL. */
    test_hmac_drbg();

    test_report_pass();
    return 0;
}
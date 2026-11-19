#include "test_common.h"
#include "test_rand_drbg_common.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

/*
 * test_rand_hash_drbg -- HASH-DRBG (EVP_RAND) with an explicit SHA2-256
 * digest, with and without a personalization string, one binary per RAND
 * (the shared param helpers, the generate-and-compare flow and the family
 * explanatory comment live in test_rand_drbg_common.c).
 *
 * Covered behavior: instantiation with the "digest" parameter, the READY
 * state after instantiation, the shared generate sizes (1, 32, 1024 bytes;
 * successive outputs differ), uninstantiation, and a second instantiation
 * carrying a personalization string whose generate still succeeds.
 */

/* (2) HASH-DRBG with an explicit SHA2-256 digest, with and without a
 * personalization string. */
static void test_hash_drbg(void)
{
    EVP_RAND *rand = EVP_RAND_fetch(NULL, "HASH-DRBG", NULL);
    EVP_RAND_CTX *ctx;
    unsigned char *out1024 = ALLOCATE_OUTPUT(1024);
    unsigned char *buf32 = ALLOCATE_OUTPUT(32);
    unsigned char *buf32b = ALLOCATE_OUTPUT(32);
    unsigned char *buf1 = ALLOCATE_OUTPUT(1);
    unsigned int state = 0xbeef;

    CHECK(rand != NULL, "EVP_RAND_fetch(\"HASH-DRBG\") failed");
    ctx = EVP_RAND_CTX_new(rand, NULL);
    CHECK(ctx != NULL, "EVP_RAND_CTX_new failed");
    rand_set_string_param(ctx, OSSL_DRBG_PARAM_DIGEST, "SHA2-256");

    CHECK(EVP_RAND_instantiate(ctx, 256, 0, NULL, 0, NULL) == 1,
          "EVP_RAND_instantiate failed for HASH-DRBG");
    CHECK(rand_get_uint_param(ctx, OSSL_RAND_PARAM_STATE, &state) == 1 &&
          state == EVP_RAND_STATE_READY,
          "HASH-DRBG state is not READY after instantiation");

    /* Generate sizes 1, 32, 1024; successive outputs differ. */
    rand_drbg_generate_flow(ctx, "HASH-DRBG", buf1, buf32, buf32b, out1024);

    CHECK(EVP_RAND_uninstantiate(ctx) == 1, "EVP_RAND_uninstantiate failed");
    EVP_RAND_CTX_free(ctx);

    /* A personalization string is accepted at instantiation time. */
    ctx = EVP_RAND_CTX_new(rand, NULL);
    CHECK(ctx != NULL, "EVP_RAND_CTX_new failed");
    rand_set_string_param(ctx, OSSL_DRBG_PARAM_DIGEST, "SHA2-256");
    CHECK(EVP_RAND_instantiate(ctx, 256, 0,
                               (const unsigned char *)"pers-string", 11,
                               NULL) == 1,
          "HASH-DRBG instantiation with a personalization string failed");
    CHECK(EVP_RAND_generate(ctx, buf32, 32, 256, 0, NULL, 0) == 1,
          "HASH-DRBG generate after a personalized instantiation failed");
    EVP_RAND_CTX_free(ctx);
    EVP_RAND_free(rand);

    free_poison_allocations();
}

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    /* The DRBGs and TEST-RAND are core OpenSSL 3 functionality: never
     * SKIP, only FAIL. */
    test_hash_drbg();

    test_report_pass();
    return 0;
}
#include "test_common.h"
#include "test_rand_drbg_common.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

/*
 * test_rand_ctr_drbg -- CTR-DRBG (EVP_RAND) with an explicit AES-256-CTR
 * cipher, one binary per RAND (the shared param helpers, the
 * generate-and-compare flow and the family explanatory comment live in
 * test_rand_drbg_common.c).
 *
 * Covered behavior: instantiation with the "cipher" parameter, the READY /
 * UNINITIALISED state transitions, the 256-bit strength, max_request, the
 * shared generate sizes (1, 32, 1024 bytes; successive outputs differ), the
 * no-op zero-length generate, chunked requests above max_request, the
 * reseed_requests = 1 seeding knob (a reseed is forced before each
 * generate, and reseed_counter increases), prediction resistance on
 * generate, and uninstantiation.
 */

/* (1) CTR-DRBG with an explicit AES-256-CTR cipher: instantiation, state,
 * strength, max_request, generate sizes, no-op zero-length generate,
 * chunked large requests, reseed_requests, prediction resistance, and the
 * uninstantiate state transition. */
static void test_ctr_drbg(void)
{
    EVP_RAND *rand = EVP_RAND_fetch(NULL, "CTR-DRBG", NULL);
    EVP_RAND_CTX *ctx;
    unsigned char *out1024 = ALLOCATE_OUTPUT(1024);
    unsigned char *big = ALLOCATE_OUTPUT(65536 + 1);
    unsigned char *buf32 = ALLOCATE_OUTPUT(32);
    unsigned char *buf32b = ALLOCATE_OUTPUT(32);
    unsigned char *buf1 = ALLOCATE_OUTPUT(1);
    size_t max_request = 0;
    unsigned int state = 0xbeef, strength = 0xbeef, reseed_counter;

    CHECK(rand != NULL, "EVP_RAND_fetch(\"CTR-DRBG\") failed");
    CHECK(strcmp(EVP_RAND_get0_name(rand), "CTR-DRBG") == 0,
          "fetched CTR-DRBG has an unexpected name");
    ctx = EVP_RAND_CTX_new(rand, NULL);
    CHECK(ctx != NULL, "EVP_RAND_CTX_new failed");
    rand_set_string_param(ctx, OSSL_DRBG_PARAM_CIPHER, "AES-256-CTR");

    CHECK(EVP_RAND_instantiate(ctx, 256, 0, NULL, 0, NULL) == 1,
          "EVP_RAND_instantiate failed for CTR-DRBG");
    CHECK(rand_get_uint_param(ctx, OSSL_RAND_PARAM_STATE, &state) == 1 &&
          state == EVP_RAND_STATE_READY,
          "CTR-DRBG state is not READY after instantiation");
    CHECK(rand_get_uint_param(ctx, OSSL_RAND_PARAM_STRENGTH, &strength) == 1 &&
          strength == 256,
          "CTR-DRBG (AES-256-CTR) strength is not 256");
    CHECK(rand_get_size_param(ctx, OSSL_RAND_PARAM_MAX_REQUEST, &max_request) == 1 &&
          max_request > 0,
          "CTR-DRBG max_request could not be read");

    /* Generate sizes 1, 32, 1024; successive outputs differ. */
    rand_drbg_generate_flow(ctx, "CTR-DRBG", buf1, buf32, buf32b, out1024);

    /* A 0-length generate is a no-op. */
    unpoison_all();
    memcpy(buf32b, buf32, 32);
    CHECK(EVP_RAND_generate(ctx, buf32b, 0, 256, 0, NULL, 0) == 1,
          "CTR-DRBG 0-length generate failed");
    CHECK(memcmp(buf32b, buf32, 32) == 0,
          "CTR-DRBG 0-length generate was not a no-op");
    repoison_all();

    /* EVP_RAND_generate transparently chunks requests above max_request. */
    CHECK(EVP_RAND_generate(ctx, big, max_request + 1, 256, 0, NULL, 0) == 1,
          "CTR-DRBG generate above max_request failed");

    /* reseed_requests = 1: a reseed is forced before each generate, and
     * both generates still succeed while reseed_counter increases. */
    rand_set_uint_param(ctx, OSSL_DRBG_PARAM_RESEED_REQUESTS, 1);
    CHECK(rand_get_uint_param(ctx, OSSL_DRBG_PARAM_RESEED_COUNTER,
                              &reseed_counter) == 1,
          "reseed_counter could not be read before the reseeding generates");
    CHECK(EVP_RAND_generate(ctx, buf32, 32, 256, 0, NULL, 0) == 1,
          "CTR-DRBG generate #1 with reseed_requests=1 failed");
    CHECK(EVP_RAND_generate(ctx, buf32b, 32, 256, 0, NULL, 0) == 1,
          "CTR-DRBG generate #2 with reseed_requests=1 failed");
    {
        unsigned int reseed_counter2 = 0;
        CHECK(rand_get_uint_param(ctx, OSSL_DRBG_PARAM_RESEED_COUNTER,
                                  &reseed_counter2) == 1 &&
              reseed_counter2 > reseed_counter,
              "reseed_counter did not increase across reseeding generates");
    }

    /* Prediction resistance on generate: reseeds every time. */
    rand_set_uint_param(ctx, OSSL_DRBG_PARAM_RESEED_REQUESTS, 0);
    CHECK(EVP_RAND_generate(ctx, buf32, 32, 256, 1, NULL, 0) == 1,
          "CTR-DRBG prediction-resistant generate #1 failed");
    CHECK(EVP_RAND_generate(ctx, buf32b, 32, 256, 1, NULL, 0) == 1,
          "CTR-DRBG prediction-resistant generate #2 failed");
    unpoison_all();
    CHECK(memcmp(buf32, buf32b, 32) != 0,
          "CTR-DRBG prediction-resistant generates repeated output");
    repoison_all();

    CHECK(EVP_RAND_uninstantiate(ctx) == 1, "EVP_RAND_uninstantiate failed");
    CHECK(rand_get_uint_param(ctx, OSSL_RAND_PARAM_STATE, &state) == 1 &&
          state == EVP_RAND_STATE_UNINITIALISED,
          "CTR-DRBG state is not UNINITIALISED after uninstantiation");

    EVP_RAND_CTX_free(ctx);
    EVP_RAND_free(rand);

    free_poison_allocations();
}

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    /* The DRBGs and TEST-RAND are core OpenSSL 3 functionality: never
     * SKIP, only FAIL. */
    test_ctr_drbg();

    test_report_pass();
    return 0;
}
#include "test_common.h"
#include "test_rand_drbg_common.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

/*
 * test_rand_test_rand -- the TEST-RAND EVP_RAND, one binary per RAND (the
 * shared param helpers and the family explanatory comment live in
 * test_rand_drbg_common.c).
 *
 * Covered behavior:
 *
 *   - TEST-RAND determinism in entropy mode (portable to all of OpenSSL
 *     3.x): two fresh TEST-RAND contexts fed the same "test_entropy" bytes
 *     produce identical outputs, which are exactly the first 32 entropy
 *     bytes; a 0-length generate is a no-op that leaves the buffer
 *     untouched; the state reads UNINITIALISED after uninstantiation.
 *   - TEST-RAND "generate" mode (OSSL_RAND_PARAM_GENERATE, new in 3.6; the
 *     macro is absent from older headers so the name is spelled out): a
 *     fresh context deterministically produces the same xorshift stream,
 *     pinned by an embedded known answer (verified with a scratch probe
 *     against the authoritative pizfix OpenSSL 3.6.5 per README house rule
 *     7).  Compiled only against 3.6+ headers.
 */

/* The fixed TEST-RAND "test_entropy" bytes: 0x01,0x04,0x07,... (i*3+1).
 * In entropy mode TEST-RAND hands these out verbatim, so two fresh
 * contexts must produce identical output. */
static const char TEST_ENTROPY_HEX[] =
    "0104070a0d101316191c1f2225282b2e3134373a3d404346494c4f5255585b5e"
    "6164676a6d707376797c7f8285888b8e9194979a9da0a3a6a9acafb2b5b8bbbe";

#define TEST_ENTROPY_LEN 64

/* (4) TEST-RAND determinism in entropy mode (portable to all of OpenSSL
 * 3.x): two fresh contexts with the same test_entropy produce identical
 * 32-byte outputs, which are exactly the first 32 entropy bytes. */
static void test_test_rand_entropy_mode(void)
{
    EVP_RAND *rand = EVP_RAND_fetch(NULL, "TEST-RAND", NULL);
    EVP_RAND_CTX *c1, *c2;
    unsigned char *entropy = ALLOCATE_SALT(TEST_ENTROPY_LEN);
    unsigned char *buf1 = ALLOCATE_OUTPUT(32);
    unsigned char *buf2 = ALLOCATE_OUTPUT(32);
    OSSL_PARAM params[2];
    unsigned int state = 0xbeef;

    CHECK(rand != NULL, "EVP_RAND_fetch(\"TEST-RAND\") failed");
    from_hex(TEST_ENTROPY_HEX, entropy, TEST_ENTROPY_LEN);

    c1 = EVP_RAND_CTX_new(rand, NULL);
    c2 = EVP_RAND_CTX_new(rand, NULL);
    CHECK(c1 != NULL && c2 != NULL, "EVP_RAND_CTX_new failed");

    params[0] = OSSL_PARAM_construct_octet_string(OSSL_RAND_PARAM_TEST_ENTROPY,
                                                  entropy, TEST_ENTROPY_LEN);
    params[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_RAND_CTX_set_params(c1, params) == 1,
          "setting \"test_entropy\" failed (context 1)");
    params[0] = OSSL_PARAM_construct_octet_string(OSSL_RAND_PARAM_TEST_ENTROPY,
                                                  entropy, TEST_ENTROPY_LEN);
    params[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_RAND_CTX_set_params(c2, params) == 1,
          "setting \"test_entropy\" failed (context 2)");

    CHECK(EVP_RAND_instantiate(c1, 0, 0, NULL, 0, NULL) == 1,
          "TEST-RAND instantiation failed (context 1)");
    CHECK(EVP_RAND_instantiate(c2, 0, 0, NULL, 0, NULL) == 1,
          "TEST-RAND instantiation failed (context 2)");

    CHECK(EVP_RAND_generate(c1, buf1, 32, 0, 0, NULL, 0) == 1,
          "TEST-RAND generate failed (context 1)");
    CHECK(EVP_RAND_generate(c2, buf2, 32, 0, 0, NULL, 0) == 1,
          "TEST-RAND generate failed (context 2)");
    unpoison_all();
    CHECK(memcmp(buf1, buf2, 32) == 0,
          "two fresh TEST-RAND contexts produced different output");
    repoison_all();
    unpoison_all();
    CHECK(memcmp(buf1, entropy, 32) == 0,
          "TEST-RAND entropy-mode output is not the test entropy");
    repoison_all();

    /* A 0-length generate is a no-op. */
    unpoison_all();
    memcpy(buf2, buf1, 32);
    CHECK(EVP_RAND_generate(c1, buf2, 0, 0, 0, NULL, 0) == 1,
          "TEST-RAND 0-length generate failed");
    CHECK(memcmp(buf2, buf1, 32) == 0,
          "TEST-RAND 0-length generate was not a no-op");
    repoison_all();

    CHECK(EVP_RAND_uninstantiate(c1) == 1, "EVP_RAND_uninstantiate failed");
    CHECK(rand_get_uint_param(c1, OSSL_RAND_PARAM_STATE, &state) == 1 &&
          state == EVP_RAND_STATE_UNINITIALISED,
          "TEST-RAND state is not UNINITIALISED after uninstantiation");

    EVP_RAND_CTX_free(c1);
    EVP_RAND_CTX_free(c2);
    EVP_RAND_free(rand);

    free_poison_allocations();
}

/* (5) TEST-RAND "generate" mode (OSSL_RAND_PARAM_GENERATE, new in 3.6; the
 * macro is absent from older headers so the name is spelled out): a fresh
 * context deterministically produces the same xorshift stream.  Compiled
 * only against 3.6+ headers. */
#if OPENSSL_VERSION_NUMBER >= 0x30600000L
static void test_test_rand_generate_mode(void)
{
    /* The 32 bytes a freshly instantiated TEST-RAND "generate" mode context
     * produces (fixed xorshift seed 221953166; verified against the
     * authoritative pizfix OpenSSL 3.6.5). */
    static const char *const TEST_RAND_GEN_KAT_HEX =
        "bbf02512c9bc57c2564a35bfde0835606d8cce7e8c11d190488507517c16be84";
    EVP_RAND *rand = EVP_RAND_fetch(NULL, "TEST-RAND", NULL);
    EVP_RAND_CTX *c1, *c2;
    unsigned char *want = ALLOCATE_OUTPUT(32);
    unsigned char *buf1 = ALLOCATE_OUTPUT(32);
    unsigned char *buf2 = ALLOCATE_OUTPUT(32);

    CHECK(rand != NULL, "EVP_RAND_fetch(\"TEST-RAND\") failed");
    from_hex(TEST_RAND_GEN_KAT_HEX, want, 32);

    c1 = EVP_RAND_CTX_new(rand, NULL);
    c2 = EVP_RAND_CTX_new(rand, NULL);
    CHECK(c1 != NULL && c2 != NULL, "EVP_RAND_CTX_new failed");
    rand_set_uint_param(c1, "generate", 1);
    rand_set_uint_param(c2, "generate", 1);
    CHECK(EVP_RAND_instantiate(c1, 0, 0, NULL, 0, NULL) == 1,
          "TEST-RAND instantiation failed (generate mode, context 1)");
    CHECK(EVP_RAND_instantiate(c2, 0, 0, NULL, 0, NULL) == 1,
          "TEST-RAND instantiation failed (generate mode, context 2)");
    CHECK(EVP_RAND_generate(c1, buf1, 32, 0, 0, NULL, 0) == 1,
          "TEST-RAND generate-mode generate failed (context 1)");
    CHECK(EVP_RAND_generate(c2, buf2, 32, 0, 0, NULL, 0) == 1,
          "TEST-RAND generate-mode generate failed (context 2)");
    unpoison_all();
    CHECK(memcmp(buf1, buf2, 32) == 0,
          "two fresh TEST-RAND generate-mode contexts produced different output");
    repoison_all();
    unpoison_all();
    CHECK(memcmp(buf1, want, 32) == 0,
          "TEST-RAND generate-mode output does not match the KAT");
    repoison_all();

    EVP_RAND_CTX_free(c1);
    EVP_RAND_CTX_free(c2);
    EVP_RAND_free(rand);

    free_poison_allocations();
}
#endif /* OPENSSL_VERSION_NUMBER >= 0x30600000L */

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    /* The DRBGs and TEST-RAND are core OpenSSL 3 functionality: never
     * SKIP, only FAIL. */
    test_test_rand_entropy_mode();
#if OPENSSL_VERSION_NUMBER >= 0x30600000L
    test_test_rand_generate_mode();
#endif

    test_report_pass();
    return 0;
}
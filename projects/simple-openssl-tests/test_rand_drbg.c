#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

/*
 * test_rand_drbg -- the EVP_RAND deterministic random bit generators that
 * every OpenSSL 3.x ships: CTR-DRBG (AES-256-CTR), HASH-DRBG (SHA2-256),
 * HMAC-DRBG (SHA2-256, built on the HMAC MAC), and TEST-RAND.
 *
 * Covered behavior, identical on OpenSSL 3.0 through 3.6:
 *
 *   - EVP_RAND_fetch for each DRBG and EVP_RAND_CTX_new on top of it.
 *   - Instantiation with algorithm parameters: "cipher" = AES-256-CTR for
 *     CTR-DRBG, "digest" = SHA2-256 for HASH-DRBG, and "mac" = HMAC plus
 *     "digest" = SHA2-256 in a single EVP_RAND_CTX_set_params call for
 *     HMAC-DRBG (splitting mac and digest across two calls fails there).
 *   - EVP_RAND_generate of 1, 32, and 1024 bytes; successive outputs
 *     differ; a 0-length generate is a no-op that leaves the buffer
 *     untouched; requests above max_request are transparently chunked by
 *     EVP_RAND_generate.
 *   - Seeding knobs: setting OSSL_DRBG_PARAM_RESEED_REQUESTS to 1 makes
 *     every generate trigger a reseed first (SP 800-90Ar1: "the DRBG
 *     shall be reseeded after no more than reseed_requests generate
 *     calls"; see drbg.c's generate_counter >= reseed_interval check) and
 *     both generates still succeed with reseed_counter increasing; the
 *     per-generate prediction_resistance flag reseeds as well and its
 *     outputs differ.
 *   - State reporting via the "state" parameter (READY after
 *     instantiation, UNINITIALISED after uninstantiation).
 *   - TEST-RAND determinism: two fresh TEST-RAND contexts fed the same
 *     "test_entropy" bytes produce identical outputs (the entropy itself).
 *   - The personalization string is accepted at instantiation time.
 *
 * Version-gated extra: OpenSSL 3.6 added TEST-RAND's "generate" mode (a
 * fixed-seed xorshift stream, OSSL_RAND_PARAM_GENERATE), which produces a
 * reproducible stream from a freshly instantiated context.  That section
 * (and its embedded known-answer bytes, verified with a scratch probe
 * against the authoritative pizfix OpenSSL 3.6.5 per README house rule 7)
 * is compiled only against 3.6+ headers.
 */

/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */
static void *ALLOCATE_SALT(size_t n)   { return poison_alloc(POISON_SALT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }

static unsigned g_checks = 0;

static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_rand_drbg: %s\n", msg);
    exit(1);
}

#define CHECK(cond, msg)                      \
    do {                                      \
        if (!(cond))                          \
            test_failed(msg);                 \
        g_checks++;                           \
    } while (0)

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Copies a hex constant into a buffer allocated by an ALLOCATE_* function
 * (house rule: constants live in the source as hex strings, but are copied
 * into labeled working buffers before use).  Returns the byte length. */
static size_t from_hex(const char *hex, unsigned char *dst, size_t dstcap)
{
    size_t n = strlen(hex) / 2;
    size_t i;
    if (strlen(hex) % 2 != 0)
        test_failed("odd-length hex constant");
    if (n > dstcap)
        test_failed("hex constant larger than its destination buffer");
    for (i = 0; i < n; i++) {
        int hi = hexval(hex[2 * i]);
        int lo = hexval(hex[2 * i + 1]);
        if (hi < 0 || lo < 0)
            test_failed("bad hex digit in constant");
        dst[i] = (unsigned char)((hi << 4) | lo);
    }
    return n;
}

/* Reads an unsigned integer context parameter ("state", "strength",
 * "reseed_counter", ...).  Returns 1 on success. */
static int get_uint_param(EVP_RAND_CTX *ctx, const char *name, unsigned int *out)
{
    OSSL_PARAM params[2];

    params[0] = OSSL_PARAM_construct_uint(name, out);
    params[1] = OSSL_PARAM_construct_end();
    return EVP_RAND_CTX_get_params(ctx, params);
}

/* Reads a size_t context parameter ("max_request").  Returns 1 on success. */
static int get_size_param(EVP_RAND_CTX *ctx, const char *name, size_t *out)
{
    OSSL_PARAM params[2];

    params[0] = OSSL_PARAM_construct_size_t(name, out);
    params[1] = OSSL_PARAM_construct_end();
    return EVP_RAND_CTX_get_params(ctx, params);
}

/* Sets a single UTF8 string parameter (a static copy is made, as the
 * OSSL_PARAM utf8 constructor requests). */
static void set_string_param(EVP_RAND_CTX *ctx, const char *name, const char *value)
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
static void set_uint_param(EVP_RAND_CTX *ctx, const char *name, unsigned int value)
{
    OSSL_PARAM params[2];

    params[0] = OSSL_PARAM_construct_uint(name, &value);
    params[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_RAND_CTX_set_params(ctx, params) == 1,
          "setting an integer RAND parameter failed");
}

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
    unsigned char buf32[32], buf32b[32], buf1[1];
    size_t max_request = 0;
    unsigned int state = 0xbeef, strength = 0xbeef, reseed_counter;

    CHECK(rand != NULL, "EVP_RAND_fetch(\"CTR-DRBG\") failed");
    CHECK(strcmp(EVP_RAND_get0_name(rand), "CTR-DRBG") == 0,
          "fetched CTR-DRBG has an unexpected name");
    ctx = EVP_RAND_CTX_new(rand, NULL);
    CHECK(ctx != NULL, "EVP_RAND_CTX_new failed");
    set_string_param(ctx, OSSL_DRBG_PARAM_CIPHER, "AES-256-CTR");

    CHECK(EVP_RAND_instantiate(ctx, 256, 0, NULL, 0, NULL) == 1,
          "EVP_RAND_instantiate failed for CTR-DRBG");
    CHECK(get_uint_param(ctx, OSSL_RAND_PARAM_STATE, &state) == 1 &&
          state == EVP_RAND_STATE_READY,
          "CTR-DRBG state is not READY after instantiation");
    CHECK(get_uint_param(ctx, OSSL_RAND_PARAM_STRENGTH, &strength) == 1 &&
          strength == 256,
          "CTR-DRBG (AES-256-CTR) strength is not 256");
    CHECK(get_size_param(ctx, OSSL_RAND_PARAM_MAX_REQUEST, &max_request) == 1 &&
          max_request > 0,
          "CTR-DRBG max_request could not be read");

    /* Generate sizes 1, 32, 1024. */
    CHECK(EVP_RAND_generate(ctx, buf1, 1, 256, 0, NULL, 0) == 1,
          "CTR-DRBG generate of 1 byte failed");
    CHECK(EVP_RAND_generate(ctx, buf32, 32, 256, 0, NULL, 0) == 1,
          "CTR-DRBG generate of 32 bytes failed");
    CHECK(EVP_RAND_generate(ctx, out1024, 1024, 256, 0, NULL, 0) == 1,
          "CTR-DRBG generate of 1024 bytes failed");

    /* Successive outputs differ. */
    CHECK(EVP_RAND_generate(ctx, buf32b, 32, 256, 0, NULL, 0) == 1,
          "CTR-DRBG second generate failed");
    CHECK(memcmp(buf32, buf32b, 32) != 0,
          "CTR-DRBG generated the same 32 bytes twice");

    /* A 0-length generate is a no-op. */
    memcpy(buf32b, buf32, 32);
    CHECK(EVP_RAND_generate(ctx, buf32b, 0, 256, 0, NULL, 0) == 1,
          "CTR-DRBG 0-length generate failed");
    CHECK(memcmp(buf32b, buf32, 32) == 0,
          "CTR-DRBG 0-length generate was not a no-op");

    /* EVP_RAND_generate transparently chunks requests above max_request. */
    CHECK(EVP_RAND_generate(ctx, big, max_request + 1, 256, 0, NULL, 0) == 1,
          "CTR-DRBG generate above max_request failed");

    /* reseed_requests = 1: a reseed is forced before each generate, and
     * both generates still succeed while reseed_counter increases. */
    set_uint_param(ctx, OSSL_DRBG_PARAM_RESEED_REQUESTS, 1);
    CHECK(get_uint_param(ctx, OSSL_DRBG_PARAM_RESEED_COUNTER,
                         &reseed_counter) == 1,
          "reseed_counter could not be read before the reseeding generates");
    CHECK(EVP_RAND_generate(ctx, buf32, 32, 256, 0, NULL, 0) == 1,
          "CTR-DRBG generate #1 with reseed_requests=1 failed");
    CHECK(EVP_RAND_generate(ctx, buf32b, 32, 256, 0, NULL, 0) == 1,
          "CTR-DRBG generate #2 with reseed_requests=1 failed");
    {
        unsigned int reseed_counter2 = 0;
        CHECK(get_uint_param(ctx, OSSL_DRBG_PARAM_RESEED_COUNTER,
                             &reseed_counter2) == 1 &&
              reseed_counter2 > reseed_counter,
              "reseed_counter did not increase across reseeding generates");
    }

    /* Prediction resistance on generate: reseeds every time. */
    set_uint_param(ctx, OSSL_DRBG_PARAM_RESEED_REQUESTS, 0);
    CHECK(EVP_RAND_generate(ctx, buf32, 32, 256, 1, NULL, 0) == 1,
          "CTR-DRBG prediction-resistant generate #1 failed");
    CHECK(EVP_RAND_generate(ctx, buf32b, 32, 256, 1, NULL, 0) == 1,
          "CTR-DRBG prediction-resistant generate #2 failed");
    CHECK(memcmp(buf32, buf32b, 32) != 0,
          "CTR-DRBG prediction-resistant generates repeated output");

    CHECK(EVP_RAND_uninstantiate(ctx) == 1, "EVP_RAND_uninstantiate failed");
    CHECK(get_uint_param(ctx, OSSL_RAND_PARAM_STATE, &state) == 1 &&
          state == EVP_RAND_STATE_UNINITIALISED,
          "CTR-DRBG state is not UNINITIALISED after uninstantiation");

    EVP_RAND_CTX_free(ctx);
    EVP_RAND_free(rand);
}

/* (2) HASH-DRBG with an explicit SHA2-256 digest, with and without a
 * personalization string. */
static void test_hash_drbg(void)
{
    EVP_RAND *rand = EVP_RAND_fetch(NULL, "HASH-DRBG", NULL);
    EVP_RAND_CTX *ctx;
    unsigned char *out1024 = ALLOCATE_OUTPUT(1024);
    unsigned char buf32[32], buf32b[32], buf1[1];
    unsigned int state = 0xbeef;

    CHECK(rand != NULL, "EVP_RAND_fetch(\"HASH-DRBG\") failed");
    ctx = EVP_RAND_CTX_new(rand, NULL);
    CHECK(ctx != NULL, "EVP_RAND_CTX_new failed");
    set_string_param(ctx, OSSL_DRBG_PARAM_DIGEST, "SHA2-256");

    CHECK(EVP_RAND_instantiate(ctx, 256, 0, NULL, 0, NULL) == 1,
          "EVP_RAND_instantiate failed for HASH-DRBG");
    CHECK(get_uint_param(ctx, OSSL_RAND_PARAM_STATE, &state) == 1 &&
          state == EVP_RAND_STATE_READY,
          "HASH-DRBG state is not READY after instantiation");

    CHECK(EVP_RAND_generate(ctx, buf1, 1, 256, 0, NULL, 0) == 1,
          "HASH-DRBG generate of 1 byte failed");
    CHECK(EVP_RAND_generate(ctx, buf32, 32, 256, 0, NULL, 0) == 1,
          "HASH-DRBG generate of 32 bytes failed");
    CHECK(EVP_RAND_generate(ctx, out1024, 1024, 256, 0, NULL, 0) == 1,
          "HASH-DRBG generate of 1024 bytes failed");
    CHECK(EVP_RAND_generate(ctx, buf32b, 32, 256, 0, NULL, 0) == 1,
          "HASH-DRBG second generate failed");
    CHECK(memcmp(buf32, buf32b, 32) != 0,
          "HASH-DRBG generated the same 32 bytes twice");

    CHECK(EVP_RAND_uninstantiate(ctx) == 1, "EVP_RAND_uninstantiate failed");
    EVP_RAND_CTX_free(ctx);

    /* A personalization string is accepted at instantiation time. */
    ctx = EVP_RAND_CTX_new(rand, NULL);
    CHECK(ctx != NULL, "EVP_RAND_CTX_new failed");
    set_string_param(ctx, OSSL_DRBG_PARAM_DIGEST, "SHA2-256");
    CHECK(EVP_RAND_instantiate(ctx, 256, 0,
                               (const unsigned char *)"pers-string", 11,
                               NULL) == 1,
          "HASH-DRBG instantiation with a personalization string failed");
    CHECK(EVP_RAND_generate(ctx, buf32, 32, 256, 0, NULL, 0) == 1,
          "HASH-DRBG generate after a personalized instantiation failed");
    EVP_RAND_CTX_free(ctx);
    EVP_RAND_free(rand);
}

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
    unsigned char buf32[32], buf32b[32], buf1[1];
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
    CHECK(get_uint_param(ctx, OSSL_RAND_PARAM_STATE, &state) == 1 &&
          state == EVP_RAND_STATE_READY,
          "HMAC-DRBG state is not READY after instantiation");
    CHECK(get_uint_param(ctx, OSSL_RAND_PARAM_STRENGTH, &strength) == 1 &&
          strength == 256,
          "HMAC-DRBG (SHA2-256) strength is not 256");

    CHECK(EVP_RAND_generate(ctx, buf1, 1, 256, 0, NULL, 0) == 1,
          "HMAC-DRBG generate of 1 byte failed");
    CHECK(EVP_RAND_generate(ctx, buf32, 32, 256, 0, NULL, 0) == 1,
          "HMAC-DRBG generate of 32 bytes failed");
    CHECK(EVP_RAND_generate(ctx, out1024, 1024, 256, 0, NULL, 0) == 1,
          "HMAC-DRBG generate of 1024 bytes failed");
    CHECK(EVP_RAND_generate(ctx, buf32b, 32, 256, 0, NULL, 0) == 1,
          "HMAC-DRBG second generate failed");
    CHECK(memcmp(buf32, buf32b, 32) != 0,
          "HMAC-DRBG generated the same 32 bytes twice");

    CHECK(EVP_RAND_uninstantiate(ctx) == 1, "EVP_RAND_uninstantiate failed");
    EVP_RAND_CTX_free(ctx);
    EVP_RAND_free(rand);
}

/* The fixed TEST-RAND "test_entropy" bytes: 0x01,0x04,0x07,... (i*3+1).
 * In entropy mode TEST-RAND hands these out verbatim, so two fresh
 * contexts must produce identical output. */
static const char *const TEST_ENTROPY_HEX =
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
    CHECK(memcmp(buf1, buf2, 32) == 0,
          "two fresh TEST-RAND contexts produced different output");
    CHECK(memcmp(buf1, entropy, 32) == 0,
          "TEST-RAND entropy-mode output is not the test entropy");

    /* A 0-length generate is a no-op. */
    memcpy(buf2, buf1, 32);
    CHECK(EVP_RAND_generate(c1, buf2, 0, 0, 0, NULL, 0) == 1,
          "TEST-RAND 0-length generate failed");
    CHECK(memcmp(buf2, buf1, 32) == 0,
          "TEST-RAND 0-length generate was not a no-op");

    CHECK(EVP_RAND_uninstantiate(c1) == 1, "EVP_RAND_uninstantiate failed");
    CHECK(get_uint_param(c1, OSSL_RAND_PARAM_STATE, &state) == 1 &&
          state == EVP_RAND_STATE_UNINITIALISED,
          "TEST-RAND state is not UNINITIALISED after uninstantiation");

    EVP_RAND_CTX_free(c1);
    EVP_RAND_CTX_free(c2);
    EVP_RAND_free(rand);
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
    set_uint_param(c1, "generate", 1);
    set_uint_param(c2, "generate", 1);
    CHECK(EVP_RAND_instantiate(c1, 0, 0, NULL, 0, NULL) == 1,
          "TEST-RAND instantiation failed (generate mode, context 1)");
    CHECK(EVP_RAND_instantiate(c2, 0, 0, NULL, 0, NULL) == 1,
          "TEST-RAND instantiation failed (generate mode, context 2)");
    CHECK(EVP_RAND_generate(c1, buf1, 32, 0, 0, NULL, 0) == 1,
          "TEST-RAND generate-mode generate failed (context 1)");
    CHECK(EVP_RAND_generate(c2, buf2, 32, 0, 0, NULL, 0) == 1,
          "TEST-RAND generate-mode generate failed (context 2)");
    CHECK(memcmp(buf1, buf2, 32) == 0,
          "two fresh TEST-RAND generate-mode contexts produced different output");
    CHECK(memcmp(buf1, want, 32) == 0,
          "TEST-RAND generate-mode output does not match the KAT");

    EVP_RAND_CTX_free(c1);
    EVP_RAND_CTX_free(c2);
    EVP_RAND_free(rand);
}
#endif /* OPENSSL_VERSION_NUMBER >= 0x30600000L */


int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    /* The DRBGs and TEST-RAND are core OpenSSL 3 functionality: never
     * SKIP, only FAIL. */

    test_ctr_drbg();
    test_hash_drbg();
    test_hmac_drbg();
    test_test_rand_entropy_mode();
#if OPENSSL_VERSION_NUMBER >= 0x30600000L
    test_test_rand_generate_mode();
#endif

    printf("test_rand_drbg: all %u checks passed\n", g_checks);
    return 0;
}

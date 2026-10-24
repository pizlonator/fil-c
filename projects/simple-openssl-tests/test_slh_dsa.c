#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>

/*
 * test_slh_dsa -- SLH-DSA (FIPS 205) tests for the "fast" (f) parameter
 * sets SLH-DSA-SHA2-128f, SLH-DSA-SHAKE-128f, and SLH-DSA-SHA2-192f:
 * deterministic seed keygen, public-key known answers, one-shot
 * signing/verification of small and large (256 KiB) messages, signature
 * sizes, tamper rejection, and context-string binding.
 *
 * Determinism hooks used (all public OpenSSL 3 API):
 *
 *   - Key generation accepts the key-derivation seed via the keymgmt
 *     "seed" parameter (OSSL_PKEY_PARAM_SLH_DSA_SEED, new in 3.5).  The
 *     seed is the FIPS 205 (SK.seed, SK.prf, PUB.seed) triple, 3n bytes:
 *     48 bytes for the 128-bit sets (n = 16), 72 bytes for the 192-bit set
 *     (n = 24).  Identical seeds derive identical key pairs.
 *   - SLH-DSA signing is deterministic given (key, message, context
 *     string) when the integer "deterministic" parameter
 *     (OSSL_SIGNATURE_PARAM_DETERMINISTIC, new in 3.5) is set; by default
 *     OpenSSL uses randomized (hedged) signing with per-signature
 *     randomness.
 *
 * Spec adaptation: FIPS 205 has no streaming signing interface and this
 * OpenSSL does not provide one either -- the SLH-DSA provider implements
 * only one-shot OSSL_FUNC_SIGNATURE_SIGN/VERIFY (there are no
 * SIGN_MESSAGE_UPDATE/DIGEST_SIGN_UPDATE hooks), so a large message is
 * covered with a one-shot sign/verify of ~256 KiB instead of chunked
 * EVP_DigestSignUpdate updates.
 *
 * Known-answer constants below were derived and cross-verified with a
 * scratch probe compiled against the authoritative pizfix OpenSSL 3.6.5
 * (build/bin/filcc), per README house rule 7:
 *
 *   - SEED_*:    the key-derivation seeds (0x00,0x01,... for 48 or 72
 *                bytes depending on the set).
 *   - KAT_PUB_*: the raw public keys exported from those seeds (32 / 32 /
 *                48 bytes, the FIPS 205 pk sizes).
 *
 * The signature sizes (17088 / 17088 / 35664 bytes, the FIPS 205 signature
 * sizes for the f sets) are asserted via the one-shot NULL-buffer
 * EVP_DigestSign call, and run-to-run signing determinism is asserted per
 * parameter set.
 */

/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */
static void *ALLOCATE_KEY(size_t n)    { return poison_alloc(POISON_KEY, n); }
static void *ALLOCATE_INPUT(size_t n)  { return poison_alloc(POISON_INPUT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }
static void *ALLOCATE_OTHER(size_t n)  { return poison_alloc(POISON_OTHER, n); }

static unsigned g_checks = 0;

static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_slh_dsa: %s\n", msg);
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

/* Length-and-content comparison of two working buffers. */
static void expect_equal(const char *what, const unsigned char *got, size_t gotlen,
                         const unsigned char *want, size_t wantlen)
{
    CHECK(gotlen == wantlen, what);
    if (wantlen > 0)
        CHECK(memcmp(got, want, wantlen) == 0, what);
}

/* The FIPS 205 key-derivation seeds: 0x00,0x01,...,0x2f (48 bytes, 3n for
 * n = 16) shared by the two 128-bit sets, and 0x00,0x01,...,0x47 (72
 * bytes, 3n for n = 24) for the 192-bit set. */
static const char *const SLH_SEED_128_HEX =
    "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
    "202122232425262728292a2b2c2d2e2f";
static const char *const SLH_SEED_192_HEX =
    "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
    "202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f"
    "4041424344454647";


static const char *const KAT_PUB_SHA2_128F_HEX =
    "202122232425262728292a2b2c2d2e2f3b56e816847f000386aeec2e2bb9e1b5";

static const char *const KAT_PUB_SHAKE_128F_HEX =
    "202122232425262728292a2b2c2d2e2fa90e4715b9a925c332801767fd786371";

static const char *const KAT_PUB_SHA2_192F_HEX =
    "303132333435363738393a3b3c3d3e3f40414243444546479236ccebbb3a90ac"
    "2452dd89de49dab1340ec02419a2870e";

/* The three FIPS 205 parameter sets.  The 128-bit sets share the same
 * 48-byte (3n, n = 16) seed; the 192-bit set uses 72 bytes (3n, n = 24). */
static const struct slh_dsa_set {
    const char *name;
    size_t seed_len;    /* (SK.seed, SK.prf, PUB.seed): 3n = 48 / 48 / 72 */
    size_t pub_len;     /* pk:  32 / 32 / 48 */
    size_t sig_len;     /* sig: 17088 / 17088 / 35664 */
    const char *seedhex;
    const char *pubhex;
} slh_dsa_sets[] = {
    { "SLH-DSA-SHA2-128f",  48, 32, 17088,
      SLH_SEED_128_HEX, KAT_PUB_SHA2_128F_HEX },
    { "SLH-DSA-SHAKE-128f", 48, 32, 17088,
      SLH_SEED_128_HEX, KAT_PUB_SHAKE_128F_HEX },
    { "SLH-DSA-SHA2-192f",  72, 48, 35664,
      SLH_SEED_192_HEX, KAT_PUB_SHA2_192F_HEX },
};

/* The fixed 16-byte KAT message: 00 01 ... 0f. */
static const char *const MSG16_HEX =
    "000102030405060708090a0b0c0d0e0f";

/* A non-empty signing/verification context string (FIPS 205 ctx). */
static const char CTXSTR[] = "FilC-context";

#define SLH_MAX_PUB 48
#define SLH_MAX_SIG 35664
#define SLH_MSG_LEN 16

/* Size of the large message: 256 KiB, signed one-shot (see the spec
 * adaptation note in the file comment). */
#define BIG_LEN (256u * 1024u)

/* The "deterministic" signature parameter (OSSL_SIGNATURE_PARAM_DETERMINISTIC;
 * spelled as a literal because the macro is new in 3.5). */
static void set_deterministic(EVP_PKEY_CTX *pctx)
{
    OSSL_PARAM params[2];
    int one = 1;

    params[0] = OSSL_PARAM_construct_int("deterministic", &one);
    params[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_PKEY_CTX_set_params(pctx, params) == 1,
          "setting the \"deterministic\" signature parameter failed");
}

/* True when this OpenSSL provides the named algorithm (its keymgmt can be
 * instantiated and key generation initialized). */
static int pkey_alg_available(const char *name)
{
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(NULL, name, NULL);
    int ok = ctx != NULL && EVP_PKEY_keygen_init(ctx) == 1;
    EVP_PKEY_CTX_free(ctx);
    return ok;
}

/* Deterministic key generation from the FIPS 205 seed via the keymgmt
 * "seed" parameter (OSSL_PKEY_PARAM_SLH_DSA_SEED; spelled as a literal
 * because the macro is new in 3.5). */
static EVP_PKEY *keygen_from_seed(const struct slh_dsa_set *set,
                                  unsigned char *seedbuf)
{
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(NULL, set->name, NULL);
    EVP_PKEY *pkey = NULL;
    OSSL_PARAM params[2];

    CHECK(ctx != NULL, "EVP_PKEY_CTX_new_from_name failed for an available SLH-DSA");
    CHECK(EVP_PKEY_keygen_init(ctx) == 1, "EVP_PKEY_keygen_init failed");
    from_hex(set->seedhex, seedbuf, set->seed_len);
    params[0] = OSSL_PARAM_construct_octet_string("seed", seedbuf, set->seed_len);
    params[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_PKEY_CTX_set_params(ctx, params) == 1,
          "setting the SLH-DSA keygen \"seed\" parameter failed");
    CHECK(EVP_PKEY_keygen(ctx, &pkey) == 1, "EVP_PKEY_keygen failed");
    CHECK(pkey != NULL, "EVP_PKEY_keygen produced no key");
    EVP_PKEY_CTX_free(ctx);
    return pkey;
}

/* One-shot deterministic EVP_DigestSign.  *siglen is an in/out capacity per
 * the EVP contract. */
static void sign_one_shot(EVP_PKEY *pkey, const unsigned char *msg, size_t msglen,
                          unsigned char *sig, size_t *siglen)
{
    EVP_MD_CTX *mctx = EVP_MD_CTX_new();
    EVP_PKEY_CTX *pctx;

    CHECK(mctx != NULL, "EVP_MD_CTX_new failed");
    CHECK(EVP_DigestSignInit(mctx, &pctx, NULL, NULL, pkey) == 1,
          "EVP_DigestSignInit failed");
    set_deterministic(pctx);
    CHECK(EVP_DigestSign(mctx, sig, siglen, msg, msglen) == 1,
          "EVP_DigestSign failed");
    EVP_MD_CTX_free(mctx);
}

/* One-shot verify.  Returns 1 only for a valid signature. */
static int verify_one_shot(EVP_PKEY *pkey, const unsigned char *sig, size_t siglen,
                           const unsigned char *msg, size_t msglen)
{
    EVP_MD_CTX *mctx = EVP_MD_CTX_new();
    EVP_PKEY_CTX *pctx;
    int rc;

    CHECK(mctx != NULL, "EVP_MD_CTX_new failed");
    CHECK(EVP_DigestVerifyInit(mctx, &pctx, NULL, NULL, pkey) == 1,
          "EVP_DigestVerifyInit failed");
    rc = EVP_DigestVerify(mctx, sig, siglen, msg, msglen);
    EVP_MD_CTX_free(mctx);
    return rc;
}

/* (1) Seed keygen: reproduces the KAT public key for every parameter set
 * and is reproducible run-to-run. */
static void test_seed_keygen_kats(void)
{
    size_t s;
    for (s = 0; s < sizeof(slh_dsa_sets) / sizeof(slh_dsa_sets[0]); s++) {
        const struct slh_dsa_set *set = &slh_dsa_sets[s];
        unsigned char *seed = ALLOCATE_KEY(set->seed_len);
        unsigned char *want_pub = ALLOCATE_OTHER(SLH_MAX_PUB);
        unsigned char *pub1 = ALLOCATE_OUTPUT(SLH_MAX_PUB);
        unsigned char *pub2 = ALLOCATE_OUTPUT(SLH_MAX_PUB);
        EVP_PKEY *pkey;
        size_t pub1len, pub2len, want_len;

        pkey = keygen_from_seed(set, seed);
        want_len = from_hex(set->pubhex, want_pub, SLH_MAX_PUB);

        pub1len = 0;
        CHECK(EVP_PKEY_get_octet_string_param(pkey, OSSL_PKEY_PARAM_PUB_KEY,
                                              pub1, SLH_MAX_PUB, &pub1len) == 1,
              "public key export failed");
        expect_equal("seed-derived public key does not match the KAT",
                     pub1, pub1len, want_pub, want_len);
        EVP_PKEY_free(pkey);

        /* Key generation from the same seed must be deterministic. */
        pkey = keygen_from_seed(set, seed);
        pub2len = 0;
        CHECK(EVP_PKEY_get_octet_string_param(pkey, OSSL_PKEY_PARAM_PUB_KEY,
                                              pub2, SLH_MAX_PUB, &pub2len) == 1,
              "public key export failed (second keygen)");
        expect_equal("repeated seed keygen produced a different public key",
                     pub2, pub2len, want_pub, want_len);
        EVP_PKEY_free(pkey);
    }
}

/* (2) One-shot signing: the NULL-buffer EVP_DigestSign call reports the
 * FIPS 205 signature size; deterministic signing of the 16-byte KAT
 * message is reproducible, verifies, rejects tampered signatures and wrong
 * messages; randomized (default) signing differs but verifies. */
static void test_sign_verify(void)
{
    size_t s;
    for (s = 0; s < sizeof(slh_dsa_sets) / sizeof(slh_dsa_sets[0]); s++) {
        const struct slh_dsa_set *set = &slh_dsa_sets[s];
        unsigned char *seed = ALLOCATE_KEY(set->seed_len);
        unsigned char *msg = ALLOCATE_INPUT(SLH_MSG_LEN);
        unsigned char *sig = ALLOCATE_OUTPUT(SLH_MAX_SIG);
        unsigned char *sig2 = ALLOCATE_OUTPUT(SLH_MAX_SIG);
        EVP_PKEY *pkey;
        size_t msglen, siglen, sig2len;
        char msgbuf[128];

        pkey = keygen_from_seed(set, seed);
        msglen = from_hex(MSG16_HEX, msg, SLH_MSG_LEN);

        /* Signature size via the one-shot NULL-buffer call. */
        siglen = 0;
        {
            EVP_MD_CTX *mctx = EVP_MD_CTX_new();
            EVP_PKEY_CTX *pctx;
            CHECK(mctx != NULL, "EVP_MD_CTX_new failed");
            CHECK(EVP_DigestSignInit(mctx, &pctx, NULL, NULL, pkey) == 1,
                  "EVP_DigestSignInit failed");
            set_deterministic(pctx);
            CHECK(EVP_DigestSign(mctx, NULL, &siglen, msg, msglen) == 1,
                  "EVP_DigestSign (size query) failed");
            snprintf(msgbuf, sizeof(msgbuf),
                     "%s signature size is %zu, want %zu", set->name, siglen,
                     set->sig_len);
            CHECK(siglen == set->sig_len, msgbuf);
            EVP_MD_CTX_free(mctx);
        }

        siglen = SLH_MAX_SIG;
        sign_one_shot(pkey, msg, msglen, sig, &siglen);
        CHECK(siglen == set->sig_len, "deterministic signature size changed");

        /* Deterministic signing must reproduce the signature exactly. */
        sig2len = SLH_MAX_SIG;
        sign_one_shot(pkey, msg, msglen, sig2, &sig2len);
        expect_equal("repeated deterministic signing produced a different signature",
                     sig2, sig2len, sig, siglen);

        /* The signature verifies. */
        CHECK(verify_one_shot(pkey, sig, siglen, msg, msglen) == 1,
              "the SLH-DSA signature does not verify");

        /* A tampered signature must fail verification. */
        sig2[0] ^= 0x01;
        CHECK(verify_one_shot(pkey, sig2, sig2len, msg, msglen) != 1,
              "tampered signature verified");

        /* A wrong message must fail verification. */
        msg[0] ^= 0x80;
        CHECK(verify_one_shot(pkey, sig, siglen, msg, msglen) != 1,
              "signature verified over the wrong message");
        msg[0] ^= 0x80;

        /* Default (randomized/hedged) signing differs from the deterministic
         * signature but must still verify. */
        {
            EVP_MD_CTX *mctx = EVP_MD_CTX_new();
            EVP_PKEY_CTX *pctx;
            CHECK(mctx != NULL, "EVP_MD_CTX_new failed");
            CHECK(EVP_DigestSignInit(mctx, &pctx, NULL, NULL, pkey) == 1,
                  "EVP_DigestSignInit failed");
            sig2len = SLH_MAX_SIG;
            CHECK(EVP_DigestSign(mctx, sig2, &sig2len, msg, msglen) == 1,
                  "randomized EVP_DigestSign failed");
            EVP_MD_CTX_free(mctx);
            CHECK(sig2len == siglen, "randomized signature size changed");
            CHECK(memcmp(sig2, sig, siglen) != 0,
                  "randomized signing did not randomize");
            CHECK(verify_one_shot(pkey, sig2, sig2len, msg, msglen) == 1,
                  "randomized signature does not verify");
        }

        EVP_PKEY_free(pkey);
    }
}

/* (3) Context-string binding: signatures made with a context string verify
 * only under the same context string, and differ from the no-context
 * signature. */
static void test_context_string(void)
{
    size_t s;
    for (s = 0; s < sizeof(slh_dsa_sets) / sizeof(slh_dsa_sets[0]); s++) {
        const struct slh_dsa_set *set = &slh_dsa_sets[s];
        unsigned char *seed = ALLOCATE_KEY(set->seed_len);
        unsigned char *msg = ALLOCATE_INPUT(SLH_MSG_LEN);
        unsigned char *ctxstr = ALLOCATE_INPUT(sizeof(CTXSTR) - 1);
        unsigned char *sig = ALLOCATE_OUTPUT(SLH_MAX_SIG);
        unsigned char *plain = ALLOCATE_OUTPUT(SLH_MAX_SIG);
        EVP_PKEY *pkey;
        size_t msglen, siglen, plainlen;

        pkey = keygen_from_seed(set, seed);
        msglen = from_hex(MSG16_HEX, msg, SLH_MSG_LEN);
        memcpy(ctxstr, CTXSTR, sizeof(CTXSTR) - 1);

        /* Sign with the context string. */
        {
            EVP_MD_CTX *mctx = EVP_MD_CTX_new();
            EVP_PKEY_CTX *pctx;
            OSSL_PARAM params[2];
            CHECK(mctx != NULL, "EVP_MD_CTX_new failed");
            CHECK(EVP_DigestSignInit(mctx, &pctx, NULL, NULL, pkey) == 1,
                  "EVP_DigestSignInit failed");
            set_deterministic(pctx);
            params[0] = OSSL_PARAM_construct_octet_string("context-string",
                                                          ctxstr,
                                                          sizeof(CTXSTR) - 1);
            params[1] = OSSL_PARAM_construct_end();
            CHECK(EVP_PKEY_CTX_set_params(pctx, params) == 1,
                  "setting the \"context-string\" parameter failed");
            siglen = SLH_MAX_SIG;
            CHECK(EVP_DigestSign(mctx, sig, &siglen, msg, msglen) == 1,
                  "EVP_DigestSign with a context string failed");
            EVP_MD_CTX_free(mctx);
        }

        /* Same context string: verifies. */
        {
            EVP_MD_CTX *mctx = EVP_MD_CTX_new();
            EVP_PKEY_CTX *pctx;
            OSSL_PARAM params[2];
            CHECK(mctx != NULL, "EVP_MD_CTX_new failed");
            CHECK(EVP_DigestVerifyInit(mctx, &pctx, NULL, NULL, pkey) == 1,
                  "EVP_DigestVerifyInit failed");
            params[0] = OSSL_PARAM_construct_octet_string("context-string",
                                                          ctxstr,
                                                          sizeof(CTXSTR) - 1);
            params[1] = OSSL_PARAM_construct_end();
            CHECK(EVP_PKEY_CTX_set_params(pctx, params) == 1,
                  "setting the \"context-string\" parameter failed");
            CHECK(EVP_DigestVerify(mctx, sig, siglen, msg, msglen) == 1,
                  "context-string signature does not verify under its context");
            EVP_MD_CTX_free(mctx);
        }

        /* Without the context string: must fail. */
        CHECK(verify_one_shot(pkey, sig, siglen, msg, msglen) != 1,
              "context-string signature verified without the context");

        /* And it must differ from the no-context signature. */
        plainlen = SLH_MAX_SIG;
        sign_one_shot(pkey, msg, msglen, plain, &plainlen);
        CHECK(siglen == plainlen && memcmp(sig, plain, plainlen) != 0,
              "context string did not change the signature");

        EVP_PKEY_free(pkey);
    }
}

/* (4) Large message: a ~256 KiB message signs and verifies one-shot in
 * both deterministic and randomized modes (see the spec adaptation note in
 * the file comment: SLH-DSA has no streaming interface in this OpenSSL). */
static void test_large_message(void)
{
    unsigned char *big = ALLOCATE_INPUT(BIG_LEN);
    size_t s, i;

    for (i = 0; i < BIG_LEN; i++)
        big[i] = (unsigned char)(i * 7 + 3);

    for (s = 0; s < sizeof(slh_dsa_sets) / sizeof(slh_dsa_sets[0]); s++) {
        const struct slh_dsa_set *set = &slh_dsa_sets[s];
        unsigned char *seed = ALLOCATE_KEY(set->seed_len);
        unsigned char *sig = ALLOCATE_OUTPUT(SLH_MAX_SIG);
        unsigned char *sig2 = ALLOCATE_OUTPUT(SLH_MAX_SIG);
        EVP_PKEY *pkey;
        size_t siglen, sig2len;

        pkey = keygen_from_seed(set, seed);

        siglen = SLH_MAX_SIG;
        sign_one_shot(pkey, big, BIG_LEN, sig, &siglen);
        CHECK(siglen == set->sig_len, "large-message signature size changed");
        CHECK(verify_one_shot(pkey, sig, siglen, big, BIG_LEN) == 1,
              "large-message signature does not verify");

        /* Deterministic mode reproduces the large signature exactly. */
        sig2len = SLH_MAX_SIG;
        sign_one_shot(pkey, big, BIG_LEN, sig2, &sig2len);
        expect_equal("repeated large-message signing differs",
                     sig2, sig2len, sig, siglen);

        /* Randomized mode differs but verifies. */
        {
            EVP_MD_CTX *mctx = EVP_MD_CTX_new();
            EVP_PKEY_CTX *pctx;
            CHECK(mctx != NULL, "EVP_MD_CTX_new failed");
            CHECK(EVP_DigestSignInit(mctx, &pctx, NULL, NULL, pkey) == 1,
                  "EVP_DigestSignInit failed");
            sig2len = SLH_MAX_SIG;
            CHECK(EVP_DigestSign(mctx, sig2, &sig2len, big, BIG_LEN) == 1,
                  "randomized large-message EVP_DigestSign failed");
            EVP_MD_CTX_free(mctx);
            CHECK(sig2len == siglen, "randomized large-message signature size changed");
            CHECK(memcmp(sig2, sig, siglen) != 0,
                  "randomized signing did not randomize");
            CHECK(verify_one_shot(pkey, sig2, sig2len, big, BIG_LEN) == 1,
                  "randomized large-message signature does not verify");
        }

        EVP_PKEY_free(pkey);
    }
}

int main(int argc, char **argv)
{
    size_t s;

    poison_parse_args(argc, argv);

    /* SLH-DSA is version-gated (OpenSSL 3.5+): if any parameter set is not
     * provided by this OpenSSL, take the SKIP path and exit 0. */
    for (s = 0; s < sizeof(slh_dsa_sets) / sizeof(slh_dsa_sets[0]); s++) {
        if (!pkey_alg_available(slh_dsa_sets[s].name)) {
            printf("SKIP test_slh_dsa: %s is unavailable\n", slh_dsa_sets[s].name);
            return 0;
        }
    }

    test_seed_keygen_kats();
    test_sign_verify();
    test_context_string();
    test_large_message();

    printf("test_slh_dsa: all %u checks passed\n", g_checks);
    return 0;
}

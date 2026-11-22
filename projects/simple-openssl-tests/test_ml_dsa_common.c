/*
 * test_ml_dsa_common.c -- shared implementation of the ML-DSA family tests
 * (API in test_ml_dsa_common.h; one binary per parameter set, one struct
 * ml_dsa_set row per binary).
 *
 * ML-DSA-44/65/87 (FIPS 204) tests: deterministic seed keygen, one-shot
 * signature known answers, streaming (chunked) signing and verification,
 * tamper detection, and context-string binding.
 *
 * Determinism hooks used (all public OpenSSL 3 API):
 *
 *   - Key generation accepts the 32-byte FIPS 204 seed xi via the keymgmt
 *     "seed" parameter (OSSL_PKEY_PARAM_ML_DSA_SEED, new in 3.5); identical
 *     seeds derive identical key pairs.
 *   - Signing accepts the integer "deterministic" parameter
 *     (OSSL_SIGNATURE_PARAM_DETERMINISTIC, new in 3.5).  With it set, the
 *     32-byte signing randomizer is zeroed and signatures are fully
 *     deterministic given (key, message, context string).  Without it,
 *     ML-DSA uses randomized (hedged) signing.
 *   - Streaming uses EVP_PKEY_sign_message_init / _update / _final and
 *     EVP_PKEY_verify_message_init / _update / _final (new in 3.6): ML-DSA
 *     is a pure message-signature algorithm whose provider does not offer
 *     the digest_sign_update/digest_verify_update streaming hooks, so the
 *     EVP_DigestSignUpdate/EVP_DigestVerifyUpdate route does not apply.
 *     Those symbols first appear in OpenSSL 3.6, so the streaming section
 *     is compiled only against 3.6+ headers (the whole program skips on
 *     OpenSSL 3.0, which has no ML-DSA at all).
 *
 * Known-answer constants: each parameter set's seed and deterministic
 * one-shot signature of the family-wide 8-byte KAT message
 * (ml_dsa_msg8_hex below) live in its own binary.  They were derived and
 * cross-verified with a scratch probe compiled against the authoritative
 * pizfix OpenSSL 3.6.5 (build/bin/filcc), per README house rule 7:
 *
 *   - SEED_*:   the key-derivation seeds (0x00,0x01,...,0x1f for ML-DSA-44,
 *               0x40,...,0x5f for ML-DSA-65, 0x80,...,0x9f for ML-DSA-87).
 *   - KAT_SIG_*: the deterministic one-shot signatures of the 8-byte
 *               message 00 01 02 03 04 05 06 07 (2420 / 3309 / 4627 bytes,
 *               the FIPS 204 signature sizes).
 *
 * The public/secret key raw export sizes (1312/1952/2592 and
 * 2560/4032/4896 bytes) and the exact KAT signature bytes are asserted
 * below; keygen is additionally required to be reproducible run-to-run.
 *
 * Version gate: ML-DSA is version-gated (OpenSSL 3.5+).  Each per-set
 * binary probes its own parameter set right after test_parse_args() and
 * exits through test_skip() with the original reason text (e.g. the system
 * OpenSSL 3.0.13 provides no ML-DSA at all).
 */

#include "test_ml_dsa_common.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>

/* The fixed 8-byte KAT message, family-wide.  An array, not a pointer, so
 * that it is an address constant usable in the static initializers of the
 * per-set binaries. */
const char ml_dsa_msg8_hex[] = "0001020304050607";

/* The shared signing/verification context string ("FilC-context"). */
const char ml_dsa_ctxstr[] = "FilC-context";

/* Size of the streaming message: 1 MiB signed in three chunks. */
#define BIG_LEN (1024u * 1024u)

/* True when this OpenSSL provides the named algorithm (its keymgmt can be
 * instantiated and key generation initialized). */
static int pkey_alg_available(const char *name)
{
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(NULL, name, NULL);
    int ok = ctx != NULL && EVP_PKEY_keygen_init(ctx) == 1;

    EVP_PKEY_CTX_free(ctx);
    return ok;
}

/* The whole-program availability gate: SKIP with the original reason text
 * when the binary's parameter set is not provided by this OpenSSL. */
void ml_dsa_test_gate(const struct ml_dsa_set *set)
{
    char msg[128];

    if (!pkey_alg_available(set->name)) {
        snprintf(msg, sizeof(msg), "%s is unavailable", set->name);
        test_skip(msg);
    }
}

/* The "deterministic" signature parameter (OSSL_SIGNATURE_PARAM_DETERMINISTIC;
 * spelled as a literal because the macro is new in 3.5). */
void ml_dsa_set_deterministic(EVP_PKEY_CTX *pctx)
{
    OSSL_PARAM params[2];
    int one = 1;

    params[0] = OSSL_PARAM_construct_int("deterministic", &one);
    params[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_PKEY_CTX_set_params(pctx, params) == 1,
          "setting the \"deterministic\" signature parameter failed");
}

/* Deterministic key generation from the FIPS 204 seed xi via the keymgmt
 * "seed" parameter (OSSL_PKEY_PARAM_ML_DSA_SEED; spelled as a literal
 * because the macro is new in 3.5). */
EVP_PKEY *ml_dsa_keygen_from_seed(const struct ml_dsa_set *set,
                                  unsigned char *seedbuf)
{
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(NULL, set->name, NULL);
    EVP_PKEY *pkey = NULL;
    OSSL_PARAM params[2];

    CHECK(ctx != NULL, "EVP_PKEY_CTX_new_from_name failed for an available ML-DSA");
    CHECK(EVP_PKEY_keygen_init(ctx) == 1, "EVP_PKEY_keygen_init failed");
    from_hex(set->seedhex, seedbuf, set->seed_len);
    params[0] = OSSL_PARAM_construct_octet_string("seed", seedbuf, set->seed_len);
    params[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_PKEY_CTX_set_params(ctx, params) == 1,
          "setting the ML-DSA keygen \"seed\" parameter failed");
    CHECK(EVP_PKEY_keygen(ctx, &pkey) == 1, "EVP_PKEY_keygen failed");
    CHECK(pkey != NULL, "EVP_PKEY_keygen produced no key");
    EVP_PKEY_CTX_free(ctx);
    return pkey;
}

/* One-shot deterministic EVP_DigestSign of a message.  *siglen is an in/out
 * capacity per the EVP contract. */
void ml_dsa_sign_one_shot(EVP_PKEY *pkey, const unsigned char *msg,
                          size_t msglen, unsigned char *sig, size_t *siglen)
{
    EVP_MD_CTX *mctx = EVP_MD_CTX_new();
    EVP_PKEY_CTX *pctx;

    CHECK(mctx != NULL, "EVP_MD_CTX_new failed");
    CHECK(EVP_DigestSignInit(mctx, &pctx, NULL, NULL, pkey) == 1,
          "EVP_DigestSignInit failed");
    ml_dsa_set_deterministic(pctx);
    CHECK(EVP_DigestSign(mctx, sig, siglen, msg, msglen) == 1,
          "EVP_DigestSign failed");
    EVP_MD_CTX_free(mctx);
}

/* One-shot deterministic verify.  Returns 1 only for a valid signature. */
int ml_dsa_verify_one_shot(EVP_PKEY *pkey, const unsigned char *sig,
                           size_t siglen, const unsigned char *msg,
                           size_t msglen)
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

/* (1) Seed keygen: exports keys of the FIPS 204 pk/sk sizes and is
 * reproducible run-to-run. */
void ml_dsa_test_seed_keygen(const struct ml_dsa_set *set)
{
    unsigned char *seed = ALLOCATE_KEY(set->seed_len);
    unsigned char *pub1 = ALLOCATE_OUTPUT(ML_DSA_MAX_PUB);
    unsigned char *pub2 = ALLOCATE_OUTPUT(ML_DSA_MAX_PUB);
    unsigned char *priv = ALLOCATE_OUTPUT(ML_DSA_MAX_PRIV);
    EVP_PKEY *pkey;
    size_t pub1len, pub2len, privlen;
    char msg[128];

    pkey = ml_dsa_keygen_from_seed(set, seed);

    pub1len = 0;
    CHECK(EVP_PKEY_get_octet_string_param(pkey, OSSL_PKEY_PARAM_PUB_KEY,
                                          pub1, ML_DSA_MAX_PUB, &pub1len) == 1,
          "public key export failed");
    snprintf(msg, sizeof(msg), "%s public key export is %zu bytes, want %zu",
             set->name, pub1len, set->pub_len);
    CHECK(pub1len == set->pub_len, msg);

    privlen = 0;
    CHECK(EVP_PKEY_get_octet_string_param(pkey, OSSL_PKEY_PARAM_PRIV_KEY,
                                          priv, ML_DSA_MAX_PRIV, &privlen) == 1,
          "private key export failed");
    snprintf(msg, sizeof(msg), "%s private key export is %zu bytes, want %zu",
             set->name, privlen, set->priv_len);
    CHECK(privlen == set->priv_len, msg);
    EVP_PKEY_free(pkey);

    /* Key generation from the same seed must be deterministic. */
    pkey = ml_dsa_keygen_from_seed(set, seed);
    pub2len = 0;
    CHECK(EVP_PKEY_get_octet_string_param(pkey, OSSL_PKEY_PARAM_PUB_KEY,
                                          pub2, ML_DSA_MAX_PUB, &pub2len) == 1,
          "public key export failed (second keygen)");
    expect_equal("repeated seed keygen produced a different public key",
                 pub2, pub2len, pub1, pub1len);
    EVP_PKEY_free(pkey);

    free_poison_allocations();
}

/* (2) One-shot signing KAT: the deterministic signature of the 8-byte KAT
 * message matches the embedded bytes (FIPS 204 signature size), signs
 * identically when repeated, verifies, and rejects tampered signatures and
 * wrong messages.  The default (randomized) mode differs from the
 * deterministic signature but still verifies. */
void ml_dsa_test_oneshot_kats(const struct ml_dsa_set *set)
{
    unsigned char *seed = ALLOCATE_KEY(set->seed_len);
    unsigned char *msg = ALLOCATE_INPUT(ML_DSA_MSG_LEN);
    unsigned char *want_sig = ALLOCATE_OTHER(ML_DSA_MAX_SIG);
    unsigned char *sig = ALLOCATE_OUTPUT(ML_DSA_MAX_SIG);
    unsigned char *sig2 = ALLOCATE_OUTPUT(ML_DSA_MAX_SIG);
    EVP_PKEY *pkey;
    size_t msglen, wantlen, siglen, sig2len;
    char msgbuf[128];

    pkey = ml_dsa_keygen_from_seed(set, seed);
    msglen = from_hex(ml_dsa_msg8_hex, msg, ML_DSA_MSG_LEN);
    wantlen = from_hex(set->sighex, want_sig, ML_DSA_MAX_SIG);

    /* The one-shot NULL-buffer call reports the exact FIPS 204 size. */
    siglen = 0;
    {
        EVP_MD_CTX *mctx = EVP_MD_CTX_new();
        EVP_PKEY_CTX *pctx;
        CHECK(mctx != NULL, "EVP_MD_CTX_new failed");
        CHECK(EVP_DigestSignInit(mctx, &pctx, NULL, NULL, pkey) == 1,
              "EVP_DigestSignInit failed");
        ml_dsa_set_deterministic(pctx);
        CHECK(EVP_DigestSign(mctx, NULL, &siglen, msg, msglen) == 1,
              "EVP_DigestSign (size query) failed");
        snprintf(msgbuf, sizeof(msgbuf),
                 "%s signature size is %zu, want %zu", set->name, siglen,
                 set->sig_len);
        CHECK(siglen == set->sig_len, msgbuf);
        EVP_MD_CTX_free(mctx);
    }

    siglen = ML_DSA_MAX_SIG;
    ml_dsa_sign_one_shot(pkey, msg, msglen, sig, &siglen);
    expect_equal("deterministic one-shot signature does not match the KAT",
                 sig, siglen, want_sig, wantlen);

    /* Signing the same message again must reproduce the signature. */
    sig2len = ML_DSA_MAX_SIG;
    ml_dsa_sign_one_shot(pkey, msg, msglen, sig2, &sig2len);
    expect_equal("repeated one-shot signing produced a different signature",
                 sig2, sig2len, want_sig, wantlen);

    /* The KAT signature verifies. */
    CHECK(ml_dsa_verify_one_shot(pkey, want_sig, wantlen, msg, msglen) == 1,
          "the KAT signature does not verify");

    /* A tampered signature must fail verification. */
    sig2[0] ^= 0x01;
    CHECK(ml_dsa_verify_one_shot(pkey, sig2, sig2len, msg, msglen) != 1,
          "tampered signature verified");

    /* A wrong message must fail verification. */
    sig2[0] ^= 0x01; /* restore */
    msg[0] ^= 0x80;
    CHECK(ml_dsa_verify_one_shot(pkey, sig, siglen, msg, msglen) != 1,
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
        sig2len = ML_DSA_MAX_SIG;
        CHECK(EVP_DigestSign(mctx, sig2, &sig2len, msg, msglen) == 1,
              "randomized EVP_DigestSign failed");
        EVP_MD_CTX_free(mctx);
        CHECK(sig2len == siglen, "randomized signature size changed");
        unpoison_all();
        CHECK(memcmp(sig2, sig, siglen) != 0,
              "randomized signing did not randomize");
        repoison_all();
        CHECK(ml_dsa_verify_one_shot(pkey, sig2, sig2len, msg, msglen) == 1,
              "randomized signature does not verify");
    }

    EVP_PKEY_free(pkey);

    free_poison_allocations();
}

/* (3) Context-string binding: signatures made with a context string verify
 * only under the same context string, and differ from the no-context
 * signature. */
void ml_dsa_test_context_string(const struct ml_dsa_set *set)
{
    unsigned char *seed = ALLOCATE_KEY(set->seed_len);
    unsigned char *msg = ALLOCATE_INPUT(ML_DSA_MSG_LEN);
    unsigned char *ctxstr = ALLOCATE_INPUT(ML_DSA_CTXSTR_LEN);
    unsigned char *sig = ALLOCATE_OUTPUT(ML_DSA_MAX_SIG);
    unsigned char *plain = ALLOCATE_OUTPUT(ML_DSA_MAX_SIG);
    EVP_PKEY *pkey;
    size_t msglen, siglen, plainlen;

    pkey = ml_dsa_keygen_from_seed(set, seed);
    msglen = from_hex(ml_dsa_msg8_hex, msg, ML_DSA_MSG_LEN);
    memcpy(ctxstr, ml_dsa_ctxstr, ML_DSA_CTXSTR_LEN);

    /* Sign with the context string. */
    {
        EVP_MD_CTX *mctx = EVP_MD_CTX_new();
        EVP_PKEY_CTX *pctx;
        OSSL_PARAM params[2];
        CHECK(mctx != NULL, "EVP_MD_CTX_new failed");
        CHECK(EVP_DigestSignInit(mctx, &pctx, NULL, NULL, pkey) == 1,
              "EVP_DigestSignInit failed");
        ml_dsa_set_deterministic(pctx);
        params[0] = OSSL_PARAM_construct_octet_string("context-string",
                                                      ctxstr,
                                                      ML_DSA_CTXSTR_LEN);
        params[1] = OSSL_PARAM_construct_end();
        CHECK(EVP_PKEY_CTX_set_params(pctx, params) == 1,
              "setting the \"context-string\" parameter failed");
        siglen = ML_DSA_MAX_SIG;
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
                                                      ML_DSA_CTXSTR_LEN);
        params[1] = OSSL_PARAM_construct_end();
        CHECK(EVP_PKEY_CTX_set_params(pctx, params) == 1,
              "setting the \"context-string\" parameter failed");
        CHECK(EVP_DigestVerify(mctx, sig, siglen, msg, msglen) == 1,
              "context-string signature does not verify under its context");
        EVP_MD_CTX_free(mctx);
    }

    /* Without the context string: must fail. */
    CHECK(ml_dsa_verify_one_shot(pkey, sig, siglen, msg, msglen) != 1,
          "context-string signature verified without the context");

    /* And it must differ from the no-context signature. */
    plainlen = ML_DSA_MAX_SIG;
    ml_dsa_sign_one_shot(pkey, msg, msglen, plain, &plainlen);
    unpoison_all();
    CHECK(siglen == plainlen && memcmp(sig, plain, plainlen) != 0,
          "context string did not change the signature");
    repoison_all();

    EVP_PKEY_free(pkey);

    free_poison_allocations();
}
/*
 * (4) Streaming signing and verification of a ~1MB message fed in three
 * chunks, via EVP_PKEY_sign_message_* and EVP_PKEY_verify_message_* (3.6+;
 * see the file comment).  The streaming signature must equal the one-shot
 * EVP_DigestSign signature over the same message (deterministic mode), the
 * streaming verification must accept it, and a one-shot verification of the
 * streaming signature over the full message must also accept it, while a
 * tampered streaming signature is rejected.  Compiled only against 3.6+
 * headers: these symbols do not exist in OpenSSL 3.0 (the program skips
 * there long before reaching this section).
 */
#if OPENSSL_VERSION_NUMBER >= 0x30600000L
void ml_dsa_test_streaming(const struct ml_dsa_set *set)
{
    static const size_t chunks[3] = { 400000, 500000, BIG_LEN - 900000 };
    unsigned char *big = ALLOCATE_INPUT(BIG_LEN);
    unsigned char *seed = ALLOCATE_KEY(set->seed_len);
    unsigned char *stream_sig = ALLOCATE_OUTPUT(ML_DSA_MAX_SIG);
    unsigned char *oneshot_sig = ALLOCATE_OUTPUT(ML_DSA_MAX_SIG);
    EVP_PKEY *pkey;
    size_t i, stream_len, oneshot_len, off;

    for (i = 0; i < BIG_LEN; i++)
        big[i] = (unsigned char)(i * 7 + 3);

    pkey = ml_dsa_keygen_from_seed(set, seed);

    /* Streaming signature over the 1MB message, three updates. */
    {
        EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new(pkey, NULL);
        CHECK(pctx != NULL, "EVP_PKEY_CTX_new failed");
        CHECK(EVP_PKEY_sign_message_init(pctx, NULL, NULL) == 1,
              "EVP_PKEY_sign_message_init failed");
        ml_dsa_set_deterministic(pctx);
        off = 0;
        for (i = 0; i < 3; i++) {
            CHECK(EVP_PKEY_sign_message_update(pctx, big + off,
                                               chunks[i]) == 1,
                  "EVP_PKEY_sign_message_update failed");
            off += chunks[i];
        }
        CHECK(off == BIG_LEN, "streaming chunks did not cover the message");
        stream_len = ML_DSA_MAX_SIG;
        CHECK(EVP_PKEY_sign_message_final(pctx, stream_sig,
                                          &stream_len) == 1,
              "EVP_PKEY_sign_message_final failed");
        EVP_PKEY_CTX_free(pctx);
    }

    /* One-shot signature over the same message must be identical. */
    oneshot_len = ML_DSA_MAX_SIG;
    ml_dsa_sign_one_shot(pkey, big, BIG_LEN, oneshot_sig, &oneshot_len);
    expect_equal("streaming signature differs from the one-shot signature",
                 stream_sig, stream_len, oneshot_sig, oneshot_len);

    /* Streaming verification of the streaming signature. */
    {
        EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new(pkey, NULL);
        OSSL_PARAM params[2];
        CHECK(pctx != NULL, "EVP_PKEY_CTX_new failed");
        CHECK(EVP_PKEY_verify_message_init(pctx, NULL, NULL) == 1,
              "EVP_PKEY_verify_message_init failed");
        params[0] = OSSL_PARAM_construct_octet_string("signature",
                                                      stream_sig,
                                                      stream_len);
        params[1] = OSSL_PARAM_construct_end();
        CHECK(EVP_PKEY_CTX_set_params(pctx, params) == 1,
              "setting the \"signature\" parameter failed");
        off = 0;
        for (i = 0; i < 3; i++) {
            CHECK(EVP_PKEY_verify_message_update(pctx, big + off,
                                                 chunks[i]) == 1,
                  "EVP_PKEY_verify_message_update failed");
            off += chunks[i];
        }
        CHECK(EVP_PKEY_verify_message_final(pctx) == 1,
              "streaming verification of a valid signature failed");
        EVP_PKEY_CTX_free(pctx);
    }

    /* One-shot verification of the streaming signature. */
    CHECK(ml_dsa_verify_one_shot(pkey, stream_sig, stream_len, big,
                                 BIG_LEN) == 1,
          "one-shot verification of the streaming signature failed");

    /* A tampered streaming signature must be rejected. */
    stream_sig[0] ^= 0x01;
    {
        EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new(pkey, NULL);
        OSSL_PARAM params[2];
        int vrc;
        CHECK(pctx != NULL, "EVP_PKEY_CTX_new failed");
        CHECK(EVP_PKEY_verify_message_init(pctx, NULL, NULL) == 1,
              "EVP_PKEY_verify_message_init failed");
        params[0] = OSSL_PARAM_construct_octet_string("signature",
                                                      stream_sig,
                                                      stream_len);
        params[1] = OSSL_PARAM_construct_end();
        CHECK(EVP_PKEY_CTX_set_params(pctx, params) == 1,
              "setting the \"signature\" parameter failed");
        off = 0;
        for (i = 0; i < 3; i++) {
            CHECK(EVP_PKEY_verify_message_update(pctx, big + off,
                                                 chunks[i]) == 1,
                  "EVP_PKEY_verify_message_update failed");
            off += chunks[i];
        }
        vrc = EVP_PKEY_verify_message_final(pctx);
        CHECK(vrc != 1, "tampered streaming signature verified");
        EVP_PKEY_CTX_free(pctx);
    }
    EVP_PKEY_free(pkey);

    free_poison_allocations();
}
#endif /* OPENSSL_VERSION_NUMBER >= 0x30600000L */

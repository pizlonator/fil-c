/*
 * test_ml_kem_common.c -- shared implementation of the ML-KEM family tests
 * (API in test_ml_kem_common.h; one binary per parameter set, one struct
 * ml_kem_set row per binary).
 *
 * ML-KEM-512/768/1024 (FIPS 203) tests: deterministic seed keygen,
 * encapsulation/decapsulation known answers, implicit rejection of tampered
 * ciphertexts, and raw private-key import/export.
 *
 * Determinism hooks used (all public OpenSSL 3 API):
 *
 *   - Key generation accepts the FIPS 203 (d, z) seed, 64 bytes, via the
 *     keymgmt "seed" parameter (OSSL_PKEY_PARAM_ML_KEM_SEED, new in 3.5);
 *     identical seeds derive identical key pairs.
 *   - Encapsulation accepts the 32-byte ephemeral randomness via the KEM
 *     "ikme" parameter (OSSL_KEM_PARAM_IKME, new in 3.2), which makes
 *     encapsulation deterministic.  The PQ-specific parameter names are
 *     spelled as string literals because their macros are absent from
 *     pre-3.5 headers; the family binaries still compile against OpenSSL
 *     3.0 (and take the SKIP path there).
 *
 * Known-answer constants: each parameter set's seed, encapsulation key,
 * ciphertext and shared-secret hex live in its own binary; the fixed
 * encapsulation ephemeral randomness is family-wide (ml_kem_ikme_hex
 * below).  They were derived and cross-verified with a scratch probe
 * compiled against the authoritative pizfix OpenSSL 3.6.5
 * (build/bin/filcc), per README house rule 7:
 *
 *   - SEED_*:    the key-derivation seeds (0x00,0x01,...,0x3f for
 *                ML-KEM-512, 0x40,...,0x7f for ML-KEM-768, 0x80,...,0xbf
 *                for ML-KEM-1024).
 *   - KAT_PUB_*: the raw encapsulation keys exported from those seeds
 *                (800 / 1184 / 1568 bytes, the FIPS 203 ek sizes).
 *   - KAT_CT_*:  the ciphertexts produced by encapsulating against those
 *                public keys with the fixed ephemeral IKME (0xa0,...,0xbf)
 *                (768 / 1088 / 1568 bytes, the FIPS 203 ct sizes).
 *   - KAT_SS_*:  the shared secrets paired with those ciphertexts (32
 *                bytes, the FIPS 203 ss size for every parameter set).
 *
 * The same probe verified every behavior asserted below: decapsulating a
 * KAT ciphertext with the seed-derived private key yields the KAT shared
 * secret, a raw-imported private key does too, and a tampered ciphertext
 * decapsulates without error but yields a different shared secret (FIPS
 * 203 implicit rejection).  Private keys are not embedded as hex; their
 * correctness is pinned by the embedded (ct, ss) pairs and their raw
 * sizes are checked against FIPS 203 (1632 / 2400 / 3168 bytes).
 *
 * Version gate: ML-KEM is version-gated (OpenSSL 3.5+).  Each per-set
 * binary probes its own parameter set right after test_parse_args() and
 * exits through test_skip() with the original reason text (e.g. the system
 * OpenSSL 3.0.13 provides no ML-KEM at all).
 */

#include "test_ml_kem_common.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>

/* The fixed encapsulation ephemeral randomness ("ikmE"), family-wide: the
 * same 32 bytes (0xa0,...,0xbf) drive the encapsulation KAT of every
 * parameter set.  An array, not a pointer, so that it is an address
 * constant usable in the static initializers of the per-set binaries. */
const char ml_kem_ikme_hex[] =
    "a0a1a2a3a4a5a6a7a8a9aaabacadaeafb0b1b2b3b4b5b6b7b8b9babbbcbdbebf";

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
void ml_kem_test_gate(const struct ml_kem_set *set)
{
    char msg[128];

    if (!pkey_alg_available(set->name)) {
        snprintf(msg, sizeof(msg), "%s is unavailable", set->name);
        test_skip(msg);
    }
}

/* Deterministic key generation from the FIPS 203 (d, z) seed via the
 * keymgmt "seed" parameter (OSSL_PKEY_PARAM_ML_KEM_SEED; spelled as a
 * literal because the macro is new in 3.5). */
EVP_PKEY *ml_kem_keygen_from_seed(const struct ml_kem_set *set,
                                  unsigned char *seedbuf)
{
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(NULL, set->name, NULL);
    EVP_PKEY *pkey = NULL;
    OSSL_PARAM params[2];

    CHECK(ctx != NULL, "EVP_PKEY_CTX_new_from_name failed for an available ML-KEM");
    CHECK(EVP_PKEY_keygen_init(ctx) == 1, "EVP_PKEY_keygen_init failed");
    from_hex(set->seedhex, seedbuf, set->seed_len);
    params[0] = OSSL_PARAM_construct_octet_string("seed", seedbuf, set->seed_len);
    params[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_PKEY_CTX_set_params(ctx, params) == 1,
          "setting the ML-KEM keygen \"seed\" parameter failed");
    CHECK(EVP_PKEY_keygen(ctx, &pkey) == 1, "EVP_PKEY_keygen failed");
    CHECK(pkey != NULL, "EVP_PKEY_keygen produced no key");
    EVP_PKEY_CTX_free(ctx);
    return pkey;
}

/* Exports a raw key component.  Returns its length. */
size_t ml_kem_export_raw(EVP_PKEY *pkey, const char *param_name,
                         unsigned char *out, size_t outcap)
{
    size_t len = 0;

    CHECK(EVP_PKEY_get_octet_string_param(pkey, param_name, out, outcap, &len) == 1,
          "raw key export failed");
    return len;
}

/* Builds a peer EVP_PKEY by importing a raw encapsulation key with
 * EVP_PKEY_fromdata. */
EVP_PKEY *ml_kem_import_pub(const struct ml_kem_set *set,
                            const unsigned char *pub, size_t publen)
{
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(NULL, set->name, NULL);
    EVP_PKEY *pkey = NULL;
    OSSL_PARAM params[2];

    CHECK(ctx != NULL, "EVP_PKEY_CTX_new_from_name failed for an available ML-KEM");
    CHECK(EVP_PKEY_fromdata_init(ctx) == 1, "EVP_PKEY_fromdata_init failed");
    params[0] = OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_PUB_KEY,
                                                  (void *)pub, publen);
    params[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_PKEY_fromdata(ctx, &pkey, EVP_PKEY_PUBLIC_KEY, params) == 1,
          "EVP_PKEY_fromdata of the encapsulation key failed");
    CHECK(pkey != NULL, "EVP_PKEY_fromdata produced no key");
    EVP_PKEY_CTX_free(ctx);
    return pkey;
}

/* Encapsulates against `peer` with the fixed ephemeral randomness via the
 * KEM "ikme" parameter (OSSL_KEM_PARAM_IKME; spelled as a literal because
 * the macro is absent from pre-3.2 headers).  *ctlen and *sslen are in/out
 * capacities, per the EVP contract. */
void ml_kem_encapsulate_with_ikme(EVP_PKEY *peer, const unsigned char *ikme,
                                  size_t ikmelen, unsigned char *ct,
                                  size_t *ctlen, unsigned char *ss,
                                  size_t *sslen)
{
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(peer, NULL);
    OSSL_PARAM params[2];

    CHECK(ctx != NULL, "EVP_PKEY_CTX_new failed for the peer key");
    CHECK(EVP_PKEY_encapsulate_init(ctx, NULL) == 1,
          "EVP_PKEY_encapsulate_init failed");
    params[0] = OSSL_PARAM_construct_octet_string("ikme", (void *)ikme, ikmelen);
    params[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_PKEY_CTX_set_params(ctx, params) == 1,
          "setting the encapsulation \"ikme\" parameter failed");
    CHECK(EVP_PKEY_encapsulate(ctx, ct, ctlen, ss, sslen) == 1,
          "EVP_PKEY_encapsulate failed");
    EVP_PKEY_CTX_free(ctx);
}

/* Decapsulates `ct` with `pkey` into `ss` (in/out capacity in *sslen).
 * Returns what EVP_PKEY_decapsulate returned (1 on success; ML-KEM
 * decapsulation reports implicit rejection through a success code and a
 * pseudo-random shared secret, never through an error). */
int ml_kem_decapsulate(EVP_PKEY *pkey, const unsigned char *ct, size_t ctlen,
                       unsigned char *ss, size_t *sslen)
{
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(pkey, NULL);
    int rc;

    CHECK(ctx != NULL, "EVP_PKEY_CTX_new failed for the private key");
    CHECK(EVP_PKEY_decapsulate_init(ctx, NULL) == 1,
          "EVP_PKEY_decapsulate_init failed");
    rc = EVP_PKEY_decapsulate(ctx, ss, sslen, ct, ctlen);
    EVP_PKEY_CTX_free(ctx);
    return rc;
}
/* (1) Seed keygen reproduces the KAT encapsulation key, twice in a row
 * (key generation is deterministic), and exports a private key of the
 * FIPS 203 dk size. */
void ml_kem_test_seed_keygen_kat(const struct ml_kem_set *set)
{
    unsigned char *seed = ALLOCATE_KEY(set->seed_len);
    unsigned char *want_pub = ALLOCATE_OTHER(ML_KEM_MAX_PUB);
    unsigned char *pub1 = ALLOCATE_OUTPUT(ML_KEM_MAX_PUB);
    unsigned char *pub2 = ALLOCATE_OUTPUT(ML_KEM_MAX_PUB);
    unsigned char *priv = ALLOCATE_OUTPUT(ML_KEM_MAX_PRIV);
    EVP_PKEY *pkey;
    size_t pub1len, pub2len, privlen, want_len;
    char msg[128];

    pkey = ml_kem_keygen_from_seed(set, seed);
    want_len = from_hex(set->pubhex, want_pub, ML_KEM_MAX_PUB);

    pub1len = ml_kem_export_raw(pkey, OSSL_PKEY_PARAM_PUB_KEY, pub1,
                                ML_KEM_MAX_PUB);
    expect_equal("seed-derived encapsulation key does not match the KAT",
                 pub1, pub1len, want_pub, want_len);

    privlen = ml_kem_export_raw(pkey, OSSL_PKEY_PARAM_PRIV_KEY, priv,
                                ML_KEM_MAX_PRIV);
    snprintf(msg, sizeof(msg), "%s private key export is %zu bytes, want %zu",
             set->name, privlen, set->priv_len);
    CHECK(privlen == set->priv_len, msg);
    EVP_PKEY_free(pkey);

    /* Key generation from the same seed must be deterministic. */
    pkey = ml_kem_keygen_from_seed(set, seed);
    pub2len = ml_kem_export_raw(pkey, OSSL_PKEY_PARAM_PUB_KEY, pub2,
                                ML_KEM_MAX_PUB);
    expect_equal("repeated seed keygen produced a different encapsulation key",
                 pub2, pub2len, want_pub, want_len);
    EVP_PKEY_free(pkey);

    free_poison_allocations();
}

/* (2) Encapsulation KAT: importing the KAT encapsulation key and
 * encapsulating with the fixed ephemeral randomness reproduces the KAT
 * ciphertext and shared secret; repeating the encapsulation with the same
 * randomness reproduces them exactly; and decapsulating the KAT ciphertext
 * with the seed-derived private key yields the KAT shared secret. */
void ml_kem_test_encap_kat(const struct ml_kem_set *set)
{
    unsigned char *seed = ALLOCATE_KEY(set->seed_len);
    unsigned char *ikme = ALLOCATE_KEY(ML_KEM_SS_LEN);
    unsigned char *kat_pub = ALLOCATE_OTHER(ML_KEM_MAX_PUB);
    unsigned char *kat_ct = ALLOCATE_OUTPUT(ML_KEM_MAX_CT);
    unsigned char *kat_ss = ALLOCATE_OUTPUT(ML_KEM_SS_LEN);
    unsigned char *ct = ALLOCATE_OUTPUT(ML_KEM_MAX_CT);
    unsigned char *ss = ALLOCATE_OUTPUT(ML_KEM_SS_LEN);
    unsigned char *ss2 = ALLOCATE_OUTPUT(ML_KEM_SS_LEN);
    EVP_PKEY *pkey, *peer;
    size_t kat_publen, kat_ctlen, kat_sslen, ctlen, sslen, ss2len;

    pkey = ml_kem_keygen_from_seed(set, seed);
    kat_publen = from_hex(set->pubhex, kat_pub, ML_KEM_MAX_PUB);
    kat_ctlen = from_hex(set->cthex, kat_ct, ML_KEM_MAX_CT);
    kat_sslen = from_hex(set->sshex, kat_ss, ML_KEM_SS_LEN);
    from_hex(ml_kem_ikme_hex, ikme, ML_KEM_SS_LEN);
    peer = ml_kem_import_pub(set, kat_pub, kat_publen);

    ctlen = ML_KEM_MAX_CT;
    sslen = ML_KEM_SS_LEN;
    ml_kem_encapsulate_with_ikme(peer, ikme, ML_KEM_SS_LEN, ct, &ctlen,
                                 ss, &sslen);
    expect_equal("encapsulation ciphertext does not match the KAT",
                 ct, ctlen, kat_ct, kat_ctlen);
    expect_equal("encapsulation shared secret does not match the KAT",
                 ss, sslen, kat_ss, kat_sslen);

    /* A fresh encapsulation context with the same ikme must produce the
     * same ciphertext and shared secret again. */
    ctlen = ML_KEM_MAX_CT;
    ss2len = ML_KEM_SS_LEN;
    ml_kem_encapsulate_with_ikme(peer, ikme, ML_KEM_SS_LEN, ct, &ctlen,
                                 ss2, &ss2len);
    expect_equal("repeated encapsulation with the same ikme did not reproduce "
                 "the KAT shared secret",
                 ss2, ss2len, kat_ss, kat_sslen);

    /* Decapsulation with the seed-derived private key returns the KAT
     * shared secret. */
    ss2len = ML_KEM_SS_LEN;
    CHECK(ml_kem_decapsulate(pkey, kat_ct, kat_ctlen, ss2, &ss2len) == 1,
          "decapsulation of the KAT ciphertext failed");
    expect_equal("decapsulated shared secret does not match the KAT",
                 ss2, ss2len, kat_ss, kat_sslen);

    EVP_PKEY_free(peer);
    EVP_PKEY_free(pkey);

    free_poison_allocations();
}

/* (3) Implicit rejection: a tampered ciphertext decapsulates without any
 * error but yields a shared secret different from the KAT one.  Both the
 * first (encapsulation-key) byte and the last (ciphertext) byte are
 * tampered, one at a time. */
void ml_kem_test_tampered_ciphertext(const struct ml_kem_set *set)
{
    unsigned char *seed = ALLOCATE_KEY(set->seed_len);
    unsigned char *kat_ct = ALLOCATE_OUTPUT(ML_KEM_MAX_CT);
    unsigned char *kat_ss = ALLOCATE_OUTPUT(ML_KEM_SS_LEN);
    unsigned char *bad_ct = ALLOCATE_INPUT(ML_KEM_MAX_CT);
    unsigned char *ss = ALLOCATE_OUTPUT(ML_KEM_SS_LEN);
    EVP_PKEY *pkey;
    size_t kat_ctlen, kat_sslen, sslen;
    int rc;

    pkey = ml_kem_keygen_from_seed(set, seed);
    kat_ctlen = from_hex(set->cthex, kat_ct, ML_KEM_MAX_CT);
    kat_sslen = from_hex(set->sshex, kat_ss, ML_KEM_SS_LEN);

    memcpy(bad_ct, kat_ct, kat_ctlen);
    bad_ct[0] ^= 0x01;
    sslen = ML_KEM_SS_LEN;
    rc = ml_kem_decapsulate(pkey, bad_ct, kat_ctlen, ss, &sslen);
    CHECK(rc == 1, "decapsulation of a tampered ciphertext errored");
    CHECK(sslen == kat_sslen, "tampered-ciphertext shared secret size changed");
    unpoison_all();
    CHECK(memcmp(ss, kat_ss, kat_sslen) != 0,
          "tampered ciphertext did not trigger implicit rejection");
    repoison_all();

    memcpy(bad_ct, kat_ct, kat_ctlen);
    bad_ct[kat_ctlen - 1] ^= 0x80;
    sslen = ML_KEM_SS_LEN;
    rc = ml_kem_decapsulate(pkey, bad_ct, kat_ctlen, ss, &sslen);
    CHECK(rc == 1, "decapsulation of a tampered ciphertext errored");
    unpoison_all();
    CHECK(memcmp(ss, kat_ss, kat_sslen) != 0,
          "tampered ciphertext did not trigger implicit rejection");
    repoison_all();
    EVP_PKEY_free(pkey);

    free_poison_allocations();
}

/* (4) A raw private key exported from the seed-derived key pair can be
 * re-imported with EVP_PKEY_fromdata and decapsulates the KAT ciphertext
 * to the KAT shared secret. */
void ml_kem_test_privkey_import(const struct ml_kem_set *set)
{
    unsigned char *seed = ALLOCATE_KEY(set->seed_len);
    unsigned char *kat_ct = ALLOCATE_OUTPUT(ML_KEM_MAX_CT);
    unsigned char *kat_ss = ALLOCATE_OUTPUT(ML_KEM_SS_LEN);
    unsigned char *priv = ALLOCATE_OUTPUT(ML_KEM_MAX_PRIV);
    unsigned char *ss = ALLOCATE_OUTPUT(ML_KEM_SS_LEN);
    EVP_PKEY *pkey, *impkey = NULL;
    EVP_PKEY_CTX *ctx;
    OSSL_PARAM params[2];
    size_t kat_ctlen, kat_sslen, privlen, sslen;

    pkey = ml_kem_keygen_from_seed(set, seed);
    kat_ctlen = from_hex(set->cthex, kat_ct, ML_KEM_MAX_CT);
    kat_sslen = from_hex(set->sshex, kat_ss, ML_KEM_SS_LEN);
    privlen = ml_kem_export_raw(pkey, OSSL_PKEY_PARAM_PRIV_KEY, priv,
                                ML_KEM_MAX_PRIV);
    CHECK(privlen == set->priv_len, "private key export size mismatch");
    EVP_PKEY_free(pkey);

    ctx = EVP_PKEY_CTX_new_from_name(NULL, set->name, NULL);
    CHECK(ctx != NULL, "EVP_PKEY_CTX_new_from_name failed for an available ML-KEM");
    CHECK(EVP_PKEY_fromdata_init(ctx) == 1, "EVP_PKEY_fromdata_init failed");
    params[0] = OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_PRIV_KEY,
                                                  priv, privlen);
    params[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_PKEY_fromdata(ctx, &impkey, EVP_PKEY_PRIVATE_KEY, params) == 1,
          "EVP_PKEY_fromdata of the raw private key failed");
    EVP_PKEY_CTX_free(ctx);

    sslen = ML_KEM_SS_LEN;
    CHECK(ml_kem_decapsulate(impkey, kat_ct, kat_ctlen, ss, &sslen) == 1,
          "decapsulation with the imported private key failed");
    expect_equal("imported private key decapsulated to the wrong shared secret",
                 ss, sslen, kat_ss, kat_sslen);
    EVP_PKEY_free(impkey);

    free_poison_allocations();
}

/* test_ml_kem_common.h -- shared machinery for the ML-KEM family tests.
 *
 * One binary per parameter set (test_ml_kem_512.c, test_ml_kem_768.c and
 * test_ml_kem_1024.c); each binary defines one struct ml_kem_set row with
 * its own FIPS 203 KAT constants and calls the per-set test routines below.
 * The big explanatory comment (the determinism hooks, the KAT provenance
 * and the version gate) lives in test_ml_kem_common.c.
 */

#ifndef TEST_ML_KEM_COMMON_H
#define TEST_ML_KEM_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* One row per FIPS 203 parameter set: the algorithm name, the FIPS 203
 * component sizes, and the KAT hex for the (d, z) seed, the encapsulation
 * key derived from it, and the ciphertext/shared-secret pair produced by
 * encapsulating against that key with the family-wide fixed ephemeral
 * randomness (ml_kem_ikme_hex in the common module). */
struct ml_kem_set {
    const char *name;
    size_t seed_len;    /* (d, z) seed: 64 bytes for every set */
    size_t pub_len;     /* ek:  800 / 1184 / 1568 */
    size_t priv_len;    /* dk:  1632 / 2400 / 3168 */
    size_t ct_len;      /* ct:  768 / 1088 / 1568 */
    size_t ss_len;      /* ss:  32 for every set */
    const char *seedhex;
    const char *pubhex;
    const char *cthex;
    const char *sshex;
};

/* Largest FIPS 203 component sizes across the family (working-buffer caps;
 * the KAT ciphertexts are tampered in place and the raw keys are exported
 * into buffers of exactly these sizes). */
#define ML_KEM_MAX_PUB  1568
#define ML_KEM_MAX_PRIV 3168
#define ML_KEM_MAX_CT   1568
#define ML_KEM_SS_LEN   32

/* The fixed encapsulation ephemeral randomness ("ikmE", 32 bytes), shared
 * by every parameter set's encapsulation KAT.  Declared as an array, not a
 * pointer, so that it is an address constant usable where the per-set
 * binaries need it. */
extern const char ml_kem_ikme_hex[];

/* The whole-program availability gate: ML-KEM arrived in OpenSSL 3.5, so a
 * parameter set this OpenSSL does not provide takes the SKIP path with the
 * original reason text (the original monolith probed all three sets and
 * skipped on the first unavailable one; each binary now probes its own).
 * Call right after test_parse_args(). */
void ml_kem_test_gate(const struct ml_kem_set *set);

/* Deterministic key generation from the FIPS 203 (d, z) seed via the
 * keymgmt "seed" parameter (OSSL_PKEY_PARAM_ML_KEM_SEED; spelled as a
 * literal because the macro is new in 3.5).  seedbuf must have room for
 * set->seed_len bytes. */
EVP_PKEY *ml_kem_keygen_from_seed(const struct ml_kem_set *set,
                                  unsigned char *seedbuf);

/* Exports a raw key component (OSSL_PKEY_PARAM_PUB_KEY or
 * OSSL_PKEY_PARAM_PRIV_KEY).  Returns its length. */
size_t ml_kem_export_raw(EVP_PKEY *pkey, const char *param_name,
                         unsigned char *out, size_t outcap);

/* Builds a peer EVP_PKEY by importing a raw encapsulation key with
 * EVP_PKEY_fromdata. */
EVP_PKEY *ml_kem_import_pub(const struct ml_kem_set *set,
                            const unsigned char *pub, size_t publen);

/* Encapsulates against `peer` with the fixed ephemeral randomness via the
 * KEM "ikme" parameter (OSSL_KEM_PARAM_IKME; spelled as a literal because
 * the macro is absent from pre-3.2 headers).  *ctlen and *sslen are in/out
 * capacities, per the EVP contract. */
void ml_kem_encapsulate_with_ikme(EVP_PKEY *peer, const unsigned char *ikme,
                                  size_t ikmelen, unsigned char *ct,
                                  size_t *ctlen, unsigned char *ss,
                                  size_t *sslen);

/* Decapsulates `ct` with `pkey` into `ss` (in/out capacity in *sslen).
 * Returns what EVP_PKEY_decapsulate returned (1 on success; ML-KEM
 * decapsulation reports implicit rejection through a success code and a
 * pseudo-random shared secret, never through an error). */
int ml_kem_decapsulate(EVP_PKEY *pkey, const unsigned char *ct, size_t ctlen,
                       unsigned char *ss, size_t *sslen);

/* (1) Seed keygen reproduces the KAT encapsulation key, twice in a row
 * (key generation is deterministic), and exports a private key of the
 * FIPS 203 dk size. */
void ml_kem_test_seed_keygen_kat(const struct ml_kem_set *set);

/* (2) Encapsulation KAT: importing the KAT encapsulation key and
 * encapsulating with the fixed ephemeral randomness reproduces the KAT
 * ciphertext and shared secret; repeating the encapsulation with the same
 * randomness reproduces them exactly; and decapsulating the KAT ciphertext
 * with the seed-derived private key yields the KAT shared secret. */
void ml_kem_test_encap_kat(const struct ml_kem_set *set);

/* (3) Implicit rejection: a tampered ciphertext decapsulates without any
 * error but yields a shared secret different from the KAT one.  Both the
 * first (encapsulation-key) byte and the last (ciphertext) byte are
 * tampered, one at a time. */
void ml_kem_test_tampered_ciphertext(const struct ml_kem_set *set);

/* (4) A raw private key exported from the seed-derived key pair can be
 * re-imported with EVP_PKEY_fromdata and decapsulates the KAT ciphertext
 * to the KAT shared secret. */
void ml_kem_test_privkey_import(const struct ml_kem_set *set);

#endif /* TEST_ML_KEM_COMMON_H */
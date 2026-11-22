/* test_ml_dsa_common.h -- shared machinery for the ML-DSA family tests.
 *
 * One binary per parameter set (test_ml_dsa_44.c, test_ml_dsa_65.c and
 * test_ml_dsa_87.c); each binary defines one struct ml_dsa_set row with
 * its own FIPS 204 KAT constants and calls the per-set test routines below.
 * The big explanatory comment (the determinism hooks, the KAT provenance
 * and the version gate) lives in test_ml_dsa_common.c.
 */

#ifndef TEST_ML_DSA_COMMON_H
#define TEST_ML_DSA_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* One row per FIPS 204 parameter set: the algorithm name, the FIPS 204
 * component sizes, and the KAT hex for the (xi) seed and the deterministic
 * one-shot signature of the family-wide 8-byte KAT message. */
struct ml_dsa_set {
    const char *name;
    size_t seed_len;    /* xi: 32 bytes for every set */
    size_t pub_len;     /* pk:  1312 / 1952 / 2592 */
    size_t priv_len;    /* sk:  2560 / 4032 / 4896 */
    size_t sig_len;     /* sig: 2420 / 3309 / 4627 */
    const char *seedhex;
    const char *sighex;
};

/* Largest FIPS 204 component sizes across the family (working-buffer caps;
 * signatures are tampered in place and the raw keys are exported into
 * buffers of exactly these sizes). */
#define ML_DSA_MAX_PUB  2592
#define ML_DSA_MAX_PRIV 4896
#define ML_DSA_MAX_SIG  4627

/* The fixed 8-byte KAT message (00 01 02 03 04 05 06 07), shared by every
 * parameter set, and its byte length.  Declared as an array, not a pointer,
 * so that it is an address constant usable where the per-set binaries need
 * it. */
extern const char ml_dsa_msg8_hex[];
#define ML_DSA_MSG_LEN 8

/* A non-empty signing/verification context string (FIPS 204 ctx), shared by
 * every parameter set, and its byte length. */
extern const char ml_dsa_ctxstr[];
#define ML_DSA_CTXSTR_LEN 12

/* The whole-program availability gate: ML-DSA arrived in OpenSSL 3.5, so a
 * parameter set this OpenSSL does not provide takes the SKIP path with the
 * original reason text.  Call right after test_parse_args(). */
void ml_dsa_test_gate(const struct ml_dsa_set *set);

/* Sets the integer "deterministic" signature parameter
 * (OSSL_SIGNATURE_PARAM_DETERMINISTIC; spelled as a literal because the
 * macro is new in 3.5).  With it set, the 32-byte signing randomizer is
 * zeroed and signatures are fully deterministic given (key, message,
 * context string). */
void ml_dsa_set_deterministic(EVP_PKEY_CTX *pctx);

/* Deterministic key generation from the FIPS 204 seed xi via the keymgmt
 * "seed" parameter (OSSL_PKEY_PARAM_ML_DSA_SEED; spelled as a literal
 * because the macro is new in 3.5).  seedbuf must have room for
 * set->seed_len bytes. */
EVP_PKEY *ml_dsa_keygen_from_seed(const struct ml_dsa_set *set,
                                  unsigned char *seedbuf);

/* One-shot deterministic EVP_DigestSign of a message.  *siglen is an in/out
 * capacity per the EVP contract. */
void ml_dsa_sign_one_shot(EVP_PKEY *pkey, const unsigned char *msg,
                          size_t msglen, unsigned char *sig, size_t *siglen);

/* One-shot deterministic verify.  Returns 1 only for a valid signature. */
int ml_dsa_verify_one_shot(EVP_PKEY *pkey, const unsigned char *sig,
                           size_t siglen, const unsigned char *msg,
                           size_t msglen);

/* (1) Seed keygen: exports keys of the FIPS 204 pk/sk sizes and is
 * reproducible run-to-run. */
void ml_dsa_test_seed_keygen(const struct ml_dsa_set *set);

/* (2) One-shot signing KAT: the deterministic signature of the 8-byte KAT
 * message matches the embedded bytes (FIPS 204 signature size), signs
 * identically when repeated, verifies, and rejects tampered signatures and
 * wrong messages.  The default (randomized) mode differs from the
 * deterministic signature but still verifies. */
void ml_dsa_test_oneshot_kats(const struct ml_dsa_set *set);

/* (3) Context-string binding: signatures made with a context string verify
 * only under the same context string, and differ from the no-context
 * signature. */
void ml_dsa_test_context_string(const struct ml_dsa_set *set);

#if OPENSSL_VERSION_NUMBER >= 0x30600000L
/* (4) Streaming signing and verification of a ~1MB message fed in three
 * chunks, via EVP_PKEY_sign_message_* and EVP_PKEY_verify_message_* (3.6+;
 * see the file comment in test_ml_dsa_common.c).  The streaming signature
 * must equal the one-shot EVP_DigestSign signature over the same message
 * (deterministic mode), the streaming verification must accept it, and a
 * one-shot verification of the streaming signature over the full message
 * must also accept it, while a tampered streaming signature is rejected.
 * Compiled only against 3.6+ headers: these symbols do not exist in
 * OpenSSL 3.0 (the program skips there long before reaching this section). */
void ml_dsa_test_streaming(const struct ml_dsa_set *set);
#endif

#endif /* TEST_ML_DSA_COMMON_H */
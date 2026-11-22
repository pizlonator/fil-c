/* test_slh_dsa_common.h -- shared machinery for the SLH-DSA family tests.
 *
 * One binary per parameter set (test_slh_dsa_sha2_128f.c,
 * test_slh_dsa_shake_128f.c and test_slh_dsa_sha2_192f.c); each binary
 * defines one struct slh_dsa_set row with its own public-key KAT constant
 * and calls the per-set test routines below.  The big explanatory comment
 * (the determinism hooks, the spec adaptation note, the KAT provenance and
 * the version gate) lives in test_slh_dsa_common.c.
 */

#ifndef TEST_SLH_DSA_COMMON_H
#define TEST_SLH_DSA_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* One row per FIPS 205 parameter set: the algorithm name, the FIPS 205
 * component sizes, and the KAT hex for the key-derivation seed and the raw
 * public key exported from it.  The 128-bit sets share the family-wide
 * 48-byte seed (slh_seed_128_hex), the 192-bit set uses the 72-byte one
 * (slh_seed_192_hex). */
struct slh_dsa_set {
    const char *name;
    size_t seed_len;    /* (SK.seed, SK.prf, PUB.seed): 3n = 48 / 48 / 72 */
    size_t pub_len;     /* pk:  32 / 32 / 48 */
    size_t sig_len;     /* sig: 17088 / 17088 / 35664 */
    const char *seedhex;
    const char *pubhex;
};

/* The FIPS 205 key-derivation seeds, family-wide: 0x00,0x01,...,0x2f (48
 * bytes, 3n for n = 16) shared by the two 128-bit sets, and
 * 0x00,0x01,...,0x47 (72 bytes, 3n for n = 24) for the 192-bit set.
 * Declared as arrays, not pointers, so that they are address constants
 * usable in the static struct slh_dsa_set initializers of the per-set
 * binaries. */
extern const char slh_seed_128_hex[];
extern const char slh_seed_192_hex[];

/* The fixed 16-byte KAT message (00 01 ... 0f), shared by every parameter
 * set, and its byte length.  An array, not a pointer, so that it is an
 * address constant usable where the per-set binaries need it. */
extern const char slh_msg16_hex[];
#define SLH_MSG_LEN 16

/* A non-empty signing/verification context string (FIPS 205 ctx), shared by
 * every parameter set, and its byte length. */
extern const char slh_ctxstr[];
#define SLH_CTXSTR_LEN 12

/* Largest FIPS 205 component sizes across the family (working-buffer caps;
 * signatures are tampered in place and the raw keys are exported into
 * buffers of exactly these sizes). */
#define SLH_MAX_PUB 48
#define SLH_MAX_SIG 35664

/* The whole-program availability gate: SLH-DSA arrived in OpenSSL 3.5, so a
 * parameter set this OpenSSL does not provide takes the SKIP path with the
 * original reason text.  Call right after test_parse_args(). */
void slh_test_gate(const struct slh_dsa_set *set);

/* Sets the integer "deterministic" signature parameter
 * (OSSL_SIGNATURE_PARAM_DETERMINISTIC; spelled as a literal because the
 * macro is new in 3.5).  SLH-DSA signing is deterministic given (key,
 * message, context string) with it set; by default OpenSSL uses randomized
 * (hedged) signing with per-signature randomness. */
void slh_set_deterministic(EVP_PKEY_CTX *pctx);

/* Deterministic key generation from the FIPS 205 seed via the keymgmt
 * "seed" parameter (OSSL_PKEY_PARAM_SLH_DSA_SEED; spelled as a literal
 * because the macro is new in 3.5).  seedbuf must have room for
 * set->seed_len bytes. */
EVP_PKEY *slh_keygen_from_seed(const struct slh_dsa_set *set,
                               unsigned char *seedbuf);

/* One-shot deterministic EVP_DigestSign.  *siglen is an in/out capacity per
 * the EVP contract. */
void slh_sign_one_shot(EVP_PKEY *pkey, const unsigned char *msg,
                       size_t msglen, unsigned char *sig, size_t *siglen);

/* One-shot verify.  Returns 1 only for a valid signature. */
int slh_verify_one_shot(EVP_PKEY *pkey, const unsigned char *sig,
                        size_t siglen, const unsigned char *msg,
                        size_t msglen);

/* (1) Seed keygen: reproduces the KAT public key and is reproducible
 * run-to-run. */
void slh_test_seed_keygen_kat(const struct slh_dsa_set *set);

/* (2) One-shot signing: the NULL-buffer EVP_DigestSign call reports the
 * FIPS 205 signature size; deterministic signing of the 16-byte KAT message
 * is reproducible, verifies, rejects tampered signatures and wrong
 * messages; randomized (default) signing differs but verifies. */
void slh_test_sign_verify(const struct slh_dsa_set *set);

/* (3) Context-string binding: signatures made with a context string verify
 * only under the same context string, and differ from the no-context
 * signature. */
void slh_test_context_string(const struct slh_dsa_set *set);

/* (4) Large message: a ~256 KiB message signs and verifies one-shot in
 * both deterministic and randomized modes (see the spec adaptation note in
 * the file comment: SLH-DSA has no streaming interface in this OpenSSL). */
void slh_test_large_message(const struct slh_dsa_set *set);

#endif /* TEST_SLH_DSA_COMMON_H */
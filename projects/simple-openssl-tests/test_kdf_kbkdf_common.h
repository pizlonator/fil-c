/* test_kdf_kbkdf_common.h -- shared machinery for the KBKDF-SHA1 / KBKDF-
 * SHA256 / KBKDF-SHA512 family tests (EVP_KDF "KBKDF", SP 800-108).
 *
 * One binary per digest (test_kdf_kbkdf_sha1.c, test_kdf_kbkdf_sha256.c and
 * test_kdf_kbkdf_sha512.c); each binary defines one struct kbkdf_family row
 * (its OSSL_KDF_PARAM_DIGEST value for the HMAC PRF, the digest's prose
 * name for the failure messages, and the PRF output size), its own digest-
 * specific KAT constants and rows, then calls the per-digest test routines
 * below.  The big explanatory comment (the SP 800-108 / CAVS provenance,
 * the parameter naming quirk, the CLI verification commands and the
 * distribution decisions) lives in test_kdf_kbkdf_common.c.
 *
 * Distribution rule (carried out of the monolith): the CMAC-AES128 and
 * KMAC128 PRFs take a cipher or a MAC rather than a digest, so their checks
 * are digest-independent and run in ONE binary -- the representative digest
 * the monolith's parameterized sections ran on (SHA-256), whose binary also
 * carries their constants.  The "r" width behavior and the missing-input
 * negatives generalize and run in every binary for its own digest.
 */

#ifndef TEST_KDF_KBKDF_COMMON_H
#define TEST_KDF_KBKDF_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* One row per digest: the OSSL_KDF_PARAM_DIGEST value passed with the HMAC
 * PRF, the digest's prose name (for the failure messages, where the
 * original monolith wrote "SHA-256" with the hyphen), and the PRF output
 * size (the feedback-mode IV must be exactly this long). */
struct kbkdf_family {
    const char *digest;
    const char *digest_prose;
    size_t mdsize;
};

/* One counter-mode r-width KAT row of the calling binary: the PRF (mac,
 * optionally a cipher and/or a digest), its inputs and expected output, and
 * the counter width r.  `label` names the PRF in the failure messages
 * ("CMAC-AES128", "HMAC-SHA1", ...), exactly as the original monolith's
 * messages did. */
struct kbkdf_r_kat {
    const char *label;
    const char *mac;
    const char *cipher;   /* NULL: no cipher param */
    const char *digest;   /* NULL: no digest param */
    const char *keyhex;
    const char *infohex;
    const char *outhex;
    int r;
};

/* Shared KAT material: the small HMAC key and info every binary's counter
 * switch/behavior checks and negatives run over (the original monolith's
 * KSMALL_HEX/ISMALL_HEX; the switch KAT *values* themselves are SHA-256
 * vectors and stay in the SHA-256 binary).  Declared as arrays, not
 * pointers, so that they are address constants usable in the per-digest
 * binaries' static struct initializers. */
extern const char kbkdf_ksmall_hex[];
extern const char kbkdf_ismall_hex[];

/* (1) Counter mode, CMAC-AES128, default 32-bit counter: the CAVS KAT.
 * Digest-independent (CMAC takes a cipher, not a digest): only the
 * representative digest's binary runs it. */
void kbkdf_test_counter_cmac(const char *keyhex, const char *infohex,
                             const char *outhex);

/* (2) Counter mode with the HMAC PRF of the calling binary's digest: the
 * use-l / use-separator switch behavior, then the "r" counter width where
 * the release supports it.  kat_default_hex / kat_use_l_hex /
 * kat_use_sep_hex are the verified switch KATs (the SHA-256 binary's rows);
 * a NULL KAT means this digest has none, and the switch behavior is then
 * checked by difference -- use-l=1 and use-separator=1 must both change the
 * output (the behavior the monolith's CLI-verified KAT constants embody).
 * The rkats rows run under the "r" feature gate (EVP_KDF_settable_ctx_
 * params, since the parameter does not exist before OpenSSL 3.1), followed
 * by the r=12 rejection over the shared small material; without "r" the
 * section takes the original note path. */
void kbkdf_test_counter_hmac(const struct kbkdf_family *fam,
                             const char *kat_default_hex,
                             const char *kat_use_l_hex,
                             const char *kat_use_sep_hex,
                             const struct kbkdf_r_kat *rkats, size_t nrkats);

/* (3) Feedback mode with the HMAC PRF of the calling binary's digest, with
 * and without the IV ("seed", whose length must equal the PRF output size).
 * ivhex is the IV input (the SHA-256 binary's verified 32-byte pattern,
 * which is mdsize bytes there); NULL means the same 000102.. byte pattern
 * is built up to mdsize bytes instead.  kat_noiv_hex / kat_withiv_hex are
 * the verified KATs (the SHA-256 binary's rows); a NULL KAT means this
 * digest has none, and the section then checks the behavior: the no-IV
 * derive succeeds and its first block equals the counter-mode default
 * output (the property the monolith's no-IV KAT embodies -- re-verified
 * with the CLI on 3.0.13 and 3.6.5 for every digest of the family), the
 * mdsize-byte IV derive succeeds and changes the output, and a one-byte-
 * short IV is rejected. */
void kbkdf_test_feedback_hmac(const struct kbkdf_family *fam,
                              const char *ivhex, const char *kat_noiv_hex,
                              const char *kat_withiv_hex);

/* (3b) Feedback mode with CMAC-AES128 and a 16-byte IV.  Digest-independent
 * (CMAC takes a cipher, not a digest): only the representative digest's
 * binary runs it. */
void kbkdf_test_feedback_cmac(const char *keyhex, const char *infohex,
                              const char *ivhex, const char *outhex);

/* (4) KMAC128 as the PRF (OpenSSL 3.1+; 3.0 has the KMAC MAC itself but
 * KBKDF rejects it as a PRF, so the check gates on the version).  Digest-
 * independent (KMAC takes a MAC, not a digest): only the representative
 * digest's binary runs it. */
void kbkdf_test_kmac(const char *keyhex, const char *labelhex,
                     const char *infohex, const char *outhex);

/* (4b) Three info chunks of the calling binary's HMAC-digest CAVS data
 * reproduce the single-chunk result where both "r" and repeated-info
 * concatenation exist, gated on the "r" parameter with the original
 * early-return and the pre-3.2 note.  Digest-specific: only the digest
 * whose CAVS data it is runs it. */
void kbkdf_test_multi_info3(const char *keyhex, const char *infohex,
                            const char *outhex);

/* (5) A zero output length must fail, and so must a missing key -- with the
 * calling binary's own digest over the shared small material. */
void kbkdf_test_negative(const struct kbkdf_family *fam);

#endif /* TEST_KDF_KBKDF_COMMON_H */

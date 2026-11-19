/* test_aes_ccm_common.h -- shared machinery for the AES-CCM family tests.

 * One binary per cipher and payload regime:

 *   test_aes_{128,192,256}_ccm_small.c -- the key size's RFC 3610 KAT rows
 *       plus the parameter-matrix and ordering-rule behavior sections;
 *   test_aes_{128,192}_ccm_large.c -- the 1 MiB single-update regime (only
 *       128/192, as in the original combined program) with its own behavior
 *       sections.
 *
 * One common module (this one) serves both suites: it exports the
 * parameterized single-update CCM helpers plus the ccm_small_* and
 * ccm_large_* test routines.  Each binary defines its own struct ccm_kat
 * row(s) with its cipher's KAT constants and calls the routines with its
 * cipher name and key (the shared RFC 3610 #1 tuple material lives in this
 * module as arrays).  The big explanatory comment (ordering rules, vector
 * provenance) lives in test_aes_ccm_common.c.
 */

#ifndef TEST_AES_CCM_COMMON_H
#define TEST_AES_CCM_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* One CCM known-answer row: the label, the cipher name, and the tuple as
 * hex strings (RFC 3610 shape: M = 8, 13-byte nonce, cleartext header used
 * as AAD). */
struct ccm_kat {
    const char *label;
    const char *ciphername;
    const char *keyhex;
    const char *noncehex;
    const char *aadhex;
    const char *pthex;
    const char *cthex;
    const char *taghex;
};

/* The RFC 3610 packet vector #1 tuple and its derived 24-byte-key
 * reshaping: shared by the small and large binaries of their key size.
 * Declared as arrays, not pointers, so that they are address constants
 * usable in the static struct ccm_kat initializers of the per-cipher
 * binaries (nonce, AAD, and plaintext are the same for every reshaping;
 * the small-256 binary's derived reshaping stays inline there, since only
 * that binary uses it). */
extern const char ccm_rfc3610_key_hex[];
extern const char ccm_rfc3610_nonce_hex[];
extern const char ccm_rfc3610_aad_hex[];
extern const char ccm_rfc3610_pt_hex[];
extern const char ccm_rfc3610_ct_hex[];
extern const char ccm_rfc3610_tag_hex[];
extern const char ccm_rfc3610_192_key_hex[];
extern const char ccm_rfc3610_192_ct_hex[];
extern const char ccm_rfc3610_192_tag_hex[];

/* ------------------------------------------------------------------ */
/* Small-payload suite (RFC 3610 rows + matrix + ordering rules).      */
/* ------------------------------------------------------------------ */

/* (s1) The RFC 3610 KAT rows (and derived reshapings) of the binary's
 * cipher: encrypt == KAT and decrypting the KAT gives back the plaintext
 * (ivlen 13, taglen 8, as published). */
void ccm_small_test_kats(const struct ccm_kat *kats, size_t nkats);

/* (s2) Roundtrip matrix: taglen {4,8,12,16} x nonce {7,12,13} x payload
 * {0,1,15,16,17} x AAD {0,32}, each with a roundtrip and a tampered-tag
 * rejection, plus a wrong-nonce rejection. */
void ccm_small_test_matrix(const char *ciphername);

/* (s3) A second payload update on the same context must fail: the payload
 * length is fixed by the first update (encrypt and decrypt side). */
void ccm_small_test_second_data_update(const char *ciphername);

/* (s4) AAD-only operation (empty payload) is valid, including decrypt; and
 * a nonzero AAD update before the length announcement must fail. */
void ccm_small_test_aad_only(const char *ciphername);

/* (s5) Parameter rules: the defaults are ivlen 7 / taglen 12; nonce lengths
 * outside 7..13 and odd or out-of-range tag lengths are rejected. */
void ccm_small_test_param_rules(const char *ciphername);

/* ------------------------------------------------------------------ */
/* Large-payload suite (1 MiB single-update regime).                   */
/* ------------------------------------------------------------------ */

/* (l1) KAT anchor: the RFC 3610 #1 row of the binary's key size through the
 * same single-update helpers used for the 1 MiB payload. */
void ccm_large_test_kat_anchor(const struct ccm_kat *kat);

/* (l2) The 1 MiB payload with 255 bytes of AAD round-trips in a single
 * update, and the output buffer obeys the EVP size contract. */
void ccm_large_test_roundtrip(const char *ciphername);

/* (l3) A second data update after the 1 MiB payload must fail. */
void ccm_large_test_second_data_update(const char *ciphername);

/* (l4) A zero-length payload (AAD-only record) is valid and round-trips. */
void ccm_large_test_zero_length_payload(const char *ciphername);

/* (l5) Tampering with the ciphertext or the tag must be detected inside the
 * payload update. */
void ccm_large_test_tamper(const char *ciphername);

/* (l6) Output-size contract: CCM reports block_size 1, so the EVP contract
 * lets a caller pass an update buffer of exactly inl + 1 bytes; the update
 * must emit exactly inl bytes and Final none. */
void ccm_large_test_output_buffer_contract(const char *ciphername);

#endif /* TEST_AES_CCM_COMMON_H */

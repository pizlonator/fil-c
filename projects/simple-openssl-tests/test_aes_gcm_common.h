/* test_aes_gcm_common.h -- shared machinery for the AES-GCM family tests.

 * One binary per cipher and payload regime:

 *   test_aes_{128,192,256}_gcm_small.c -- payloads <= 64 bytes: the key
 *       size's McGrew/Viega KAT rows and the small-payload behavior
 *       sections;
 *   test_aes_{128,192,256}_gcm_large.c -- the 2 MiB streaming-vs-one-shot
 *       regime with its own behavior sections.
 *
 * One common module (this one) serves both suites: it exports the
 * parameterized GCM helpers plus the gcm_small_* and gcm_large_* test
 * routines.  Each binary defines its own struct gcm_kat row(s) with its
 * cipher's KAT constants and calls the routines with its cipher name and
 * key (the shared key/IV/AAD/plaintext material lives in this module as
 * arrays).  The big explanatory comment (vector provenance, the 2 MiB
 * chunk-grid rationale, the GHASH regimes) lives in test_aes_gcm_common.c.
 */

#ifndef TEST_AES_GCM_COMMON_H
#define TEST_AES_GCM_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* One GCM known-answer row: the McGrew/Viega label, the cipher name, and
 * the tuple as hex strings ("" means "no AAD" / "empty plaintext"; cthex
 * and taghex may be NULL for rows that only carry part of the tuple -- the
 * large-suite anchor uses that to pin just a tag). */
struct gcm_kat {
    const char *label;
    const char *ciphername;
    const char *keyhex;
    const char *ivhex;
    const char *aadhex;  /* "" means no AAD */
    const char *pthex;   /* "" means empty plaintext */
    const char *cthex;   /* NULL means the row carries no ciphertext KAT */
    const char *taghex;  /* NULL means the row carries no tag KAT */
};

/* Shared KAT material: the McGrew/Viega TC3/TC4 key/IV/AAD/plaintext shapes
 * used by every GCM behavior section, plus the tuple constants that more
 * than one binary of a size needs (the small and the large binary of the
 * same key size both anchor to TC3/TC4).  Declared as arrays, not pointers,
 * so that they are address constants usable in the static struct gcm_kat
 * initializers of the per-cipher binaries. */
extern const char gcm_key_128_hex[];
extern const char gcm_key_192_hex[];
extern const char gcm_key_256_hex[];
extern const char gcm_iv12_hex[];
extern const char gcm_aad20_hex[];
extern const char gcm_pt60_hex[];
extern const char gcm_pt64_hex[];
extern const char gcm_tc3_ct_hex[];
extern const char gcm_tc3_tag_hex[];
extern const char gcm_tc4_tag_hex[];
extern const char gcm_tc3_192_ct_hex[];
extern const char gcm_tc3_192_tag_hex[];

/* ------------------------------------------------------------------ */
/* Small-payload suite (payloads <= 64 bytes; one call per section).   */
/* ------------------------------------------------------------------ */

/* (s1) One-shot encrypt == KAT for every row the binary passes in,
 * including AAD ordering (AAD first with out == NULL, then data, then
 * Final, then GET_TAG). */
void gcm_small_test_kats(const struct gcm_kat *kats, size_t nkats);

/* (s2) Decrypt with the right tag succeeds and recovers the plaintext; with
 * a tampered tag DecryptFinal must fail.  Runs the TC4 shape (IV12, 20
 * bytes of AAD, 60-byte payload) of the given cipher. */
void gcm_small_test_decrypt_and_tamper(const char *ciphername,
                                       const char *keyhex);

/* (s3) Splitting the 64-byte no-AAD KAT row's plaintext into updates of
 * {5,5,6,16,32} bytes must give the same ciphertext and the same tag as the
 * one-shot update. */
void gcm_small_test_split_updates(const struct gcm_kat *kat);

/* (4) AAD after the first data update must fail. */
void gcm_small_test_aad_ordering(const char *ciphername, const char *keyhex);

/* (s5) IV lengths: the default is 12 (queried through OSSL_PARAM); 0 is
 * rejected; 8, 16, and 17 round-trip via EVP_CTRL_AEAD_SET_IVLEN. */
void gcm_small_test_iv_lengths(const char *ciphername, const char *keyhex);

/* (s6) Short tags: asking for a 12-byte tag via the
 * OSSL_CIPHER_PARAM_AEAD_TAGLEN param at init time yields the 12-byte
 * prefix of the 16-byte TC4-shape tag, and decryption with the short tag
 * round-trips.  tag12hex is that prefix. */
void gcm_small_test_short_tag(const char *ciphername, const char *keyhex,
                              const char *tag12hex);

/* (s7) Zero-length payload: AAD-only encryption emits no ciphertext, just
 * the tag (the classic "tag-only" GCM self-test); aadonlytag_hex is the
 * expected tag. */
void gcm_small_test_zero_length_payload(const char *ciphername,
                                        const char *keyhex,
                                        const char *aadonlytag_hex);

/* (s8) Update output-buffer contract: a single 20-byte GCM update into a
 * buffer of exactly inl + 16 bytes emits exactly inl bytes (GCM is a stream
 * cipher), Final adds nothing, and the tag is still retrievable. */
void gcm_small_test_output_buffer_contract(const char *ciphername,
                                           const char *keyhex);

/* ------------------------------------------------------------------ */
/* Large-payload suite (2 MiB payloads; one call per section).         */
/* ------------------------------------------------------------------ */

/* (l1) The one-shot and streaming helpers agree with the anchor KAT rows
 * the binary passes in, both one-shot and with the row plaintext pushed
 * through the large chunk list.  Rows may carry only a tag (cthex NULL) or
 * neither (both NULL). */
void gcm_large_test_kat_anchor(const struct gcm_kat *kats, size_t nkats);

/* (l2) 2 MiB one-shot == streaming, encrypt and decrypt, for the given
 * cipher.  The 2 MiB payload is deterministic but arbitrary. */
void gcm_large_test_stream_equals_one_shot(const char *ciphername,
                                           const char *keyhex);

/* (l3) Same as (l2) but with a 1 KiB AAD fed before the data; AAD must be
 * counted identically by both update strategies. */
void gcm_large_test_with_aad(const char *ciphername, const char *keyhex);

/* (l4) AAD updates interleaved between data updates must fail (an AAD
 * update before any data is still legal and is checked first so the
 * failure is attributable to the interleaving). */
void gcm_large_test_aad_interleaved(const char *ciphername,
                                    const char *keyhex);

/* (l5) IV lengths 8/12/16 must round-trip the full 2 MiB payload (J0
 * computation differs for non-12-byte IVs; gcm128.c GHASHes long IVs). */
void gcm_large_test_iv_lengths(const char *ciphername, const char *keyhex);

/* (l6) Flipping a bit of the final ciphertext block (and of the last
 * ciphertext byte) must be detected by DecryptFinal. */
void gcm_large_test_tampered_final_block(const char *ciphername,
                                         const char *keyhex);

#endif /* TEST_AES_GCM_COMMON_H */

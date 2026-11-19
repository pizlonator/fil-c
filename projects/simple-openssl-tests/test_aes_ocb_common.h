/* test_aes_ocb_common.h -- shared machinery for the AES-OCB family tests
 * (the small-payload regime and the >= 1 MiB regime).
 *
 * One binary per cipher: test_aes_{128,192,256}_ocb_small.c (RFC 7253
 * KAT rows, nonce/tag lengths, partial blocks, AAD after data, tamper) and
 * test_aes_{192,256}_ocb_large.c (1 MiB streaming vs one-shot, large AAD,
 * tampered final block).  Each binary defines its cipher's struct ocb_kat
 * rows and then calls the per-cipher test routines below.  The big
 * explanatory comments (the OCB update rules and their provenance, the
 * vector-derivation notes, the large-payload self-consistency oracle) live
 * in test_aes_ocb_common.c.
 */

#ifndef TEST_AES_OCB_COMMON_H
#define TEST_AES_OCB_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* One RFC 7253 Appendix A tuple (or a derived tuple in the same format):
 * the label used in the check messages, the cipher under test, its key,
 * the nonce, the AAD and plaintext ("" means empty), the ciphertext (""
 * means a tag-only tuple) and the tag. */
struct ocb_kat {
    const char *label;
    const char *ciphername;
    const char *keyhex;
    const char *noncehex;
    const char *aadhex;  /* "" means empty AAD */
    const char *pthex;   /* "" means empty plaintext */
    const char *cthex;   /* "" means tag-only tuple */
    const char *taghex;
};

/* Shared KAT material: the 40-byte 000102.. A/P sequence of the RFC's
 * largest sample tuple (reused by the AES-128 A.1 #13 tuple, the 96-bit-tag
 * tuple, and the derived AES-192/256 tuples) and the BBAA...110D anchor
 * nonce that all of those tuples share.  Declared as arrays, not pointers,
 * so that they are address constants usable in the static struct ocb_kat
 * initializers of the per-cipher binaries. */
extern const char ocb_seq40_hex[];
extern const char ocb_nonce_anchor_hex[];

/* One-shot OCB encrypt: ivlen/taglen before the key+nonce init, AAD first
 * (out == NULL), data, Final, GET_TAG.  `out` needs inlen + 16 bytes. */
void ocb_encrypt(const EVP_CIPHER *cipher, const unsigned char *key,
                 const unsigned char *nonce, size_t ivlen,
                 unsigned char *tag, size_t taglen,
                 const unsigned char *aad, size_t aadlen,
                 const unsigned char *pt, size_t ptlen,
                 unsigned char *out, size_t outcap, size_t *outlen);

/* One-shot OCB decrypt with tag verification (the tag is set before the
 * payload update). */
void ocb_decrypt(const EVP_CIPHER *cipher, const unsigned char *key,
                 const unsigned char *nonce, size_t ivlen,
                 const unsigned char *tag, size_t taglen,
                 const unsigned char *aad, size_t aadlen,
                 const unsigned char *ct, size_t ctlen,
                 unsigned char *out, size_t outcap, size_t *outlen);

/* Streaming OCB encrypt: AAD updates in aadchunks (cycled, truncated at the
 * end), then payload updates in ptchunks, Final, GET_TAG.  Must match the
 * one-shot result exactly (the provider buffers partial blocks and flushes
 * them at Final or on the next full block). */
void ocb_encrypt_stream(const EVP_CIPHER *cipher, const unsigned char *key,
                        const unsigned char *nonce, size_t ivlen,
                        unsigned char *tag, size_t taglen,
                        const unsigned char *aad, size_t aadlen,
                        const size_t *aadchunks, size_t naadchunks,
                        const unsigned char *pt, size_t ptlen,
                        const size_t *ptchunks, size_t nptchunks,
                        unsigned char *out, size_t outcap, size_t *outlen);

/* Streaming OCB decrypt with tag verification, fed through the same chunk
 * grids as ocb_encrypt_stream. */
void ocb_decrypt_stream(const EVP_CIPHER *cipher, const unsigned char *key,
                        const unsigned char *nonce, size_t ivlen,
                        const unsigned char *tag, size_t taglen,
                        const unsigned char *aad, size_t aadlen,
                        const size_t *aadchunks, size_t naadchunks,
                        const unsigned char *ct, size_t ctlen,
                        const size_t *ctchunks, size_t nctchunks,
                        unsigned char *out, size_t outcap, size_t *outlen);

/* Fills a buffer with a deterministic pattern (no KAT dependence). */
void ocb_fill_pattern(unsigned char *buf, size_t n);

/* Small-payload regime, parameterized per cipher:

 * (1) the RFC 7253 KAT rows (kats/nkats) through the one-shot helpers,
 *     each tuple also decrypted back to its plaintext;
 * (2) IV length rules: the default is 12; 1..15 round-trip; 0 and 16 are
 *     rejected;
 * (3) tag length rules: 1..16 round-trip; 0 is accepted at set time but the
 *     operation must fail; 17 is rejected outright;
 * (4) partial final blocks: {15} in one update, {16,7} across two updates,
 *     and AAD {20,5} across two updates -- all equal to the one-shot
 *     result;
 * (5) AAD after the first data update is accepted, and the record verifies
 *     when the AAD is replayed in the same (late) order;
 * (6) tamper detection: flipping a ciphertext bit or a tag bit must fail
 *     DecryptFinal.
 * keyhex is the cipher's key (the 000102.. pattern or the RFC's second-key
 * pattern, whichever the binary's rows use). */
void ocb_test_kats(const struct ocb_kat *kats, size_t nkats);
void ocb_test_iv_lengths(const char *ciphername, const char *keyhex);
void ocb_test_tag_lengths(const char *ciphername, const char *keyhex);
void ocb_test_partial_blocks(const char *ciphername, const char *keyhex);
void ocb_test_aad_after_data(const char *ciphername, const char *keyhex);
void ocb_test_tamper(const char *ciphername, const char *keyhex);

/* Large-payload regime, parameterized per cipher:

 * (1) the KAT anchor row through the one-shot helper, then the same cipher
 *     with a 64 KiB patterned AAD: streaming must equal one-shot;
 * (2) a 1 MiB payload through the mixed {16384, 16391} chunk grid must
 *     equal the one-shot result, and the streaming decrypt must recover
 *     the plaintext;
 * (3) a 64 KiB AAD in two 32 KiB chunks must equal the one-shot AAD;
 * (4) tampering with the final ciphertext block (first and last byte) must
 *     fail DecryptFinal.
 * keylen is the cipher's key length (24/32). */
void ocb_test_kat_anchor(const struct ocb_kat *kat);
void ocb_test_large_stream_equals_one_shot(const char *ciphername,
                                           size_t keylen);
void ocb_test_large_aad(const char *ciphername, size_t keylen);
void ocb_test_tampered_final_block(const char *ciphername, size_t keylen);

#endif /* TEST_AES_OCB_COMMON_H */

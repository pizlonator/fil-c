/* test_aes_xts_common.h -- shared machinery for the AES-XTS family tests.
 *
 * One binary per cipher (test_aes_128_xts.c, test_aes_256_xts.c, and the
 * data-unit size boundary binary test_aes_256_xts_large.c); each small-suite
 * binary defines its own struct xts_kat rows (the IEEE 1619-2007 vectors for
 * its cipher) and calls the per-cipher test routines below with its first
 * row's key/IV material.  The big explanatory comment (XTS key/IV
 * conventions, the IEEE 1619-2007 vector provenance, the behavior notes, and
 * the 16 MiB data-unit boundary) lives in test_aes_xts_common.c.
 */

#ifndef TEST_AES_XTS_COMMON_H
#define TEST_AES_XTS_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* XTS_MAX_BLOCKS_PER_DATA_UNIT * AES_BLOCK_SIZE (include/crypto/modes.h):
 * the largest data unit OpenSSL accepts, 16 MiB. */
#define XTS_MAX_DATA_UNIT ((size_t)((1u << 20) * 16))
#define XTS_MEBIBYTE ((size_t)(1024 * 1024))

/* One row per known-answer vector: keyhex is always 32 (AES-128-XTS) or 64
 * (AES-256-XTS) hex digits = two concatenated AES keys; ivhex is the 128-bit
 * data-unit number; pthex/cthex are the plaintext and ciphertext of one data
 * unit. */
struct xts_kat {
    const char *label;
    const char *ciphername;
    const char *keyhex;
    const char *ivhex;
    const char *pthex;
    const char *cthex;
};

/* Shared KAT material: the IEEE Std 1619-2007 XTS-AES keys (two concatenated
 * same-length AES keys), used by the small-suite rows and the large
 * AES-256-XTS binary.  Declared as arrays, not pointers, so that they are
 * address constants usable in the static struct xts_kat initializers of the
 * per-cipher binaries. */
extern const char xts_key_128_ieee_hex[];
extern const char xts_key_256_ieee_hex[];

/* One-shot XTS encrypt (enc != 0) / decrypt (enc == 0) of a single data
 * unit.  Returns the output length; `out` must have room for inlen bytes
 * (XTS emits exactly inlen bytes; the EVP layer passes outsize = inl for
 * this block-size-1 cipher). */
size_t xts_crypt(const EVP_CIPHER *cipher, const unsigned char *key,
                 const unsigned char *iv, const unsigned char *in,
                 size_t inlen, unsigned char *out, size_t outcap, int enc);

/* (1) One-shot KATs for the given rows: encrypt the plaintext and compare
 * against the IEEE vector; decrypt the vector ciphertext and compare against
 * the plaintext.  Covers 16- and 32-byte aligned data units plus the
 * ciphertext-stealing lengths. */
void xts_test_kats(const struct xts_kat *kats, size_t nkats);

/* (2) Data units shorter than 16 bytes must fail, and a zero-length update
 * must fail too (aes_xts_cipher rejects inl < AES_BLOCK_SIZE and in ==
 * NULL).  Each negative test uses a fresh context, and a 16-byte unit still
 * works afterwards. */
void xts_test_short_data_units_fail(const char *ciphername,
                                    const char *keyhex, const char *ivhex);

/* (3) Ciphertext stealing round-trips for non-multiple-of-16 lengths on the
 * decrypt route (and the matching encryption, per the KATs above): a 21-byte
 * data unit one-shot and in place, and a 53-byte message through two split
 * updates (a 32-byte and a 21-byte data unit; every XTS update is a data
 * unit of at least 16 bytes). */
void xts_test_stealing_roundtrip(const char *ciphername, const char *keyhex,
                                 const char *ivhex);

/* (4) Every update is an independent data unit: the tweak is never advanced
 * between updates, so encrypting the same 16-byte block twice through one
 * context yields identical ciphertext, while the second block of a single
 * 32-byte one-shot data unit differs (its tweak has advanced). */
void xts_test_each_update_is_a_data_unit(const char *ciphername,
                                         const char *keyhex,
                                         const char *ivhex);

/* (5) Weak keys: a key whose two halves are equal (the key is
 * strlen(keyhex)/2 bytes of 0x77) must be rejected when initializing for
 * encryption; for decryption the default provider permits it and the route
 * is a well-behaved, deterministic permutation on aligned data units. */
void xts_test_weak_key(const char *ciphername, const char *keyhex,
                       const char *ivhex);

/* (6) A half-length key (16 bytes for AES-128-XTS, 32 for AES-256-XTS)
 * cannot be used: the only public way to present it is
 * EVP_CIPHER_CTX_set_key_length() + init, and OpenSSL must reject the
 * operation at one of those two points. */
void xts_test_wrong_key_length(const char *ciphername, const char *keyhex,
                               const char *ivhex);

/* (L1) The maximum data unit: a single 16777216-byte update encrypts and
 * decrypts back. */
void xts_test_max_data_unit_roundtrip(const char *ciphername,
                                      const char *keyhex, const char *ivhex);

/* (L2) One byte beyond the maximum data unit must fail with
 * XTS_DATA_UNIT_IS_TOO_LARGE -- for encryption and decryption. */
void xts_test_data_unit_too_large(const char *ciphername, const char *keyhex,
                                  const char *ivhex);

/* (L3) A 1 MiB data unit round-trips in one shot and in place. */
void xts_test_one_mebibyte_unit(const char *ciphername, const char *keyhex,
                                const char *ivhex);

/* (L4) A 4 MiB stream through four 1 MiB updates round-trips, and each
 * update is an independent data unit: the first 1 MiB of the streamed
 * ciphertext equals the one-shot 1 MiB data-unit ciphertext with the same
 * data-unit number. */
void xts_test_four_mebibyte_stream(const char *ciphername, const char *keyhex,
                                   const char *ivhex);

#endif /* TEST_AES_XTS_COMMON_H */

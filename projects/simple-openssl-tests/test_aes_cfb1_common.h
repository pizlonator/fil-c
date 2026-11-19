/* test_aes_cfb1_common.h -- shared machinery for the AES-CFB1 family tests,
 * covering both of CFB1's OpenSSL operating modes: byte mode (the default)
 * and bit-length mode (EVP_CIPH_FLAG_LENGTH_BITS).
 *
 * One binary per cipher (test_aes_{128,192,256}_cfb1.c); each binary defines
 * one struct cfb1_kat row with its own byte-mode SP 800-38A key/ciphertext
 * constants and then calls the per-cipher test routines below.  The big
 * explanatory comment (the vector provenance, the byte-mode verification
 * command, and the whole bit-length-mode route/pitfalls discussion) lives in
 * test_aes_cfb1_common.c.
 */

#ifndef TEST_AES_CFB1_COMMON_H
#define TEST_AES_CFB1_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* One row per AES-CFB1 key size: the cipher name and its byte-mode
 * key/ciphertext hex for the shared 4-block plaintext (the plaintext and the
 * IV are family-wide and live in the common module as arrays). */
struct cfb1_kat {
    const char *ciphername;
    const char *keyhex;
    const char *cthex;
};

/* Shared SP 800-38A KAT material (the shared 4-block plaintext and the IV
 * 000102..0f).  Declared as arrays, not pointers, so that they are address
 * constants usable in static initializers of the per-cipher binaries. */
extern const char cfb1_kat_pt_hex[];   /* the shared 64-byte plaintext */
extern const char cfb1_kat_iv_hex[];   /* IV 000102..0f */

/* Length of the shared plaintext in bytes. */
#define CFB1_KAT_PTLEN 64

/* One-shot CFB1 encrypt/decrypt in BYTE mode (inl is a byte count; each byte
 * feeds 8 one-bit CFB steps).  Returns the total output length written to
 * `out`, which must have room for inlen + block_size bytes per the EVP
 * output-buffer contract. */
size_t cfb1_crypt(const EVP_CIPHER *cipher, const unsigned char *key,
                  const unsigned char *iv, const unsigned char *in,
                  size_t inlen, unsigned char *out, size_t outcap, int enc);

/* Feed `inlen` bytes through updates whose lengths cycle through
 * {1,15,16,17,31} (the last chunk is truncated to what remains).  Byte mode
 * only.  Returns the total output length. */
size_t cfb1_stream(const EVP_CIPHER *cipher, const unsigned char *key,
                   const unsigned char *iv, const unsigned char *in,
                   size_t inlen, unsigned char *out, size_t outcap, int enc);

/* Creates a fresh context with bit-length mode enabled using the public
 * two-phase route, then passes key+IV.  A fresh context is used for every
 * bit-mode operation because EVP forwards only flag *changes* to the
 * provider (see test_aes_cfb1_common.c).  Returns the context; caller frees
 * it. */
EVP_CIPHER_CTX *cfb1_bitmode_ctx_new(const EVP_CIPHER *cipher,
                                     const unsigned char *key,
                                     const unsigned char *iv, int enc);

/* (1) Byte-mode EVP_CIPHER_fetch one-shot KAT, plus the decrypt direction
 * (CFB1 decryption uses the forward cipher). */
void cfb1_test_byte_mode_kat(const struct cfb1_kat *kat);

/* (2) Byte-mode streaming: updates of {1,15,16,17,31} bytes must reproduce
 * the one-shot ciphertext, and a 37-byte message round-trips. */
void cfb1_test_byte_mode_streaming(const struct cfb1_kat *kat);

/* (3) Byte-mode state continuity: a zero-length update is a no-op, and the
 * one-bit feedback state (which lives in the shifted IV) must carry across
 * update boundaries: 5 + 0 + 11 bytes == one 16-byte update. */
void cfb1_test_byte_mode_zero_update(const struct cfb1_kat *kat);

/* (4) Consistency: byte-mode encryption of n bytes == bit-mode encryption of
 * 8n bits, for full-byte bit lengths (1, 3, 5, 6, and 16 bytes).  Each
 * bit-mode update runs on a freshly reset context. */
void cfb1_test_bit_mode_consistency(const struct cfb1_kat *kat);

/* (5) Bit mode with a bit length that is not a multiple of 8: encrypting 41
 * bits writes ceil(41/8) = 6 bytes, where the last byte carries only the top
 * bit (the remaining seven bits of the destination byte are left untouched),
 * and the 41 bits round-trip through bit-mode decryption. */
void cfb1_test_bit_mode_41_bits(const struct cfb1_kat *kat);

/* (6) Bit-mode state continuity across updates: the keystream (shifted-IV)
 * state carries across update boundaries, so 8 bits + 0 bits + 120 bits
 * (all byte-aligned in the input) must equal a single 128-bit update
 * (16 bytes). */
void cfb1_test_bit_mode_split(const struct cfb1_kat *kat);

#endif /* TEST_AES_CFB1_COMMON_H */

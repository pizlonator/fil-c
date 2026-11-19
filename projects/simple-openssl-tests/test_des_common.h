/* test_des_common.h -- shared machinery for the single-DES family tests
 * (DES-ECB/CBC/OFB/CFB/CFB1/CFB8, DESX-CBC, and the two-key DES-EDE
 * family in ECB/CBC/CFB/OFB).
 *
 * One binary per cipher (test_des_{ecb,cbc,ofb,cfb,cfb8,cfb1}.c,
 * test_desx_cbc.c, test_des_{ede,ede_cbc,ede_cfb,ede_ofb}.c); each binary
 * defines one struct des_kat row with its own key/IV/plaintext/KAT
 * constants and then calls the per-cipher test routines below.  The big
 * explanatory comment (the provider situation, the derivation provenance
 * of the CLI-derived constants, the 8-byte-block semantics, the DESX key
 * layout, and the EDE stream-mode notes) lives in test_des_common.c.
 */

#ifndef TEST_DES_COMMON_H
#define TEST_DES_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* One row per DES-family cipher: the cipher name, the key hex (8 bytes for
 * single DES, 16 for two-key DES-EDE, 24 for DESX-CBC), the IV hex ("" for
 * ECB, which takes no IV), the plaintext constant, its byte length, and
 * this cipher's KAT ciphertext. */
struct des_kat {
    const char *ciphername;
    const char *keyhex;
    const char *ivhex;   /* "" for ECB */
    const char *pthex;
    size_t ptlen;
    const char *cthex;
};

/* Shared KAT material (the single-DES key and IV, the 8/16/32-byte
 * plaintexts, and the two-key DES-EDE key).  Declared as arrays, not
 * pointers, so that they are address constants usable in the static
 * struct des_kat initializers of the per-cipher binaries.  The DESX key/
 * ciphertext and the EDE stream-mode ciphertexts belong to a single
 * cipher each and live in that cipher's binary. */
extern const char des_key_hex[];     /* the single-DES 8-byte key */
extern const char des_iv_hex[];      /* the family-wide 8-byte IV */
extern const char des_pt8_hex[];     /* the 8-byte ECB plaintext */
extern const char des_pt16_hex[];    /* the 16-byte CBC plaintext */
extern const char des_pt32_hex[];    /* the 32-byte stream-mode plaintext */
extern const char des_ede_key_hex[]; /* the two-key DES-EDE 16-byte key */

/* Loading the legacy provider explicitly turns OFF default-provider
 * auto-load, so both providers are loaded here (see the "Behavioral
 * notes" section of README.md); the DES-EDE ciphers then still fetch from
 * the default provider.  Every DES-family binary calls this right after
 * test_parse_args(), before its first fetch. */
void des_load_providers(void);

/* One-shot encrypt/decrypt with padding disabled.  Returns the total
 * output length written to `out` (room for inlen + block_size per the EVP
 * contract; the helper CHECKs that). */
size_t des_crypt(const EVP_CIPHER *cipher, const unsigned char *key,
                 const unsigned char *iv, const unsigned char *in,
                 size_t inlen, unsigned char *out, size_t outcap, int enc);

/* Encrypt/decrypt with an explicit padding flag (mode-agnostic: the ECB
 * rows pass a NULL IV). */
size_t des_cbc_crypt(const EVP_CIPHER *cipher, const unsigned char *key,
                     const unsigned char *iv, const unsigned char *in,
                     size_t inlen, unsigned char *out, size_t outcap,
                     int enc, int padding);

/* Feed `inlen` bytes through updates whose lengths cycle through `chunks`
 * (last one truncated); padding disabled.  A split must reproduce the
 * one-shot result exactly. */
size_t des_stream(const EVP_CIPHER *cipher, const unsigned char *key,
                  const unsigned char *iv, const unsigned char *in,
                  size_t inlen, unsigned char *out, size_t outcap,
                  int enc, const size_t *chunks, size_t nchunks);

/* (1) EVP_CIPHER_fetch(ciphername): the row's key length, the one-shot
 * KAT, and decrypting the KAT ciphertext back to its plaintext. */
void des_test_kat(const struct des_kat *kat);

/* (2) 8-byte-block semantics (the block ciphers: ECB/CBC/DESX-CBC/DES-EDE/
 * DES-EDE-CBC): nopad exact-multiple roundtrip, padded roundtrips of odd
 * sizes {1,7,8,9,33}, streaming {1,7,8,9} equality, and the 0-byte
 * no-op. */
void des_test_block_semantics(const struct des_kat *kat);

/* (3) Stream-mode details (the block-size-1 ciphers: OFB/CFB and the EDE
 * stream modes): the block size is 1, partial updates {1,7,8,9} equal the
 * one-shot result in both directions (the shift register plus num offset
 * carry across updates), a 0-byte update is a no-op, and the one-shot
 * roundtrip recovers the plaintext. */
void des_test_stream_modes(const struct des_kat *kat);

/* (4) CFB8: byte-at-a-time updates must reproduce the one-shot ciphertext
 * (the 8-bit shift register carries the byte boundary), for both
 * directions. */
void des_test_cfb8_bytewise(const struct des_kat *kat);

/* (5) CFB1 byte mode, plus what EVP_CIPH_FLAG_LENGTH_BITS does for DES:
 * the flag is accepted but the update length remains a BYTE count, so 8
 * "flagged" bytes produce exactly the byte-mode result (DES-CFB1 pins
 * byte mode only; see test_des_common.c). */
void des_test_cfb1_byte_mode(const struct des_kat *kat);

/* (6) DESX-CBC: the per-block composition
 * ct_i = DES-ECB(pt_i ^ in_w ^ chain_i) ^ out_w with chain_0 = IV and
 * chain_i = the WHITENED previous ciphertext, rebuilt from per-block
 * DES-ECB calls exactly as DES_xcbc_encrypt chains them. */
void des_test_desx_composition(const struct des_kat *kat);

#endif /* TEST_DES_COMMON_H */

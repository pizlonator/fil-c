/* test_des3_common.h -- shared machinery for the three-key DES (DES-EDE3)
 * family tests (ECB/CBC/OFB/CFB/CFB8/CFB1).
 *
 * One binary per cipher (test_des_ede3{,_cbc,_ofb,_cfb,_cfb8,_cfb1}.c);
 * each binary defines one struct des3_kat row with its own IV/plaintext/
 * KAT constants and then calls the per-cipher test routines below.  The
 * big explanatory comment (the provider situation, the derivation
 * provenance of the CLI-derived constants, the 8-byte-block semantics,
 * and the fixed-key-length route) lives in test_des3_common.c.
 */

#ifndef TEST_DES3_COMMON_H
#define TEST_DES3_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* One row per DES-EDE3 cipher: the cipher name, the IV hex ("" for ECB,
 * which takes no IV), the plaintext constant, its byte length, and this
 * cipher's KAT ciphertext.  The key is family-wide (DES-EDE3 has a fixed
 * 24-byte key): every row uses the shared des3_key_hex. */
struct des3_kat {
    const char *ciphername;
    const char *ivhex;   /* "" for ECB */
    const char *pthex;
    size_t ptlen;
    const char *cthex;
};

/* Shared KAT material (the fixed 24-byte key, the 8-byte IV, and the
 * 8/16/32-byte plaintexts).  Declared as arrays, not pointers, so that
 * they are address constants usable in the static struct des3_kat
 * initializers of the per-cipher binaries. */
extern const char des3_key_hex[];    /* the fixed 24-byte DES-EDE3 key */
extern const char des3_iv_hex[];     /* the family-wide 8-byte IV */
extern const char des3_pt8_hex[];    /* the 8-byte ECB plaintext */
extern const char des3_pt16_hex[];   /* the 16-byte CBC plaintext */
extern const char des3_pt32_hex[];   /* the 32-byte stream-mode plaintext */

/* One-shot encrypt/decrypt with padding disabled.  Returns the total
 * output length written to `out` (room for inlen + block_size per the EVP
 * contract; the helper CHECKs that). */
size_t des3_crypt(const EVP_CIPHER *cipher, const unsigned char *key,
                  const unsigned char *iv, const unsigned char *in,
                  size_t inlen, unsigned char *out, size_t outcap, int enc);

/* Encrypt/decrypt with an explicit padding flag (mode-agnostic: the ECB
 * row passes a NULL IV). */
size_t des3_cbc_crypt(const EVP_CIPHER *cipher, const unsigned char *key,
                      const unsigned char *iv, const unsigned char *in,
                      size_t inlen, unsigned char *out, size_t outcap,
                      int enc, int padding);

/* Feed `inlen` bytes through updates whose lengths cycle through `chunks`
 * (last one truncated); padding disabled.  A split must reproduce the
 * one-shot result exactly. */
size_t des3_stream(const EVP_CIPHER *cipher, const unsigned char *key,
                   const unsigned char *iv, const unsigned char *in,
                   size_t inlen, unsigned char *out, size_t outcap,
                   int enc, const size_t *chunks, size_t nchunks);

/* (1) EVP_CIPHER_fetch(ciphername): the fixed 24-byte key length, the
 * one-shot KAT, and decrypting the KAT ciphertext back to its plaintext. */
void des3_test_kat(const struct des3_kat *kat);

/* (2) Block streaming (the 8-byte-block ciphers with padding: ECB/CBC):
 * the nopad one-shot KAT, its streaming equality under {1,7,8,9,16,17}
 * updates in both directions, and a padded roundtrip of a generated
 * 33-byte pattern (33 -> 40 with a full pad block). */
void des3_test_block_streaming(const struct des3_kat *kat);

/* (3) Stream details (the stream modes OFB/CFB/CFB8/CFB1): a 17-then-15
 * split equals the one-shot result in both directions (the shift register
 * carries state across updates) and a 0-byte update is a no-op. */
void des3_test_stream_details(const struct des3_kat *kat);

/* (4) A 16-byte key must not be usable with DES-EDE3: set_key_length must
 * refuse to shrink the fixed-length key (the provider TDES flags carry no
 * VARIABLE_LENGTH bit), or the key init must fail.  Written against
 * DES-EDE3, the cipher the original check fetched. */
void des3_test_wrong_key_length(const struct des3_kat *kat);

#endif /* TEST_DES3_COMMON_H */

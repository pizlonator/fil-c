/* test_sm4_common.h -- shared machinery for the SM4 family tests
 * (SM4-ECB/CBC/CTR/OFB/CFB plus SM4-GCM/CCM/XTS).
 *
 * One binary per cipher (test_sm4_{ecb,cbc,ctr,ofb,cfb,gcm,ccm,xts}.c);
 * each binary defines one struct sm4_kat row with its own KAT ciphertext
 * constant and then calls the per-cipher test routines below.  The big
 * explanatory comment (the GB/T 32907-2016 anchor, the derivation provenance
 * of the CLI-derived constants, the AEAD ordering rules, and the version
 * gates) lives in test_sm4_common.c.
 */

#ifndef TEST_SM4_COMMON_H
#define TEST_SM4_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* One row per SM4 mode: the cipher name, the IV constant (NULL for ECB,
 * which takes no IV), the plaintext constant and its byte length, and this
 * mode's KAT ciphertext.  The key is family-wide (SM4 has a single 128-bit
 * key size): every row uses the shared sm4_key_hex. */
struct sm4_kat {
    const char *ciphername;
    const char *ivhex;   /* NULL for ECB */
    const char *pthex;
    size_t ptlen;
    const char *cthex;
};

/* Shared KAT material.  Declared as arrays, not pointers, so that they are
 * address constants usable in the static struct sm4_kat initializers of the
 * per-cipher binaries. */
extern const char sm4_key_hex[];        /* the GB/T 32907-2016 128-bit key */
extern const char sm4_pt16_hex[];       /* the GB/T 16-byte plaintext */
extern const char sm4_pt32_hex[];       /* the 32-byte stream-mode plaintext */
extern const char sm4_pt64_hex[];       /* the 64-byte CBC/CTR plaintext */
extern const char sm4_iv_hex[];         /* the stream-mode and CBC IV */
extern const char sm4_ctr_iv_hex[];     /* the CTR initial counter 000102..0f */

/* AEAD shared material (SM4-GCM and SM4-CCM): the AEAD key, the 16-byte
 * plaintext 000102...0f and the 12-byte nonce used by the GCM/CCM
 * behavior sections. */
extern const char sm4_aead_key_hex[];
extern const char sm4_aead_pt_hex[];
extern const char sm4_aead_nonce_hex[];

/* Byte lengths of the shared plaintext constants. */
#define SM4_PT16_LEN 16
#define SM4_PT32_LEN 32
#define SM4_PT64_LEN 64

/* One-shot classical-mode encrypt/decrypt with padding disabled.  Returns
 * the total output length written to `out` (room for inlen + block_size
 * per the EVP contract; the helper CHECKs that). */
size_t sm4_crypt(const EVP_CIPHER *cipher, const unsigned char *key,
                 const unsigned char *iv, const unsigned char *in,
                 size_t inlen, unsigned char *out, size_t outcap, int enc);

/* CBC encrypt/decrypt with an explicit padding flag. */
size_t sm4_cbc_crypt(const EVP_CIPHER *cipher, const unsigned char *key,
                     const unsigned char *iv, const unsigned char *in,
                     size_t inlen, unsigned char *out, size_t outcap,
                     int enc, int padding);

/* Feed `inlen` bytes through updates whose lengths cycle through
 * {1,15,16,17,31} (the last chunk is truncated to what remains); padding
 * disabled.  A split must reproduce the one-shot result exactly. */
size_t sm4_stream(const EVP_CIPHER *cipher, const unsigned char *key,
                  const unsigned char *iv, const unsigned char *in,
                  size_t inlen, unsigned char *out, size_t outcap, int enc);

/* Whole-program version gate for the post-3.0 SM4 variants (SM4-GCM/CCM/
 * XTS fetch on the authoritative pizfix 3.6.5 build and fail on the system
 * 3.0.13): fetches `ciphername` and, when that fails, SKIPs the program
 * with the documented reason.  The GCM/CCM/XTS binaries call this right
 * after test_parse_args(); the classical-mode binaries never do (SM4 core
 * modes are always present -- a NULL fetch there is a FAIL, not a SKIP). */
void sm4_probe_or_skip(const char *ciphername);

/* (1) EVP_CIPHER_fetch(ciphername): the family key length, the one-shot
 * KAT, streaming equality ({1,15,16,17,31} updates), and the streaming
 * decryption recovering the plaintext. */
void sm4_test_kat(const struct sm4_kat *kat);

/* (2) Pad/nopad roundtrips (the 16-byte-block ciphers: ECB and CBC): a
 * nopad roundtrip of the row's block-aligned plaintext and a padded
 * roundtrip of a generated 37-byte pattern (37 -> 48 with a full pad
 * block). */
void sm4_test_roundtrips(const struct sm4_kat *kat);

#endif /* TEST_SM4_COMMON_H */

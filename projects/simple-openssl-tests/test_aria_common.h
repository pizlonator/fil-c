/* test_aria_common.h -- shared machinery for the ARIA family tests
 * (ARIA-{128,192,256} in ECB/CBC/CTR/OFB/CFB/CFB8/CFB1, plus ARIA-{128,192,
 * 256}-GCM and ARIA-{128,192,256}-CCM).
 *
 * One binary per cipher (test_aria_{128,192,256}_{ecb,cbc,ctr,ofb,cfb,cfb8,
 * cfb1,gcm,ccm}.c); each binary defines one struct aria_kat / aria_gcm_kat /
 * aria_ccm_kat row with its own KAT constants and then calls the per-cipher
 * test routines below.  The big explanatory comment (the derivation
 * provenance of the CLI-derived classical constants, the scratch-probe
 * provenance of the AEAD constants, and the AEAD ordering rules) lives in
 * test_aria_common.c.
 */

#ifndef TEST_ARIA_COMMON_H
#define TEST_ARIA_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* One row per classical ARIA cipher (ECB/CBC/CTR/OFB/CFB/CFB8/CFB1): the
 * cipher name, the shared key/IV/plaintext constants (arrays in the common
 * module; ivhex is NULL for ECB, which takes no IV), the byte length of the
 * plaintext constant, and this cipher's KAT ciphertext. */
struct aria_kat {
    const char *ciphername;
    const char *keyhex;
    const char *ivhex;
    const char *pthex;
    size_t ptlen;
    const char *cthex;
};

/* One GCM row: the cipher name and the no-AAD KAT tuple (ciphertext and
 * 16-byte tag).  The AAD/streaming/tamper behavior routines take the same
 * row and combine its cipher name and key with the shared IV, AAD, and
 * plaintext constants of the common module. */
struct aria_gcm_kat {
    const char *ciphername;
    const char *keyhex;
    const char *cthex;
    const char *taghex;
};

/* One CCM row: the cipher name and the KAT tuple (the helpers run it with
 * ivlen 12 and taglen 12; the nonce is the shared 12-byte CBC IV prefix). */
struct aria_ccm_kat {
    const char *ciphername;
    const char *keyhex;
    const char *cthex;
    const char *taghex;
};

/* Shared KAT material (the family keys 00112233..., the RFC 3713-style ECB
 * plaintext block, the 32-byte stream-mode plaintext, the 64-byte CBC/CTR
 * plaintext, the CBC IV, the stream-mode IV, the CTR initial counter, the
 * 16-byte AEAD plaintext, and the GCM AAD).  Declared as arrays, not
 * pointers, so that they are address constants usable in the static struct
 * *_kat initializers of the per-cipher binaries. */
extern const char aria_key_128_hex[];     /* 00112233..eeff */
extern const char aria_key_192_hex[];     /* ... + 0011223344556677 */
extern const char aria_key_256_hex[];     /* ... + a second 00112233..eeff */
extern const char aria_pt16_hex[];        /* the RFC 3713-style ECB block */
extern const char aria_pt32_hex[];        /* the stream-mode plaintext */
extern const char aria_pt64_hex[];        /* the CBC/CTR plaintext */
extern const char aria_cbc_iv_hex[];      /* the CBC IV (AEAD nonce source) */
extern const char aria_stream_iv_hex[];   /* the OFB/CFB/CFB8/CFB1 IV */
extern const char aria_ctr_iv_hex[];      /* the CTR initial counter */
extern const char aria_aead_pt_hex[];     /* the 16-byte GCM/CCM plaintext */
extern const char aria_gcm_aad_hex[];     /* the 20-byte GCM AAD */

/* Byte lengths of the shared plaintext constants. */
#define ARIA_PT16_LEN 16
#define ARIA_PT32_LEN 32
#define ARIA_PT64_LEN 64
#define ARIA_AEAD_PT_LEN 16

/* One-shot encrypt/decrypt with padding disabled; used for the KATs and the
 * decrypt direction.  Returns the total output length written to `out`
 * (room for inlen + block_size per the EVP contract; the helper CHECKs
 * that).  iv is NULL for ECB. */
size_t aria_crypt(const EVP_CIPHER *cipher, const unsigned char *key,
                  const unsigned char *iv, const unsigned char *in,
                  size_t inlen, unsigned char *out, size_t outcap, int enc);

/* Feed `inlen` bytes through updates whose lengths cycle through
 * {1,15,16,17,31} (last one truncated); padding disabled.  A split must
 * reproduce the one-shot result exactly. */
size_t aria_stream(const EVP_CIPHER *cipher, const unsigned char *key,
                   const unsigned char *iv, const unsigned char *in,
                   size_t inlen, unsigned char *out, size_t outcap, int enc);

/* (1) EVP_CIPHER_fetch(ciphername): the one-shot KAT for the row, plus the
 * decrypt direction (decrypting the KAT ciphertext gives back the
 * plaintext). */
void aria_test_kat(const struct aria_kat *kat);

/* (2) Streaming: updates of {1,15,16,17,31} bytes (last one truncated) must
 * give exactly the one-shot (KAT) ciphertext, and the same split on decrypt
 * must give exactly the plaintext. */
void aria_test_streaming(const struct aria_kat *kat);

/* (3) GCM KAT: the no-AAD one-shot tuple (ciphertext and 16-byte tag) under
 * the shared 12-byte IV prefix and plaintext. */
void aria_test_gcm_kat(const struct aria_gcm_kat *kat);

/* (4) GCM AAD ordering roundtrip: encrypt with the shared 20-byte AAD fed
 * first (out == NULL), then authenticated-decrypt and recover the
 * plaintext. */
void aria_test_gcm_aad_roundtrip(const struct aria_gcm_kat *kat);

/* (5) GCM streaming: the same AAD tuple pushed through {5,5,6}-byte updates
 * must reproduce the one-shot ciphertext and tag. */
void aria_test_gcm_streaming(const struct aria_gcm_kat *kat);

/* (6) GCM tamper: flipping one tag bit must fail DecryptFinal (the decrypt
 * here runs on the default 12-byte IV route, as in the original). */
void aria_test_gcm_tamper(const struct aria_gcm_kat *kat);

/* (7) CCM KAT (ivlen 12, taglen 12): encrypt == KAT (ciphertext and 12-byte
 * tag) and authenticated-decrypting the KAT tuple gives back the
 * plaintext. */
void aria_test_ccm_kat(const struct aria_ccm_kat *kat);

/* (8) CCM ordering rule: a second data update after the announced single
 * payload must fail (encrypt side; Final must still succeed afterwards). */
void aria_test_ccm_second_data_update(const struct aria_ccm_kat *kat);

/* (9) CCM roundtrips of 1-, 16- and 33-byte payloads (each encrypt/decrypt
 * through the single-update CCM helpers). */
void aria_test_ccm_roundtrips(const struct aria_ccm_kat *kat);

#endif /* TEST_ARIA_COMMON_H */

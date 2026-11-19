/* test_cbc_hmac_common.h -- shared machinery for the stitched AES-CBC-HMAC
 * TLS-record cipher family tests.
 *
 * One binary per cipher (test_aes_{128,256}_cbc_hmac_sha1.c,
 * test_aes_{128,256}_cbc_hmac_sha256.c, and the encrypt-then-MAC variants
 * test_aes_{128,192,256}_cbc_hmac_sha256_etm.c); each non-ETM binary defines
 * one struct cbhmac_spec row with its own embedded KAT record constant (the
 * AES-128 rows carry one; the AES-256 rows pass NULL and rely on the
 * composition as the KAT) and then calls the per-cipher test routines below.
 * The big explanatory comment (record format, KAT-as-composition logic, and
 * the ETM variant note) lives in test_cbc_hmac_common.c.
 */

#ifndef TEST_CBC_HMAC_COMMON_H
#define TEST_CBC_HMAC_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* One row per stitched cipher under test: the cipher name, the matching
 * plain-CBC cipher name (the streaming-mode reference in the AAD rules), the
 * digest behind the HMAC ("SHA1"/"SHA256") and its length (20 or 32), the
 * AES key hex (16 or 32 bytes; the MAC key, the EVP IV, and the explicit-IV
 * slot are shared via the common module), the embedded KAT record hex for
 * the shared 5-byte KAT payload (NULL when the binary carries no embedded
 * constant), and the expected KAT record body length (48 for SHA1, 64 for
 * SHA256 at L = 5). */
struct cbhmac_spec {
    const char *ciphername;
    const char *cbcname;
    const char *digestname;
    size_t maclen;
    const char *keyhex;
    const char *kat_record_hex;
    size_t kat_bodylen;
};

/* Shared KAT material (identical in the old test_aes_cbc_hmac_sha1.c and
 * test_aes_cbc_hmac_sha256.c): the AES keys (the 000102... pattern continued
 * to the key size), the HMAC (TLS MAC) key, the EVP CBC IV, the 16-byte
 * explicit-IV slot, and the 5-byte KAT payload.  Declared as arrays, not
 * pointers, so that they are address constants usable in the static struct
 * cbhmac_spec initializers of the per-cipher binaries. */
extern const char cbhmac_key_128_hex[];
extern const char cbhmac_key_256_hex[];
extern const char cbhmac_mackey_hex[];
extern const char cbhmac_iv_hex[];
extern const char cbhmac_expiv_hex[];
extern const char cbhmac_kat_payload_hex[];

/* The whole-program gate: the stitched cipher is AES-NI gated, so on a build
 * or CPU without it the fetch or the key setup fails.  Probes exactly what
 * the two original programs probed in main() (fetch, then a key setup) and
 * takes the sanctioned whole-program SKIP path (test_skip) when either
 * fails. */
void cbhmac_probe_gate(const char *ciphername);

/* (1) The embedded KAT record (when the binary carries one) and the
 * KAT-as-composition equality for spec's cipher: the record is rebuilt with
 * separate EVP_MAC(HMAC) and AES-CBC calls following the record format and
 * must equal the stitched output byte for byte; the stitched (or embedded)
 * record decrypts back to the KAT payload. */
void cbhmac_test_kat_and_composition(const struct cbhmac_spec *spec);

/* (2) Compose-equals-stitch and roundtrip for payload sizes
 * {1, 16, 64, 256, 1024} on spec's cipher. */
void cbhmac_test_composition_and_roundtrip(const struct cbhmac_spec *spec);

/* (3) A tampered record (a flipped ciphertext byte, and a flipped byte in
 * the transmitted explicit-IV slot) must fail the decrypt update. */
void cbhmac_test_tamper(const struct cbhmac_spec *spec);

/* (4) A wrong AAD length must be rejected by the AAD ctrl; a missing AAD
 * degrades to the documented streaming mode (plain CBC of the raw buffer,
 * no MAC/padding), which differs from the TLS-mode record. */
void cbhmac_test_aad_rules(const struct cbhmac_spec *spec);

/* (5) A TLS 1.0-style record (version 0x0300): no explicit-IV slot, the body
 * is CBC(payload || MAC || padding) from the EVP IV, and the AAD length
 * field is the payload length itself (not rewritten). */
void cbhmac_test_tls10_style(const struct cbhmac_spec *spec);

/* (6) The ETM variants: if a build offers the cipher, at least run a
 * streaming-mode init (no AAD) to demonstrate the fetch is live; the ETM
 * composition itself is documented in test_cbc_hmac_common.c and left to the
 * build that has the algorithm, since it cannot be validated here. */
void cbhmac_test_etm_streaming_init(const char *ciphername);

#endif /* TEST_CBC_HMAC_COMMON_H */

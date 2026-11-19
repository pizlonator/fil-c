/* test_aes_gcm_siv_common.h -- shared machinery for the AES-GCM-SIV family
 * tests.

 * One binary per cipher (test_aes_{128,192,256}_gcm_siv.c); each binary
 * defines its own RFC 8452 KAT row(s) and calls the per-cipher test routines
 * below.  Every binary version-gates on its own cipher's fetch failing
 * (AES-*-GCM-SIV was added in OpenSSL 3.5) and takes the whole-program SKIP
 * path.  The big explanatory comment (RFC 8452 behavior notes, KAT
 * provenance) lives in test_aes_gcm_siv_common.c.
 */

#ifndef TEST_AES_GCM_SIV_COMMON_H
#define TEST_AES_GCM_SIV_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* One RFC 8452 Appendix C row (or derived row): the label, the cipher name,
 * and the tuple as hex strings ("" means empty AAD / empty plaintext /
 * tag-only vector). */
struct gsiv_kat {
    const char *label;
    const char *ciphername;
    const char *keyhex;
    const char *aadhex;  /* "" means empty AAD */
    const char *pthex;   /* "" means empty plaintext */
    const char *cthex;   /* "" means tag-only vector */
    const char *taghex;
};

/* The fixed 12-byte RFC 8452 nonce shared by every row and every behavior
 * section.  Declared as an array, not a pointer, so that it is an address
 * constant usable in static initializers of the per-cipher binaries. */
extern const char gsiv_nonce_hex[];

/* (1) The RFC 8452 Appendix C KAT rows (and derived rows) of the binary's
 * cipher: encrypt == KAT and decrypting the KAT gives back the plaintext. */
void gsiv_test_kats(const struct gsiv_kat *kats, size_t nkats);

/* (2) AAD in two chunks must equal one-shot AAD.  The kat row provides the
 * cipher name and the suite key. */
void gsiv_test_aad_chunking(const struct gsiv_kat *kat);

/* (3) Payload roundtrips for sizes {0,16,17,64}. */
void gsiv_test_roundtrip_sizes(const struct gsiv_kat *kat);

/* (4) One payload update per init: a second data update must fail (used_enc
 * / used_dec in cipher_aes_gcm_siv_hw.c), on both directions. */
void gsiv_test_second_update_fails(const struct gsiv_kat *kat);

/* (5) Tamper detection: a flipped ciphertext byte or tag bit must fail
 * DecryptFinal. */
void gsiv_test_tamper(const struct gsiv_kat *kat);

#endif /* TEST_AES_GCM_SIV_COMMON_H */

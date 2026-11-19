/* test_mac_cmac_common.h -- shared machinery for the CMAC family tests
 * (EVP_MAC "CMAC" over AES-128-CBC, AES-192-CBC, AES-256-CBC and
 * DES-EDE3-CBC).
 *
 * One binary per cipher (test_mac_cmac_aes_128_cbc.c,
 * test_mac_cmac_aes_192_cbc.c, test_mac_cmac_aes_256_cbc.c and
 * test_mac_cmac_des_ede3_cbc.c); each binary defines one struct cmac_family
 * row (its "cipher" parameter, the key length and CMAC tag size, and the
 * wrong cipher for the negative init test) and its own KAT rows, then calls
 * the per-cipher test routines below.  The big explanatory comment (the CLI
 * verification recipe, the per-section distribution, and the wrong-cipher
 * rationale) lives in test_mac_cmac_common.c.
 */

#ifndef TEST_MAC_CMAC_COMMON_H
#define TEST_MAC_CMAC_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* One binary per cipher: the "cipher" parameter under test, the key length
 * ciphername expects (bytes), the CMAC tag/block size (16 for the AES-CBC
 * ciphers, 8 for DES-EDE3-CBC; also the value EVP_MAC_CTX_get_mac_size must
 * return), and a cipher whose key length differs from keylen, for the
 * negative init test. */
struct cmac_family {
    const char *ciphername;
    size_t keylen;
    size_t macsize;
    const char *wrongcipher;
};

/* One KAT row: the label (kept from the original twelve-row table, for the
 * failure messages), the key hex, the number of bytes of the shared message
 * the row runs over (0, 16, 40 or 64), and the expected tag. */
struct cmac_kat {
    const char *label;
    const char *keyhex;
    size_t msglen;
    const char *taghex;
};

/* The shared KAT message: the SP 800-38A message (also RFC 4493's M), of
 * which every row runs a prefix (the DES-EDE3-CBC rows its 16-byte prefix).
 * Declared as an array, not a pointer, so that it is an address constant
 * usable where the per-cipher binaries need it. */
extern const char cmac_msg64_hex[];

/* (1) The one-shot KATs of nkats rows (an empty-message row -- example 1 of
 * each AES set -- doubles as the subkey-generation sanity check). */
void cmac_test_kats(const struct cmac_family *fam,
                    const struct cmac_kat *kats, size_t nkats);

/* (2) Partial updates: kat's message fed as updates of exactly {a, b, rest}
 * bytes must produce exactly kat's one-shot tag. */
void cmac_test_partial_updates(const struct cmac_family *fam,
                               const struct cmac_kat *kat, size_t a, size_t b);

/* (3) EVP_MAC_CTX_get_mac_size must return fam's CMAC block size, read back
 * after a plain init with the key hex keyhex (the size is only meaningful
 * once the context has been initialized). */
void cmac_test_mac_size(const struct cmac_family *fam, const char *keyhex);

/* (4) A wrong cipher must fail at init: init with the key hex keyhex (of
 * length fam->keylen) and the fam->wrongcipher parameter must be rejected. */
void cmac_test_wrong_cipher(const struct cmac_family *fam, const char *keyhex);

#endif /* TEST_MAC_CMAC_COMMON_H */

/* test_legacy_block_common.h -- shared machinery for the legacy-provider
 * block cipher family tests: CAST5, Blowfish, IDEA, SEED and RC2 in their
 * ECB/CBC/OFB/CFB variants, plus RC2-40-CBC and RC2-64-CBC.
 *
 * One binary per cipher (test_cast5_{ecb,cbc,ofb,cfb}.c, test_bf_{ecb,cbc,
 * ofb,cfb}.c, test_idea_{ecb,cbc,ofb,cfb}.c, test_seed_{ecb,cbc,ofb,cfb}.c,
 * test_rc2_{ecb,cbc,cfb,ofb}.c, test_rc2_{40,64}_cbc.c); each binary
 * defines one struct legacy_block_row with its own KAT constants and calls
 * the shared runner below.  The big explanatory comment (the provider
 * situation, the derivation provenance of the CLI-derived constants, the
 * IDEA version gate, and the RC2 40/64-bit key lengths) lives in
 * test_legacy_block_common.c.
 */

#ifndef TEST_LEGACY_BLOCK_COMMON_H
#define TEST_LEGACY_BLOCK_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* One row per legacy block cipher: the cipher name, the key hex, the IV
 * hex ("" for ECB, which takes no IV), the plaintext constant, and this
 * cipher's KAT ciphertext. */
struct legacy_block_row {
    const char *ciphername;
    const char *keyhex;
    const char *ivhex;   /* "" for ECB */
    const char *pthex;
    const char *cthex;
};

/* Shared KAT material (the default 16-byte key, the 8-byte IV most rows
 * use, SEED's full-block 16-byte IV, and the 16/32-byte plaintexts).
 * Declared as arrays, not pointers, so that they are address constants
 * usable in the static struct legacy_block_row initializers of the
 * per-cipher binaries.  The RC2-40-CBC and RC2-64-CBC rows carry their
 * own fixed 5/8-byte keys in their binaries. */
extern const char legacy_block_key16_hex[]; /* the default 16-byte key */
extern const char legacy_block_iv8_hex[];   /* the 8-byte IV */
extern const char legacy_block_iv16_hex[];  /* SEED's full-block IV */
extern const char legacy_block_pt16_hex[];  /* the 16-byte plaintext */
extern const char legacy_block_pt32_hex[];  /* the stream-mode plaintext */

/* Loading the legacy provider explicitly turns OFF default-provider
 * auto-load, so both providers are loaded here (every cipher of this
 * family lives in the legacy provider).  Every family binary calls this
 * right after test_parse_args(), before its first fetch. */
void legacy_block_load_providers(void);

/* Whole-program version gate: some builds genuinely lack some of these
 * ciphers (IDEA is absent from Ubuntu's 3.0.13, which compiles it out),
 * so a binary whose cipher does not fetch SKIPs right after
 * test_parse_args() with the documented reason ("%s is unavailable"),
 * per the house SKIP contract -- the monolith used to skip just its row
 * and keep going. */
void legacy_block_probe_or_skip(const char *ciphername);

/* The data-driven sweep for one row (the row is passed in per binary):
 * the fetched cipher's key length matches the row's key, the KAT one-shot
 * ciphertext matches, the KAT ciphertext decrypts back to the plaintext,
 * and the {5,3,8,2} streaming grid reproduces the one-shot result in both
 * directions. */
void legacy_block_test_row(const struct legacy_block_row *row);

#endif /* TEST_LEGACY_BLOCK_COMMON_H */

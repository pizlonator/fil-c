/* test_cbc_cts_common.h -- shared machinery for the CBC ciphertext stealing
 * (CS1/CS2/CS3) family tests.
 *
 * One binary per cipher (test_aes_{128,192,256}_cbc_cts.c and
 * test_camellia_{128,192,256}_cbc_cts.c); each binary defines one struct
 * cts_kat row with its own KAT ciphertext constants and then calls the
 * per-cipher test routines below.  The big explanatory comment (CTS rules,
 * version note, derivation provenance) lives in test_cbc_cts_common.c.
 */

#ifndef TEST_CBC_CTS_COMMON_H
#define TEST_CBC_CTS_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* One row per CBC-CTS cipher under test: the CTS cipher name, the matching
 * plain-CBC cipher name (used to compute the plain-CBC reference in the
 * length sweep), the key hex (the 000102... pattern continued to the key
 * size; CAMELLIA-128 shares its key with AES-128), and the 54-byte KAT
 * ciphertexts for the shared 54-byte plaintext pattern in each cts_mode. */
struct cts_kat {
    const char *ctsname;
    const char *cbcname;
    const char *keyhex;
    const char *cs1_54hex;
    const char *cs2_54hex;
    const char *cs3_54hex;
};

/* Shared KAT material (the 000102... key pattern continued to the key size;
 * IV = 000102..0F; the shared plaintext patterns).  Declared as arrays, not
 * pointers, so that they are address constants usable in the static struct
 * cts_kat initializers of the per-cipher binaries. */
extern const char cts_key_128_hex[];
extern const char cts_key_192_hex[];
extern const char cts_key_256_hex[];
extern const char cts_iv_hex[];
extern const char cts_pt54_hex[];
extern const char cts_pt32_hex[];
extern const char cts_pt16_hex[];

/* One-shot CTS encrypt (enc != 0) / decrypt (enc == 0) with the given
 * cts_mode ("CS1"/"CS2"/"CS3").  Returns the output length (== inlen; CTS
 * preserves length). */
size_t cts_crypt(const EVP_CIPHER *cipher, int enc, const char *cts_mode,
                 const unsigned char *key, const unsigned char *iv,
                 const unsigned char *in, size_t inlen,
                 unsigned char *out, size_t outcap);

/* (a) The default cts_mode is CS1 and is readable back through the same
 * param at init time. */
void cts_test_default_mode_readback(const struct cts_kat *kat);

/* (b) The 54-byte CS1/CS2/CS3 KATs for kat, plus the CS3 decrypt
 * roundtrip. */
void cts_test_kat_54(const struct cts_kat *kat);

/* (c) The block-aligned (32-byte) CS1/CS3 KATs for kat (CS1 = plain CBC,
 * CS3 swaps the last two blocks).  Only the ciphers that carry the
 * constants run the section: NULL arguments skip it. */
void cts_test_kat_32(const struct cts_kat *kat, const char *cs1_32hex,
                     const char *cs3_32hex);

/* (d) The single-block (16-byte) CS3 KAT for kat (all three variants are
 * plain CBC for a one-block message).  NULL cs3_16hex skips it. */
void cts_test_kat_16_cs3(const struct cts_kat *kat, const char *cs3_16hex);

/* (e) A generic single plaintext/ciphertext KAT for kat in the given
 * cts_mode (used for the Camellia-128 38-byte CS3 vector).  NULL arguments
 * skip it. */
void cts_test_kat_generic(const struct cts_kat *kat, const char *pthex,
                          const char *cthex, const char *ctsmode);

/* (f) Length sweep {17,31,32,33,48} for kat: CS1/CS3 round-trip everywhere,
 * the documented CS1/CS2/CS3 equalities and differences, and the plain-CBC
 * reference fetched via kat->cbcname for the aligned lengths. */
void cts_test_length_sweep(const struct cts_kat *kat);

/* (g) A single 16-byte generated message: CS1 == CS2 == CS3 for kat. */
void cts_test_single_block(const struct cts_kat *kat);

/* (h) Update rules for kat: a sub-block update fails and a second update
 * fails, on both the encrypt and the decrypt side. */
void cts_test_update_rules(const struct cts_kat *kat);

#endif /* TEST_CBC_CTS_COMMON_H */

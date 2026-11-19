/* test_rc4_common.h -- shared machinery for the RC4 family tests: RC4 and
 * RC4-40 (legacy provider) plus the RC4 large-data regime.
 *
 * One binary per cipher (test_rc4.c, test_rc4_40.c) plus the large-data
 * regime binary test_rc4_large.c; each binary defines its own KAT
 * constants (the classic RC4 vectors, the RC4-40 CLI-derived vector) and
 * calls the per-cipher test routines below.  The big explanatory comment
 * (the classic-vector provenance, the variable-key-length route, the
 * 8-byte bulk path, and the large-regime self-consistency oracle) lives in
 * test_rc4_common.c.
 */

#ifndef TEST_RC4_COMMON_H
#define TEST_RC4_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* One row of the classic RC4 KAT table: the (short) key, the plaintext,
 * and the expected ciphertext, all as hex strings.  RC4 is symmetric, so
 * the runner decrypts each KAT ciphertext too. */
struct rc4_kat {
    const char *keyhex;
    const char *pthex;
    const char *cthex;
};

/* The 16-byte key the shared behavior sections run under (the per-cipher
 * KAT constants carry their own keys).  Declared as an array, not a
 * pointer, so that it is an address constant. */
extern const char rc4_key16_hex[];

/* Fetches an RC4-family cipher by name, CHECK()ing success (a NULL fetch
 * after rc4_load_providers() is a broken build, not a version gate). */
EVP_CIPHER *rc4_fetch(const char *ciphername);

/* Loading the legacy provider disables default-provider auto-load, so both
 * are loaded explicitly (RC4 and RC4-40 live in the legacy provider).
 * Every RC4-family binary calls this right after test_parse_args(),
 * before its first fetch. */
void rc4_load_providers(void);

/* RC4-encrypts/decrypts `inlen` bytes one-shot with an explicit key
 * length (the key length is re-set with EVP_CIPHER_CTX_set_key_length()
 * between a bare init and the key init when it differs from the default
 * 16; RC4 carries the VARIABLE_LENGTH flag).  Returns the output length. */
size_t rc4_crypt(const EVP_CIPHER *cipher, const unsigned char *key,
                 size_t keylen, const unsigned char *in, size_t inlen,
                 unsigned char *out, size_t outcap, int enc);

/* Feed `inlen` bytes through updates whose lengths cycle through `chunks`
 * (last one truncated); same variable-key-length route as rc4_crypt().
 * Returns the total output length. */
size_t rc4_chunked(const EVP_CIPHER *cipher, const unsigned char *key,
                   size_t keylen, const unsigned char *in, size_t inlen,
                   unsigned char *out, size_t outcap, int enc,
                   const size_t *chunks, size_t nchunks);

/* (1) The classic KAT table (test_rc4.c): each row's one-shot ciphertext
 * matches through the variable-key-length route and decrypting the KAT
 * ciphertext recovers the plaintext. */
void rc4_test_kats(const struct rc4_kat *kats, size_t nkats);

/* (2) One-shot sizes 0..24 under `keyhex` at `keylen` (every RC4-family
 * binary runs it for its own cipher): every output length equals its
 * input length and the 8-byte-boundary lengths roundtrip. */
void rc4_test_sizes(const char *ciphername, const char *keyhex,
                    size_t keylen);

/* (3) Streaming {1,1,1,1,5} == one-shot for lengths 9..24: the i/j state
 * carries across updates, including the 8-byte bulk path boundary, in
 * both directions. */
void rc4_test_streaming(const char *ciphername, const char *keyhex,
                        size_t keylen);

/* (4) 8-byte-boundary behavior: chunk grids that straddle the bulk
 * path's 8-at-a-time steps ({7,1,8}, {8,8,5}, {15,1,7}, {9,7,5}) must
 * equal one-shot for lengths around every multiple of 8. */
void rc4_test_eight_byte_boundaries(const char *ciphername,
                                    const char *keyhex, size_t keylen);

/* (5) Empty updates are no-ops (RC4 state untouched): a 0-byte update
 * between two real updates must not perturb the keystream. */
void rc4_test_empty_update(const char *ciphername, const char *keyhex,
                           size_t keylen);

/* (6) Large-data regime (test_rc4_large only): 64 KiB through the odd
 * grid {1000,1,64,8} == one-shot in both directions, with 200+ updates
 * proving the state continuity. */
void rc4_large_test_64k(void);

/* (7) 128 KiB one-shot roundtrip and a byte-at-a-time decrypt of the
 * first 64 bytes (state continuity across the 8-byte bulk boundary). */
void rc4_large_test_128k_and_bytewise(void);

#endif /* TEST_RC4_COMMON_H */

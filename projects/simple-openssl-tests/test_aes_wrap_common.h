/* test_aes_wrap_common.h -- shared machinery for the AES key wrap (RFC
 * 3394) family tests.

 * One binary per cipher: test_aes_{128,192,256}_wrap.c (the RFC 3394 KATs
 * and the plain-wrap rules) and test_aes_{128,192,256}_wrap_inv.c (the
 * SP 800-38F inverse variants).  Each binary defines its struct wrap_kat
 * row (plain binaries) or carries no KAT constants (-INV binaries: the RFC
 * has no -INV examples) and then calls the per-cipher test routines below.
 * The big explanatory comments (the length rules, the default initial
 * value, the -INV provenance) live in test_aes_wrap_common.c.
 */

#ifndef TEST_AES_WRAP_COMMON_H
#define TEST_AES_WRAP_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* One RFC 3394 section 4 row: the label used in the check messages, the
 * cipher under test, its KEK, and the wrapped output for the shared CEK. */
struct wrap_kat {
    const char *label;
    const char *ciphername;
    const char *kekhex;
    const char *wraphex;
};

/* Shared KAT material: the RFC 3394 128-bit CEK that all three section 4
 * rows wrap.  Declared as an array, not a pointer, so that it is an
 * address constant usable in static initializers. */
extern const char wrap_cek_hex[];

/* Wrap `inlen` bytes in one update; returns the wrapped length (inlen+8).
 * `out` must have room for inlen + 8 bytes (the EVP contract's inl +
 * block_size for this cipher). */
size_t wrap_key(const EVP_CIPHER *cipher, const unsigned char *kek,
                const unsigned char *in, size_t inlen,
                unsigned char *out, size_t outcap);

/* Unwrap `inlen` bytes in one update; returns the key data length. */
size_t unwrap_key(const EVP_CIPHER *cipher, const unsigned char *kek,
                  const unsigned char *in, size_t inlen,
                  unsigned char *out, size_t outcap);

/* (1) The row's RFC 3394 KAT, wrap and unwrap directions. */
void wrap_test_kat(const struct wrap_kat *kat);

/* (2) Length rules: 15 bytes fail, 16 bytes wrap, 17 bytes fail (for the
 * binary's own cipher). */
void wrap_test_length_rules(const char *ciphername);

/* (3) A 64-byte payload round-trips (for the binary's own cipher). */
void wrap_test_64_byte_roundtrip(const char *ciphername);

/* (4) A corrupted wrapped key must fail to unwrap: the RFC 3394 integrity
 * check (the recovered A6A6A6A6A6A6A6A6 initial value) rejects it (for the
 * binary's own cipher). */
void wrap_test_corrupted_unwrap(const char *ciphername);

/* (5) The -INV variant's roundtrips: a 16-byte payload wraps to exactly 24
 * bytes and a 64-byte payload to exactly 72 bytes, both round-tripping
 * (for the binary's own cipher).  Whole-program gate: if the cipher is
 * unavailable the binary SKIPs. */
void wrap_test_inv_roundtrips(const char *ciphername);

#endif /* TEST_AES_WRAP_COMMON_H */

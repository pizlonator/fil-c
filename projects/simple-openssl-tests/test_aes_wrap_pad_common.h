/* test_aes_wrap_pad_common.h -- shared machinery for the AES key wrap with
 * padding (RFC 5649) family tests.

 * One binary per cipher: test_aes_{128,192,256}_wrap_pad.c (the RFC 5649
 * KATs where they exist and the plain wrap-pad rules) and
 * test_aes_{128,192,256}_wrap_pad_inv.c (the SP 800-38F inverse variants).
 * Each binary carries its own KAT constants (the RFC publishes only the
 * 192-bit example; the derived constants live with the binaries that use
 * them) and calls the per-cipher test routines below.  The big explanatory
 * comments (the padding rules, the A65959A6 header, the output-size math,
 * the -INV provenance) live in test_aes_wrap_pad_common.c.
 */

#ifndef TEST_AES_WRAP_PAD_COMMON_H
#define TEST_AES_WRAP_PAD_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* Shared KAT material: RFC 5649's 20 octets of key data (wrapped by the
 * RFC's 192-bit example, the derived 256-bit example, and the derived
 * 192/256-bit -INV examples) and the two KEKs those examples use.  The
 * constants are arrays, not pointers, so that they are address constants
 * usable in static initializers. */
extern const char wrap_pad_k20_hex[];
extern const char wrap_pad_kek192_hex[];
extern const char wrap_pad_kek256_hex[];

/* Wrap-pad `inlen` bytes in one update; returns the wrapped length.  `out`
 * must have room for roundup8(inlen) + 8 + 8 bytes. */
size_t wrap_pad(const EVP_CIPHER *cipher, const unsigned char *kek,
                const unsigned char *in, size_t inlen,
                unsigned char *out, size_t outcap);

/* Unwrap-pad `inlen` bytes in one update; returns the key data length. */
size_t unwrap_pad(const EVP_CIPHER *cipher, const unsigned char *kek,
                  const unsigned char *in, size_t inlen,
                  unsigned char *out, size_t outcap);

/* (1) A single wrap-pad KAT: wrapping the kdatahex octets with kekhex
 * gives exactly roundup8(n) + 8 bytes matching wraphex, and unwrapping
 * wraphex gives the key data back.  `source` ("RFC 5649" or "derived")
 * names the constant's provenance in the check messages. */
void wrap_pad_test_kat(const char *ciphername, const char *kekhex,
                       const char *kdatahex, const char *wraphex,
                       const char *source);

/* (2) Plaintext sizes {1,4,7,8,13,20,255} round-trip and the wrapped
 * length is exactly roundup8(n) + 8 (asserted, not just observed) -- for
 * the binary's own cipher. */
void wrap_pad_test_size_roundtrips(const char *ciphername);

/* (3) A corrupted wrapped output must fail to unwrap-pad, whether the
 * corruption hits the AIV block or the padding block -- for the binary's
 * own cipher. */
void wrap_pad_test_corrupted_unwrap(const char *ciphername);

/* (4) Feeding a plain RFC 3394 wrapped key (no AIV) to unwrap-pad must
 * fail: the recovered initial value will not be A65959A6 || len.  Runs on
 * the wrapname/wrap-pad pair of the binary's own key size. */
void wrap_pad_test_plain_wrap_rejected(const char *wrapname,
                                       const char *wpadname);

/* (5) The -INV variant's checks: the derived KAT for the 20 RFC 5649
 * key-data octets (kathex != NULL), the 13-octet roundtrip at exactly 24
 * bytes, and the corrupted-last-byte rejection.  kekhex != NULL selects
 * the derived KEK; NULL fills the KEK with the pattern the plain binaries
 * use.  Whole-program gate: if the cipher is unavailable the binary
 * SKIPs. */
void wrap_pad_test_inv(const char *ciphername, const char *kekhex,
                       const char *kathex);

#endif /* TEST_AES_WRAP_PAD_COMMON_H */

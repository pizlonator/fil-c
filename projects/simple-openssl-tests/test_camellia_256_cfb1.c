#include "test_common.h"
#include "test_camellia_common.h"

#include <openssl/evp.h>

/*
 * test_camellia_256_cfb1 -- CAMELLIA-256-CFB1 (1-bit segments) in both of its
 * OpenSSL operating modes (byte mode the default, bit-length mode via
 * EVP_CIPH_FLAG_LENGTH_BITS), one binary per cipher (the shared Camellia
 * machinery, the derivation provenance of the CLI-derived constants, and the
 * whole bit-mode route/pitfalls discussion live in
 * test_camellia_common.c).
 *
 * Known-answer provenance (CAMELLIA-256-CFB1): the plaintext is the 32-byte
 * stream-mode vector, the IV is the family-wide stream-mode/CBC IV
 * fedcba98765432100123456789abcdef, and the key is the RFC 3713 128-bit key
 * extended with 00112233445566778899aabbccddeeff (shared constants in the
 * common module); the KAT ciphertext below is a byte-mode one, derived with
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -camellia-256-cfb1 -nopad -K <keyhex> -iv <ivhex>
 *
 * and cross-checked byte for byte under the system OpenSSL 3.0.13.
 * Bit-length mode has no independent KAT (`openssl enc` cannot pass bit
 * lengths); it is verified against byte mode by camellia_test_cfb1_bitmode
 * in the common module.
 */

/* This cipher's row: the shared key, IV, and 32-byte stream-mode plaintext
 * plus this cipher's byte-mode KAT ciphertext. */
static const struct camellia_kat kat = {
    "CAMELLIA-256-CFB1",
    camellia_key_256_hex,
    camellia_iv_hex,
    camellia_pt32_hex, CAMELLIA_PT32_LEN,
    /* KAT ciphertext */
    "5ef6c5c1fa7f7d09be9eead4145a751d"
    "725638a9bd60c9918ca5116f3f3044d3",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    camellia_test_kat(&kat);
    camellia_test_alternate_routes(&kat, EVP_camellia_256_cfb1(),
                                   "EVP_camellia_256_cfb1()", "camellia-256-cfb1");
    camellia_test_streaming(&kat);
    camellia_test_cfb1_bitmode(&kat);

    test_report_pass();
    return 0;
}

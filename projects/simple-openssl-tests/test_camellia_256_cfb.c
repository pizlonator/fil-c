#include "test_common.h"
#include "test_camellia_common.h"

#include <openssl/evp.h>

/*
 * test_camellia_256_cfb -- CAMELLIA-256-CFB (full-block feedback, 128-bit
 * segments) known-answer and behavior tests, one binary per cipher (the
 * shared Camellia machinery, the derivation provenance of the CLI-derived
 * constants, and the family-wide commentary live in
 * test_camellia_common.c).
 *
 * Known-answer provenance (CAMELLIA-256-CFB): the plaintext is the 32-byte
 * stream-mode vector, the IV is the family-wide stream-mode/CBC IV
 * fedcba98765432100123456789abcdef, and the key is the RFC 3713 128-bit key
 * extended with 00112233445566778899aabbccddeeff (shared constants in the
 * common module); the KAT ciphertext below was derived with
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -camellia-256-cfb -nopad -K <keyhex> -iv <ivhex>
 *
 * and cross-checked byte for byte under the system OpenSSL 3.0.13.  The
 * legacy static route of this mode is spelled EVP_camellia_256_cfb128()
 * (openssl/evp.h aliases EVP_camellia_256_cfb to it), while the by-name
 * lookup takes the enc spelling "camellia-256-cfb".
 */

/* This cipher's row: the shared key, IV, and 32-byte stream-mode plaintext
 * plus this cipher's KAT ciphertext. */
static const struct camellia_kat kat = {
    "CAMELLIA-256-CFB",
    camellia_key_256_hex,
    camellia_iv_hex,
    camellia_pt32_hex, CAMELLIA_PT32_LEN,
    /* KAT ciphertext */
    "5bc493556c1e09b7d6f5620af6bc254e"
    "da29e8210924f2327b9a229b86fedc62",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    camellia_test_kat(&kat);
    camellia_test_alternate_routes(&kat, EVP_camellia_256_cfb128(),
                                   "EVP_camellia_256_cfb128()",
                                   "camellia-256-cfb");
    camellia_test_streaming(&kat);

    test_report_pass();
    return 0;
}

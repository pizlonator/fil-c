#include "test_common.h"
#include "test_camellia_common.h"

#include <openssl/evp.h>

/*
 * test_camellia_192_cfb -- CAMELLIA-192-CFB (full-block feedback, 128-bit
 * segments) known-answer and behavior tests, one binary per cipher (the
 * shared Camellia machinery, the derivation provenance of the CLI-derived
 * constants, and the family-wide commentary live in
 * test_camellia_common.c).
 *
 * Known-answer provenance (CAMELLIA-192-CFB): the plaintext is the 32-byte
 * stream-mode vector, the IV is the family-wide stream-mode/CBC IV
 * fedcba98765432100123456789abcdef, and the key is the RFC 3713 128-bit key
 * extended with 0011223344556677 (shared constants in the common module);
 * the KAT ciphertext below was derived with
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -camellia-192-cfb -nopad -K <keyhex> -iv <ivhex>
 *
 * and cross-checked byte for byte under the system OpenSSL 3.0.13.  The
 * legacy static route of this mode is spelled EVP_camellia_192_cfb128()
 * (openssl/evp.h aliases EVP_camellia_192_cfb to it), while the by-name
 * lookup takes the enc spelling "camellia-192-cfb".
 */

/* This cipher's row: the shared key, IV, and 32-byte stream-mode plaintext
 * plus this cipher's KAT ciphertext. */
static const struct camellia_kat kat = {
    "CAMELLIA-192-CFB",
    camellia_key_192_hex,
    camellia_iv_hex,
    camellia_pt32_hex, CAMELLIA_PT32_LEN,
    /* KAT ciphertext */
    "a836b1c0e51774077455c63f34f3234c"
    "68a4340d8ab9e801e40ad86bb56285d7",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    camellia_test_kat(&kat);
    camellia_test_alternate_routes(&kat, EVP_camellia_192_cfb128(),
                                   "EVP_camellia_192_cfb128()",
                                   "camellia-192-cfb");
    camellia_test_streaming(&kat);

    test_report_pass();
    return 0;
}

#include "test_common.h"
#include "test_camellia_common.h"

#include <openssl/evp.h>

/*
 * test_camellia_256_cbc -- CAMELLIA-256-CBC known-answer and behavior tests,
 * one binary per cipher (the shared Camellia machinery, the derivation
 * provenance of the CLI-derived constants, and the family-wide commentary
 * live in test_camellia_common.c).
 *
 * Known-answer provenance (CAMELLIA-256-CBC): the plaintext is the 64-byte
 * SP 800-38A shared vector, the IV is the family-wide CBC IV
 * fedcba98765432100123456789abcdef, and the key is the RFC 3713 128-bit key
 * extended with 00112233445566778899aabbccddeeff (shared constants in the
 * common module); the KAT ciphertext below was derived with
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -camellia-256-cbc -nopad -K <keyhex> -iv <ivhex>
 *
 * and cross-checked byte for byte under the system OpenSSL 3.0.13.
 */

/* This cipher's row: the shared key, IV, and 64-byte plaintext plus this
 * cipher's KAT ciphertext. */
static const struct camellia_kat kat = {
    "CAMELLIA-256-CBC",
    camellia_key_256_hex,
    camellia_iv_hex,
    camellia_pt64_hex, CAMELLIA_PT64_LEN,
    /* KAT ciphertext */
    "c8eb72eccaf5dd428d8264319f2dcd76"
    "25921e4e44602faea3eabb93417d153a"
    "6e9a902b2919451efa66c9bf27105631"
    "5c51a2cd76ddfef48c0bf4c21084920f",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    camellia_test_kat(&kat);
    camellia_test_alternate_routes(&kat, EVP_camellia_256_cbc(),
                                   "EVP_camellia_256_cbc()", "camellia-256-cbc");
    camellia_test_streaming(&kat);
    camellia_test_cbc_padding(&kat);

    test_report_pass();
    return 0;
}

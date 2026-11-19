#include "test_common.h"
#include "test_camellia_common.h"

#include <openssl/evp.h>

/*
 * test_camellia_128_cbc -- CAMELLIA-128-CBC known-answer and behavior tests,
 * one binary per cipher (the shared Camellia machinery, the derivation
 * provenance of the CLI-derived constants, and the family-wide commentary
 * live in test_camellia_common.c).
 *
 * Known-answer provenance (CAMELLIA-128-CBC): the plaintext is the 64-byte
 * SP 800-38A shared vector and the IV is the family-wide CBC IV
 * fedcba98765432100123456789abcdef (shared constants in the common module);
 * the KAT ciphertext below was derived with
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -camellia-128-cbc -nopad -K <keyhex> -iv <ivhex>
 *
 * and cross-checked byte for byte under the system OpenSSL 3.0.13.
 */

/* This cipher's row: the shared key, IV, and 64-byte plaintext plus this
 * cipher's KAT ciphertext. */
static const struct camellia_kat kat = {
    "CAMELLIA-128-CBC",
    camellia_key_128_hex,
    camellia_iv_hex,
    camellia_pt64_hex, CAMELLIA_PT64_LEN,
    /* KAT ciphertext */
    "30be05b50273d419cf1107015d56e9c4"
    "9757a56f8ca27bb6eaa588fd164b95d0"
    "e05d414371d02bd82d1de17740dcb0ec"
    "e8e9e90eeed14699d835d5f5e3e9ab2d",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    camellia_test_kat(&kat);
    camellia_test_alternate_routes(&kat, EVP_camellia_128_cbc(),
                                   "EVP_camellia_128_cbc()", "camellia-128-cbc");
    camellia_test_streaming(&kat);
    camellia_test_cbc_padding(&kat);

    test_report_pass();
    return 0;
}

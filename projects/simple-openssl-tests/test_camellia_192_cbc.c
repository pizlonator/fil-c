#include "test_common.h"
#include "test_camellia_common.h"

#include <openssl/evp.h>

/*
 * test_camellia_192_cbc -- CAMELLIA-192-CBC known-answer and behavior tests,
 * one binary per cipher (the shared Camellia machinery, the derivation
 * provenance of the CLI-derived constants, and the family-wide commentary
 * live in test_camellia_common.c).
 *
 * Known-answer provenance (CAMELLIA-192-CBC): the plaintext is the 64-byte
 * SP 800-38A shared vector, the IV is the family-wide CBC IV
 * fedcba98765432100123456789abcdef, and the key is the RFC 3713 128-bit key
 * extended with 0011223344556677 (shared constants in the common module);
 * the KAT ciphertext below was derived with
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -camellia-192-cbc -nopad -K <keyhex> -iv <ivhex>
 *
 * and cross-checked byte for byte under the system OpenSSL 3.0.13.
 */

/* This cipher's row: the shared key, IV, and 64-byte plaintext plus this
 * cipher's KAT ciphertext. */
static const struct camellia_kat kat = {
    "CAMELLIA-192-CBC",
    camellia_key_192_hex,
    camellia_iv_hex,
    camellia_pt64_hex, CAMELLIA_PT64_LEN,
    /* KAT ciphertext */
    "acba88f7eb7d0ea364b8689b3740e409"
    "2f4e9abfd6e7b829c1e886ecfa94f345"
    "14fd4a7634a424d09bfa85828a1b0da4"
    "c19325afabc6d9bc98365e68b1e38f72",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    camellia_test_kat(&kat);
    camellia_test_alternate_routes(&kat, EVP_camellia_192_cbc(),
                                   "EVP_camellia_192_cbc()", "camellia-192-cbc");
    camellia_test_streaming(&kat);
    camellia_test_cbc_padding(&kat);

    test_report_pass();
    return 0;
}

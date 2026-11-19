#include "test_common.h"
#include "test_camellia_common.h"

#include <openssl/evp.h>

/*
 * test_camellia_192_ecb -- CAMELLIA-192-ECB known-answer and behavior tests,
 * one binary per cipher (the shared Camellia machinery, the derivation
 * provenance of the CLI-derived constants, and the family-wide commentary
 * live in test_camellia_common.c).
 *
 * Known-answer provenance (CAMELLIA-192-ECB): the key is the RFC 3713
 * 128-bit key extended with 0011223344556677 and the plaintext is the RFC
 * 3713 plaintext (shared family constants in the common module); the KAT
 * ciphertext below was derived with
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -camellia-192-ecb -nopad -K <keyhex>
 *
 * and cross-checked byte for byte under the system OpenSSL 3.0.13.
 */

/* This cipher's row: the shared key and plaintext plus this cipher's KAT
 * ciphertext (ECB takes no IV). */
static const struct camellia_kat kat = {
    "CAMELLIA-192-ECB",
    camellia_key_192_hex,
    NULL,
    camellia_pt16_hex, CAMELLIA_PT16_LEN,
    /* KAT ciphertext */
    "b4993401b3e996f84ee5cee7d79b09b9",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    camellia_test_kat(&kat);
    camellia_test_alternate_routes(&kat, EVP_camellia_192_ecb(),
                                   "EVP_camellia_192_ecb()", "camellia-192-ecb");
    camellia_test_streaming(&kat);

    test_report_pass();
    return 0;
}

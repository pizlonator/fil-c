#include "test_common.h"
#include "test_camellia_common.h"

#include <openssl/evp.h>

/*
 * test_camellia_256_ecb -- CAMELLIA-256-ECB known-answer and behavior tests,
 * one binary per cipher (the shared Camellia machinery, the derivation
 * provenance of the CLI-derived constants, and the family-wide commentary
 * live in test_camellia_common.c).
 *
 * Known-answer provenance (CAMELLIA-256-ECB): the key is the RFC 3713
 * 128-bit key extended with 00112233445566778899aabbccddeeff and the
 * plaintext is the RFC 3713 plaintext (shared family constants in the common
 * module); the KAT ciphertext below was derived with
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -camellia-256-ecb -nopad -K <keyhex>
 *
 * and cross-checked byte for byte under the system OpenSSL 3.0.13.
 */

/* This cipher's row: the shared key and plaintext plus this cipher's KAT
 * ciphertext (ECB takes no IV). */
static const struct camellia_kat kat = {
    "CAMELLIA-256-ECB",
    camellia_key_256_hex,
    NULL,
    camellia_pt16_hex, CAMELLIA_PT16_LEN,
    /* KAT ciphertext */
    "9acc237dff16d76c20ef7c919e3a7509",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    camellia_test_kat(&kat);
    camellia_test_alternate_routes(&kat, EVP_camellia_256_ecb(),
                                   "EVP_camellia_256_ecb()", "camellia-256-ecb");
    camellia_test_streaming(&kat);

    test_report_pass();
    return 0;
}

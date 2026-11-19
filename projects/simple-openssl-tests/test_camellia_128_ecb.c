#include "test_common.h"
#include "test_camellia_common.h"

#include <openssl/evp.h>

/*
 * test_camellia_128_ecb -- CAMELLIA-128-ECB known-answer and behavior tests,
 * one binary per cipher (the shared Camellia machinery, the derivation
 * provenance of the CLI-derived constants, and the family-wide commentary
 * live in test_camellia_common.c).
 *
 * Known-answer provenance (CAMELLIA-128-ECB): this row is anchored by the
 * RFC 3713 sample vector -- key = plaintext = 0123456789abcdeffedcba9876543210
 * (shared family constants in the common module), ciphertext
 * 67673138549669730857065648eabe43.
 */

/* This cipher's row: the shared RFC 3713 key and plaintext plus the RFC 3713
 * sample ciphertext (ECB takes no IV). */
static const struct camellia_kat kat = {
    "CAMELLIA-128-ECB",
    camellia_key_128_hex,
    NULL,
    camellia_pt16_hex, CAMELLIA_PT16_LEN,
    /* RFC 3713 sample vector */
    "67673138549669730857065648eabe43",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    camellia_test_kat(&kat);
    camellia_test_alternate_routes(&kat, EVP_camellia_128_ecb(),
                                   "EVP_camellia_128_ecb()", "camellia-128-ecb");
    camellia_test_streaming(&kat);

    test_report_pass();
    return 0;
}

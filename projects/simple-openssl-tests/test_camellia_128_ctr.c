#include "test_common.h"
#include "test_camellia_common.h"

#include <openssl/evp.h>

/*
 * test_camellia_128_ctr -- CAMELLIA-128-CTR known-answer and behavior tests,
 * one binary per cipher (the shared Camellia machinery, the derivation
 * provenance of the CLI-derived constants, and the family-wide commentary
 * live in test_camellia_common.c).
 *
 * Known-answer provenance (CAMELLIA-128-CTR): the plaintext is the 64-byte
 * SP 800-38A shared vector and the initial counter is the family-wide
 * 000102...0f (shared constants in the common module); the KAT ciphertext
 * below was derived with
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -camellia-128-ctr -nopad -K <keyhex> -iv <ivhex>
 *
 * and cross-checked byte for byte under the system OpenSSL 3.0.13.
 */

/* This cipher's row: the shared key, initial counter, and 64-byte plaintext
 * plus this cipher's KAT ciphertext. */
static const struct camellia_kat kat = {
    "CAMELLIA-128-CTR",
    camellia_key_128_hex,
    camellia_ctr_iv_hex,
    camellia_pt64_hex, CAMELLIA_PT64_LEN,
    /* KAT ciphertext */
    "3b5dd6f915870064fd3296068eddc95b"
    "c02bb23a923e1a05ffb182867a7a0149"
    "ed3b7eb2f321fc0fe156ae309559698f"
    "8c441fb113d76d776e66e46640c171dc",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    camellia_test_kat(&kat);
    camellia_test_alternate_routes(&kat, EVP_camellia_128_ctr(),
                                   "EVP_camellia_128_ctr()", "camellia-128-ctr");
    camellia_test_streaming(&kat);

    test_report_pass();
    return 0;
}

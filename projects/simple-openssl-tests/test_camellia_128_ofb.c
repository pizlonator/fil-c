#include "test_common.h"
#include "test_camellia_common.h"

#include <openssl/evp.h>

/*
 * test_camellia_128_ofb -- CAMELLIA-128-OFB known-answer and behavior tests,
 * one binary per cipher (the shared Camellia machinery, the derivation
 * provenance of the CLI-derived constants, and the family-wide commentary
 * live in test_camellia_common.c).
 *
 * Known-answer provenance (CAMELLIA-128-OFB): the plaintext is the 32-byte
 * stream-mode vector and the IV is the family-wide stream-mode/CBC IV
 * fedcba98765432100123456789abcdef (shared constants in the common module);
 * the KAT ciphertext below was derived with
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -camellia-128-ofb -nopad -K <keyhex> -iv <ivhex>
 *
 * and cross-checked byte for byte under the system OpenSSL 3.0.13.  Like the
 * CFB rows, the first block of this KAT is plaintext XOR E(IV).
 */

/* This cipher's row: the shared key, IV, and 32-byte stream-mode plaintext
 * plus this cipher's KAT ciphertext. */
static const struct camellia_kat kat = {
    "CAMELLIA-128-OFB",
    camellia_key_128_hex,
    camellia_iv_hex,
    camellia_pt32_hex, CAMELLIA_PT32_LEN,
    /* KAT ciphertext */
    "88b82d6aef416e6b60acf7aff9fc5d39"
    "55712efe628fdd5833b8243e92c281e5",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    camellia_test_kat(&kat);
    camellia_test_alternate_routes(&kat, EVP_camellia_128_ofb(),
                                   "EVP_camellia_128_ofb()", "camellia-128-ofb");
    camellia_test_streaming(&kat);

    test_report_pass();
    return 0;
}

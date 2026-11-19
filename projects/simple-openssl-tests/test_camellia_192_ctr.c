#include "test_common.h"
#include "test_camellia_common.h"

#include <openssl/evp.h>

/*
 * test_camellia_192_ctr -- CAMELLIA-192-CTR known-answer and behavior tests,
 * one binary per cipher (the shared Camellia machinery, the derivation
 * provenance of the CLI-derived constants, and the family-wide commentary
 * live in test_camellia_common.c).
 *
 * Known-answer provenance (CAMELLIA-192-CTR): the plaintext is the 64-byte
 * SP 800-38A shared vector, the initial counter is the family-wide
 * 000102...0f, and the key is the RFC 3713 128-bit key extended with
 * 0011223344556677 (shared constants in the common module); the KAT
 * ciphertext below was derived with
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -camellia-192-ctr -nopad -K <keyhex> -iv <ivhex>
 *
 * and cross-checked byte for byte under the system OpenSSL 3.0.13.
 */

/* This cipher's row: the shared key, initial counter, and 64-byte plaintext
 * plus this cipher's KAT ciphertext. */
static const struct camellia_kat kat = {
    "CAMELLIA-192-CTR",
    camellia_key_192_hex,
    camellia_ctr_iv_hex,
    camellia_pt64_hex, CAMELLIA_PT64_LEN,
    /* KAT ciphertext */
    "895af9df74daf66f538ed8cda3670300"
    "024fe8874aa754b3fe9626679b0823b7"
    "1143e13cfd801f4328bff34e1e197b84"
    "6dee50a0fab2da37362ae2c8881418e4",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    camellia_test_kat(&kat);
    camellia_test_alternate_routes(&kat, EVP_camellia_192_ctr(),
                                   "EVP_camellia_192_ctr()", "camellia-192-ctr");
    camellia_test_streaming(&kat);

    test_report_pass();
    return 0;
}

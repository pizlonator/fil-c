#include "test_common.h"
#include "test_aria_common.h"

#include <openssl/evp.h>

/*
 * test_aria_128_cbc -- ARIA-128-CBC known-answer and behavior tests, one
 * binary per cipher (the shared ARIA machinery, the derivation provenance
 * of the CLI-derived constants, and the family-wide commentary live in
 * test_aria_common.c).
 *
 * Known-answer provenance (ARIA-128-CBC): the plaintext is the 64-byte SP
 * 800-38A shared vector and the IV is the family CBC IV
 * 0f1e2d3c4b5a69788796a5b4c3d2e1f0; both constants are
 * shared with the whole family (see test_aria_common.c).  The KAT
 * ciphertext below was derived with
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -aria-128-cbc -nopad -K <keyhex> -iv <ivhex>
 *
 * decrypted back to its plaintext with the same CLI, and cross-checked byte
 * for byte under the system OpenSSL 3.0.13.
 */

/* This cipher's row: the shared key, IV, and 64-byte plaintext plus this
 * cipher's KAT ciphertext. */
static const struct aria_kat kat = {
    "ARIA-128-CBC",
    aria_key_128_hex,
    aria_cbc_iv_hex,
    aria_pt64_hex, ARIA_PT64_LEN,
    /* KAT ciphertext */
    "908945a7077a4311c4d6d1acb9854be3"
    "5441c1cd0fc7edc13aedde8a18e90112"
    "1f6d2100ca8e1d6c1e1459c3ce7f0a44"
    "b57894138aae8721ac3a715d00ab5ef0",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    aria_test_kat(&kat);
    aria_test_streaming(&kat);

    test_report_pass();
    return 0;
}

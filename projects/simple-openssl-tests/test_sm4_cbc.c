#include "test_common.h"

#include "test_sm4_common.h"

#include <openssl/evp.h>

/*

 * test_sm4_cbc -- SM4-CBC known-answer and behavior tests, one binary per
 * cipher (the shared SM4 machinery, the derivation provenance of the
 * CLI-derived constants, and the family-wide commentary live in
 * test_sm4_common.c).

 * Known-answer provenance (SM4-CBC): the 64-byte plaintext and the IV
 * fedcba98765432100123456789abcdef are family-wide constants in the
 * common module; the KAT ciphertext below was verified with

 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -sm4-cbc -nopad -K <keyhex> -iv <ivhex>

 * and cross-checked byte for byte under the system OpenSSL 3.0.13.  The
 * vectors are block-aligned, so every KAT comparison runs with padding
 * disabled; padding itself gets its own section (sm4_test_cbc_roundtrips).
 */

/* This cipher's row: the shared key, IV, and 64-byte plaintext plus this
 * cipher's KAT ciphertext. */
static const struct sm4_kat kat = {
    "SM4-CBC",
    sm4_iv_hex,
    sm4_pt64_hex, SM4_PT64_LEN,
    /* KAT ciphertext */
    "58a407c9da5780d7c92bfa6a1a59ded1"
    "0ac66047f3a8fc6cd361228cba2f9243"
    "347c015b04929ce12ee5196f34f57d96"
    "006b90f3f14ff037c5ef7f26dd9cad7c",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    sm4_test_kat(&kat);
    sm4_test_roundtrips(&kat);

    test_report_pass();
    return 0;
}

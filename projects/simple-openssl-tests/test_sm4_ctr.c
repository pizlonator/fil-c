#include "test_common.h"

#include "test_sm4_common.h"

#include <openssl/evp.h>

/*

 * test_sm4_ctr -- SM4-CTR known-answer and behavior tests, one binary per
 * cipher (the shared SM4 machinery, the derivation provenance of the
 * CLI-derived constants, and the family-wide commentary live in
 * test_sm4_common.c).

 * Known-answer provenance (SM4-CTR): the 64-byte plaintext and the initial
 * counter 000102...0f are family-wide constants in the common module; the
 * KAT ciphertext below was verified with

 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -sm4-ctr -nopad -K <keyhex> -iv <ivhex>

 * and cross-checked byte for byte under the system OpenSSL 3.0.13.  CTR is
 * a stream mode, so partial updates must reproduce the one-shot result
 * exactly (the counter and offset carry across updates).
 */

/* This cipher's row: the shared key, initial counter, and 64-byte
 * plaintext plus this cipher's KAT ciphertext. */
static const struct sm4_kat kat = {
    "SM4-CTR",
    sm4_ctr_iv_hex,
    sm4_pt64_hex, SM4_PT64_LEN,
    /* KAT ciphertext */
    "6d59228313e6f73bc3b08993923bee40"
    "c12a871c5ea0509d44267c49c4af234b"
    "2c124ea66973137fe716b8d7e916d682"
    "3237723f8f458b4e9db9c9454055c9bf",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    sm4_test_kat(&kat);

    test_report_pass();
    return 0;
}

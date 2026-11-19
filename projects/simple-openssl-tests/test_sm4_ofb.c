#include "test_common.h"

#include "test_sm4_common.h"

#include <openssl/evp.h>

/*

 * test_sm4_ofb -- SM4-OFB known-answer and behavior tests, one binary per
 * cipher (the shared SM4 machinery, the derivation provenance of the
 * CLI-derived constants, and the family-wide commentary live in
 * test_sm4_common.c).

 * Known-answer provenance (SM4-OFB): the 32-byte stream-mode plaintext and
 * the IV fedcba98765432100123456789abcdef are family-wide constants in the
 * common module; the KAT ciphertext below was verified with

 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -sm4-ofb -nopad -K <keyhex> -iv <ivhex>

 * and cross-checked byte for byte under the system OpenSSL 3.0.13.  OFB is
 * a stream mode, so partial updates must reproduce the one-shot result
 * exactly (the keystream register and offset carry across updates).
 */

/* This cipher's row: the shared key, IV, and 32-byte plaintext plus this
 * cipher's KAT ciphertext. */
static const struct sm4_kat kat = {
    "SM4-OFB",
    sm4_iv_hex,
    sm4_pt32_hex, SM4_PT32_LEN,
    /* KAT ciphertext */
    "f181f519ed76e1ca91da7b29543b365b"
    "2db6ba0d1dd13606943215eeaba61907",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    sm4_test_kat(&kat);

    test_report_pass();
    return 0;
}

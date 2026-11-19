#include "test_common.h"

#include "test_sm4_common.h"

#include <openssl/evp.h>

/*

 * test_sm4_cfb -- SM4-CFB (CFB64) known-answer and behavior tests, one
 * binary per cipher (the shared SM4 machinery, the derivation provenance
 * of the CLI-derived constants, and the family-wide commentary live in
 * test_sm4_common.c).

 * Known-answer provenance (SM4-CFB): the 32-byte stream-mode plaintext and
 * the IV fedcba98765432100123456789abcdef are family-wide constants in the
 * common module; the KAT ciphertext below was verified with

 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -sm4-cfb -nopad -K <keyhex> -iv <ivhex>

 * and cross-checked byte for byte under the system OpenSSL 3.0.13.  CFB is
 * a stream mode, so partial updates must reproduce the one-shot result
 * exactly (the ciphertext shift register and offset carry across updates);
 * with the shared IV it agrees with the OFB row on the first block (both
 * are plaintext XOR E(IV) there).
 */

/* This cipher's row: the shared key, IV, and 32-byte plaintext plus this
 * cipher's KAT ciphertext. */
static const struct sm4_kat kat = {
    "SM4-CFB",
    sm4_iv_hex,
    sm4_pt32_hex, SM4_PT32_LEN,
    /* KAT ciphertext */
    "f181f519ed76e1ca91da7b29543b365b"
    "0ba4051427288582e6d4d9107b45ca63",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    sm4_test_kat(&kat);

    test_report_pass();
    return 0;
}

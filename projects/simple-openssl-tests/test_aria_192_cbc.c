#include "test_common.h"
#include "test_aria_common.h"

#include <openssl/evp.h>

/*
 * test_aria_192_cbc -- ARIA-192-CBC known-answer and behavior tests, one
 * binary per cipher (the shared ARIA machinery, the derivation provenance
 * of the CLI-derived constants, and the family-wide commentary live in
 * test_aria_common.c).
 *
 * Known-answer provenance (ARIA-192-CBC): the plaintext is the 64-byte SP
 * 800-38A shared vector and the IV is the family CBC IV
 * 0f1e2d3c4b5a69788796a5b4c3d2e1f0; both constants are
 * shared with the whole family (see test_aria_common.c).  The KAT
 * ciphertext below was derived with
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -aria-192-cbc -nopad -K <keyhex> -iv <ivhex>
 *
 * decrypted back to its plaintext with the same CLI, and cross-checked byte
 * for byte under the system OpenSSL 3.0.13.
 */

/* This cipher's row: the shared key, IV, and 64-byte plaintext plus this
 * cipher's KAT ciphertext. */
static const struct aria_kat kat = {
    "ARIA-192-CBC",
    aria_key_192_hex,
    aria_cbc_iv_hex,
    aria_pt64_hex, ARIA_PT64_LEN,
    /* KAT ciphertext */
    "26745d0e789c1028cbf17d53d9a1e86d"
    "19a412e37679aef12bffc9a7d224fd6d"
    "be880f76254fa21f7a4ac7b5cd440851"
    "faabfcacb44af36d44bc6522a7f742b0",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    aria_test_kat(&kat);
    aria_test_streaming(&kat);

    test_report_pass();
    return 0;
}

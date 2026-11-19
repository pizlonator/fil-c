#include "test_common.h"
#include "test_aria_common.h"

#include <openssl/evp.h>

/*
 * test_aria_128_ctr -- ARIA-128-CTR known-answer and behavior tests, one
 * binary per cipher (the shared ARIA machinery, the derivation provenance
 * of the CLI-derived constants, and the family-wide commentary live in
 * test_aria_common.c).
 *
 * Known-answer provenance (ARIA-128-CTR): the plaintext is the 64-byte SP
 * 800-38A shared vector and the IV is the CTR initial counter
 * 000102030405060708090a0b0c0d0e0f; both constants are
 * shared with the whole family (see test_aria_common.c).  The KAT
 * ciphertext below was derived with
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -aria-128-ctr -nopad -K <keyhex> -iv <ivhex>
 *
 * decrypted back to its plaintext with the same CLI, and cross-checked byte
 * for byte under the system OpenSSL 3.0.13.
 */

/* This cipher's row: the shared key, IV, and 64-byte plaintext plus this
 * cipher's KAT ciphertext. */
static const struct aria_kat kat = {
    "ARIA-128-CTR",
    aria_key_128_hex,
    aria_ctr_iv_hex,
    aria_pt64_hex, ARIA_PT64_LEN,
    /* KAT ciphertext */
    "dd5f0b0c89d8d9c370788e617d4d98a5"
    "86bca3d53f9eaa4e34c3315ec7f7463f"
    "97dfd483532f509c8f2b6c91cf73a074"
    "94a2ea4248e5ab124685144ace678478",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    aria_test_kat(&kat);
    aria_test_streaming(&kat);

    test_report_pass();
    return 0;
}

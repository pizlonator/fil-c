#include "test_common.h"

#include "test_sm4_common.h"

#include <openssl/evp.h>

/*

 * test_sm4_ecb -- SM4-ECB known-answer and behavior tests, one binary per
 * cipher (the shared SM4 machinery, the GB/T anchor, and the derivation
 * provenance live in test_sm4_common.c).

 * Known-answer provenance (SM4-ECB): the constants below are the GB/T
 * 32907-2016 standard ECB vector -- key = plaintext =
 * 0123456789abcdeffedcba9876543210 (both family-wide constants in the
 * common module) -> ciphertext 681edf34d206965e86b3e94f536e4246, verified
 * with

 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -sm4-ecb -nopad -K <keyhex>

 * (and likewise for the sibling modes in the other binaries).  SM4 is core
 * OpenSSL 3 functionality: never SKIP, only FAIL.

 * Block semantics: the vectors are block-aligned, so every KAT comparison
 * runs with padding disabled; padding itself gets its own section
 * (sm4_test_roundtrips: a nopad roundtrip of the block-aligned GB/T
 * plaintext and a padded roundtrip of a generated 37-byte pattern, 37 ->
 * 48 with a full pad block).
 */

/* This cipher's row: the GB/T key and 16-byte plaintext plus the GB/T
 * ciphertext. */
static const struct sm4_kat kat = {
    "SM4-ECB",
    NULL,                        /* ECB takes no IV */
    sm4_pt16_hex, SM4_PT16_LEN,
    /* the GB/T 32907-2016 ciphertext */
    "681edf34d206965e86b3e94f536e4246",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    sm4_test_kat(&kat);
    sm4_test_roundtrips(&kat);

    test_report_pass();
    return 0;
}

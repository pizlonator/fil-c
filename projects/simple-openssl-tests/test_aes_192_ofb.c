#include "test_common.h"
#include "test_aes_ofb_common.h"

#include <openssl/evp.h>

/*
 * test_aes_192_ofb -- AES-192-OFB (OFB-128) known-answer and behavior tests,
 * one binary per cipher (the shared OFB machinery, the vector provenance,
 * and the OFB-symmetry note live in test_aes_ofb_common.c).
 *
 * Known-answer provenance (AES-192): the constants below are the NIST SP
 * 800-38A F.4.2 row for the shared 4-block plaintext (the plaintext and the
 * IV 00..0f live in the common module), verified with:
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -aes-192-ofb -K <keyhex> -iv 000102...0f
 *
 * (and likewise for -aes-128-ofb and -aes-256-ofb in the sibling binaries).
 */

/* This cipher's F.4 row: key and ciphertext for the shared plaintext. */
static const struct ofb_kat kat = {
    "AES-192-OFB",
    "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b",
    "cdc80d6fddf18cab34c25909c99a4174"
    "fcc28b8d4c63837c09e81700c1100401"
    "8d9a9aeac0f6596f559c6d4daf59a5f2"
    "6d9f200857ca6c3e9cac524bd9acc92a",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    ofb_test_fetched_kat(&kat);
    ofb_test_streaming(&kat);
    ofb_test_decrypt_is_encrypt(&kat);
    ofb_test_zero_byte_update_noop(&kat);
    ofb_test_partial_final_block(&kat);

    test_report_pass();
    return 0;
}

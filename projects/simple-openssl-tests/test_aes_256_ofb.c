#include "test_common.h"
#include "test_aes_ofb_common.h"

#include <openssl/evp.h>

/*
 * test_aes_256_ofb -- AES-256-OFB (OFB-128) known-answer and behavior tests,
 * one binary per cipher (the shared OFB machinery, the vector provenance,
 * and the OFB-symmetry note live in test_aes_ofb_common.c).
 *
 * Known-answer provenance (AES-256): the constants below are the NIST SP
 * 800-38A F.4.3 row for the shared 4-block plaintext (the plaintext and the
 * IV 00..0f live in the common module), verified with:
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -aes-256-ofb -K <keyhex> -iv 000102...0f
 *
 * (and likewise for -aes-128-ofb and -aes-192-ofb in the sibling binaries).
 */

/* This cipher's F.4 row: key and ciphertext for the shared plaintext. */
static const struct ofb_kat kat = {
    "AES-256-OFB",
    "603deb1015ca71be2b73aef0857d7781"
    "1f352c073b6108d72d9810a30914dff4",
    "dc7e84bfda79164b7ecd8486985d3860"
    "4febdc6740d20b3ac88f6ad82a4fb08d"
    "71ab47a086e86eedf39d1c5bba97c408"
    "0126141d67f37be8538f5a8be740e484",
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

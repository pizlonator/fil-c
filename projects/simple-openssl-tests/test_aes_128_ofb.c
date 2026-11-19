#include "test_common.h"
#include "test_aes_ofb_common.h"

#include <openssl/evp.h>

/*
 * test_aes_128_ofb -- AES-128-OFB (OFB-128) known-answer and behavior tests,
 * one binary per cipher (the shared OFB machinery, the vector provenance,
 * and the OFB-symmetry note live in test_aes_ofb_common.c).
 *
 * Known-answer provenance (AES-128): the constants below are the NIST SP
 * 800-38A F.4.1 row for the shared 4-block plaintext (the plaintext and the
 * IV 00..0f live in the common module), verified with:
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -aes-128-ofb -K <keyhex> -iv 000102...0f
 *
 * (and likewise for -aes-192-ofb and -aes-256-ofb in the sibling binaries).
 */

/* This cipher's F.4 row: key and ciphertext for the shared plaintext. */
static const struct ofb_kat kat = {
    "AES-128-OFB",
    "2b7e151628aed2a6abf7158809cf4f3c",
    "3b3fd92eb72dad20333449f8e83cfb4a"
    "7789508d16918f03f53c52dac54ed825"
    "9740051e9c5fecf64344f7a82260edcc"
    "304c6528f659c77866a510d9c1d6ae5e",
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

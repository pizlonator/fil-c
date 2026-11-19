#include "test_common.h"
#include "test_aes_cfb_common.h"

#include <openssl/evp.h>

/*
 * test_aes_128_cfb -- AES-128-CFB(128) known-answer and behavior tests, one
 * binary per cipher (the shared CFB128 machinery, the vector provenance, and
 * the shift-register note live in test_aes_cfb_common.c).
 *
 * Known-answer provenance (AES-128): the constants below are the NIST SP
 * 800-38A F.3.1 CFB128-AES128.Encrypt row for the shared 4-block plaintext
 * (the plaintext and the IV 00..0f live in the common module; the matching
 * .Decrypt behavior is checked by decrypting the KAT, since CFB decryption
 * uses the forward cipher), verified with:
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -aes-128-cfb -K <keyhex> -iv 000102...0f
 *
 * (and likewise for -aes-192-cfb and -aes-256-cfb in the sibling binaries).
 */

/* This cipher's F.3 row: key and ciphertext for the shared plaintext. */
static const struct cfb_kat kat = {
    "AES-128-CFB",
    "2b7e151628aed2a6abf7158809cf4f3c",
    "3b3fd92eb72dad20333449f8e83cfb4a"
    "c8a64537a0b3a93fcde3cdad9f1ce58b"
    "26751f67a3cbb140b1808cf187a4f4df"
    "c04b05357c5d1c0eeac4c66f9ff7f2e6",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    cfb_test_fetched_kat(&kat);
    cfb_test_streaming(&kat);
    cfb_test_num_continuity(&kat);
    cfb_test_decrypt_num_and_roundtrip(&kat);

    test_report_pass();
    return 0;
}

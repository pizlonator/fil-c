#include "test_common.h"
#include "test_aes_cfb_common.h"

#include <openssl/evp.h>

/*
 * test_aes_192_cfb -- AES-192-CFB(128) known-answer and behavior tests, one
 * binary per cipher (the shared CFB128 machinery, the vector provenance, and
 * the shift-register note live in test_aes_cfb_common.c).
 *
 * Known-answer provenance (AES-192): the constants below are the NIST SP
 * 800-38A F.3.2 CFB128-AES192.Encrypt row for the shared 4-block plaintext
 * (the plaintext and the IV 00..0f live in the common module; the matching
 * .Decrypt behavior is checked by decrypting the KAT, since CFB decryption
 * uses the forward cipher), verified with:
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -aes-192-cfb -K <keyhex> -iv 000102...0f
 *
 * (and likewise for -aes-128-cfb and -aes-256-cfb in the sibling binaries).
 */

/* This cipher's F.3 row: key and ciphertext for the shared plaintext. */
static const struct cfb_kat kat = {
    "AES-192-CFB",
    "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b",
    "cdc80d6fddf18cab34c25909c99a4174"
    "67ce7f7f81173621961a2b70171d3d7a"
    "2e1e8a1dd59b88b1c8e60fed1efac4c9"
    "c05f9f9ca9834fa042ae8fba584b09ff",
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

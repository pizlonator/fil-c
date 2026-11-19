#include "test_common.h"
#include "test_aes_cfb_common.h"

#include <openssl/evp.h>

/*
 * test_aes_256_cfb -- AES-256-CFB(128) known-answer and behavior tests, one
 * binary per cipher (the shared CFB128 machinery, the vector provenance, and
 * the shift-register note live in test_aes_cfb_common.c).
 *
 * Known-answer provenance (AES-256): the constants below are the NIST SP
 * 800-38A F.3.3 CFB128-AES256.Encrypt row for the shared 4-block plaintext
 * (the plaintext and the IV 00..0f live in the common module; the matching
 * .Decrypt behavior is checked by decrypting the KAT, since CFB decryption
 * uses the forward cipher), verified with:
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -aes-256-cfb -K <keyhex> -iv 000102...0f
 *
 * (and likewise for -aes-128-cfb and -aes-192-cfb in the sibling binaries).
 */

/* This cipher's F.3 row: key and ciphertext for the shared plaintext. */
static const struct cfb_kat kat = {
    "AES-256-CFB",
    "603deb1015ca71be2b73aef0857d7781"
    "1f352c073b6108d72d9810a30914dff4",
    "dc7e84bfda79164b7ecd8486985d3860"
    "39ffed143b28b1c832113c6331e5407b"
    "df10132415e54b92a13ed0a8267ae2f9"
    "75a385741ab9cef82031623d55b1e471",
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

#include "test_common.h"
#include "test_aes_ecb_common.h"

#include <openssl/evp.h>

/*
 * test_aes_256_ecb -- AES-256-ECB known-answer and behavior tests, one
 * binary per cipher (the shared ECB machinery and the vector provenance
 * live in test_aes_ecb_common.c).
 *
 * Known-answer provenance (AES-256): the constants below are the NIST SP
 * 800-38A F.1 row for the shared 4-block plaintext (the plaintext lives in
 * the common module; ECB has no IV); the hex was verified against the
 * authoritative build with:
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -aes-256-ecb -nopad -K <keyhex>
 *
 * (and likewise for -aes-128-ecb and -aes-192-ecb in the sibling binaries).
 * The vectors are block-aligned, so every KAT comparison runs with padding
 * disabled; padding itself gets its own section in the common module.
 */

/* This cipher's F.1 row: key and ciphertext for the shared plaintext. */
static const struct ecb_kat kat = {
    "AES-256-ECB",
    "603deb1015ca71be2b73aef0857d7781"
    "1f352c073b6108d72d9810a30914dff4",
    "f3eed1bdb5d2a03c064b5a7e3db181f8"
    "591ccb10d410ed26dc5ba74a31362870"
    "b6ed21b99ca6f4f9f153e7b1beafed1d"
    "23304b7a39f9f3ff067d8d8f9e24ecc7",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    ecb_test_fetched_kat(&kat);
    ecb_test_alternate_routes(&kat, EVP_aes_256_ecb(), "aes-256-ecb");
    ecb_test_streaming(&kat);
    ecb_test_padding(&kat);
    ecb_test_zero_length(&kat);
    ecb_test_wrong_key_length(&kat, 16);

    test_report_pass();
    return 0;
}

#include "test_common.h"
#include "test_aes_cbc_common.h"

#include <openssl/evp.h>

/*
 * test_aes_256_cbc -- AES-256-CBC known-answer and behavior tests, one
 * binary per cipher (the shared CBC machinery and the vector provenance
 * live in test_aes_cbc_common.c).
 *
 * Known-answer provenance (AES-256): the constants below are the NIST SP
 * 800-38A F.2.9 row for the shared 4-block plaintext (the plaintext and the
 * IV 00..0f live in the common module); the hex was verified against the
 * authoritative build with:
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -aes-256-cbc -nopad -K <key> -iv <iv>
 *
 * (and likewise for -aes-128-cbc and -aes-192-cbc in the sibling binaries).
 * The vectors are block-aligned, so every CBC comparison runs with padding
 * disabled; padding itself gets its own section in the common module.
 */

/* This cipher's F.2 row: key and ciphertext for the shared plaintext. */
static const struct cbc_kat kat = {
    "AES-256-CBC",
    "603deb1015ca71be2b73aef0857d7781"
    "1f352c073b6108d72d9810a30914dff4",
    "f58c4c04d6e5f1ba779eabfb5f7bfbd6"
    "9cfc4e967edb808d679f777bc6702c7d"
    "39f23369a9d9bacfa530e26304231461"
    "b2eb05e2c39be9fcda6c19078c6a9d1b",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    cbc_test_fetched_kat(&kat);
    cbc_test_alternate_routes(&kat, EVP_aes_256_cbc(), "aes-256-cbc");
    cbc_test_streaming(&kat);
    cbc_test_padding(&kat);
    cbc_test_output_buffer_contract(&kat);

    test_report_pass();
    return 0;
}

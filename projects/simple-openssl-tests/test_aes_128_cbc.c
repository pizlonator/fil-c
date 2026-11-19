#include "test_common.h"
#include "test_aes_cbc_common.h"

#include <openssl/evp.h>

/*
 * test_aes_128_cbc -- AES-128-CBC known-answer and behavior tests, one
 * binary per cipher (the shared CBC machinery and the vector provenance
 * live in test_aes_cbc_common.c).
 *
 * Known-answer provenance (AES-128): the constants below are the NIST SP
 * 800-38A F.2.1 row for the shared 4-block plaintext (the plaintext and the
 * IV 00..0f live in the common module); the hex was verified against the
 * authoritative build with:
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -aes-128-cbc -nopad -K <key> -iv <iv>
 *
 * (and likewise for -aes-192-cbc and -aes-256-cbc in the sibling binaries).
 * The vectors are block-aligned, so every CBC comparison runs with padding
 * disabled; padding itself gets its own section in the common module.
 */

/* This cipher's F.2 row: key and ciphertext for the shared plaintext. */
static const struct cbc_kat kat = {
    "AES-128-CBC",
    "2b7e151628aed2a6abf7158809cf4f3c",
    "7649abac8119b246cee98e9b12e9197d"
    "5086cb9b507219ee95db113a917678b2"
    "73bed6b8e3c1743b7116e69e22229516"
    "3ff1caa1681fac09120eca307586e1a7",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    cbc_test_fetched_kat(&kat);
    cbc_test_alternate_routes(&kat, EVP_aes_128_cbc(), "aes-128-cbc");
    cbc_test_streaming(&kat);
    cbc_test_padding(&kat);
    cbc_test_output_buffer_contract(&kat);

    test_report_pass();
    return 0;
}

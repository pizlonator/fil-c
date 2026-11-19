#include "test_common.h"
#include "test_aes_cbc_common.h"

#include <openssl/evp.h>

/*
 * test_aes_192_cbc -- AES-192-CBC known-answer and behavior tests, one
 * binary per cipher (the shared CBC machinery and the vector provenance
 * live in test_aes_cbc_common.c).
 *
 * Known-answer provenance (AES-192): the constants below are the NIST SP
 * 800-38A F.2.5 row for the shared 4-block plaintext (the plaintext and the
 * IV 00..0f live in the common module); the hex was verified against the
 * authoritative build with:
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -aes-192-cbc -nopad -K <key> -iv <iv>
 *
 * (and likewise for -aes-128-cbc and -aes-256-cbc in the sibling binaries).
 * The vectors are block-aligned, so every CBC comparison runs with padding
 * disabled; padding itself gets its own section in the common module.
 */

/* This cipher's F.2 row: key and ciphertext for the shared plaintext. */
static const struct cbc_kat kat = {
    "AES-192-CBC",
    "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b",
    "4f021db243bc633d7178183a9fa071e8"
    "b4d9ada9ad7dedf4e5e738763f69145a"
    "571b242012fb7ae07fa9baac3df102e0"
    "08b0e27988598881d920a9e64f5615cd",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    cbc_test_fetched_kat(&kat);
    cbc_test_alternate_routes(&kat, EVP_aes_192_cbc(), "aes-192-cbc");
    cbc_test_streaming(&kat);
    cbc_test_padding(&kat);
    cbc_test_output_buffer_contract(&kat);

    test_report_pass();
    return 0;
}

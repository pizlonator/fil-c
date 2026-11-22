#include "test_common.h"
#include "test_aes_ecb_common.h"

#include <openssl/evp.h>

/*
 * test_aes_192_ecb -- AES-192-ECB known-answer and behavior tests, one
 * binary per cipher (the shared ECB machinery and the vector provenance
 * live in test_aes_ecb_common.c).
 *
 * Known-answer provenance (AES-192): the constants below are the NIST SP
 * 800-38A F.1 row for the shared 4-block plaintext (the plaintext lives in
 * the common module; ECB has no IV); the hex was verified against the
 * authoritative build with:
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -aes-192-ecb -nopad -K <keyhex>
 *
 * (and likewise for -aes-128-ecb and -aes-256-ecb in the sibling binaries).
 * The vectors are block-aligned, so every KAT comparison runs with padding
 * disabled; padding itself gets its own section in the common module.
 */

/* This cipher's F.1 row: key and ciphertext for the shared plaintext. */
static const struct ecb_kat kat = {
    "AES-192-ECB",
    "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b",
    "bd334f1d6e45f25ff712a214571fa5cc"
    "974104846d0ad3ad7734ecb3ecee4eef"
    "ef7afd2270e2e60adce0ba2face6444e"
    "9a4b41ba738d6c72fb16691603c18e0e",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    ecb_test_fetched_kat(&kat);
    ecb_test_alternate_routes(&kat, EVP_aes_192_ecb(), "AES-192-ECB");
    ecb_test_streaming(&kat);
    ecb_test_padding(&kat);
    ecb_test_zero_length(&kat);
    ecb_test_wrong_key_length(&kat, 32);

    test_report_pass();
    return 0;
}

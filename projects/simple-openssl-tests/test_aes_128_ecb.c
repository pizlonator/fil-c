#include "test_common.h"
#include "test_aes_ecb_common.h"

#include <openssl/evp.h>

/*
 * test_aes_128_ecb -- AES-128-ECB known-answer and behavior tests, one
 * binary per cipher (the shared ECB machinery and the vector provenance
 * live in test_aes_ecb_common.c).
 *
 * Known-answer provenance (AES-128): the constants below are the NIST SP
 * 800-38A F.1 row for the shared 4-block plaintext (the plaintext lives in
 * the common module; ECB has no IV); the hex was verified against the
 * authoritative build with:
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -aes-128-ecb -nopad -K <keyhex>
 *
 * (and likewise for -aes-192-ecb and -aes-256-ecb in the sibling binaries).
 * The vectors are block-aligned, so every KAT comparison runs with padding
 * disabled; padding itself gets its own section in the common module.
 */

/* This cipher's F.1 row: key and ciphertext for the shared plaintext. */
static const struct ecb_kat kat = {
    "AES-128-ECB",
    "2b7e151628aed2a6abf7158809cf4f3c",
    "3ad77bb40d7a3660a89ecaf32466ef97"
    "f5d3d58503b9699de785895a96fdbaaf"
    "43b1cd7f598ece23881b00e3ed030688"
    "7b0c785e27e8ad3f8223207104725dd4",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    ecb_test_fetched_kat(&kat);
    ecb_test_alternate_routes(&kat, EVP_aes_128_ecb(), "aes-128-ecb");
    ecb_test_streaming(&kat);
    ecb_test_padding(&kat);
    ecb_test_zero_length(&kat);
    ecb_test_wrong_key_length(&kat, 24);

    test_report_pass();
    return 0;
}

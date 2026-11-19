#include "test_common.h"
#include "test_aes_ctr_common.h"

#include <openssl/evp.h>

/*
 * test_aes_128_ctr -- AES-128-CTR known-answer and behavior tests, one
 * binary per cipher (the shared CTR machinery, the vector provenance, and
 * the counter-carry explanation live in test_aes_ctr_common.c).
 *
 * Known-answer provenance (AES-128): the constants below are the NIST SP
 * 800-38A F.5.1 row for the shared 4-block plaintext (plaintext, IV
 * f0f1..feff, and the counter-wrap IV ff..fd live in the common module),
 * verified with:
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -aes-128-ctr -K <keyhex> -iv f0f1...feff
 *
 * and the counter-wrap ciphertext below with the same CLI at -iv ff..fd
 * (and likewise for -aes-192-ctr and -aes-256-ctr in the sibling binaries).
 */

/* This cipher's F.5 row and its counter-wrap row for the shared IV ff..fd
 * (whose fourth block uses counter 0x0000...0000 -- the 128-bit carry). */
static const struct ctr_kat kat = {
    "AES-128-CTR",
    "2b7e151628aed2a6abf7158809cf4f3c",
    /* F.5.1, IV f0f1..feff */
    "874d6191b620e3261bef6864990db6ce"
    "9806f66b7970fdff8617187bb9fffdff"
    "5ae4df3edbd5d35e5b4f09020db03eab"
    "1e031dda2fbe03d1792170a0f3009cee",
    /* counter-wrap row, IV ff..fd */
    "953b86f8c8073dbe7e23a5132ffd65c8"
    "7f9a9ee1e5f6536db62d818609e163f2"
    "ba3a9a47e1ab62e5eccbbd032574f843"
    "8b684f49c5f702a49369b13c5f77637f",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    ctr_test_fetched_kat(&kat);
    ctr_test_streaming(&kat);
    ctr_test_counter_carry(&kat);
    ctr_test_partial_final_block(&kat);
    ctr_test_in_place(&kat);
    ctr_test_keystream_and_num(&kat);

    test_report_pass();
    return 0;
}

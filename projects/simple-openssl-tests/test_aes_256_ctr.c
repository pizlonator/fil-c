#include "test_common.h"
#include "test_aes_ctr_common.h"

#include <openssl/evp.h>

/*
 * test_aes_256_ctr -- AES-256-CTR known-answer and behavior tests, one
 * binary per cipher (the shared CTR machinery, the vector provenance, and
 * the counter-carry explanation live in test_aes_ctr_common.c).
 *
 * Known-answer provenance (AES-256): the constants below are the NIST SP
 * 800-38A F.5.3 row for the shared 4-block plaintext (plaintext, IV
 * f0f1..feff, and the counter-wrap IV ff..fd live in the common module),
 * verified with:
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -aes-256-ctr -K <keyhex> -iv f0f1...feff
 *
 * and the counter-wrap ciphertext below with the same CLI at -iv ff..fd
 * (and likewise for -aes-128-ctr and -aes-192-ctr in the sibling binaries).
 */

/* This cipher's F.5 row and its counter-wrap row for the shared IV ff..fd
 * (whose fourth block uses counter 0x0000...0000 -- the 128-bit carry). */
static const struct ctr_kat kat = {
    "AES-256-CTR",
    "603deb1015ca71be2b73aef0857d7781"
    "1f352c073b6108d72d9810a30914dff4",
    /* F.5.3, IV f0f1..feff */
    "601ec313775789a5b7a7f504bbf3d228"
    "f443e3ca4d62b59aca84e990cacaf5c5"
    "2b0930daa23de94ce87017ba2d84988d"
    "dfc9c58db67aada613c2dd08457941a6",
    /* counter-wrap row, IV ff..fd */
    "df1b19459e821b099fa3ed1c76994b65"
    "e1b03d150b59699e5c1947ae7e9cb8b5"
    "0bf435676b06c0cf7f3dc7d777173223"
    "13f7d2c44b80edc1ba660dbba57c9f44",
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

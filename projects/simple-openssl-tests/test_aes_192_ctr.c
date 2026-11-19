#include "test_common.h"
#include "test_aes_ctr_common.h"

#include <openssl/evp.h>

/*
 * test_aes_192_ctr -- AES-192-CTR known-answer and behavior tests, one
 * binary per cipher (the shared CTR machinery, the vector provenance, and
 * the counter-carry explanation live in test_aes_ctr_common.c).
 *
 * Known-answer provenance (AES-192): the constants below are the NIST SP
 * 800-38A F.5.2 row for the shared 4-block plaintext (plaintext, IV
 * f0f1..feff, and the counter-wrap IV ff..fd live in the common module),
 * verified with:
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -aes-192-ctr -K <keyhex> -iv f0f1...feff
 *
 * and the counter-wrap ciphertext below with the same CLI at -iv ff..fd
 * (and likewise for -aes-128-ctr and -aes-256-ctr in the sibling binaries).
 */

/* This cipher's F.5 row and its counter-wrap row for the shared IV ff..fd
 * (whose fourth block uses counter 0x0000...0000 -- the 128-bit carry). */
static const struct ctr_kat kat = {
    "AES-192-CTR",
    "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b",
    /* F.5.2, IV f0f1..feff */
    "1abc932417521ca24f2b0459fe7e6e0b"
    "090339ec0aa6faefd5ccc2c6f4ce8e94"
    "1e36b26bd1ebc670d1bd1d665620abf7"
    "4f78a7f6d29809585a97daec58c6b050",
    /* counter-wrap row, IV ff..fd */
    "9c4c15d9d83c72a9416b76444ad20b41"
    "4de612fb986512ea8b367261ef5ccbc9"
    "07ca2d0fbdaad7e0d004a17368b78a5d"
    "d4da09cb96e73e84325860b50c01665b",
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

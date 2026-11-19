#include "test_common.h"
#include "test_aes_cfb8_common.h"

#include <openssl/evp.h>

/*
 * test_aes_256_cfb8 -- AES-256-CFB8 known-answer and behavior tests, one
 * binary per cipher (the shared CFB8 machinery, the vector provenance, the
 * byte-granular continuity property, and the implementation note live in
 * test_aes_cfb8_common.c).
 *
 * Known-answer provenance (AES-256): the constants below are the NIST SP
 * 800-38A F.3.15 CFB8-AES256 row for the shared 4-block plaintext (the
 * plaintext and the IV 00..0f live in the common module), verified with:
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -aes-256-cfb8 -K <keyhex> -iv 000102...0f
 *
 * (and likewise for -aes-128-cfb8 and -aes-192-cfb8 in the sibling
 * binaries).
 */

/* This cipher's F.3 row: key and ciphertext for the shared plaintext. */
static const struct cfb8_kat kat = {
    "AES-256-CFB8",
    "603deb1015ca71be2b73aef0857d7781"
    "1f352c073b6108d72d9810a30914dff4",
    "dc1f1a8520a64db55fcc8ac554844e88"
    "9700adc6e10c63cf2d8cd2d8ce668f3e"
    "b9191719c47444fb43bff9b9883c2cd0"
    "51120402009f974998c89d195722a75b",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    cfb8_test_fetched_kat(&kat);
    cfb8_test_byte_at_a_time(&kat);
    cfb8_test_streaming(&kat);
    cfb8_test_zero_update_and_roundtrip(&kat);

    test_report_pass();
    return 0;
}

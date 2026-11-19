#include "test_common.h"
#include "test_aes_cfb8_common.h"

#include <openssl/evp.h>

/*
 * test_aes_128_cfb8 -- AES-128-CFB8 known-answer and behavior tests, one
 * binary per cipher (the shared CFB8 machinery, the vector provenance, the
 * byte-granular continuity property, and the implementation note live in
 * test_aes_cfb8_common.c).
 *
 * Known-answer provenance (AES-128): the constants below are the NIST SP
 * 800-38A F.3.13 CFB8-AES128 row for the shared 4-block plaintext (the
 * plaintext and the IV 00..0f live in the common module), verified with:
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -aes-128-cfb8 -K <keyhex> -iv 000102...0f
 *
 * (and likewise for -aes-192-cfb8 and -aes-256-cfb8 in the sibling
 * binaries).  The first ciphertext byte agrees with the published F.3
 * CFB8-AES128 example (3b79424c...).
 */

/* This cipher's F.3 row: key and ciphertext for the shared plaintext. */
static const struct cfb8_kat kat = {
    "AES-128-CFB8",
    "2b7e151628aed2a6abf7158809cf4f3c",
    "3b79424c9c0dd436bace9e0ed4586a4f"
    "32b9ded50ae3ba69d472e88267fb5052"
    "70cbad1e257691f7c47c5038297edda3"
    "2ff26d0ed19174096161ecc14086dd62",
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

#include "test_common.h"
#include "test_aes_cfb8_common.h"

#include <openssl/evp.h>

/*
 * test_aes_192_cfb8 -- AES-192-CFB8 known-answer and behavior tests, one
 * binary per cipher (the shared CFB8 machinery, the vector provenance, the
 * byte-granular continuity property, and the implementation note live in
 * test_aes_cfb8_common.c).
 *
 * Known-answer provenance (AES-192): the constants below are the NIST SP
 * 800-38A F.3.14 CFB8-AES192 row for the shared 4-block plaintext (the
 * plaintext and the IV 00..0f live in the common module), verified with:
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -aes-192-cfb8 -K <keyhex> -iv 000102...0f
 *
 * (and likewise for -aes-128-cfb8 and -aes-256-cfb8 in the sibling
 * binaries).
 */

/* This cipher's F.3 row: key and ciphertext for the shared plaintext. */
static const struct cfb8_kat kat = {
    "AES-192-CFB8",
    "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b",
    "cda2521ef0a905ca44cd057cbf0d47a0"
    "678a7bcfb6aeaa3047b38936021f48bb"
    "b63cefdac02b2e840904efce6f4326be"
    "228683739063dc30e937ffedd63e3c94",
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

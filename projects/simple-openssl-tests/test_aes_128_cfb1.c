#include "test_common.h"
#include "test_aes_cfb1_common.h"

#include <openssl/evp.h>

/*
 * test_aes_128_cfb1 -- AES-128-CFB1 in both of its OpenSSL operating modes
 * (byte mode the default, bit-length mode via EVP_CIPH_FLAG_LENGTH_BITS),
 * one binary per cipher (the shared CFB1 machinery, the vector provenance,
 * and the whole bit-mode route/pitfalls discussion live in
 * test_aes_cfb1_common.c).
 *
 * Known-answer provenance (AES-128): the constants below are the byte-mode
 * key/ciphertext for the shared 4-block plaintext (the plaintext and the IV
 * 00..0f live in the common module), verified with:
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -aes-128-cfb1 -K <keyhex> -iv 000102...0f
 *
 * (and likewise for -aes-192-cfb1 and -aes-256-cfb1 in the sibling
 * binaries).  Bit-length mode has no independent KAT; it is verified against
 * byte mode in the common module (exact, since byte mode feeds 8 bits per
 * byte), including odd bit counts.
 */

/* This cipher's byte-mode row: key and ciphertext for the shared plaintext. */
static const struct cfb1_kat kat = {
    "AES-128-CFB1",
    "2b7e151628aed2a6abf7158809cf4f3c",
    "68b3a264f838f5f8c3101070d1ab4c2e"
    "22e7f950383a0b71ade4fad0095cb188"
    "a57972c3c1882615f7511411fbebf119"
    "3997069704fc1d1f27028434c99e60f4",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    cfb1_test_byte_mode_kat(&kat);
    cfb1_test_byte_mode_streaming(&kat);
    cfb1_test_byte_mode_zero_update(&kat);
    cfb1_test_bit_mode_consistency(&kat);
    cfb1_test_bit_mode_41_bits(&kat);
    cfb1_test_bit_mode_split(&kat);

    test_report_pass();
    return 0;
}

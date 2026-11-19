#include "test_common.h"
#include "test_aes_cfb1_common.h"

#include <openssl/evp.h>

/*
 * test_aes_256_cfb1 -- AES-256-CFB1 in both of its OpenSSL operating modes
 * (byte mode the default, bit-length mode via EVP_CIPH_FLAG_LENGTH_BITS),
 * one binary per cipher (the shared CFB1 machinery, the vector provenance,
 * and the whole bit-mode route/pitfalls discussion live in
 * test_aes_cfb1_common.c).
 *
 * Known-answer provenance (AES-256): the constants below are the byte-mode
 * key/ciphertext for the shared 4-block plaintext (the plaintext and the IV
 * 00..0f live in the common module), verified with:
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -aes-256-cfb1 -K <keyhex> -iv 000102...0f
 *
 * (and likewise for -aes-128-cfb1 and -aes-192-cfb1 in the sibling
 * binaries).  Bit-length mode has no independent KAT; it is verified against
 * byte mode in the common module (exact, since byte mode feeds 8 bits per
 * byte), including odd bit counts.
 */

/* This cipher's byte-mode row: key and ciphertext for the shared plaintext. */
static const struct cfb1_kat kat = {
    "AES-256-CFB1",
    "603deb1015ca71be2b73aef0857d7781"
    "1f352c073b6108d72d9810a30914dff4",
    "9029c2ba5b7d440b562023deec3de592"
    "8e4fd76528e8cc3a548a0a49edf001d0"
    "d163541e6192479f27fe19a4f75d600d"
    "e033103f1d2bc1794ce1cf1464c0603b",
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

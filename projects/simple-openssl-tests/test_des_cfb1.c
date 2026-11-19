#include "test_common.h"

#include "test_des_common.h"

#include <openssl/evp.h>

/*

 * test_des_cfb1 -- DES-CFB1 known-answer and behavior tests, one binary
 * per cipher (the shared DES machinery, the provider situation, the
 * derivation provenance of the CLI-derived constants, and the family-wide
 * commentary live in test_des_common.c).

 * Known-answer provenance (DES-CFB1): the 32-byte plaintext and the IV
 * 0102030405060708 are family-wide constants in the common module; the
 * KAT ciphertext below was verified with

 *   printf '<pt hex>' | xxd -r -p | pizfix/bin/openssl enc -des-cfb1 \
 *       -nopad -provider legacy -provider default -K <keyhex> -iv <ivhex>

 * and cross-checked against the system OpenSSL 3.0.13 CLI (identical).

 * CFB1 runs one CFB step per input BIT, but only in BYTE mode: see
 * des_test_cfb1_byte_mode in test_des_common.c for what
 * EVP_CIPH_FLAG_LENGTH_BITS does (and does not do) for DES -- the flag is
 * accepted, the update length remains a byte count, and the flagged
 * output equals the byte-mode output.  (Unlike AES-CFB1, DES-CFB1 pins
 * byte mode only.)
 */

/* This cipher's row: the shared key, IV, and 32-byte plaintext plus this
 * cipher's KAT ciphertext. */
static const struct des_kat kat = {
    "DES-CFB1",
    des_key_hex, des_iv_hex,
    des_pt32_hex, 32,
    "b0047cfd4ae7e6c1c490d237bc3c1938"
    "9d723230649de06a62e8e92519898265",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    des_load_providers();

    des_test_kat(&kat);
    des_test_cfb1_byte_mode(&kat);

    test_report_pass();
    return 0;
}

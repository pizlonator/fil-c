#include "test_common.h"

#include "test_des_common.h"

#include <openssl/evp.h>

/*

 * test_des_cfb8 -- DES-CFB8 known-answer and behavior tests, one binary
 * per cipher (the shared DES machinery, the provider situation, the
 * derivation provenance of the CLI-derived constants, and the family-wide
 * commentary live in test_des_common.c).

 * Known-answer provenance (DES-CFB8): the 32-byte plaintext and the IV
 * 0102030405060708 are family-wide constants in the common module; the
 * KAT ciphertext below was verified with

 *   printf '<pt hex>' | xxd -r -p | pizfix/bin/openssl enc -des-cfb8 \
 *       -nopad -provider legacy -provider default -K <keyhex> -iv <ivhex>

 * and cross-checked against the system OpenSSL 3.0.13 CLI (identical).

 * CFB8 runs one bit-byte per 8 input bits: byte-at-a-time updates must
 * reproduce the one-shot ciphertext (the 8-bit shift register carries the
 * byte boundary), for both directions.
 */

/* This cipher's row: the shared key, IV, and 32-byte plaintext plus this
 * cipher's KAT ciphertext. */
static const struct des_kat kat = {
    "DES-CFB8",
    des_key_hex, des_iv_hex,
    des_pt32_hex, 32,
    "e788798e955d41d7ec86f1d625b7ba7c"
    "fe86ca12b47363c6aecd77b8cb4dfa3d",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    des_load_providers();

    des_test_kat(&kat);
    des_test_cfb8_bytewise(&kat);

    test_report_pass();
    return 0;
}

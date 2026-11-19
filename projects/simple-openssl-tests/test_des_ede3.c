#include "test_common.h"

#include "test_des3_common.h"

#include <openssl/evp.h>

/*

 * test_des_ede3 -- three-key DES-EDE3 (ECB) known-answer and behavior
 * tests, one binary per cipher (the shared DES-EDE3 machinery, the
 * provider situation, the derivation provenance of the CLI-derived
 * constants, and the family-wide commentary live in test_des3_common.c).

 * Known-answer provenance (DES-EDE3): the fixed 24-byte key and the
 * 8-byte plaintext "ABCDEFGH" are family-wide constants in the common
 * module; the KAT ciphertext below was verified with

 *   printf '<pt hex>' | xxd -r -p | pizfix/bin/openssl enc -des-ede3 \
 *       -nopad -K <keyhex>

 * and cross-checked against the system OpenSSL 3.0.13 CLI (identical).
 * The DES-EDE3 ciphers are supplied by the DEFAULT provider, so no
 * provider loading is needed here.

 * 8-byte-block semantics: a nopad operation needs an exact multiple of 8;
 * partial updates {1,7,8,9,16,17} reproduce the one-shot result; padded
 * operations roundtrip odd sizes (33 bytes -> 40 with a full pad block).
 * DES-EDE3 has a FIXED 24-byte key length (the provider TDES flags carry
 * no VARIABLE_LENGTH bit), which des3_test_wrong_key_length exercises
 * through set_key_length() + init (it must refuse to shrink the key).
 */

/* This cipher's row: the shared 24-byte key and 8-byte plaintext plus
 * this cipher's KAT ciphertext ("" IV: ECB takes no IV). */
static const struct des3_kat kat = {
    "DES-EDE3",
    "",
    des3_pt8_hex, 8,
    "ddb4a8bce83fd7a2",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    des3_test_kat(&kat);
    des3_test_block_streaming(&kat);
    des3_test_wrong_key_length(&kat);

    test_report_pass();
    return 0;
}

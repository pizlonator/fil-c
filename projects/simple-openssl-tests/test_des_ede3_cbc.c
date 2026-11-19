#include "test_common.h"

#include "test_des3_common.h"

#include <openssl/evp.h>

/*

 * test_des_ede3_cbc -- three-key DES-EDE3-CBC known-answer and behavior
 * tests, one binary per cipher (the shared DES-EDE3 machinery, the
 * provider situation, the derivation provenance of the CLI-derived
 * constants, and the family-wide commentary live in test_des3_common.c).

 * Known-answer provenance (DES-EDE3-CBC): the fixed 24-byte key, the IV,
 * and the 16-byte plaintext are family-wide constants in the common
 * module; the KAT ciphertext below was verified with

 *   printf '<pt hex>' | xxd -r -p | pizfix/bin/openssl enc -des-ede3-cbc \
 *       -nopad -K <keyhex> -iv <ivhex>

 * and cross-checked against the system OpenSSL 3.0.13 CLI (identical).
 * The DES-EDE3 ciphers are supplied by the DEFAULT provider, so no
 * provider loading is needed here.

 * 8-byte-block semantics: a nopad operation needs an exact multiple of 8;
 * CBC decryption holds the last block until Final (so partial updates
 * {1,7,8,9,16,17} reproduce the one-shot result only once a full block is
 * available); padded operations roundtrip odd sizes (33 bytes -> 40 with
 * a full pad block).
 */

/* This cipher's row: the shared 24-byte key, IV, and 16-byte plaintext
 * plus this cipher's KAT ciphertext. */
static const struct des3_kat kat = {
    "DES-EDE3-CBC",
    des3_iv_hex,
    des3_pt16_hex, 16,
    "cbcc5741a0df298c97b3a5917cad747d",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    des3_test_kat(&kat);
    des3_test_block_streaming(&kat);

    test_report_pass();
    return 0;
}

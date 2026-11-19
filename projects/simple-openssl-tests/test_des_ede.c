#include "test_common.h"

#include "test_des_common.h"

#include <openssl/evp.h>

/*

 * test_des_ede -- two-key DES-EDE (ECB) known-answer and behavior tests,
 * one binary per cipher (the shared DES machinery, the provider situation,
 * the derivation provenance of the CLI-derived constants, and the
 * family-wide commentary live in test_des_common.c).

 * Known-answer provenance (DES-EDE): the two-key 16-byte key and the
 * 8-byte plaintext "ABCDEFGH" are family-wide constants in the common
 * module; the KAT ciphertext below was verified with

 *   printf '<pt hex>' | xxd -r -p | pizfix/bin/openssl enc -des-ede \
 *       -nopad -provider legacy -provider default -K <keyhex>

 * and cross-checked against the system OpenSSL 3.0.13 CLI (identical).
 * Unlike single DES and DESX, the DES-EDE ciphers are supplied by the
 * DEFAULT provider; des_load_providers() loads both (loading legacy turns
 * off default auto-load), and the fetch then still comes from "default".

 * 8-byte-block semantics: a nopad operation needs an exact multiple of 8;
 * padded operations roundtrip odd sizes (33 bytes -> 40 with a full pad
 * block); partial updates {1,7,8,9} reproduce the one-shot result; and a
 * 0-byte update is a no-op.
 */

/* This cipher's row: the shared two-key EDE key and 8-byte plaintext plus
 * this cipher's KAT ciphertext ("" IV: ECB takes no IV). */
static const struct des_kat kat = {
    "DES-EDE",
    des_ede_key_hex, "",
    des_pt8_hex, 8,
    "5b4605e606393286",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    des_load_providers();

    des_test_kat(&kat);
    des_test_block_semantics(&kat);

    test_report_pass();
    return 0;
}

#include "test_common.h"

#include "test_des_common.h"

#include <openssl/evp.h>

/*

 * test_des_cfb -- DES-CFB (CFB64) known-answer and behavior tests, one
 * binary per cipher (the shared DES machinery, the provider situation, the
 * derivation provenance of the CLI-derived constants, and the family-wide
 * commentary live in test_des_common.c).

 * Known-answer provenance (DES-CFB): the 32-byte plaintext and the IV
 * 0102030405060708 are family-wide constants in the common module; the
 * KAT ciphertext below was verified with

 *   printf '<pt hex>' | xxd -r -p | pizfix/bin/openssl enc -des-cfb \
 *       -nopad -provider legacy -provider default -K <keyhex> -iv <ivhex>

 * and cross-checked against the system OpenSSL 3.0.13 CLI (identical);
 * with the shared IV it agrees with the OFB row on the first block (both
 * are plaintext XOR E(IV) there).

 * Stream-mode semantics: EVP reports block size 1 for DES-CFB, so every
 * update streams its input through byte-for-byte, the ciphertext shift
 * register plus num offset must carry across updates (partial updates
 * {1,7,8,9} reproduce the one-shot result), and a 0-byte update is a
 * no-op that leaves that state untouched.
 */

/* This cipher's row: the shared key, IV, and 32-byte plaintext plus this
 * cipher's KAT ciphertext. */
static const struct des_kat kat = {
    "DES-CFB",
    des_key_hex, des_iv_hex,
    des_pt32_hex, 32,
    "e7ac3c7c22bd1909110f832bb76c786c"
    "0507aa1bf0eb325d9888d6d6b19ec2f3",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    des_load_providers();

    des_test_kat(&kat);
    des_test_stream_modes(&kat);

    test_report_pass();
    return 0;
}

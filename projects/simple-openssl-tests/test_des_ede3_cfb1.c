#include "test_common.h"

#include "test_des3_common.h"

#include <openssl/evp.h>

/*

 * test_des_ede3_cfb1 -- three-key DES-EDE3-CFB1 known-answer and behavior
 * tests, one binary per cipher (the shared DES-EDE3 machinery, the
 * provider situation, the derivation provenance of the CLI-derived
 * constants, and the family-wide commentary live in test_des3_common.c).

 * Known-answer provenance (DES-EDE3-CFB1): the fixed 24-byte key, the IV,
 * and the 32-byte plaintext are family-wide constants in the common
 * module; the KAT ciphertext below was verified with

 *   printf '<pt hex>' | xxd -r -p | pizfix/bin/openssl enc -des-ede3-cfb1 \
 *       -nopad -K <keyhex> -iv <ivhex>

 * and cross-checked against the system OpenSSL 3.0.13 CLI (identical).
 * The DES-EDE3 ciphers are supplied by the DEFAULT provider, so no
 * provider loading is needed here.

 * CFB1 runs one CFB step per input BIT, but only in byte mode: the shift
 * register carries state across updates -- a 17-then-15 split reproduces
 * the one-shot result in both directions -- and a 0-byte update is a
 * no-op.  (The EVP_CIPH_FLAG_LENGTH_BITS route is DES-CFB1's business --
 * see test_des_cfb1/test_des_common.c.)
 */

/* This cipher's row: the shared 24-byte key, IV, and 32-byte plaintext
 * plus this cipher's KAT ciphertext. */
static const struct des3_kat kat = {
    "DES-EDE3-CFB1",
    des3_iv_hex,
    des3_pt32_hex, 32,
    "f5f68525c37bf9e834232891126d50d0"
    "0a7f7a84916ef19dd37091d780dd4d7b",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    des3_test_kat(&kat);
    des3_test_stream_details(&kat);

    test_report_pass();
    return 0;
}

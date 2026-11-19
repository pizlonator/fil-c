#include "test_common.h"

#include "test_des3_common.h"

#include <openssl/evp.h>

/*

 * test_des_ede3_ofb -- three-key DES-EDE3-OFB known-answer and behavior
 * tests, one binary per cipher (the shared DES-EDE3 machinery, the
 * provider situation, the derivation provenance of the CLI-derived
 * constants, and the family-wide commentary live in test_des3_common.c).

 * Known-answer provenance (DES-EDE3-OFB): the fixed 24-byte key, the IV,
 * and the 32-byte plaintext are family-wide constants in the common
 * module; the KAT ciphertext below was verified with

 *   printf '<pt hex>' | xxd -r -p | pizfix/bin/openssl enc -des-ede3-ofb \
 *       -nopad -K <keyhex> -iv <ivhex>

 * and cross-checked against the system OpenSSL 3.0.13 CLI (identical).
 * The DES-EDE3 ciphers are supplied by the DEFAULT provider, so no
 * provider loading is needed here.

 * Stream-mode semantics: the keystream register carries state across
 * updates -- a 17-then-15 split reproduces the one-shot result in both
 * directions -- and a 0-byte update is a no-op.
 */

/* This cipher's row: the shared 24-byte key, IV, and 32-byte plaintext
 * plus this cipher's KAT ciphertext. */
static const struct des3_kat kat = {
    "DES-EDE3-OFB",
    des3_iv_hex,
    des3_pt32_hex, 32,
    "a8e9a1f9f02019748231553969e25ff7"
    "ea94f1fca931e937c58bada249679c4f",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    des3_test_kat(&kat);
    des3_test_stream_details(&kat);

    test_report_pass();
    return 0;
}

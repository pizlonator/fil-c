#include "test_common.h"

#include "test_des_common.h"

#include <openssl/evp.h>

/*

 * test_des_ede_ofb -- two-key DES-EDE-OFB known-answer and behavior tests,
 * one binary per cipher (the shared DES machinery, the provider situation,
 * the derivation provenance of the CLI-derived constants, and the
 * family-wide commentary live in test_des_common.c).

 * Known-answer provenance (DES-EDE-OFB): the two-key 16-byte key, the IV,
 * and the 32-byte plaintext are family-wide constants in the common
 * module; this cipher's KAT ciphertext was verified with the default
 * provider alone supplying the cipher and no -nopad needed (OFB emits one
 * byte per input byte, so `enc` adds no padding):

 *   printf '<pt hex>' | xxd -r -p | pizfix/bin/openssl enc -des-ede-ofb \
 *       -K <keyhex> -iv <ivhex>

 * and cross-checked against the system OpenSSL 3.0.13 CLI (identical).

 * DES-EDE-OFB is 8-byte-block OFB with a 16-byte (two-key) key and an
 * 8-byte IV; EVP reports block size 1 for it, so every update streams its
 * input through byte-for-byte and the keystream register plus num offset
 * must carry across updates (partial updates {1,7,8,9} reproduce the
 * one-shot result).  With the shared IV it agrees with DES-EDE-CFB on the
 * first block and diverges after it (only the feedback differs).
 */

/* This cipher's row: the shared two-key EDE key, IV, and 32-byte
 * plaintext plus this cipher's KAT ciphertext. */
static const char *const EDE_OFB_CT_HEX =
    "b233a10f047d26655050f492f0906a6c"
    "1d829b3f7c5f988fe883a88ff3103280";

static const struct des_kat kat = {
    "DES-EDE-OFB",
    des_ede_key_hex, des_iv_hex,
    des_pt32_hex, 32,
    EDE_OFB_CT_HEX,
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

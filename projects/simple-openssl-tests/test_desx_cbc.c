#include "test_common.h"

#include "test_des_common.h"

#include <openssl/evp.h>

/*

 * test_desx_cbc -- DESX-CBC known-answer and behavior tests, one binary
 * per cipher (the shared DES machinery, the provider situation, the
 * derivation provenance of the CLI-derived constants, and the family-wide
 * commentary live in test_des_common.c).

 * Known-answer provenance (DESX-CBC): the IV and the 16-byte plaintext are
 * family-wide constants in the common module; the 24-byte key and the KAT
 * ciphertext below are this cipher's own and were verified with

 *   printf '<pt hex>' | xxd -r -p | pizfix/bin/openssl enc -desx-cbc \
 *       -nopad -provider legacy -provider default -K <keyhex> -iv <ivhex>

 * (note `openssl enc -desx-cbc` also works with both providers loaded)
 * and cross-checked against the system OpenSSL 3.0.13 CLI (identical).

 * The DESX-CBC 24-byte key is [8-byte DES key][8-byte input whitening]
 * [8-byte output whitening] (cipher_desx_hw.c: DES_set_key_unchecked on
 * key[0..7], ks2 = key[8..15], ks3 = key[16..23]); the composition check
 * (des_test_desx_composition) rebuilds DESX from per-block DES-ECB calls
 * exactly as DES_xcbc_encrypt chains them (chain_0 = IV, chain_i = the
 * WHITENED previous ciphertext).

 * 8-byte-block semantics: a nopad operation needs an exact multiple of 8;
 * padded operations roundtrip odd sizes (33 bytes -> 40 with a full pad
 * block); CBC-style decryption holds the last block until Final (so
 * partial updates {1,7,8,9} reproduce the one-shot result only once a
 * full block is available); and a 0-byte update is a no-op.
 */

/* DESX-CBC: 24-byte key = [DES key][input whitening][output whitening]. */
static const char *const DESXKEY_HEX =
    "0102030405060708090a0b0c0d0e0f10"
    "1112131415161718";
static const char *const DESX_CT_HEX =
    "08a2885ed8f904ce693763307fbf570f";

/* This cipher's row: its own key and KAT ciphertext plus the shared IV
 * and 16-byte plaintext. */
static const struct des_kat kat = {
    "DESX-CBC",
    DESXKEY_HEX, des_iv_hex,
    des_pt16_hex, 16,
    DESX_CT_HEX,
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    des_load_providers();

    des_test_kat(&kat);
    des_test_block_semantics(&kat);
    des_test_desx_composition(&kat);

    test_report_pass();
    return 0;
}

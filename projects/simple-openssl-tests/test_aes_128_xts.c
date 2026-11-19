#include "test_common.h"
#include "test_aes_xts_common.h"

#include <openssl/evp.h>

/*
 * test_aes_128_xts -- AES-128-XTS known-answer and behavior tests for small
 * data units, one binary per cipher (the XTS key/IV conventions, the IEEE
 * 1619-2007 vector provenance, and the behavior notes live in
 * test_aes_xts_common.c; the test_aes_256_xts_large binary covers the
 * 16 MiB data-unit size boundary).
 *
 * Known-answer provenance (AES-128): the constants below are the IEEE Std
 * 1619-2007 XTS-AES test vectors as distributed in OpenSSL's own test data
 * (test/recipes/30-test_evp_data/evpciph_aes_common.txt, "AES XTS test
 * vectors from IEEE Std 1619-2007" and the "Exercise different lengths
 * covering even ciphertext stealing cases" set).  Every hex constant below
 * was re-verified against the authoritative pizfix OpenSSL 3.6.5 with a
 * scratch program compiled by build/bin/filcc (`openssl enc` refuses XTS
 * ciphers, so the CLI oracle is not available here).
 */

/* The vectors.  keyhex is always 32 hex digits = two concatenated AES-128
 * keys; ivhex is the 128-bit data-unit number; pthex/cthex are the plaintext
 * and ciphertext of one data unit.  (The 1111.../2222... key is the second
 * AES-128 vector set of the same IEEE 1619-2007 distribution.) */
static const struct xts_kat kats[] = {
    { "128-16B", "AES-128-XTS",
      xts_key_128_ieee_hex,
      "9a785634120000000000000000000000",
      "000102030405060708090a0b0c0d0e0f10",
      "6c1625db4671522d3d7599601de7ca09ed" },
    { "128-17B", "AES-128-XTS",
      xts_key_128_ieee_hex,
      "9a785634120000000000000000000000",
      "000102030405060708090a0b0c0d0e0f1011",
      "d069444b7a7e0cab09e24447d24deb1fedbf" },
    { "128-18B", "AES-128-XTS",
      xts_key_128_ieee_hex,
      "9a785634120000000000000000000000",
      "000102030405060708090a0b0c0d0e0f101112",
      "e5df1351c0544ba1350b3363cd8ef4beedbf9d" },
    { "128-19B", "AES-128-XTS",
      xts_key_128_ieee_hex,
      "9a785634120000000000000000000000",
      "000102030405060708090a0b0c0d0e0f10111213",
      "9d84c813f719aa2c7be3f66171c7c5c2edbf9dac" },
    { "128-32B", "AES-128-XTS",
      "1111111111111111111111111111111122222222222222222222222222222222",
      "33333333330000000000000000000000",
      "4444444444444444444444444444444444444444444444444444444444444444",
      "c454185e6a16936e39334038acef838bfb186fff7480adc4289382ecd6d394f0" },
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    xts_test_kats(kats, sizeof(kats) / sizeof(kats[0]));
    xts_test_short_data_units_fail(kats[0].ciphername, kats[0].keyhex,
                                   kats[0].ivhex);
    xts_test_stealing_roundtrip(kats[0].ciphername, kats[0].keyhex,
                                kats[0].ivhex);
    xts_test_each_update_is_a_data_unit(kats[0].ciphername, kats[0].keyhex,
                                        kats[0].ivhex);
    xts_test_weak_key(kats[0].ciphername, kats[0].keyhex, kats[0].ivhex);
    xts_test_wrong_key_length(kats[0].ciphername, kats[0].keyhex,
                              kats[0].ivhex);

    test_report_pass();
    return 0;
}

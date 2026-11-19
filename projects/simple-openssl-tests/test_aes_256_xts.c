#include "test_common.h"
#include "test_aes_xts_common.h"

#include <openssl/evp.h>

/*
 * test_aes_256_xts -- AES-256-XTS known-answer and behavior tests for small
 * data units, one binary per cipher (the XTS key/IV conventions, the IEEE
 * 1619-2007 vector provenance, and the behavior notes live in
 * test_aes_xts_common.c; the test_aes_256_xts_large binary covers the
 * 16 MiB data-unit size boundary).  Like the AES-128 row, this row
 * cross-checks that the shared XTS helpers are key-size independent.
 *
 * Known-answer provenance (AES-256): the constants below are the IEEE Std
 * 1619-2007 XTS-AES test vectors as distributed in OpenSSL's own test data
 * (test/recipes/30-test_evp_data/evpciph_aes_common.txt, "AES XTS test
 * vectors from IEEE Std 1619-2007" and the "Exercise different lengths
 * covering even ciphertext stealing cases" set).  Every hex constant below
 * was re-verified against the authoritative pizfix OpenSSL 3.6.5 with a
 * scratch program compiled by build/bin/filcc (`openssl enc` refuses XTS
 * ciphers, so the CLI oracle is not available here).
 */

/* The vectors.  keyhex is always 64 hex digits = two concatenated AES-256
 * keys; ivhex is the 128-bit data-unit number; pthex/cthex are the plaintext
 * and ciphertext of one data unit. */
static const struct xts_kat kats[] = {
    { "256-16B", "AES-256-XTS",
      xts_key_256_ieee_hex,
      "00000000000000000000000000000000",
      "000102030405060708090a0b0c0d0e0f10",
      "b5f737852b18e4cf31d7353ff220ca5e3a" },
    { "256-98B", "AES-256-XTS",
      xts_key_256_ieee_hex,
      "00000000000000000000000000000000",
      "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
      "202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f"
      "404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f"
      "6061",
      "3a060a8cad115a6f44572e3759e43c8fcad8bfcb233ff6ad71b7c1e7ca651508"
      "860aed34ec9506d368aa50274a31c16d2deae4d64c2a8096091c093f3820fb6d"
      "21089bcedaac361e3febe706cafe14b9bc89de34258d32ec3ad59fd2986a401e"
      "6bff" },
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

#include "test_common.h"
#include "test_aes_xts_common.h"

#include <openssl/evp.h>

/*
 * test_aes_256_xts_large -- AES-256-XTS at the data-unit size boundary, one
 * binary per cipher (the boundary derivation and the behavior notes live in
 * test_aes_xts_common.c).
 *
 * IEEE Std 1619-2018 (and NIST SP 800-38E) limit an XTS data unit to 2^20
 * AES blocks = 16 MiB.  OpenSSL enforces this in the provider, so a single
 * update of exactly 16777216 bytes is the largest permitted data unit and
 * 16777217 bytes must fail.  Both bounds were verified by scratch
 * experiments on OpenSSL 3.0.13 and 3.6.5 (identical behavior).  Every
 * update is an independent data unit (the tweak is never advanced between
 * updates), which the 4 x 1 MiB stream below exercises at scale.
 *
 * There is no published KAT at these sizes; the vectors are the IEEE
 * Std 1619-2007 AES-256 key/plaintext patterns and all comparisons are
 * encrypt/decrypt roundtrips plus stream-vs-one-shot consistency, all
 * deterministic.
 */

/* The fixed data-unit number for the large data-unit tests. */
static const char *const LARGE_IV_HEX =
    "01000000000000000000000000000000";

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    xts_test_max_data_unit_roundtrip("AES-256-XTS", xts_key_256_ieee_hex,
                                     LARGE_IV_HEX);
    xts_test_data_unit_too_large("AES-256-XTS", xts_key_256_ieee_hex,
                                 LARGE_IV_HEX);
    xts_test_one_mebibyte_unit("AES-256-XTS", xts_key_256_ieee_hex,
                               LARGE_IV_HEX);
    xts_test_four_mebibyte_stream("AES-256-XTS", xts_key_256_ieee_hex,
                                  LARGE_IV_HEX);

    test_report_pass();
    return 0;
}

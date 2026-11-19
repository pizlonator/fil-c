#include "test_common.h"
#include "test_aes_wrap_common.h"

#include <openssl/evp.h>

/*

 * test_aes_192_wrap -- AES-192-WRAP key wrap (RFC 3394) tests, one binary
 * per cipher (the shared length rules, the default-initial-value note, and
 * the vector provenance live in test_aes_wrap_common.c).

 * This binary carries the RFC 3394 section 4.2 row (192-bit KEK wrapping
 * the shared 128-bit key data), verified through this EVP flow against the
 * authoritative pizfix OpenSSL 3.6.5 and cross-checked against the system
 * OpenSSL 3.0.13, in both the wrap and unwrap directions.

 */

static const struct wrap_kat kat = {
    "RFC3394 4.2", "AES-192-WRAP",
    "000102030405060708090A0B0C0D0E0F1011121314151617",
    "96778B25AE6CA435F92B5B97C050AED2468AB8A17AD84E5D",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    wrap_test_kat(&kat);
    wrap_test_length_rules("AES-192-WRAP");
    wrap_test_64_byte_roundtrip("AES-192-WRAP");
    wrap_test_corrupted_unwrap("AES-192-WRAP");

    test_report_pass();
    return 0;
}

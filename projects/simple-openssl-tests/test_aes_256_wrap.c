#include "test_common.h"
#include "test_aes_wrap_common.h"

#include <openssl/evp.h>

/*

 * test_aes_256_wrap -- AES-256-WRAP key wrap (RFC 3394) tests, one binary
 * per cipher (the shared length rules, the default-initial-value note, and
 * the vector provenance live in test_aes_wrap_common.c).

 * This binary carries the RFC 3394 section 4.3 row (256-bit KEK wrapping
 * the shared 128-bit key data), verified through this EVP flow against the
 * authoritative pizfix OpenSSL 3.6.5 and cross-checked against the system
 * OpenSSL 3.0.13, in both the wrap and unwrap directions.

 */

static const struct wrap_kat kat = {
    "RFC3394 4.3", "AES-256-WRAP",
    "000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F",
    "64E8C3F9CE0F5BA263E9777905818A2A93C8191E7D6E8AE7",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    wrap_test_kat(&kat);
    wrap_test_length_rules("AES-256-WRAP");
    wrap_test_64_byte_roundtrip("AES-256-WRAP");
    wrap_test_corrupted_unwrap("AES-256-WRAP");

    test_report_pass();
    return 0;
}

#include "test_common.h"
#include "test_aes_wrap_common.h"

#include <openssl/evp.h>

/*

 * test_aes_128_wrap -- AES-128-WRAP key wrap (RFC 3394) tests, one binary
 * per cipher (the shared length rules, the default-initial-value note, and
 * the vector provenance live in test_aes_wrap_common.c).

 * This binary carries the RFC 3394 section 4.1 row (128-bit KEK wrapping
 * the shared 128-bit key data), verified through this EVP flow against the
 * authoritative pizfix OpenSSL 3.6.5 and cross-checked against the system
 * OpenSSL 3.0.13, in both the wrap and unwrap directions.

 */

static const struct wrap_kat kat = {
    "RFC3394 4.1", "AES-128-WRAP",
    "000102030405060708090A0B0C0D0E0F",
    "1FA68B0A8112B447AEF34BD8FB5A7B829D3E862371D2CFE5",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    wrap_test_kat(&kat);
    wrap_test_length_rules("AES-128-WRAP");
    wrap_test_64_byte_roundtrip("AES-128-WRAP");
    wrap_test_corrupted_unwrap("AES-128-WRAP");

    test_report_pass();
    return 0;
}

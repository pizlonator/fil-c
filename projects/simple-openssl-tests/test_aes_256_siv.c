#include "test_common.h"
#include "test_aes_siv_common.h"

#include <openssl/evp.h>

/*

 * test_aes_256_siv -- AES-256-SIV (RFC 5297 S2V-CMAC-AES + CTR) tests, one
 * binary per cipher (the shared SIV rules, the S2V size-boundary rationale,
 * the empty-payload version difference, and the vector-derivation
 * provenance live in test_aes_siv_common.c).

 * This binary carries the derived RFC 5297 A.1/A.2-format rows: the RFC
 * publishes samples only for 128-bit keys, so these rows use the same A.1
 * and A.2 inputs with the 000102.. key pattern continued to twice the
 * cipher key size (64 = 2x32 bytes); the expected SIV/ciphertext constants
 * were derived with a scratch probe against the authoritative pizfix
 * OpenSSL 3.6.5 and cross-checked byte for byte against the system OpenSSL
 * 3.0.13.

 */

/* The derived doubled key (the 000102.. pattern continued to 64 bytes). */
static const char KEY256_2X_HEX[] =
    "000102030405060708090A0B0C0D0E0F1011121314151617"
    "18191A1B1C1D1E1F202122232425262728292A2B2C2D2E2F"
    "303132333435363738393A3B3C3D3E3F";

static const struct siv_kat kat_a1 = {
    "RFC 5297 A.1 (derived)", "AES-256-SIV", KEY256_2X_HEX,
    "801aa54859afc2c7a67a2892d0058e3e",
    "4fc606d573f01104a12bf8ab150c",
};

static const struct siv_kat kat_a2 = {
    "RFC 5297 A.2 (derived)", "AES-256-SIV", KEY256_2X_HEX,
    "496225fd6411bfadca06bc6a2b8a01e4",
    "fec0526bdb17e01f8b12ddfc92d678ad"
    "9e25bd2663991f555697b41eda3e3d4f"
    "3c5081642fa480b790d825d11ae926",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    siv_test_kat_a1(&kat_a1);
    siv_test_kat_a2(&kat_a2);
    siv_test_roundtrip_sizes("AES-256-SIV", 64);
    siv_test_empty_payload("AES-256-SIV", 64);
    siv_test_wrong_siv("AES-256-SIV", 64);
    siv_test_second_payload_fails("AES-256-SIV", 64);
    siv_test_wrong_key_length("AES-256-SIV", 64);

    test_report_pass();
    return 0;
}

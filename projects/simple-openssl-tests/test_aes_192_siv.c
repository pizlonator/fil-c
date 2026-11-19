#include "test_common.h"
#include "test_aes_siv_common.h"

#include <openssl/evp.h>

/*

 * test_aes_192_siv -- AES-192-SIV (RFC 5297 S2V-CMAC-AES + CTR) tests, one
 * binary per cipher (the shared SIV rules, the S2V size-boundary rationale,
 * the empty-payload version difference, and the vector-derivation
 * provenance live in test_aes_siv_common.c).

 * This binary carries the derived RFC 5297 A.1/A.2-format rows: the RFC
 * publishes samples only for 128-bit keys, so these rows use the same A.1
 * and A.2 inputs with the 000102.. key pattern continued to twice the
 * cipher key size (48 = 2x24 bytes); the expected SIV/ciphertext constants
 * were derived with a scratch probe against the authoritative pizfix
 * OpenSSL 3.6.5 and cross-checked byte for byte against the system OpenSSL
 * 3.0.13.

 */

/* The derived doubled key (the 000102.. pattern continued to 48 bytes). */
static const char KEY192_2X_HEX[] =
    "000102030405060708090A0B0C0D0E0F1011121314151617"
    "18191A1B1C1D1E1F202122232425262728292A2B2C2D2E2F";

static const struct siv_kat kat_a1 = {
    "RFC 5297 A.1 (derived)", "AES-192-SIV", KEY192_2X_HEX,
    "df2e1ddfc2598382d1acb410c2388078",
    "d23875e91f9a8a650d5a632697f8",
};

static const struct siv_kat kat_a2 = {
    "RFC 5297 A.2 (derived)", "AES-192-SIV", KEY192_2X_HEX,
    "b2577c6d31a8b968d4dd1b9be51b448a",
    "8fff17d3aea04626335e012917c40eb0"
    "bed2270d7f7103aae0db2202f38f5a71"
    "1f4c2d579e6206fd65b183d6396f43",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    siv_test_kat_a1(&kat_a1);
    siv_test_kat_a2(&kat_a2);
    siv_test_roundtrip_sizes("AES-192-SIV", 48);
    siv_test_empty_payload("AES-192-SIV", 48);
    siv_test_wrong_siv("AES-192-SIV", 48);
    siv_test_second_payload_fails("AES-192-SIV", 48);
    siv_test_wrong_key_length("AES-192-SIV", 48);

    test_report_pass();
    return 0;
}

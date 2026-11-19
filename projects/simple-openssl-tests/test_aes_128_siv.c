#include "test_common.h"
#include "test_aes_siv_common.h"

#include <openssl/evp.h>

/*

 * test_aes_128_siv -- AES-128-SIV (RFC 5297 S2V-CMAC-AES + CTR) tests, one
 * binary per cipher (the shared SIV rules, the S2V size-boundary rationale,
 * the empty-payload version difference, and the vector-derivation
 * provenance live in test_aes_siv_common.c).

 * This binary carries the RFC 5297 constants: A.1 (deterministic example,
 * one AD) and A.2 (nonce-based example, two ADs and the nonce as the last
 * AAD update), both verified through this EVP flow against the
 * authoritative pizfix OpenSSL 3.6.5 and cross-checked against the system
 * OpenSSL 3.0.13 (the A.2 vector exercises the multiple-AAD-updates path).

 */

/* RFC 5297 A.1: deterministic example, single AD, 14-byte plaintext
 * (key = 2x16). */
static const struct siv_kat kat_a1 = {
    "RFC 5297 A.1", "AES-128-SIV",
    "fffefdfcfbfaf9f8f7f6f5f4f3f2f1f0"
    "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff",
    "85632d07c6e8f37f950acd320a2ecc93",
    "40c02b9690c4dc04daef7f6afe5c",
};

/* RFC 5297 A.2: nonce-based example, two ADs, nonce as the last AAD. */
static const struct siv_kat kat_a2 = {
    "RFC 5297 A.2", "AES-128-SIV",
    "7f7e7d7c7b7a79787776757473727170"
    "404142434445464748494a4b4c4d4e4f",
    "7bdb6e3b432667eb06f4d14bff2fbd0f",
    "cb900f2fddbe404326601965c889bf17"
    "dba77ceb094fa663b7a3f748ba8af829"
    "ea64ad544a272e9c485b62a3fd5c0d",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    siv_test_kat_a1(&kat_a1);
    siv_test_kat_a2(&kat_a2);
    siv_test_roundtrip_sizes("AES-128-SIV", 32);
    siv_test_empty_payload("AES-128-SIV", 32);
    siv_test_wrong_siv("AES-128-SIV", 32);
    siv_test_second_payload_fails("AES-128-SIV", 32);
    siv_test_wrong_key_length("AES-128-SIV", 32);

    test_report_pass();
    return 0;
}

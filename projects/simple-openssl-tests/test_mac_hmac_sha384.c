#include "test_common.h"

#include "test_mac_hmac_common.h"

#include <openssl/evp.h>

/*
 * test_mac_hmac_sha384 -- EVP_MAC "HMAC" over SHA-384 known-answer and
 * behavior tests, one binary per digest (the shared HMAC machinery, the
 * two-way vector verification provenance, and the per-section distribution
 * live in test_mac_hmac_common.c).
 *
 * Known-answer provenance (SHA-384): this digest's rows of the RFC 4231
 * table -- test cases 1-4, 6 and 7 (test case 5 is a truncation case,
 * which EVP cannot express and is therefore omitted).
 *
 * Coverage for this digest: EVP_MAC_CTX_get_mac_size == 48 (the digest
 * size, probed after a plain init); the six RFC 4231 KATs, one-shot and
 * chunked ({1, 17} updates); the key-size section (the empty key == the
 * 32-zero-byte and 64-zero-byte keys; no published empty-key vector, so
 * the equivalence anchors on the empty-key result); and a nonexistent
 * digest name rejected cleanly.
 */

/* The digest under test and its digest size. */
static const struct hmac_family fam = { "SHA384", 48 };

/* This digest's rows of the RFC 4231 table (the test-case numbers are
 * preserved, for the failure messages). */
static const struct hmac_kat rfc4231_kats[] = {
    { 1, "afd03944d84895626b0825f4ab46907f15f9dadbe4101ec682aa034c7cebc59cfaea9ea9076ede7f4af152e8b2fa9cb6" },
    { 2, "af45d2e376484031617f78d2b58a6b1b9c7ef464f5a01b47e42ec3736322445e8e2240ca5e69e2c78b3239ecfab21649" },
    { 3, "88062608d3e6ad8a0aa2ace014c8a86f0aa635d947ac9febe83ef4e55966144b2a5ab39dc13814b94e3ab6e101a34f27" },
    { 4, "3e8a69b7783c25851933ab6290af6ca77a9981480850009cc5577c6e1f573b4e6801dd23c4a7d679ccf8a386c674cffb" },
    { 6, "4ece084485813e9088d2c63a041bc5b44f9ef1012a2b588f3cd11f05033ac4c60c2ef6ab4030fe8296248df163f44952" },
    { 7, "6617178e941f020d351e2f254e8fd32c602420feb0b8fb9adccebb82461e99c5a678cc31e799176d3860e6110c46523e" },
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    hmac_test_mac_size(&fam);
    hmac_test_rfc4231(&fam, rfc4231_kats,
                      sizeof(rfc4231_kats) / sizeof(rfc4231_kats[0]));
    hmac_test_key_sizes(&fam, NULL);
    hmac_test_wrong_digest();

    test_report_pass();
    return 0;
}

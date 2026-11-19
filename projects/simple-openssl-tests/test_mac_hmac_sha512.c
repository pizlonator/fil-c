#include "test_common.h"

#include "test_mac_hmac_common.h"

#include <openssl/evp.h>

/*
 * test_mac_hmac_sha512 -- EVP_MAC "HMAC" over SHA-512 known-answer and
 * behavior tests, one binary per digest (the shared HMAC machinery, the
 * two-way vector verification provenance, and the per-section distribution
 * live in test_mac_hmac_common.c).
 *
 * Known-answer provenance (SHA-512): this digest's rows of the RFC 4231
 * table -- test cases 1-4, 6 and 7 (test case 5 is a truncation case,
 * which EVP cannot express and is therefore omitted).
 *
 * Coverage for this digest: EVP_MAC_CTX_get_mac_size == 64 (the digest
 * size, probed after a plain init); the six RFC 4231 KATs, one-shot and
 * chunked ({1, 17} updates); the key-size section (the empty key == the
 * 32-zero-byte and 64-zero-byte keys; no published empty-key vector, so
 * the equivalence anchors on the empty-key result); and a nonexistent
 * digest name rejected cleanly.
 */

/* The digest under test and its digest size. */
static const struct hmac_family fam = { "SHA512", 64 };

/* This digest's rows of the RFC 4231 table (the test-case numbers are
 * preserved, for the failure messages). */
static const struct hmac_kat rfc4231_kats[] = {
    { 1, "87aa7cdea5ef619d4ff0b4241a1d6cb02379f4e2ce4ec2787ad0b30545e17cdedaa833b7d6b8a702038b274eaea3f4e4be9d914eeb61f1702e696c203a126854" },
    { 2, "164b7a7bfcf819e2e395fbe73b56e0a387bd64222e831fd610270cd7ea2505549758bf75c05a994a6d034f65f8f0e6fdcaeab1a34d4a6b4b636e070a38bce737" },
    { 3, "fa73b0089d56a284efb0f0756c890be9b1b5dbdd8ee81a3655f83e33b2279d39bf3e848279a722c806b485a47e67c807b946a337bee8942674278859e13292fb" },
    { 4, "b0ba465637458c6990e5a8c5f61d4af7e576d97ff94b872de76f8050361ee3dba91ca5c11aa25eb4d679275cc5788063a5f19741120c4f2de2adebeb10a298dd" },
    { 6, "80b24263c7c1a3ebb71493c1dd7be8b49b46d1f41b4aeec1121b013783f8f3526b56d037e05f2598bd0fd2215d6a1e5295e64f73f63f0aec8b915a985d786598" },
    { 7, "e37b6a775dc87dbaa4dfa9f96e5e3ffddebd71f8867289865df5a32d20cdc944b6022cac3c4982b10d5eeb55c3e4de15134676fb6de0446065c97440fa8c6a58" },
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

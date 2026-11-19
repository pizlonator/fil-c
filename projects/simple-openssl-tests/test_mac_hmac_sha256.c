#include "test_common.h"

#include "test_mac_hmac_common.h"

#include <openssl/evp.h>

/*
 * test_mac_hmac_sha256 -- EVP_MAC "HMAC" over SHA-256 known-answer and
 * behavior tests, one binary per digest (the shared HMAC machinery, the
 * two-way vector verification provenance, and the per-section distribution
 * live in test_mac_hmac_common.c).
 *
 * Known-answer provenance (SHA-256): this digest's rows of the RFC 4231
 * table -- test cases 1-4, 6 and 7 (test case 5 is a truncation case,
 * which EVP cannot express and is therefore omitted) -- plus the empty-key
 * KAT of the key-size section.
 *
 * Coverage for this digest: EVP_MAC_CTX_get_mac_size == 32 (the digest
 * size, probed after a plain init); the six RFC 4231 KATs, one-shot and
 * chunked ({1, 17} updates); the key-size section (the empty key == the
 * 32-zero-byte and 64-zero-byte keys, anchored to the empty-key KAT); and
 * a nonexistent digest name rejected cleanly.
 */

/* The digest under test and its digest size. */
static const struct hmac_family fam = { "SHA256", 32 };

/* This digest's rows of the RFC 4231 table (the test-case numbers are
 * preserved, for the failure messages). */
static const struct hmac_kat rfc4231_kats[] = {
    { 1, "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7" },
    { 2, "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843" },
    { 3, "773ea91e36800e46854db8ebd09181a72959098b3ef8c122d9635514ced565fe" },
    { 4, "82558a389a443c0ea4cc819899f2083a85f0faa3e578f8077a2e3ff46729665b" },
    { 6, "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54" },
    { 7, "9b09ffa71b942fcb27635fbcd5b0e944bfdc63644f0713938a7f51535c3a35e2" },
};

/* The empty-key KAT the monolith verified for the key-size section. */
static const char EMPTY_KEY_KAT_HEX[] =
    "b613679a0814d9ec772f95d778c35fc5ff1697c493715653c6c712144292c5ad";

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    hmac_test_mac_size(&fam);
    hmac_test_rfc4231(&fam, rfc4231_kats,
                      sizeof(rfc4231_kats) / sizeof(rfc4231_kats[0]));
    hmac_test_key_sizes(&fam, EMPTY_KEY_KAT_HEX);
    hmac_test_wrong_digest();

    test_report_pass();
    return 0;
}

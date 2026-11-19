#include "test_common.h"

#include "test_mac_hmac_common.h"

#include <openssl/evp.h>

/*
 * test_mac_hmac_sha1 -- EVP_MAC "HMAC" over SHA-1 known-answer and
 * behavior tests, one binary per digest (the shared HMAC machinery, the
 * two-way vector verification provenance, and the per-section distribution
 * live in test_mac_hmac_common.c).
 *
 * Known-answer provenance (SHA-1): this digest's rows of the RFC 2202
 * table -- test cases 1-4, 6 and 7, the 20-byte-key rows (TC6/TC7 use
 * 80-byte keys, larger than the SHA-1 block size, and TC7 uses data longer
 * than one block).
 *
 * Coverage for this digest: EVP_MAC_CTX_get_mac_size == 20 (the digest
 * size, probed after a plain init); the six RFC 2202 KATs, one-shot and
 * chunked ({1, 17} updates); the key-size section (the empty key == the
 * 32-zero-byte and 64-zero-byte keys; no published empty-key vector, so
 * the equivalence anchors on the empty-key result); and a nonexistent
 * digest name rejected cleanly.
 */

/* The digest under test and its digest size. */
static const struct hmac_family fam = { "SHA1", 20 };

/* This digest's rows of the RFC 2202 table (the test-case numbers are
 * preserved, for the failure messages).  TC1/TC3/TC4 keys are 20 bytes. */
static const struct hmac_kat rfc2202_kats[] = {
    { 1, "b617318655057264e28bc0b6fb378c8ef146be00" },
    { 2, "effcdf6ae5eb2fa2d27416d5f184df9c259a7c79" },
    { 3, "125d7342b9ac11cd91a39af48aa17b4f63f175d3" },
    { 4, "fcce74c708f32de54cc7e91045628f9575dde8cf" },
    { 6, "aa4ae5e15272d00e95705637ce8a3b55ed402112" },
    { 7, "ee8089bab17872f013f572dba60ef38e32573f15" },
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    hmac_test_mac_size(&fam);
    hmac_test_rfc2202(&fam, rfc2202_kats,
                      sizeof(rfc2202_kats) / sizeof(rfc2202_kats[0]), 20);
    hmac_test_key_sizes(&fam, NULL);
    hmac_test_wrong_digest();

    test_report_pass();
    return 0;
}

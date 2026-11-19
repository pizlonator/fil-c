#include "test_common.h"

#include "test_mac_hmac_common.h"

#include <openssl/evp.h>

/*
 * test_mac_hmac_md5 -- EVP_MAC "HMAC" over MD5 known-answer and behavior
 * tests, one binary per digest (the shared HMAC machinery, the two-way
 * vector verification provenance, and the per-section distribution live in
 * test_mac_hmac_common.c).
 *
 * Known-answer provenance (MD5): this digest's rows of the RFC 2202 table
 * -- test cases 1-4, 6 and 7, the 16-byte-key rows (TC6/TC7 use 80-byte
 * keys, larger than the MD5 block size, and TC7 uses data longer than one
 * block).
 *
 * Coverage for this digest: EVP_MAC_CTX_get_mac_size == 16 (the digest
 * size, probed after a plain init); the six RFC 2202 KATs, one-shot and
 * chunked ({1, 17} updates); the key-size section (the empty key == the
 * 32-zero-byte and 64-zero-byte keys; no published empty-key vector, so
 * the equivalence anchors on the empty-key result); and a nonexistent
 * digest name rejected cleanly.
 */

/* The digest under test and its digest size. */
static const struct hmac_family fam = { "MD5", 16 };

/* This digest's rows of the RFC 2202 table (the test-case numbers are
 * preserved, for the failure messages).  TC1/TC3/TC4 keys are 16 bytes. */
static const struct hmac_kat rfc2202_kats[] = {
    { 1, "9294727a3638bb1c13f48ef8158bfc9d" },
    { 2, "750c783e6ab0b503eaa86e310a5db738" },
    { 3, "56be34521d144c88dbb8c733f0e8b3f6" },
    { 4, "997cf5e360d235a8d1f38022d1d8aa29" },
    { 6, "6b1ab7fe4bd7bf8f0b62e6ce61b9d0cd" },
    { 7, "038bcf9d93e2454bcc86a53c20b9e329" },
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    hmac_test_mac_size(&fam);
    hmac_test_rfc2202(&fam, rfc2202_kats,
                      sizeof(rfc2202_kats) / sizeof(rfc2202_kats[0]), 16);
    hmac_test_key_sizes(&fam, NULL);
    hmac_test_wrong_digest();

    test_report_pass();
    return 0;
}

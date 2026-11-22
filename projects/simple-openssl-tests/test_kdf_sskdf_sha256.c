#include "test_common.h"

#include "test_kdf_sskdf_common.h"

#include <openssl/evp.h>

/*

 * test_kdf_sskdf_sha256 -- the Single Step KDF (NIST SP 800-56C) through the
 * EVP_KDF public API for the SHA-256 digest, one binary per digest (the
 * shared SSKDF machinery, the NIST CAVS vector provenance, the CLI
 * verification commands and the CMAC-mode note live in
 * test_kdf_sskdf_common.c).
 *
 * Known-answer provenance (SHA-256): the rows below are the original
 * monolith's SHA-256 rows of the evpkdf_ss.txt table -- the 32-byte and the
 * 64-byte digest-mode outputs over the shared z/info (the 64-byte value is
 * the 32-byte one extended) and the HMAC-SHA256 salt-mode row -- every
 * value re-verified with
 *
 *   pizfix/bin/openssl kdf -keylen 32 -kdfopt digest:SHA256 \
 *       -kdfopt hexsecret:<z> -kdfopt hexinfo:<info> SSKDF
 *
 * and the same command under the system OpenSSL 3.0.13.
 *
 * Its multi-info section compares the five-chunk derive against the
 * verified 32-byte constant; the SHA-1 binary checks the same behavior
 * relatively, against a single-info reference derive.
 */

/* The verified 32-byte output of the 48-byte 000102...2f info under
 * SHA-256 (the multi-chunk info section's KAT; CLI-verified on the pizfix
 * 3.6.5 and the system 3.0.13 builds). */
static const char INFO48_OKM_HEX[] =
    "fb9ee554726e387ca4174275116db0b8e0266d3c23d37bff"
    "7f0b3f0a82f8599b";

/* This digest's HMAC-mode KAT material (only the SHA-256 table carries an
 * HMAC row): z, salt and info. */
static const char HMAC_Z_HEX[] = "6ee6c00d70a6cd14bd5a4e8fcfec8386";
static const char HMAC_SALT_HEX[] = "532f5131e0a2fecc722f87e5aa2062cb";
static const char HMAC_INFO_HEX[] = "861aa2886798231259bd0314";

/* This digest's family row: the OSSL_KDF_PARAM_DIGEST value. */
static const struct sskdf_family fam = { "SHA256" };

/* This digest's KAT rows: the 32- and 64-byte digest-mode outputs over the
 * shared CAVS z/info (from the common module), and the HMAC salt-mode row
 * over its own material above. */
static const struct ss_kat kats[] = {
    { NULL, sskdf_z16_hex, NULL, sskdf_info12_hex,
      32,
      "367186ce2e9f92a944ac1a0393f3a427b055fdab0fceda71a4765c8960823285" },
    { NULL, sskdf_z16_hex, NULL, sskdf_info12_hex,
      64,
      "367186ce2e9f92a944ac1a0393f3a427b055fdab0fceda71a4765c8960823285"
      "9ac2ae9af5330282d7e0e85107a3f1feaf0ff04ed27b4a38d6a8772a7ad71a59" },
    { "HMAC", HMAC_Z_HEX, HMAC_SALT_HEX, HMAC_INFO_HEX,
      16,
      "13479e9a91dd20fdd757d68ffe8869fb" },
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    sskdf_test_kats(&fam, kats, sizeof(kats) / sizeof(kats[0]));
    sskdf_test_key_secret_alias(&fam, &kats[0]);
    /* The verified 32-byte multi-info OKM (see INFO48_OKM_HEX above). */
    sskdf_test_multi_info(&fam, INFO48_OKM_HEX);
    sskdf_test_missing_key(&fam, &kats[0]);

    test_report_pass();
    return 0;
}
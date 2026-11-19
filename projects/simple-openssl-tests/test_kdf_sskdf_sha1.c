#include "test_common.h"

#include "test_kdf_sskdf_common.h"

#include <openssl/evp.h>

/*

 * test_kdf_sskdf_sha1 -- the Single Step KDF (NIST SP 800-56C) through the
 * EVP_KDF public API for the SHA-1 digest, one binary per digest (the
 * shared SSKDF machinery, the NIST CAVS vector provenance, the CLI
 * verification commands and the CMAC-mode note live in
 * test_kdf_sskdf_common.c).
 *
 * Known-answer provenance (SHA-1): the row below is the original monolith's
 * SHA-1 row of the evpkdf_ss.txt table, re-verified with
 *
 *   pizfix/bin/openssl kdf -keylen 16 -kdfopt digest:SHA1 \
 *       -kdfopt hexsecret:<z> -kdfopt hexinfo:<info> SSKDF
 *
 * and the same command under the system OpenSSL 3.0.13:
 *
 *   SHA1, z=d09a6b1a472f930db4f5e6b967900744,
 *   info=b117255ab5f1b6b96fc434b0 -> b5a3c52e97ae6e8c5069954354eab3c7
 *
 * The key/secret alias and the missing-key negative run over this row's
 * z/info; the multi-chunk info section has no verified SHA-1 OKM constant
 * (the monolith verified only the SHA-256 value), so it checks the
 * concatenation behavior relatively, against a single-info reference
 * derive.
 */

/* This digest's family row: the OSSL_KDF_PARAM_DIGEST value. */
static const struct sskdf_family fam = { "SHA1" };

/* This digest's digest-mode KAT row (the z and info come from the shared
 * CAVS material in the common module). */
static const struct ss_kat kats[] = {
    { NULL, sskdf_z16_hex, NULL, sskdf_info12_hex,
      16,
      "b5a3c52e97ae6e8c5069954354eab3c7" },
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    sskdf_test_kats(&fam, kats, sizeof(kats) / sizeof(kats[0]));
    sskdf_test_key_secret_alias(&fam, &kats[0]);
    /* No verified SHA-1 multi-info OKM: checked against a single-info
     * reference derive. */
    sskdf_test_multi_info(&fam, NULL);
    sskdf_test_missing_key(&fam, &kats[0]);

    test_report_pass();
    return 0;
}
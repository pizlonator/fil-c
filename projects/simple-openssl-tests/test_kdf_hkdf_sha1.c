#include "test_common.h"

#include "test_kdf_hkdf_common.h"

#include <openssl/evp.h>

/*
 * test_kdf_hkdf_sha1 -- HKDF (RFC 5869) through the EVP_KDF public API for
 * the SHA-1 digest, one binary per digest (the shared HKDF machinery, the
 * RFC 5869 vector provenance, the CLI verification commands and the
 * concrete-name version gate live in test_kdf_hkdf_common.c).
 *
 * Known-answer provenance (SHA-1): the TC1 OKM below is the original
 * monolith's "TC1 inputs with the SHA-1 and SHA-512 digests" row,
 * CLI-verified on both the pizfix 3.6.5 and the system 3.0.13 builds.
 *
 * Parameterization notes for this digest: this binary's EXTRACT_ONLY row
 * checks the mode, the PRK size and the derive success without a value
 * comparison, and its EXPAND_ONLY check derives the PRK with EXTRACT_ONLY
 * first; there is no verified TC2 OKM, so the multi-info check compares
 * the five-chunk derive against a single-info reference derive.  There is
 * no concrete HKDF-SHA1 name to
 * check (the 3.5+ concrete names are SHA-256/384/512 only), so this binary
 * runs no concrete-name section.
 */

/* This digest's family row: the bare-HKDF digest param, its prose name for
 * the failure messages, and the digest output size (the EXTRACT_ONLY PRK
 * size). */
static const struct hkdf_family fam = { "SHA1", "SHA-1", 20 };

/* This digest's RFC 5869 KAT rows over the shared TC1 inputs. */
static const struct hkdf_kat kats[] = {
    /* TC1 */
    { 1, hkdf_tc1_ikm_hex, hkdf_tc1_salt_hex, hkdf_tc1_info_hex, 42,
      "d6000ffb5b50bd3970b260017798fb9c8df9ce2e2c16b6cd709cca07dc3cf9cf"
      "26d6c6d750d0aaf5ac94" },
};

/* This digest's EXTRACT_ONLY rows: no verified SHA-1 PRK constant exists in
 * the original monolith, so prkhex is NULL and the row checks the mode, the
 * PRK size and the derive success. */
static const struct hkdf_prk_kat prks[] = {
    /* TC1 */
    { 1, hkdf_tc1_ikm_hex, hkdf_tc1_salt_hex, NULL },
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    hkdf_test_extract_and_expand(&fam, kats, sizeof(kats) / sizeof(kats[0]));
    hkdf_test_extract_only(&fam, prks, sizeof(prks) / sizeof(prks[0]));
    /* No verified SHA-1 PRK constant: the PRK is extracted first. */
    hkdf_test_expand_only(&fam, &kats[0], NULL);
    /* No verified TC2 OKM for SHA-1: checked against a single-info
     * reference derive. */
    hkdf_test_multi_info(&fam, NULL);
    hkdf_test_negative(&fam);

    test_report_pass();
    return 0;
}

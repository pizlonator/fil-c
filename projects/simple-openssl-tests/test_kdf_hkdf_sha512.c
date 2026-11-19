#include "test_common.h"

#include "test_kdf_hkdf_common.h"

#include <openssl/evp.h>

/*
 * test_kdf_hkdf_sha512 -- HKDF (RFC 5869) through the EVP_KDF public API for
 * the SHA-512 digest, one binary per digest (the shared HKDF machinery, the
 * RFC 5869 vector provenance, the CLI verification commands and the
 * concrete-name version gate live in test_kdf_hkdf_common.c).
 *
 * Known-answer provenance (SHA-512): the TC1 OKM below is the original
 * monolith's "TC1 inputs with the SHA-1 and SHA-512 digests" row,
 * CLI-verified on both the pizfix 3.6.5 and the system 3.0.13 builds.
 *
 * Parameterization notes for this digest: the original monolith verified
 * the TC1 PRK for SHA-512 as the concrete HKDF-SHA512 EXTRACT_ONLY PRK
 * (both routes resolve to the same HKDF provider code, so the bare-HKDF
 * route must agree; re-verified with the CLI on the 3.0.13 and 3.6.5 builds
 * when the family was split), and this binary uses it for its EXTRACT_ONLY
 * row and its EXPAND_ONLY key.  No verified TC2 OKM exists for SHA-512, so
 * the multi-info check compares the five-chunk derive against a single-info
 * reference derive.
 */

/* This digest's family row: the bare-HKDF digest param, its prose name for
 * the failure messages, and the digest output size (the EXTRACT_ONLY PRK
 * size). */
static const struct hkdf_family fam = { "SHA512", "SHA-512", 64 };

/* PRK of RFC 5869 TC1 for SHA-512 (HMAC-SHA512 of the TC1 salt and IKM):
 * the value the original monolith verified as the HKDF-SHA512 EXTRACT_ONLY
 * PRK (see the concrete row below), used for the bare-HKDF route too. */
static const char TC1_SHA512_PRK_HEX[] =
    "665799823737ded04a88e47e54a5890bb2c3d247c7a4254a8e61350723590a26"
    "c36238127d8661b88cf80ef802d57e2f7cebcf1e00e083848be19929c61b4237";

/* This digest's RFC 5869 KAT rows over the shared TC1 inputs. */
static const struct hkdf_kat kats[] = {
    /* TC1 */
    { 1, hkdf_tc1_ikm_hex, hkdf_tc1_salt_hex, hkdf_tc1_info_hex, 42,
      "832390086cda71fb47625bb5ceb168e4c8e26a1a16ed34d9fc7fe92c14815793"
      "38da362cb8d9f925d7cb" },
};

/* This digest's EXTRACT_ONLY rows (PRK = HMAC-SHA512 of each row's salt and
 * IKM). */
static const struct hkdf_prk_kat prks[] = {
    /* TC1 */
    { 1, hkdf_tc1_ikm_hex, hkdf_tc1_salt_hex, TC1_SHA512_PRK_HEX },
};

/* This digest's concrete-name row. */
static const struct hkdf_concrete concretes[] = {
    { "HKDF-SHA512", "SHA2-512", 64,
      "832390086cda71fb47625bb5ceb168e4c8e26a1a16ed34d9fc7fe92c14815793"
      "38da362cb8d9f925d7cb",
      "665799823737ded04a88e47e54a5890bb2c3d247c7a4254a8e61350723590a26"
      "c36238127d8661b88cf80ef802d57e2f7cebcf1e00e083848be19929c61b4237" },
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    hkdf_test_extract_and_expand(&fam, kats, sizeof(kats) / sizeof(kats[0]));
    hkdf_test_extract_only(&fam, prks, sizeof(prks) / sizeof(prks[0]));
    /* TC1's PRK, verified by the EXTRACT_ONLY section above. */
    hkdf_test_expand_only(&fam, &kats[0], TC1_SHA512_PRK_HEX);
    /* No verified TC2 OKM for SHA-512: checked against a single-info
     * reference derive. */
    hkdf_test_multi_info(&fam, NULL);
    hkdf_test_negative(&fam);
    hkdf_test_concrete_names(concretes,
                             sizeof(concretes) / sizeof(concretes[0]));

    test_report_pass();
    return 0;
}

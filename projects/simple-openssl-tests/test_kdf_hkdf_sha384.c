#include "test_common.h"

#include "test_kdf_hkdf_common.h"

#include <openssl/evp.h>

/*
 * test_kdf_hkdf_sha384 -- HKDF (RFC 5869) through the EVP_KDF public API for
 * the SHA-384 digest, one binary per digest (the shared HKDF machinery, the
 * RFC 5869 vector provenance, the CLI verification commands and the
 * concrete-name version gate live in test_kdf_hkdf_common.c).
 *
 * Known-answer provenance (SHA-384): RFC 5869 publishes vectors for the
 * SHA-1 and SHA-256 digests only, and the original monolith carried no
 * "TC1 inputs with the SHA-384 digest" row either; the TC1 OKM and PRK
 * below are the values of the HKDF-SHA384 concrete-name row (which lived
 * in test_kdf_hkdf_sha256.c while SHA-384 had no per-digest binary, and
 * moved here when this binary was added), CLI-verified on both the pizfix
 * 3.6.5 and the system 3.0.13 builds through both routes -- the concrete
 * name itself and the bare-HKDF form with -kdfopt digest:SHA2-384 (both
 * routes resolve to the same HKDF provider code, so the bare-HKDF route
 * must agree; see test_kdf_hkdf_common.c for the commands).
 *
 * Parameterization notes for this digest: no verified TC2 OKM exists for
 * SHA-384, so the multi-info check compares the five-chunk derive against
 * a single-info reference derive.
 */

/* This digest's family row: the bare-HKDF digest param, its prose name for
 * the failure messages, and the digest output size (the EXTRACT_ONLY PRK
 * size). */
static const struct hkdf_family fam = { "SHA384", "SHA-384", 48 };

/* PRK of RFC 5869 TC1 for SHA-384 (HMAC-SHA384 of the TC1 salt and IKM):
 * the value verified as the HKDF-SHA384 EXTRACT_ONLY PRK (see the concrete
 * row below; both routes resolve to the same HKDF provider code, so the
 * bare-HKDF route must agree), used for the bare-HKDF route too. */
static const char TC1_SHA384_PRK_HEX[] =
    "704b39990779ce1dc548052c7dc39f303570dd13fb39f7acc564680bef80e8de"
    "c70ee9a7e1f3e293ef68eceb072a5ade";

/* This digest's RFC 5869 KAT rows over the shared TC1 inputs. */
static const struct hkdf_kat kats[] = {
    /* TC1 */
    { 1, hkdf_tc1_ikm_hex, hkdf_tc1_salt_hex, hkdf_tc1_info_hex, 42,
      "9b5097a86038b805309076a44b3a9f38063e25b516dcbf369f394cfab43685f7"
      "48b6457763e4f0204fc5" },
};

/* This digest's EXTRACT_ONLY rows (PRK = HMAC-SHA384 of each row's salt and
 * IKM). */
static const struct hkdf_prk_kat prks[] = {
    /* TC1 */
    { 1, hkdf_tc1_ikm_hex, hkdf_tc1_salt_hex, TC1_SHA384_PRK_HEX },
};

/* This digest's concrete-name row, moved verbatim from
 * test_kdf_hkdf_sha256.c (which carried it while SHA-384 had no per-digest
 * binary of its own). */
static const struct hkdf_concrete concretes[] = {
    { "HKDF-SHA384", "SHA2-384", 48,
      "9b5097a86038b805309076a44b3a9f38063e25b516dcbf369f394cfab43685f7"
      "48b6457763e4f0204fc5",
      "704b39990779ce1dc548052c7dc39f303570dd13fb39f7acc564680bef80e8de"
      "c70ee9a7e1f3e293ef68eceb072a5ade" },
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    hkdf_test_extract_and_expand(&fam, kats, sizeof(kats) / sizeof(kats[0]));
    hkdf_test_extract_only(&fam, prks, sizeof(prks) / sizeof(prks[0]));
    /* TC1's PRK, verified by the EXTRACT_ONLY section above. */
    hkdf_test_expand_only(&fam, &kats[0], TC1_SHA384_PRK_HEX);
    /* No verified TC2 OKM for SHA-384: checked against a single-info
     * reference derive. */
    hkdf_test_multi_info(&fam, NULL);
    hkdf_test_negative(&fam);
    hkdf_test_concrete_names(concretes,
                             sizeof(concretes) / sizeof(concretes[0]));

    test_report_pass();
    return 0;
}

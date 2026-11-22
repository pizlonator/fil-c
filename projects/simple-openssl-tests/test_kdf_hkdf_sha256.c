#include "test_common.h"

#include "test_kdf_hkdf_common.h"

#include <openssl/evp.h>

/*
 * test_kdf_hkdf_sha256 -- HKDF (RFC 5869) through the EVP_KDF public API for
 * the SHA-256 digest, one binary per digest (the shared HKDF machinery, the
 * RFC 5869 vector provenance, the CLI verification commands and the
 * concrete-name version gate live in test_kdf_hkdf_common.c).
 *
 * Known-answer provenance (SHA-256): the RFC 5869 test cases 1-3 below are
 * the original monolith's first three rows, every value verified against
 * the authoritative pizfix OpenSSL 3.6.5 and the system OpenSSL 3.0.13
 * (see test_kdf_hkdf_common.c for the commands); the extract outputs were
 * additionally cross-checked against `openssl dgst -sha256 -mac hmac`
 * chains and OpenSSL's own test data
 * (test/recipes/30-test_evp_data/evpkdf_hkdf.txt).
 *
 * This binary is the family's representative digest.  It carries only its
 * own digest's rows.
 */

/* This digest's family row: the bare-HKDF digest param, its prose name for
 * the failure messages, and the digest output size (the EXTRACT_ONLY PRK
 * size). */
static const struct hkdf_family fam = { "SHA256", "SHA-256", 32 };

/* PRK of RFC 5869 TC1 (HMAC-SHA256 of the TC1 salt and IKM) -- the key the
 * expand stage below runs with. */
static const char PRK1_HEX[] =
    "077709362c2e32df0ddc3f0dc47bba6390b6c73bb50f9c3122ec844ad7c2b3e5";

/* This digest's RFC 5869 KAT rows over the shared TC1/TC2 inputs. */
static const struct hkdf_kat kats[] = {
    /* TC1 */
    { 1, hkdf_tc1_ikm_hex, hkdf_tc1_salt_hex, hkdf_tc1_info_hex, 42,
      "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf"
      "34007208d5b887185865" },
    /* TC2: 80-byte IKM (00..4f) and salt (60..af), 80-byte info (b0..ff) */
    { 2, hkdf_tc2_ikm_hex, hkdf_tc2_salt_hex, hkdf_tc2_info_hex, 82,
      "b11e398dc80327a1c8e7f78c596a49344f012eda2d4efad8a050cc4c19afa97c"
      "59045a99cac7827271cb41c65e590e09da3275600c2f09b8367793a9aca3db71"
      "cc30c58179ec3e87c14c01d5c1f3434f1d87" },
    /* TC3: empty salt and empty info */
    { 3, hkdf_tc1_ikm_hex, "", "", 42,
      "8da4e775a563c18f715f802a063c5a31b8a11f5c5ee1879ec3454e5f3c738d2d"
      "9d201395faa4b61a96c8" },
};

/* This digest's EXTRACT_ONLY rows (PRK = HMAC-SHA256 of each row's salt and
 * IKM). */
static const struct hkdf_prk_kat prks[] = {
    /* TC1: PRK = HMAC-SHA256(salt=000102..0c, IKM=0b x22) */
    { 1, hkdf_tc1_ikm_hex, hkdf_tc1_salt_hex,
      "077709362c2e32df0ddc3f0dc47bba6390b6c73bb50f9c3122ec844ad7c2b3e5" },
    /* TC2: PRK = HMAC-SHA256(salt=60..af, IKM=00..4f) */
    { 2, hkdf_tc2_ikm_hex, hkdf_tc2_salt_hex,
      "06a6b88c5853361a06104c9ceb35b45cef760014904671014a193f40c15fc244" },
    /* TC3: PRK = HMAC-SHA256(salt="", IKM=0b x22) */
    { 3, hkdf_tc1_ikm_hex, "",
      "19ef24a32c717b167f33a91d6f648bdf96596776afdb6377ac434c1c293ccb04" },
};

/* This digest's concrete-name row. */
static const struct hkdf_concrete concretes[] = {
    { "HKDF-SHA256", "SHA2-256", 32,
      /* The RFC 5869 TC1 OKM, as for the bare-HKDF route above. */
      "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf"
      "34007208d5b887185865",
      "077709362c2e32df0ddc3f0dc47bba6390b6c73bb50f9c3122ec844ad7c2b3e5" },
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    hkdf_test_extract_and_expand(&fam, kats, sizeof(kats) / sizeof(kats[0]));
    hkdf_test_extract_only(&fam, prks, sizeof(prks) / sizeof(prks[0]));
    /* TC1's PRK, verified by the EXTRACT_ONLY section above. */
    hkdf_test_expand_only(&fam, &kats[0], PRK1_HEX);
    /* TC2's OKM, verified by the EXTRACT_AND_EXPAND section above. */
    hkdf_test_multi_info(&fam, kats[1].okmhex);
    hkdf_test_negative(&fam);
    hkdf_test_concrete_names(concretes,
                             sizeof(concretes) / sizeof(concretes[0]));

    test_report_pass();
    return 0;
}

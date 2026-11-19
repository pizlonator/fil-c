#include "test_common.h"

#include "test_kdf_pbkdf2_common.h"

#include <openssl/evp.h>

/*

 * test_kdf_pbkdf2_sha1 -- PBKDF2-HMAC-SHA1 through the EVP_KDF public API,
 * one binary per digest (the shared PBKDF2 machinery, the RFC 6070 vector
 * provenance, the CLI verification commands and the not-run c=16777216 note
 * live in test_kdf_pbkdf2_common.c).
 *
 * Known-answer provenance (SHA-1): the four rows below are the original
 * monolith's RFC 6070 rows (P="password"/S="salt" at c=1, 2 and 4096, and
 * the 24-byte-password/36-byte-salt c=4096 dkLen=25 vector), every value
 * verified against the authoritative pizfix OpenSSL 3.6.5 and the system
 * OpenSSL 3.0.13 with
 *
 *   pizfix/bin/openssl kdf -keylen <L> -kdfopt digest:sha1 \
 *       -kdfopt hexpass:<pass> -kdfopt hexsalt:<salt> -kdfopt iter:<c> PBKDF2
 *
 * The SP 800-132 pkcs5 bounds run for this digest too: it has no verified
 * pkcs5=0 value (only the SHA-256 25eb86ac... was CLI-verified), so the
 * compliant pkcs5=0 derive is acceptance-checked, and the pkcs5=1 c=1
 * derive is compared against the first 16 bytes of this digest's own RFC
 * 6070 c=1 KAT below (the prefix construction the monolith used for
 * SHA-256; pkcs5 only toggles the SP 800-132 checks, never the
 * derivation).
 */

/* This digest's family row: the OSSL_KDF_PARAM_DIGEST value. */
static const struct pbkdf2_family fam = { "sha1" };

/* This digest's RFC 6070 KAT rows (the passwords and salts come from the
 * shared RFC 6070 material in the common module). */
static const struct pbkdf2_kat kats[] = {
    { pbkdf2_pass_hex,
      pbkdf2_salt_hex,
      1, 20,
      "0c60c80f961f0e71f3a9b524af6012062fe037a6" },
    { pbkdf2_pass_hex,
      pbkdf2_salt_hex,
      2, 20,
      "ea6c014dc72d6f8ccd1ed92ace1d41f0d8de8957" },
    { pbkdf2_pass_hex,
      pbkdf2_salt_hex,
      4096, 20,
      "4b007901b765489abead49d926f721d065a429c1" },
    /* P="passwordPASSWORDpassword" (24 bytes),
     * S="saltSALTsaltSALTsaltSALTsaltSALTsalt" (36 bytes), c=4096,
     * dkLen=25. */
    { pbkdf2_long_pass_hex,
      pbkdf2_long_salt_hex,
      4096, 25,
      "3d2eec4fe41c849b80c8d83662c0e44a8b291a964cf2f07038" },
};

/* The first 16 bytes of the RFC 6070 c=1 KAT above (the pkcs5=1 c=1
 * derive's expected prefix). */
static const char C1_PREFIX16_HEX[] = "0c60c80f961f0e71f3a9b524af601206";

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    pbkdf2_test_kats(&fam, kats, sizeof(kats) / sizeof(kats[0]));
    /* No verified pkcs5=0 value for SHA-1: acceptance-checked only. */
    pbkdf2_test_pkcs5_bounds(&fam, NULL, C1_PREFIX16_HEX);

    test_report_pass();
    return 0;
}
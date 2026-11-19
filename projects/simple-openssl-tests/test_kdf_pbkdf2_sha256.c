#include "test_common.h"

#include "test_kdf_pbkdf2_common.h"

#include <openssl/evp.h>

/*

 * test_kdf_pbkdf2_sha256 -- PBKDF2-HMAC-SHA256 through the EVP_KDF public
 * API, one binary per digest (the shared PBKDF2 machinery, the RFC 6070
 * vector provenance, the CLI verification commands and the not-run
 * c=16777216 note live in test_kdf_pbkdf2_common.c).
 *
 * Known-answer provenance (SHA-256): the four rows below are the original
 * monolith's SHA-256 rows (P="password"/S="salt" at c=1, 2 and 4096, and
 * the 70-byte-password c=1000 vector), every value verified against the
 * authoritative pizfix OpenSSL 3.6.5 and the system OpenSSL 3.0.13 and
 * additionally matched against OpenSSL's own test data
 * (test/recipes/30-test_evp_data/evpkdf_pbkdf2.txt).
 *
 * This is also the digest the monolith verified the pkcs5 values for, so
 * its pkcs5 section compares the compliant pkcs5=0 derive and the pkcs5=1
 * c=1 derive against those CLI-verified constants; the SHA-1 binary checks
 * the same bounds with acceptance checks only (plus its own c=1 prefix).
 */

/* This digest's family row: the OSSL_KDF_PARAM_DIGEST value. */
static const struct pbkdf2_family fam = { "sha256" };

/* A 70-byte 'p' password: crosses the SHA-256/SHA-1 HMAC block size (64). */
static const char PASS70_HEX[] =
    "7070707070707070707070707070707070707070707070707070707070707070"
    "7070707070707070707070707070707070707070707070707070707070707070"
    "707070707070";

/* This digest's KAT rows (the "password"/"salt" and 36-byte salt material
 * comes from the shared arrays in the common module). */
static const struct pbkdf2_kat kats[] = {
    { pbkdf2_pass_hex,
      pbkdf2_salt_hex,
      1, 32,
      "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b" },
    { pbkdf2_pass_hex,
      pbkdf2_salt_hex,
      2, 32,
      "ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43" },
    { pbkdf2_pass_hex,
      pbkdf2_salt_hex,
      4096, 32,
      "c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a" },
    /* The 70-byte block-size-crossing password (PASS70_HEX above) with the
     * shared 36-byte salt, c=1000. */
    { PASS70_HEX,
      pbkdf2_long_salt_hex,
      1000, 32,
      "5597152377c2edd3ba6cf1b5cb23638cc926927fa859282a2e144f8dd2f0686a" },
};

/* The CLI-verified pkcs5=0 value (c=1000, 16-byte salt, 16-byte key) and
 * the first 16 bytes of the c=1 KAT above (the well-known pkcs5=1 c=1
 * prefix). */
static const char PKCS5_STRONG16_HEX[] = "25eb86acc76e43018f18b9a8f90c2fed";
static const char C1_PREFIX16_HEX[] = "120fb6cffcf8b32c43e7225256c4f837";

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    pbkdf2_test_kats(&fam, kats, sizeof(kats) / sizeof(kats[0]));
    /* The verified pkcs5=0 value and the well-known pkcs5=1 c=1 prefix. */
    pbkdf2_test_pkcs5_bounds(&fam, PKCS5_STRONG16_HEX, C1_PREFIX16_HEX);

    test_report_pass();
    return 0;
}
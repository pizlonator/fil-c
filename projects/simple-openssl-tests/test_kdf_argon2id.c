#include "test_common.h"

#include "test_kdf_argon2_common.h"

#include <openssl/evp.h>

/*
 * test_kdf_argon2id -- Argon2id (RFC 9106) through the EVP_KDF public API,
 * one binary per variant (the shared Argon2 machinery, the RFC 9106 vector
 * provenance and the version gate live in test_kdf_argon2_common.c).
 *
 * Known-answer provenance (Argon2id): the RFC 9106 section 5.3 Argon2id
 * tag over the shared section 5 inputs (see test_kdf_argon2_common.c),
 * plus this variant's extra CLI-derived KATs below.
 *
 * KAT correction vs the task brief: the tag
 * 0d640df58d78766c...e6b01e659 is the RFC 9106 section 5.3 Argon2id vector,
 * whose inputs are P = 01 x 32, S = 02 x 16, secret = 03 x 8, associated
 * data = 04 x 12 (t=3 passes, m=32 KiB, p=4 lanes, v=0x13) -- NOT the ASCII
 * strings "password"/"somesalt" (those inputs yield bb0cc80a3e671149...
 * a6ab05c under the same cost parameters; that value is embedded below as
 * this binary's extra KAT).
 */

/* This variant's RFC 9106 section 5 tag row. */
static const struct argon2_variant variant = {
    "ARGON2ID",
    "0d640df58d78766c08c037a34a8b53c9d01ef0452d75b65eb52520e96b01e659",
};

/* This variant's extra CLI-derived KATs: the draft-vector inputs from the
 * task brief ("password"/"somesalt") and the version 1.0 behavior. */
static const char *const PW_HEX = "70617373776f7264";
static const char *const SOMESALT_HEX = "736f6d6573616c74";
static const char *const PW_TAG_HEX =
    "bb0cc80a3e671149526915418c6eefe761bb19d5d2d567a017703e0cea6ab05c";
static const char *const V10_TAG_HEX =
    "7dda2727d1d6662b9f4b53e3828a9ce7787d1a4cc7dc5bce617f11821d57d9f2";

/* Working-buffer sizes for the draft-vector inputs (the original
 * monolith's PASS_CAP/SALT_CAP/OUT_CAP). */
#define PASS_CAP 32
#define SALT_CAP 16
#define OUT_CAP 64

/* (3) The draft-vector inputs from the task brief, and version 1.0. */
static void test_draft_vector_and_v10(void)
{
    unsigned char *pass = ALLOCATE_KEY(PASS_CAP);
    unsigned char *salt = ALLOCATE_SALT(SALT_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(OUT_CAP);
    size_t passlen, saltlen, wantlen;

    passlen = from_hex(PW_HEX, pass, PASS_CAP);
    saltlen = from_hex(SOMESALT_HEX, salt, SALT_CAP);
    wantlen = from_hex(PW_TAG_HEX, want, OUT_CAP);
    CHECK(argon2_derive("ARGON2ID", pass, passlen, salt, saltlen, NULL, 0,
                        NULL, 0, 4, 1, 32, 3, -1, out, 32),
          "Argon2id password/somesalt derive failed");
    expect_equal("Argon2id password/somesalt output does not match the "
                 "CLI-verified value", out, 32, want, wantlen);

    passlen = from_hex(argon2_rfc_pass_hex, pass, PASS_CAP);
    saltlen = from_hex(argon2_rfc_salt_hex, salt, SALT_CAP);
    wantlen = from_hex(V10_TAG_HEX, want, OUT_CAP);
    CHECK(argon2_derive("ARGON2ID", pass, passlen, salt, saltlen, NULL, 0,
                        NULL, 0, 4, 1, 32, 3, 16, out, 32),
          "Argon2id version=1.0 derive failed");
    expect_equal("version 1.0 output does not match the CLI-verified value",
                 out, 32, want, wantlen);

    free_poison_allocations();
}

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    /* Argon2 is newer than the system OpenSSL 3.0 series: a failed fetch
     * takes the SKIP path. */
    argon2_test_gate(&variant);
    argon2_test_rfc_kat(&variant);
    argon2_test_variants_distinct(&variant);
    test_draft_vector_and_v10();
    argon2_test_validation(&variant);

    test_report_pass();
    return 0;
}

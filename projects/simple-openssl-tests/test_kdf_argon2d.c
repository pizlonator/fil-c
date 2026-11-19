#include "test_common.h"

#include "test_kdf_argon2_common.h"

#include <openssl/evp.h>

/*
 * test_kdf_argon2d -- Argon2d (RFC 9106) through the EVP_KDF public API,
 * one binary per variant (the shared Argon2 machinery, the RFC 9106 vector
 * provenance, the KAT correction note and the version gate live in
 * test_kdf_argon2_common.c).
 *
 * Known-answer provenance (Argon2d): the tag below is the RFC 9106 section
 * 5.1 Argon2d vector over the shared section 5 inputs (P = 01 x 32,
 * S = 02 x 16, secret = 03 x 8, associated data = 04 x 12; t=3 passes,
 * m=32 KiB, p=4 lanes, v=0x13), verified with the authoritative pizfix
 * OpenSSL 3.6.5 CLI (see test_kdf_argon2_common.c).
 */

/* This variant's RFC 9106 section 5 tag row. */
static const struct argon2_variant variant = {
    "ARGON2D",
    "512b391b6f1162975371d30919734294f868e3be3984f3c1a13a4db9fabe4acb",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    /* Argon2 is newer than the system OpenSSL 3.0 series: a failed fetch
     * takes the SKIP path. */
    argon2_test_gate(&variant);
    argon2_test_rfc_kat(&variant);
    argon2_test_variants_distinct(&variant);
    argon2_test_validation(&variant);

    test_report_pass();
    return 0;
}

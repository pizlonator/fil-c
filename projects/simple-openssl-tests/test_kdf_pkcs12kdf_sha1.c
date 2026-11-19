#include "test_common.h"

#include "test_kdf_pkcs12kdf_common.h"

#include <openssl/evp.h>

/*

 * test_kdf_pkcs12kdf_sha1 -- the PKCS#12 KDF (RFC 7292 appendix B) through
 * the EVP_KDF public API for the SHA-1 digest, one binary per digest (the
 * shared PKCS12KDF machinery, the RFC 7292 note, the CLI verification
 * commands and the constants' provenance live in
 * test_kdf_pkcs12kdf_common.c).
 *
 * Known-answer provenance (SHA-1): the three id 1/2/3 rows below are the
 * original monolith's SHA-1 rows (over the shared pass = "password" and
 * salt = "salt", iter = 2048, all 32 bytes of output), derived with the
 * authoritative pizfix OpenSSL 3.6.5 and cross-checked against the system
 * OpenSSL 3.0.13:
 *
 *   pizfix/bin/openssl kdf -keylen 32 -kdfopt digest:sha1 \
 *       -kdfopt hexpass:70617373776f7264 -kdfopt hexsalt:73616c74 \
 *       -kdfopt iter:2048 -kdfopt id:<1|2|3> PKCS12KDF
 *
 * The id-distinctness and missing-pass/missing-salt negatives run for this
 * digest too (the monolith ran them on SHA-256 only).
 */

/* This digest's family row: the OSSL_KDF_PARAM_DIGEST value. */
static const struct pkcs12_family fam = { "sha1" };

/* This digest's KAT rows: the three material ids (1 = key, 2 = IV, 3 = MAC
 * secret) at iter = 2048, all 32 bytes of output. */
static const struct pkcs12_kat kats[] = {
    { 1, 2048,
      "ba00e106a9ca89beb0f09a91c50ee7f8d501f2ebe5dde6547caaef03e0567dca" },
    { 2, 2048,
      "b46ddec0d601f8d6bb86992e8f5a1073da62a4878455562a69d4bd6be3d336c5" },
    { 3, 2048,
      "2bf931c3e4a127c5b48f1c9b9624d0aac9a5032982dde93ea47c5b167111ee43" },
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    pkcs12_test_kats(&fam, kats, sizeof(kats) / sizeof(kats[0]));
    pkcs12_test_ids_differ(&fam);
    pkcs12_test_missing_inputs(&fam);

    test_report_pass();
    return 0;
}
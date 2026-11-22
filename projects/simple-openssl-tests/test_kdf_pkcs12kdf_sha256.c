#include "test_common.h"

#include "test_kdf_pkcs12kdf_common.h"

#include <openssl/evp.h>

/*

 * test_kdf_pkcs12kdf_sha256 -- the PKCS#12 KDF (RFC 7292 appendix B)
 * through the EVP_KDF public API for the SHA-256 digest, one binary per
 * digest (the shared PKCS12KDF machinery, the RFC 7292 note, the CLI
 * verification commands and the constants' provenance live in
 * test_kdf_pkcs12kdf_common.c).
 *
 * Known-answer provenance (SHA-256): the four rows below are the original
 * monolith's SHA-256 rows (over the shared pass = "password" and salt =
 * "salt"; the three material ids at iter = 2048 and the iteration-count-
 * of-1 id-1 row, all 32 bytes of output), derived with the authoritative
 * pizfix OpenSSL 3.6.5 and cross-checked against the system OpenSSL
 * 3.0.13:
 *
 *   pizfix/bin/openssl kdf -keylen 32 -kdfopt digest:sha256 \
 *       -kdfopt hexpass:70617373776f7264 -kdfopt hexsalt:73616c74 \
 *       -kdfopt iter:<2048|1> -kdfopt id:<1|2|3> PKCS12KDF
 *
 * The id-distinctness and missing-pass/missing-salt negatives run in every
 * binary of the family.
 */

/* This digest's family row: the OSSL_KDF_PARAM_DIGEST value. */
static const struct pkcs12_family fam = { "sha256" };

/* This digest's KAT rows: the three material ids (1 = key, 2 = IV, 3 = MAC
 * secret) at iter = 2048, plus the iteration-count-of-1 id-1 row, all 32
 * bytes of output. */
static const struct pkcs12_kat kats[] = {
    { 1, 2048,
      "b3e8c22aac46cada13e88c60a32bbcd34309531cc1a97b96dfa8d50f3f9c647d" },
    { 2, 2048,
      "8ebc518cebfd4460f0373e719e5e55639e487c9d341dfa3a7b7a47789002accd" },
    { 3, 2048,
      "4dffb4aff824f6cf5c4f21203b43cbb8482750e9ce0c9bf26bbaa39ff02147d3" },
    { 1, 1,
      "ee3299fd9f1f99c72b4557890a25ba04ee65797e8cd70a7d1fadb950266f9652" },
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
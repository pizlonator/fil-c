#include "test_common.h"

#include "test_kdf_tls1prf_common.h"

#include <openssl/evp.h>

/*

 * test_kdf_tls1prf_sha256 -- the TLS 1.2 PRF (single-digest SHA-256, the
 * P_<hash> construction of RFC 5246 section 5) through the EVP_KDF public
 * API, one binary per digest (the shared TLS1-PRF machinery, the vector
 * provenance, the CLI verification commands and the distribution decisions
 * live in test_kdf_tls1prf_common.c; TLS 1.3's HKDF-based key schedule is
 * the separate test_kdf_tls13kdf binary, RFC 8446).
 *
 * Known-answer provenance (SHA-256): the NIST master-secret vector below is
 * the original monolith's TLS 1.2 row, verified against the authoritative
 * pizfix OpenSSL 3.6.5 with
 *
 *   pizfix/bin/openssl kdf -keylen 48 -kdfopt digest:SHA256 \
 *       -kdfopt hexsecret:<secret> -kdfopt 'seed:master secret' \
 *       -kdfopt hexseed:<client_random> -kdfopt hexseed:<server_random> \
 *       TLS1-PRF
 *
 * and the same command under the system OpenSSL 3.0.13; the values are the
 * NIST vectors distributed in OpenSSL's own test data
 * (test/recipes/30-test_evp_data/evpkdf_tls12_prf.txt): TLS 1.2, same shape
 * as the TLS 1.1 vector (48-byte secret, seed = "master secret" ||
 * client_random || server_random, 48 bytes out)
 * -> 202c88c00f84a17a2002707960478746...51288bf.
 */



/* This digest's family row: the bare-TLS1-PRF digest param, its prose name

 * for the failure messages (the original monolith wrote "SHA-256" with the
 * hyphen), and the TLS version it instantiates. */
static const struct tls1prf_family fam = { "SHA256", "SHA-256", "TLS 1.2" };



/* This digest's NIST master-secret KAT over the shared "master secret"

 * label. */

static const struct tls1prf_kat kat = {

    /* SECRET12: the 48-byte master secret. */
    "f8938ecc9edebc5030c0c6a441e213cd24e6f770a50dda07876f8d55da062bca"
    "db386b411fd4fe4313a604fce6c17fbc",
    tls1prf_label,
    /* CLIENT_RANDOM12: the 32-byte client_random. */
    "36c129d01a3200894b9179faac589d9835d58775f9b5ea3587cb8fd0364cae8c",
    /* SERVER_RANDOM12: the 32-byte server_random. */
    "f6c9575ed7ddd73e1f7d16eca115415812a43c2b747daaaae043abfb50053fce",
    /* OUT12: the expected 48-byte master-secret output. */
    "202c88c00f84a17a20027079604787461176455539e705be730890602c289a50"
    "01e34eeb3a043e5d52a65e66125188bf",
};



/* The empty-secret output: an explicitly set zero-length secret over the
 * shared 7-byte synthetic seed, 16 bytes out (the monolith's empty-secret
 * KAT). */

static const char EMPTY_SECRET12_OUT_HEX[] =

    "59238473e0148cb7595d8d80a82ee5a9";



int main(int argc, char **argv)

{

    test_parse_args(argc, argv);



    tls1prf_test_kat_multi_seed(&fam, &kat);
    tls1prf_test_concat_seed(&fam, &kat);
    tls1prf_test_empty_secret(&fam, EMPTY_SECRET12_OUT_HEX);
    tls1prf_test_missing_inputs(&fam, kat.secrethex);



    test_report_pass();
    return 0;

}
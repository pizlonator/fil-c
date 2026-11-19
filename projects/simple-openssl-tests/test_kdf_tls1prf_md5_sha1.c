#include "test_common.h"

#include "test_kdf_tls1prf_common.h"

#include <openssl/evp.h>

/*

 * test_kdf_tls1prf_md5_sha1 -- the TLS 1.0/1.1 PRF (the MD5-SHA1 combo,
 * which splits the secret into two halves sharing the middle byte; RFC
 * 2246/4346) through the EVP_KDF public API, one binary per digest (the
 * shared TLS1-PRF machinery, the vector provenance, the CLI verification
 * commands and the distribution decisions live in
 * test_kdf_tls1prf_common.c; TLS 1.2 and TLS 1.3's HKDF-based schedule
 * (RFC 5246, RFC 8446) are the sibling test_kdf_tls1prf_sha256 and
 * test_kdf_tls13kdf binaries).
 *
 * Known-answer provenance (MD5-SHA1): the NIST master-secret vector below
 * is the original monolith's TLS 1.1 row, verified against the
 * authoritative pizfix OpenSSL 3.6.5 with
 *
 *   pizfix/bin/openssl kdf -keylen 48 -kdfopt digest:MD5-SHA1 \
 *       -kdfopt hexsecret:<secret> -kdfopt 'seed:master secret' \
 *       -kdfopt hexseed:<client_random> -kdfopt hexseed:<server_random> \
 *       TLS1-PRF
 *
 * and the same command under the system OpenSSL 3.0.13; the values are the
 * NIST vectors distributed in OpenSSL's own test data
 * (test/recipes/30-test_evp_data/evpkdf_tls11_prf.txt): TLS 1.1, 48-byte
 * secret, seed = "master secret" || client_random || server_random,
 * 48 bytes out -> 2f6962dfbc744c4b2138bb6b3d33054c...e998f62.
 *

 */



/* This digest's family row: the bare-TLS1-PRF digest param, its prose name

 * for the failure messages, and the TLS version it instantiates. */
static const struct tls1prf_family fam = { "MD5-SHA1", "MD5-SHA1",
                                           "TLS 1.1" };



/* This digest's NIST master-secret KAT over the shared "master secret"

 * label. */

static const struct tls1prf_kat kat = {

    /* SECRET11: the 48-byte master secret. */
    "bded7fa5c1699c010be23dd06ada3a48349f21e5f86263d512c0c5cc379f0e78"
    "0ec55d9844b2f1db02a96453513568d0",
    tls1prf_label,
    /* CLIENT_RANDOM11: the 32-byte client_random. */
    "e5acaf549cd25c22d964c0d930fa4b5261d2507fad84c33715b7b9a864020693",
    /* SERVER_RANDOM11: the 32-byte server_random. */
    "135e4d557fdf3aa6406d82975d5c606a9734c9334b42136e96990fbd5358cdb2",
    /* OUT11: the expected 48-byte master-secret output. */
    "2f6962dfbc744c4b2138bb6b3d33054c5ecc14f24851d9896395a44ab3964efc"
    "2090c5bf51a0891209f46c1e1e998f62",
};



/* The odd-secret output: a 47-byte secret (the SECRET11 value above with
 * its last byte dropped -- TLS <= 1.1 splits the secret into two halves
 * sharing the middle byte), the label alone as the seed, 32 bytes out. */

static const char ODD_SECRET11_OUT_HEX[] =

    "cfb8713445f0cf55d2c70823f9cbad483d27c8306edfbfcaa"

    "b43d162c2452114";



int main(int argc, char **argv)

{

    test_parse_args(argc, argv);



    tls1prf_test_kat_multi_seed(&fam, &kat);
    tls1prf_test_concat_seed(&fam, &kat);
    tls1prf_test_odd_secret(&fam, &kat, ODD_SECRET11_OUT_HEX);
    tls1prf_test_missing_inputs(&fam, kat.secrethex);



    test_report_pass();
    return 0;

}
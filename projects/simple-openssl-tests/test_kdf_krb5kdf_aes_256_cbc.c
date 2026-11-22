#include "test_common.h"

#include "test_kdf_krb5kdf_common.h"

#include <openssl/evp.h>

/*

 * test_kdf_krb5kdf_aes_256_cbc -- the Kerberos 5 RFC 3961 "DK" (encryption
 * key) KDF through the EVP_KDF public API for AES-256-CBC (the
 * aes256-cts-hmac-sha1-96 enctype), one binary per cipher (the shared
 * KRB5KDF machinery, the RFC 3961 background, the n-fold/derive-key
 * commentary, the vector provenance and the CLI verification commands live
 * in test_kdf_krb5kdf_common.c).
 *
 * Known-answer provenance (AES-256-CBC): the row below is the original
 * monolith's AES-256-CBC row of the evpkdf_krb5.txt table -- the 32-byte
 * output over the shared 32-byte input key -- re-verified with
 *
 *   pizfix/bin/openssl kdf -keylen 32 -kdfopt cipher:AES-256-CBC \
 *       -kdfopt hexkey:<key> -kdfopt hexconstant:<constant> KRB5KDF
 *
 * and the same command under the system OpenSSL 3.0.13.
 *
 * This binary checks the blocksize-constant behavior for its own cipher
 * relatively, against the in-test zero-IV encryption-chain reference (see
 * test_kdf_krb5kdf_common.c).  Its negative sections run the monolith's
 * output-length and
 * mandatory-input checks for this cipher too (24 bytes is wrong for the
 * 32-byte key as well, and the block-size constant-length ceiling is the
 * same 16 bytes); the adaptive explicitly-empty-constant gate stays with
 * the negative section it belonged to in the monolith, in the AES-128-CBC
 * binary.
 */

/* The cipher under test and its block size (the constant-length ceiling;
 * the mandated output length is the 32-byte key length). */
static const struct krb5_family fam = { "AES-256-CBC", 16 };

/* This cipher's KAT row: the RFC 3961-derived key-usage constant over the
 * shared 32-byte input key (from the common module). */
static const struct krb5_kat kats[] = {
    { krb5_key256_hex, "0000000299", 32,
      "bfab388bdcb238e9f9c98d6a878304f04d30c82556375ac507a7a852790f4674" },
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    krb5kdf_test_kats(&fam, kats, sizeof(kats) / sizeof(kats[0]));
    /* No verified full-block-constant KAT for this cipher: the section
     * verifies the n-fold identity path relatively (outhex == NULL). */
    krb5kdf_test_blocksize_constant(&fam, krb5_key256_hex, NULL);
    krb5kdf_test_negative_output_length(&fam, &kats[0]);
    krb5kdf_test_negative_constant_too_long(&fam, &kats[0]);
    krb5kdf_test_negative_mandatory(&fam, &kats[0]);

    test_report_pass();
    return 0;
}

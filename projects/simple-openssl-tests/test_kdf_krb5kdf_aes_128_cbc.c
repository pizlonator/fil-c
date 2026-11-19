#include "test_common.h"

#include "test_kdf_krb5kdf_common.h"

#include <openssl/evp.h>

/*

 * test_kdf_krb5kdf_aes_128_cbc -- the Kerberos 5 RFC 3961 "DK" (encryption
 * key) KDF through the EVP_KDF public API for AES-128-CBC (the
 * aes128-cts-hmac-sha1-96 enctype), one binary per cipher (the shared
 * KRB5KDF machinery, the RFC 3961 background, the n-fold/derive-key
 * commentary, the vector provenance and the CLI verification commands live
 * in test_kdf_krb5kdf_common.c).
 *
 * Known-answer provenance (AES-128-CBC): the rows below are the original
 * monolith's AES-128-CBC rows of the evpkdf_krb5.txt table -- the three
 * key-usage constants over the shared 16-byte input key -- every value
 * re-verified with
 *
 *   pizfix/bin/openssl kdf -keylen 16 -kdfopt cipher:AES-128-CBC \
 *       -kdfopt hexkey:<key> -kdfopt hexconstant:<constant> KRB5KDF
 *
 * and the same command under the system OpenSSL 3.0.13.
 */

/* The cipher under test and its block size (the constant-length ceiling;
 * the mandated output length is the 16-byte key length). */
static const struct krb5_family fam = { "AES-128-CBC", 16 };

/* This cipher's KAT rows: the three RFC 3961-derived key-usage constants
 * over the shared 16-byte input key (from the common module). */
static const struct krb5_kat kats[] = {
    { krb5_key128_hex, "0000000299", 16,
      "34280a382bc92769b2da2f9ef066854b" },
    { krb5_key128_hex, "00000002aa", 16,
      "5b14fc4e250e14ddf9dccf1af6674f53" },
    { krb5_key128_hex, "0000000255", 16,
      "4ed31063621684f09ae8d89991af3e8f" },
};

/* This cipher's verified full-block-constant KAT: a full 16-byte constant
 * takes the n-fold identity path, so the value is the plain encryption of
 * 000102...0f under the key (the blocksize-constant section's KAT;
 * CLI-verified on both the 3.6.5 and the 3.0.13 builds). */
static const char BLOCKSIZE_CONSTANT_OKM_HEX[] =
    "b6838592d1101aa673277d35a505b052";

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    krb5kdf_test_kats(&fam, kats, sizeof(kats) / sizeof(kats[0]));
    /* The verified full-block-constant KAT (see BLOCKSIZE_CONSTANT_OKM_HEX
     * above). */
    krb5kdf_test_blocksize_constant(&fam, krb5_key128_hex,
                                    BLOCKSIZE_CONSTANT_OKM_HEX);
    krb5kdf_test_negative_output_length(&fam, &kats[0]);
    krb5kdf_test_negative_empty_constant(&fam, &kats[0]);
    krb5kdf_test_negative_constant_too_long(&fam, &kats[0]);
    krb5kdf_test_negative_mandatory(&fam, &kats[0]);

    test_report_pass();
    return 0;
}

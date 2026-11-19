#include "test_common.h"

#include "test_mac_cmac_common.h"

#include <openssl/evp.h>

/*
 * test_mac_cmac_des_ede3_cbc -- EVP_MAC "CMAC" over DES-EDE3-CBC
 * known-answer and behavior tests, one binary per cipher (the shared CMAC
 * machinery, the CLI verification recipe, and the wrong-cipher rationale
 * live in test_mac_cmac_common.c).
 *
 * Known-answer provenance (DES-EDE3-CBC): key 010203...18 (24 bytes) over
 * the 16-byte prefix of the shared SP 800-38A message; no published
 * vector, so the constant was produced by the authoritative CLI and
 * cross-checked on the system 3.0.13 and pizfix 3.6.5 stacks (8-byte tag,
 * DES block size).
 *
 * Coverage for this cipher: the one-shot KAT; the 16-byte message split as
 * {7, 9} partial updates == one-shot (the AES binaries split their 40-byte
 * example 3 message as {17, 23}); EVP_MAC_CTX_get_mac_size == 8 (the DES
 * block size); and a wrong cipher (AES-128-CBC, whose 16-byte key length
 * differs from the 24-byte key) rejected at init.
 */

/* The cipher under test, its key length and CMAC tag/block size, and the
 * wrong cipher for the negative init test (see test_mac_cmac_common.c for
 * the rationale). */
static const struct cmac_family fam = { "DES-EDE3-CBC", 24, 8, "AES-128-CBC" };

/* This cipher's KAT row (the constant verified as described above). */
static const struct cmac_kat kats[] = {
    { "DES-EDE3-CBC",
      "0102030405060708090a0b0c0d0e0f101112131415161718", 16,
      "c0b9bbee139722ab" },
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    cmac_test_kats(&fam, kats, sizeof(kats) / sizeof(kats[0]));
    /* kats[0] is the 16-byte message row the partial updates run over. */
    cmac_test_partial_updates(&fam, &kats[0], 7, 9);
    cmac_test_mac_size(&fam, kats[0].keyhex);
    cmac_test_wrong_cipher(&fam, kats[0].keyhex);

    test_report_pass();
    return 0;
}

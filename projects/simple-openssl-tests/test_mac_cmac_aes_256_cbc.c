#include "test_common.h"

#include "test_mac_cmac_common.h"

#include <openssl/evp.h>

/*
 * test_mac_cmac_aes_256_cbc -- EVP_MAC "CMAC" over AES-256-CBC
 * known-answer and behavior tests, one binary per cipher (the shared CMAC
 * machinery, the CLI verification recipe, and the wrong-cipher rationale
 * live in test_mac_cmac_common.c).
 *
 * Known-answer provenance (AES-256): NIST SP 800-38B examples 1-4 over the
 * same SP 800-38A message 6bc1bee2... the RFC 4493 rows use (key
 * 603deb10...).  Example 1 (the empty message) doubles as the
 * subkey-generation sanity check.
 *
 * Coverage for this cipher: the four one-shot KATs; the 40-byte example 3
 * message split as {17, 23} partial updates == one-shot;
 * EVP_MAC_CTX_get_mac_size == 16 (the AES block size); and a wrong cipher
 * (AES-128-CBC, whose 16-byte key length differs from the 32-byte key)
 * rejected at init.
 */

/* The cipher under test, its key length and CMAC tag/block size, and the
 * wrong cipher for the negative init test (see test_mac_cmac_common.c for
 * the rationale). */
static const struct cmac_family fam = { "AES-256-CBC", 32, 16, "AES-128-CBC" };

/* This cipher's rows of the original twelve-row KAT table (the labels are
 * preserved, for the failure messages). */
static const struct cmac_kat kats[] = {
    { "SP 800-38B AES-256 example 1",
      "603deb1015ca71be2b73aef0857d7781"
      "1f352c073b6108d72d9810a30914dff4", 0,
      "028962f61b7bf89efc6b551f4667d983" },
    { "SP 800-38B AES-256 example 2",
      "603deb1015ca71be2b73aef0857d7781"
      "1f352c073b6108d72d9810a30914dff4", 16,
      "28a7023f452e8f82bd4bf28d8c37c35c" },
    { "SP 800-38B AES-256 example 3",
      "603deb1015ca71be2b73aef0857d7781"
      "1f352c073b6108d72d9810a30914dff4", 40,
      "aaf3d8f1de5640c232f5b169b9c911e6" },
    { "SP 800-38B AES-256 example 4",
      "603deb1015ca71be2b73aef0857d7781"
      "1f352c073b6108d72d9810a30914dff4", 64,
      "e1992190549f6ed5696a2c056c315410" },
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    cmac_test_kats(&fam, kats, sizeof(kats) / sizeof(kats[0]));
    /* kats[2] is the 40-byte example 3 row the partial updates run over. */
    cmac_test_partial_updates(&fam, &kats[2], 17, 23);
    cmac_test_mac_size(&fam, kats[0].keyhex);
    cmac_test_wrong_cipher(&fam, kats[0].keyhex);

    test_report_pass();
    return 0;
}

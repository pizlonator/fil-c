#include "test_common.h"

#include "test_mac_cmac_common.h"

#include <openssl/evp.h>

/*
 * test_mac_cmac_aes_192_cbc -- EVP_MAC "CMAC" over AES-192-CBC
 * known-answer and behavior tests, one binary per cipher (the shared CMAC
 * machinery, the CLI verification recipe, and the wrong-cipher rationale
 * live in test_mac_cmac_common.c).
 *
 * Known-answer provenance (AES-192): NIST SP 800-38B examples 1-4 over the
 * same SP 800-38A message 6bc1bee2... the RFC 4493 rows use (key
 * 8e73b0f7...).  Example 1 (the empty message) doubles as the
 * subkey-generation sanity check.
 *
 * Coverage for this cipher: the four one-shot KATs; the 40-byte example 3
 * message split as {17, 23} partial updates == one-shot;
 * EVP_MAC_CTX_get_mac_size == 16 (the AES block size); and a wrong cipher
 * (AES-256-CBC, whose 32-byte key length differs from the 24-byte key)
 * rejected at init.
 */

/* The cipher under test, its key length and CMAC tag/block size, and the
 * wrong cipher for the negative init test (see test_mac_cmac_common.c for
 * the rationale). */
static const struct cmac_family fam = { "AES-192-CBC", 24, 16, "AES-256-CBC" };

/* This cipher's rows of the original twelve-row KAT table (the labels are
 * preserved, for the failure messages). */
static const struct cmac_kat kats[] = {
    { "SP 800-38B AES-192 example 1",
      "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b", 0,
      "d17ddf46adaacde531cac483de7a9367" },
    { "SP 800-38B AES-192 example 2",
      "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b", 16,
      "9e99a7bf31e710900662f65e617c5184" },
    { "SP 800-38B AES-192 example 3",
      "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b", 40,
      "8a1de5be2eb31aad089a82e6ee908b0e" },
    { "SP 800-38B AES-192 example 4",
      "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b", 64,
      "a1d5df0eed790f794d77589659f39a11" },
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

#include "test_common.h"

#include "test_mac_cmac_common.h"

#include <openssl/evp.h>

/*
 * test_mac_cmac_aes_128_cbc -- EVP_MAC "CMAC" over AES-128-CBC
 * known-answer and behavior tests, one binary per cipher (the shared CMAC
 * machinery, the CLI verification recipe, and the wrong-cipher rationale
 * live in test_mac_cmac_common.c).
 *
 * Known-answer provenance (AES-128): RFC 4493 examples 1-4 (Mlen
 * 0/128/320/512 bits over the SP 800-38A message 6bc1bee2..., key
 * 2b7e1516...).  Example 1 (the empty message) is the subkey-generation
 * test: it can only come out right if K1/K2 were derived correctly.
 *
 * Coverage for this cipher: the four one-shot KATs (example 1 doubles as
 * the subkey-generation sanity check); the 40-byte example 3 message split
 * as {17, 23} partial updates == one-shot; EVP_MAC_CTX_get_mac_size == 16
 * (the AES block size); and a wrong cipher (AES-256-CBC, whose 32-byte key
 * length differs from the 16-byte key) rejected at init.
 */

/* The cipher under test, its key length and CMAC tag/block size, and the
 * wrong cipher for the negative init test (see test_mac_cmac_common.c for
 * the rationale). */
static const struct cmac_family fam = { "AES-128-CBC", 16, 16, "AES-256-CBC" };

/* This cipher's rows of the original twelve-row KAT table (the labels are
 * preserved, for the failure messages). */
static const struct cmac_kat kats[] = {
    { "RFC 4493 AES-128 example 1",
      "2b7e151628aed2a6abf7158809cf4f3c", 0,
      "bb1d6929e95937287fa37d129b756746" },
    { "RFC 4493 AES-128 example 2",
      "2b7e151628aed2a6abf7158809cf4f3c", 16,
      "070a16b46b4d4144f79bdd9dd04a287c" },
    { "RFC 4493 AES-128 example 3",
      "2b7e151628aed2a6abf7158809cf4f3c", 40,
      "dfa66747de9ae63030ca32611497c827" },
    { "RFC 4493 AES-128 example 4",
      "2b7e151628aed2a6abf7158809cf4f3c", 64,
      "51f0bebf7e3b9d92fc49741779363cfe" },
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

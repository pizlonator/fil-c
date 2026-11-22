#include "test_common.h"

#include "test_kdf_sshkdf_common.h"

#include <openssl/evp.h>

/*

 * test_kdf_sshkdf_sha256 -- the SSH RFC 4253 exchange-hash KDF through the
 * EVP_KDF public API for the SHA-256 digest, one binary per digest (the
 * shared SSHKDF machinery, the NIST CAVS 14.1 vector provenance, the CLI
 * verification commands, the xcghhash spelling note and the
 * wrong-length-xcghhash behavior note live in test_kdf_sshkdf_common.c).
 *
 * Known-answer provenance (SHA-256): the type A and C rows below are the
 * original monolith's SHA-256 rows (over the shared 129-byte CAVS set 1 K,
 * the 32-byte digest-sized H below and the shared 20-byte session id),
 * CLI-verified on both the pizfix 3.6.5 and the system 3.0.13 builds; the
 * SHA-256 H must be the 32-byte digest-sized hash.  Both rows derive and
 * compare the full 32 bytes.
 *
 * The distinctness and negative sections run over this binary's own
 * digest-sized H; the wrong-length xcghhash acceptance is checked
 * relatively (two derives must agree), as no wrong-length output constant
 * was verified for SHA-256.
 */

/* This digest's digest-sized xcghhash -- the original monolith's
 * CLI-verified SHA-256 H. */
static const char H256_HEX[] =
    "a4ebd45934f56792b5112dcd75a1075fdc889245e5acaf549cd25c22d964c0d9";

/* This digest's family row: the OSSL_KDF_PARAM_DIGEST value and the
 * digest-sized xcghhash. */
static const struct sshkdf_family fam = { "SHA256", H256_HEX };

/* This digest's CAVS KAT rows: types A and C, deriving and comparing the
 * full 32 bytes. */
static const struct ssh_kat kats[] = {
    { 'A', 32, 32,
      "40f858eb8c8b5119a07d5254a8e2bc14591ee1cea7dcc88f"
      "cde8d2aa165026f5" },
    { 'C', 32, 32,
      "3f1dabb5b973cc21c5e349c31136eca5ec002ad6502080bd"
      "877abcf77a6b59ae" },
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    sshkdf_test_kats(&fam, kats, sizeof(kats) / sizeof(kats[0]));
    sshkdf_test_types_differ(&fam);
    /* No verified wrong-length xcghhash constant: the acceptance is
     * checked relatively (two derives must agree). */
    sshkdf_test_negative(&fam, NULL);

    test_report_pass();
    return 0;
}
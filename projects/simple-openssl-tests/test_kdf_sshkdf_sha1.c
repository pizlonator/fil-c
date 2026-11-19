#include "test_common.h"

#include "test_kdf_sshkdf_common.h"

#include <openssl/evp.h>

/*

 * test_kdf_sshkdf_sha1 -- the SSH RFC 4253 exchange-hash KDF through the
 * EVP_KDF public API for the SHA-1 digest, one binary per digest (the
 * shared SSHKDF machinery, the NIST CAVS 14.1 vector provenance, the CLI
 * verification commands, the xcghhash spelling note and the
 * wrong-length-xcghhash behavior note live in test_kdf_sshkdf_common.c).
 *
 * Known-answer provenance (SHA-1): the six type A..F rows below are the
 * original monolith's CAVS set 1 table (K = the shared 129-byte K1, H =
 * the shared 20-byte SHA-1 H, SID = the same 20-byte value -- the
 * initial-IV case), every value re-verified with
 *
 *   pizfix/bin/openssl kdf -keylen <L> -kdfopt digest:SHA1 \
 *       -kdfopt hexkey:<K> -kdfopt hexxcghash:<H> \
 *       -kdfopt hexsession_id:<SID> -kdfopt type:<A..F> SSHKDF
 *
 * and the same command under the system OpenSSL 3.0.13.  Every row derives
 * 24 bytes (the longest CAVS length) and compares its own CAVS length: the
 * CAVS value is always a prefix, and longer derives extend it.
 *
 * The wrong-length xcghhash output for SHA-1 (type A, an 8-byte output) is
 * the monolith's CLI-verified dd8c1159d741e4f6.
 */

/* This digest's family row: the OSSL_KDF_PARAM_DIGEST value and the
 * digest-sized xcghhash (RFC 4253 requires H to be the digest size; the
 * 20-byte SHA-1 H is the shared CAVS set 1 H). */
static const struct sshkdf_family fam = { "SHA1", sshkdf_h1_hex };

/* This digest's CAVS set 1 KAT rows: the derivation type, the bytes derived
 * (24 for every row, the longest CAVS length) and the bytes compared (the
 * row's CAVS length -- the CAVS value is a prefix of the derivation). */
static const struct ssh_kat kats[] = {
    { 'A', 24, 8,  "e2f627c0b43f1ac1" },
    { 'B', 24, 8,  "58471445f342b181" },
    { 'C', 24, 24, "1ca9d310f86d51f6cb8e7007cb2b220d55c5281ce680b533" },
    { 'D', 24, 24, "2c60df8603d34cc1dbb03c11f725a44b44008851c73d6844" },
    { 'E', 24, 20, "472eb8a26166ae6aa8e06868e45c3b26e6eeed06" },
    { 'F', 24, 20, "e3e2fdb9d7bc21165a3dbe47e1eceb7764390bab" },
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    sshkdf_test_kats(&fam, kats, sizeof(kats) / sizeof(kats[0]));
    sshkdf_test_types_differ(&fam);
    /* The verified wrong-length xcghhash output (type A, 8 bytes). */
    sshkdf_test_negative(&fam, "dd8c1159d741e4f6");

    test_report_pass();
    return 0;
}
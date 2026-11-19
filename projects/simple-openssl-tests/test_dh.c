#include "test_common.h"

#include "test_dh_common.h"

/*

 * test_dh -- finite-field DH over the RFC 7919 ffdhe2048 well-known group

 * under the plain "DH" provider name (one binary per provider flavor; the

 * fixed key material, the KAT provenance, and the shared test routines

 * live in test_dh_common.c, and the "DHX" flavor -- the one that carries

 * the subgroup order q -- is the sibling binary).

 *

 * Fixed private keys are imported through EVP_PKEY_fromdata, and the

 * known-answer vector is the CLI-cross-checked shared-secret KAT.

 *

 * Known-answer vector: the shared secret Z = (y')^x mod p = g^(x*x') mod p.

 * It was verified with `pizfix/bin/openssl pkeyutl -derive` over hand-built

 * PKCS#8 PEM files of the same fixed keys (development-time scratch in

 * /tmp; the suite's programs do no file I/O) and matches the embedded Z.

 *

 * Behavior verified empirically on OpenSSL 3.0.13 and 3.6.5 and asserted

 * below (for this flavor; see test_dh_common.c):

 *

 *   - "DH" imports the fixed keys and derives the KAT Z; the derived

 *     secret is exactly 256 bytes (the group size).

 *   - A peer key from a different group (ffdhe3072, i.e. the wrong size) is

 *     rejected by EVP_PKEY_derive_set_peer.

 *   - A peer with a zero public value is rejected as well.

 */


/* This binary's provider flavor: the plain "DH" fetch name, no subgroup
 * order q on import, and the KAT Z compared against the CLI-cross-checked
 * value. */
static const struct dh_family dh_fam = {
    "DH", 0, "the CLI-cross-checked KAT",
};


int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    dh_test_kat(&dh_fam);
    dh_test_bad_peers(&dh_fam);

    test_report_pass();
    return 0;
}

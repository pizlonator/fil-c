#include "test_common.h"

#include "test_dh_common.h"

/*

 * test_dhx -- finite-field DH over the RFC 7919 ffdhe2048 well-known group

 * under the "DHX" provider name, the flavor that carries the subgroup

 * order q (one binary per provider flavor; the fixed key material, the KAT

 * provenance, and the shared test routines live in test_dh_common.c, and

 * the plain "DH" flavor is the sibling binary).

 *

 * Fixed private keys are imported through EVP_PKEY_fromdata with p, g, q,

 * priv and pub, and the known-answer vector is the CLI-cross-checked

 * shared-secret KAT: the shared secret Z = (y')^x mod p = g^(x*x') mod p

 * verified with `pizfix/bin/openssl pkeyutl -derive` over hand-built

 * PKCS#8 PEM files of the same fixed keys (development-time scratch in

 * /tmp; the suite's programs do no file I/O).  It is the same Z the plain

 * "DH" flavor derives from the same fixed keys; the CHECK text keeps the

 * monolith's phrasing, in which the DHX secret was compared against the DH

 * KAT while both flavors ran in one program.

 *

 * Behavior verified empirically on OpenSSL 3.0.13 and 3.6.5 and asserted

 * below (for this flavor; see test_dh_common.c):

 *

 *   - "DHX" imports the fixed keys and derives the KAT Z; the derived

 *     secret is exactly 256 bytes (the group size).

 *   - A peer key from a different group (ffdhe3072, i.e. the wrong size) is

 *     rejected by EVP_PKEY_derive_set_peer.

 *   - A peer with a zero public value is rejected as well.

 */


/* This binary's provider flavor: the "DHX" fetch name, the subgroup order
 * q imported with the keys, and the monolith's DHX KAT CHECK text. */
static const struct dh_family dhx_fam = {
    "DHX", 1, "the DH shared secret (KAT)",
};


int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    dh_test_kat(&dhx_fam);
    dh_test_bad_peers(&dhx_fam);

    test_report_pass();
    return 0;
}

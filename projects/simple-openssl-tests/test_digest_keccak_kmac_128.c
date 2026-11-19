#include "test_common.h"
#include "test_digest_shake_common.h"

#include <openssl/evp.h>

/*
 * test_digest_keccak_kmac_128 -- KECCAK-KMAC-128 (the digest form of
 * KMAC128) tests, one binary per digest (the shared XOF machinery, the
 * vector provenance, and the version notes live in
 * test_digest_shake_common.c).
 *
 * Known-answer provenance (KECCAK-KMAC-128): the constant below was
 * verified with pizfix/bin/openssl, e.g.
 *
 *   printf 'abc' | pizfix/bin/openssl dgst -keccak-kmac-128 -xoflen 32
 */

/* This digest's row: "abc" squeezed to 32 bytes. */
static const struct shake_kat kat = {
    "KECCAK-KMAC-128", "abc", 32,
    "3bcfe6e0471a2168f61c444843e32aea0a09ec15bd9155f169189147f98c11fc",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    /* Whole-program version gate: the monolith printed the documented SKIP
     * line ("%s is unavailable") when a KECCAK-KMAC digest did not fetch;
     * this binary's subject IS the KMAC digest, so the gate is
     * whole-program here.  (KECCAK-KMAC-128/256 are present in every
     * OpenSSL 3.x build tested here: 3.0.13 and 3.6.5.) */
    shake_probe_or_skip("KECCAK-KMAC-128");

    /* The KAT through both output-length routes with the EVP_MD_get_size ==
     * xoflen check, plus the prefix property for this digest.  (The squeeze
     * section is SHAKE-only: the x86 provider gives the KMAC digest
     * implementations no squeeze method.) */
    shake_test_kmac_kat(&kat);
    shake_test_prefix_property("KECCAK-KMAC-128", 32);

    test_report_pass();
    return 0;
}

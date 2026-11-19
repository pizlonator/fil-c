#include "test_common.h"
#include "test_cbc_hmac_common.h"

#include <openssl/evp.h>

/*
 * test_aes_256_cbc_hmac_sha1 -- the stitched AES-256-CBC-HMAC-SHA1 TLS-record
 * cipher, one binary per cipher (the shared record format, the KAT-as-
 * composition logic, and the ETM variant note live in
 * test_cbc_hmac_common.c).  Like the AES-128 row, this row cross-checks that
 * the shared TLS-record helpers are key-size independent.
 *
 * Known-answer provenance (AES-256): no record constant is embedded for this
 * cipher; the composition (separate HMAC-SHA1 + AES-256-CBC rebuild of the
 * record) is the KAT and must equal the stitched output byte for byte.
 */

/* The cipher under test, its plain-CBC streaming reference, the digest
 * behind the HMAC, and its KAT material (key = the 000102... pattern
 * continued to 32 bytes; the MAC key, the EVP IV, the explicit-IV slot, and
 * the 5-byte KAT payload are shared via the common module).  The KAT record
 * body length for L = 5 is 48 bytes. */
static const struct cbhmac_spec spec = {
    "AES-256-CBC-HMAC-SHA1", "AES-256-CBC", "SHA1", 20,
    cbhmac_key_256_hex,
    NULL,
    48,
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    /* The stitched cipher is AES-NI gated: on a build or CPU without it,
     * the fetch or the key setup fails; handle that gracefully. */
    cbhmac_probe_gate(spec.ciphername);

    cbhmac_test_kat_and_composition(&spec);
    cbhmac_test_composition_and_roundtrip(&spec);
    cbhmac_test_tamper(&spec);
    cbhmac_test_aad_rules(&spec);
    cbhmac_test_tls10_style(&spec);

    test_report_pass();
    return 0;
}

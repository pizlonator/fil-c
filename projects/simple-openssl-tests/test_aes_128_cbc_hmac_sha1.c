#include "test_common.h"
#include "test_cbc_hmac_common.h"

#include <openssl/evp.h>

/*
 * test_aes_128_cbc_hmac_sha1 -- the stitched AES-128-CBC-HMAC-SHA1 TLS-record
 * cipher, one binary per cipher (the shared record format, the KAT-as-
 * composition logic, and the ETM variant note live in
 * test_cbc_hmac_common.c).
 *
 * Known-answer provenance (AES-128): the embedded record constant below was
 * verified against both the pizfix 3.6.5 and the system 3.0.13 builds.
 */

/* The cipher under test, its plain-CBC streaming reference, the digest
 * behind the HMAC, and its KAT material (key = the 000102... pattern; the
 * MAC key, the EVP IV, the explicit-IV slot, and the 5-byte KAT payload are
 * shared via the common module).  The embedded KAT record is the 48-byte
 * TLS 1.2 record for L = 5. */
static const struct cbhmac_spec spec = {
    "AES-128-CBC-HMAC-SHA1", "AES-128-CBC", "SHA1", 20,
    cbhmac_key_128_hex,
    /* The embedded KAT record (48 bytes). */
    "1537fc822f441fcc150fd3e428745324"
    "c97b43c413da0977bb993d5d88296de6"
    "3a953340d4371189ab6242056cc56246",
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

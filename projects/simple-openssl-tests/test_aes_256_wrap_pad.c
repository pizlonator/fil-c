#include "test_common.h"
#include "test_aes_wrap_pad_common.h"

#include <openssl/evp.h>

/*

 * test_aes_256_wrap_pad -- AES-256-WRAP-PAD key wrap with padding (RFC
 * 5649) tests, one binary per cipher (the shared padding rules, the
 * A65959A6 header note, the output-size math, and the -INV provenance live
 * in test_aes_wrap_pad_common.c).

 * This binary carries the derived 20-octet KAT (no published RFC 5649
 * counterpart): the shared RFC 5649 key data under the derived 256-bit KEK,
 * derived with a scratch probe against the authoritative pizfix OpenSSL
 * 3.6.5, cross-checked byte for byte against the system OpenSSL 3.0.13, and
 * additionally reproduced with the pizfix `openssl enc` CLI (see the
 * provenance note in test_aes_wrap_pad_common.c).

 */

/* Derived: AES-256-WRAP-PAD of the shared 20 RFC 5649 key-data octets. */
static const char W20_KEK256_HEX[] =
    "29b7fa191c2165684374eee9f74595e2a42bace75c425b3053efa26ffe1bb32f";

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    wrap_pad_test_kat("AES-256-WRAP-PAD", wrap_pad_kek256_hex,
                      wrap_pad_k20_hex, W20_KEK256_HEX, "derived");
    wrap_pad_test_size_roundtrips("AES-256-WRAP-PAD");
    wrap_pad_test_corrupted_unwrap("AES-256-WRAP-PAD");
    wrap_pad_test_plain_wrap_rejected("AES-256-WRAP", "AES-256-WRAP-PAD");

    test_report_pass();
    return 0;
}

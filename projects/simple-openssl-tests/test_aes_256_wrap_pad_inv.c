#include "test_common.h"
#include "test_aes_wrap_pad_common.h"

#include <openssl/evp.h>

/*

 * test_aes_256_wrap_pad_inv -- AES-256-WRAP-PAD-INV tests, one binary per
 * cipher (the shared -INV rules -- the inverse AES block transform per SP
 * 800-38F section 5.1 with the RFC 5649 block structure, so the output
 * sizes are identical -- and the whole-program SKIP gate live in
 * test_aes_wrap_pad_common.c).

 * This binary carries the derived 20-octet -INV KAT (no published RFC 5649
 * counterpart): the inverse cipher wraps the shared RFC 5649 key data under
 * the derived 256-bit KEK with the inverse AES block transform, so
 * ENC(K20) is a fresh constant and DEC recovers K20.  Derived with a
 * scratch probe against the authoritative pizfix OpenSSL 3.6.5 and
 * cross-checked byte for byte against the system OpenSSL 3.0.13 (see the
 * provenance note in test_aes_wrap_pad_common.c).

 */

/* Derived: AES-256-WRAP-PAD-INV of the shared 20 RFC 5649 key-data octets. */
static const char WINV256_K20_HEX[] =
    "d39d42cc4f1fd43d58c4583b2e2bfc5ddea02af2d0f6b6ea67700bc1ae6d8ea7";

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    wrap_pad_test_inv("AES-256-WRAP-PAD-INV", wrap_pad_kek256_hex,
                      WINV256_K20_HEX);

    test_report_pass();
    return 0;
}

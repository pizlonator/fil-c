#include "test_common.h"
#include "test_aes_wrap_pad_common.h"

#include <openssl/evp.h>

/*

 * test_aes_192_wrap_pad -- AES-192-WRAP-PAD key wrap with padding (RFC
 * 5649) tests, one binary per cipher (the shared padding rules, the
 * A65959A6 header note, the output-size math, and the -INV provenance live
 * in test_aes_wrap_pad_common.c).

 * This binary carries the RFC 5649 section 6 vectors (the only published
 * wrap-pad example): the 20-octet and the 7-octet ("ForPasi") key data
 * under the RFC's 192-bit KEK, verified through this EVP flow against the
 * authoritative pizfix OpenSSL 3.6.5 and cross-checked against the system
 * OpenSSL 3.0.13, in both the wrap and unwrap directions.  The 20 octets
 * of key data and the KEK are shared with the other family binaries via
 * test_aes_wrap_pad_common.c.

 */

/* RFC 5649 section 6 vectors (192-bit KEK; the 20 octets and the KEK are
 * the shared arrays from the common module). */
static const char K7_HEX[] = "466f7250617369";   /* "ForPasi" */
static const char W20_HEX[] =
    "138bdeaa9b8fa7fc61f97742e72248ee"
    "5ae6ae5360d1ae6a5f54f373fa543b6a";
static const char W7_HEX[] =
    "afbeb0f07dfbf5419200f2ccb50bb24f";

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    wrap_pad_test_kat("AES-192-WRAP-PAD", wrap_pad_kek192_hex,
                      wrap_pad_k20_hex, W20_HEX, "RFC 5649");
    wrap_pad_test_kat("AES-192-WRAP-PAD", wrap_pad_kek192_hex,
                      K7_HEX, W7_HEX, "RFC 5649");
    wrap_pad_test_size_roundtrips("AES-192-WRAP-PAD");
    wrap_pad_test_corrupted_unwrap("AES-192-WRAP-PAD");
    wrap_pad_test_plain_wrap_rejected("AES-192-WRAP", "AES-192-WRAP-PAD");

    test_report_pass();
    return 0;
}

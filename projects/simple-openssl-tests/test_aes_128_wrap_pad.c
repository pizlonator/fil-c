#include "test_common.h"
#include "test_aes_wrap_pad_common.h"

#include <openssl/evp.h>

/*

 * test_aes_128_wrap_pad -- AES-128-WRAP-PAD key wrap with padding (RFC
 * 5649) tests, one binary per cipher (the shared padding rules, the
 * A65959A6 header note, the output-size math, and the -INV provenance live
 * in test_aes_wrap_pad_common.c).

 * RFC 5649 publishes only the 192-bit section 6 example, so this binary
 * carries no KAT constants; its sections are the size roundtrips, the
 * corrupted-unwrap rejection, and the plain-WRAP rejection at the 128-bit
 * key size.

 */

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    wrap_pad_test_size_roundtrips("AES-128-WRAP-PAD");
    wrap_pad_test_corrupted_unwrap("AES-128-WRAP-PAD");
    wrap_pad_test_plain_wrap_rejected("AES-128-WRAP", "AES-128-WRAP-PAD");

    test_report_pass();
    return 0;
}

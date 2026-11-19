#include "test_common.h"

#include "test_x25519_x448_common.h"

#include <openssl/evp.h>

/*

 * test_x448 -- X448 (RFC 7748): the section 6.2 known-answer vectors in

 * both derive directions, the section 5.2 iterated vectors, and the

 * low-order point rejection (one binary per function; the shared

 * iterated/low-order machinery and the vector provenance live in

 * test_x25519_x448_common.c, and the X25519 vectors and the X25519-as-KEM

 * test are the sibling's).

 *

 * Known-answer vectors, taken verbatim from RFC 7748 and verified with

 * filcc-compiled scratch probes before being embedded:

 *

 *   - Section 6.2: the X448 counterparts (56-byte keys, shared secret K),

 *     in both derive directions.

 *   - Section 5.2: the iterated vectors -- starting from k = u = 0x05, each

 *     iteration computes k = f(k, u) and u = old k; the result after 1 and

 *     1000 iterations must match the RFC.
 */

/* RFC 7748 section 6.2 (X448) vectors. */
static const char *const X448_A_SK_HEX =
    "9a8f4925d1519f5775cf46b04b5800d4ee9ee8bae8bc5565d498c28dd9c9baf5"
    "74a9419744897391006382a6f127ab1d9ac2d8c0a598726b";
static const char *const X448_A_PK_HEX =
    "9b08f7cc31b7e3e67d22d5aea121074a273bd2b83de09c63faa73d2c22c5d9bb"
    "c836647241d953d40c5b12da88120d53177f80e532c41fa0";
static const char *const X448_B_SK_HEX =
    "1c306a7ac2a0e2e0990b294470cba339e6453772b075811d8fad0d1d6927c120"
    "bb5ee8972b0d3e21374c9c921b09d1b0366f10b65173992d";
static const char *const X448_B_PK_HEX =
    "3eb7a829b0cd20f5bcfc0b599b6feccf6da4627107bdb0d4f345b43027d8b972"
    "fc3e34fb4232a13ca706dcb57aec3dae07bdc1c67bf33609";
static const char *const X448_K_HEX =
    "07fff4181ac6cc95ec1c16a94a0f74d12da232ce40a77552281d282bb60c0b56"
    "fd2464c335543936521c24403085d59a449a5037514a879d";



/* The X448 family row (provenance in test_x25519_x448_common.c). */
static const struct x25519_x448_fam fam = {
    "X448", 56,
    X448_A_SK_HEX, X448_A_PK_HEX,
    X448_B_SK_HEX, X448_B_PK_HEX, X448_K_HEX,
    /* Section 5.2 starts: k = u = 0x05, i.e. the base-point encoding
     * (little-endian). */
    "0500000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
    "3f482c8a9f19b01e6c46ee9711d9dc14fd4bf67af30765c2ae2b846a4d23a8cd0db897086239492caf350b51f833868b9bc2b3bca9cf4113",
    "aa3b4749d55b9daf1e5b00288826c467274ce3ebbdd5c17b975e09d4af6c67cf10d087202db88286e2b79fceea3ec353ef54faa26e219f38",
};


int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    x25519_x448_test_kats(&fam);
    x25519_x448_test_iterated(&fam);
    x25519_x448_test_low_order_peer(&fam);

    test_report_pass();
    return 0;
}

#include "test_common.h"

#include "test_x25519_x448_common.h"

#include <openssl/evp.h>

/*

 * test_x25519 -- X25519 (RFC 7748): the section 6.1 known-answer vectors in

 * both derive directions, the section 5.2 iterated vectors, the low-order

 * point rejection, and X25519-as-KEM (one binary per function; the shared

 * iterated/low-order machinery and the vector provenance live in

 * test_x25519_x448_common.c, and the X448 vectors are the sibling's).

 *

 * Known-answer vectors, taken verbatim from RFC 7748 and verified with

 * filcc-compiled scratch probes before being embedded:

 *

 *   - Section 6.1: Alice (a -> X25519(a,9)), Bob (b -> X25519(b,9)) and the

 *     shared secret K, in both derive directions.

 *   - Section 5.2: the iterated vectors -- starting from k = u = 0x09, each

 *     iteration computes k = f(k, u) and u = old k; the result after 1 and

 *     1000 iterations must match the RFC.

 *

 * X25519-as-KEM (EVP_PKEY_encapsulate/decapsulate) is a 3.5+ feature and

 * gates mid-run with a SKIP note on older builds (it is the only SKIP-able

 * feature here; the raw DH KATs exist in all OpenSSL 3.x): under the

 * system gcc/OpenSSL 3.0.13 it skips, on pizfix 3.6.5 it runs.

 */


/* RFC 7748 section 6.1 (X25519) vectors. */
static const char *const X25519_A_SK_HEX =
    "77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a";
static const char *const X25519_A_PK_HEX =
    "8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a";
static const char *const X25519_B_SK_HEX =
    "5dab087e624a8a4b79e17f8b83800ee66f3bb1292618b6fd1c2f8b27ff88e0eb";
static const char *const X25519_B_PK_HEX =
    "de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f";
static const char *const X25519_K_HEX =
    "4a5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742";


/* The X25519 family row (provenance in test_x25519_x448_common.c). */
static const struct x25519_x448_fam fam = {
    "X25519", 32,
    X25519_A_SK_HEX, X25519_A_PK_HEX,
    X25519_B_SK_HEX, X25519_B_PK_HEX, X25519_K_HEX,
    /* Section 5.2 starts: k = u = 0x09, i.e. the base-point encoding
     * (little-endian). */
    "0900000000000000000000000000000000000000000000000000000000000000",
    "422c8e7a6227d7bca1350b3e2bb7279f7897b87bb6854b783c60e80311ae3079",
    "684cf59ba83309552800ef566f2f4d3c1c3887c49360e3875f2eb94d99532c51",
};


/* X25519 as a KEM (OpenSSL 3.5+): encapsulate/decapsulate round-trip. */
static void test_x25519_kem(void)
{
    EVP_PKEY *key = NULL;
    EVP_PKEY_CTX *kctx, *dctx;
    unsigned char *ct, *ss, *ss2;
    size_t ctlen = 0, sslen = 0, ss2len = 0;

    kctx = EVP_PKEY_CTX_new_from_name(NULL, "X25519", NULL);
    CHECK(kctx != NULL, "EVP_PKEY_CTX_new_from_name(X25519) failed");
    CHECK(EVP_PKEY_keygen_init(kctx) == 1, "X25519 keygen_init failed");
    CHECK(EVP_PKEY_generate(kctx, &key) == 1, "X25519 key generation failed");
    EVP_PKEY_CTX_free(kctx);

    kctx = EVP_PKEY_CTX_new_from_pkey(NULL, key, NULL);
    CHECK(kctx != NULL, "EVP_PKEY_CTX_new_from_pkey failed");
    if (kctx == NULL || EVP_PKEY_encapsulate_init(kctx, NULL) != 1 ||
        EVP_PKEY_encapsulate(kctx, NULL, &ctlen, NULL, &sslen) != 1) {
        printf("SKIP %s: X25519 KEM is unavailable\n", test_prog_name());
        EVP_PKEY_CTX_free(kctx);
        EVP_PKEY_free(key);
        return;
    }
    ct = ALLOCATE_OUTPUT(ctlen);
    ss = ALLOCATE_OUTPUT(sslen);
    CHECK(EVP_PKEY_encapsulate(kctx, ct, &ctlen, ss, &sslen) == 1,
          "X25519 KEM encapsulation failed");
    EVP_PKEY_CTX_free(kctx);

    dctx = EVP_PKEY_CTX_new_from_pkey(NULL, key, NULL);
    CHECK(dctx != NULL && EVP_PKEY_decapsulate_init(dctx, NULL) == 1,
          "X25519 KEM decapsulate_init failed");
    CHECK(EVP_PKEY_decapsulate(dctx, NULL, &ss2len, ct, ctlen) == 1,
          "X25519 KEM decapsulate size query failed");
    ss2 = ALLOCATE_OUTPUT(ss2len);
    CHECK(EVP_PKEY_decapsulate(dctx, ss2, &ss2len, ct, ctlen) == 1,
          "X25519 KEM decapsulation failed");
    unpoison_all();
    CHECK(ss2len == sslen && memcmp(ss2, ss, sslen) == 0,
          "X25519 KEM decapsulation did not recover the shared secret");
    repoison_all();
    EVP_PKEY_CTX_free(dctx);
    EVP_PKEY_free(key);

    free_poison_allocations();
}


int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    x25519_x448_test_kats(&fam);
    x25519_x448_test_iterated(&fam);
    x25519_x448_test_low_order_peer(&fam);
    test_x25519_kem();

    test_report_pass();
    return 0;
}

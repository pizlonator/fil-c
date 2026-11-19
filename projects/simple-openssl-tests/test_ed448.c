#include "test_common.h"

#include "test_ed25519_ed448_common.h"

#include <openssl/evp.h>

/*

 * test_ed448 -- Ed448 (PureEdDSA): the RFC 8032 section 7.4 known-answer

 * vectors and their checks (one binary per algorithm; the shared

 * sign/verify helpers, the context gate, and the vector provenance live in

 * test_ed25519_ed448_common.c, and the Ed25519 vectors and instances are

 * the sibling binary's).

 *

 * Known-answer vectors, taken verbatim from RFC 8032 section 7.4 and

 * verified with filcc-compiled scratch probes before being embedded:

 * "Blank" (empty message) and "1 octet" (0x03), plus the "1 octet (with

 * context)" vector (context = "foo").

 *

 * The "context-string" parameter only exists from OpenSSL 3.2 on; on 3.0

 * the contextualized subtest is skipped silently (as in the monolith --

 * the plain Ed448 KATs are version-independent).

 */


/* RFC 8032 section 7.4 for Ed448: "Blank" (empty message) and "1 octet"

 * (0x03), plus the "1 octet (with context)" vector (context "foo"). */
static const char *const ED448_BLANK_SK_HEX =
    "6c82a562cb808d10d632be89c8513ebf6c929f34ddfa8c9f63c9960ef6e348a3"
    "528c8a3fcc2f044e39a3fc5b94492f8f032e7549a20098f95b";
static const char *const ED448_BLANK_PK_HEX =
    "5fd7449b59b461fd2ce787ec616ad46a1da1342485a70e1f8a0ea75d80e96778"
    "edf124769b46c7061bd6783df1e50f6cd1fa1abeafe8256180";
static const char *const ED448_BLANK_SIG_HEX =
    "533a37f6bbe457251f023c0d88f976ae2dfb504a843e34d2074fd823d41a591f"
    "2b233f034f628281f2fd7a22ddd47d7828c59bd0a21bfd3980ff0d2028d4b18a"
    "9df63e006c5d1c2d345b925d8dc00b4104852db99ac5c7cdda8530a113a0f4db"
    "b61149f05a7363268c71d95808ff2e652600";
static const char *const ED448_1OCT_SK_HEX =
    "c4eab05d357007c632f3dbb48489924d552b08fe0c353a0d4a1f00acda2c463a"
    "fbea67c5e8d2877c5e3bc397a659949ef8021e954e0a12274e";
static const char *const ED448_1OCT_PK_HEX =
    "43ba28f430cdff456ae531545f7ecd0ac834a55d9358c0372bfa0c6c6798c086"
    "6aea01eb00742802b8438ea4cb82169c235160627b4c3a9480";
static const char *const ED448_1OCT_SIG_HEX =
    "26b8f91727bd62897af15e41eb43c377efb9c610d48f2335cb0bd0087810f435"
    "2541b143c4b981b7e18f62de8ccdf633fc1bf037ab7cd779805e0dbcc0aae1cb"
    "cee1afb2e027df36bc04dcecbf154336c19f0af7e0a6472905e799f1953d2a0f"
    "f3348ab21aa4adafd1d234441cf807c03a00";
static const char *const ED448_CTX_SIG_HEX =
    "d4f8f6131770dd46f40867d6fd5d5055de43541f8c5e35abbcd001b32a89f7d2"
    "151f7647f11d8ca2ae279fb842d607217fce6e042f6815ea000c85741de5c8da"
    "1144a6a1aba7f96de42505d7a7298524fda538fccbbb754f578c1cad10d54d0d"
    "5428407e85dcbc98a49155c13764e66c3c00";



static void test_ed448(void)
{
    unsigned char *msg = ALLOCATE_INPUT(64);
    unsigned char *sig = ALLOCATE_OUTPUT(256);
    unsigned char *want = ALLOCATE_OUTPUT(256);
    unsigned char *seed = ALLOCATE_KEY(57);
    unsigned char *pub = ALLOCATE_KEY(57);
    EVP_PKEY *key;
    size_t msglen = 0, wantlen, siglen = 0;

    /* "Blank": empty message. */
    from_hex(ED448_BLANK_SK_HEX, seed, 57);
    from_hex(ED448_BLANK_PK_HEX, pub, 57);
    key = ed25519_ed448_make_key("ED448", seed, 57, pub, 57);
    wantlen = from_hex(ED448_BLANK_SIG_HEX, want, 256);
    CHECK(wantlen == 114, "Ed448 Blank KAT signature length mismatch");
    CHECK(ed25519_ed448_sign(key, NULL, msg, 0, sig, &siglen) == 1,
          "Ed448 Blank signing failed");
    unpoison_all();
    CHECK(siglen == 114 && memcmp(sig, want, 114) == 0,
          "Ed448 Blank signature != RFC 8032 7.4 vector");
    repoison_all();
    CHECK(ed25519_ed448_verify(key, NULL, msg, 0, want, wantlen) == 1,
          "Ed448 Blank RFC signature does not verify");
    EVP_PKEY_free(key);

    /* "1 octet": message 0x03. */
    from_hex(ED448_1OCT_SK_HEX, seed, 57);
    from_hex(ED448_1OCT_PK_HEX, pub, 57);
    key = ed25519_ed448_make_key("ED448", seed, 57, pub, 57);
    msg[0] = 0x03;
    msglen = 1;
    wantlen = from_hex(ED448_1OCT_SIG_HEX, want, 256);
    CHECK(wantlen == 114, "Ed448 1-octet KAT signature length mismatch");
    CHECK(ed25519_ed448_sign(key, NULL, msg, msglen, sig, &siglen) == 1,
          "Ed448 1-octet signing failed");
    unpoison_all();
    CHECK(siglen == 114 && memcmp(sig, want, 114) == 0,
          "Ed448 1-octet signature != RFC 8032 7.4 vector");
    repoison_all();
    EVP_PKEY_free(key);

    /* "1 octet (with context foo)" -- gated on context-string support. */
    from_hex(ED448_1OCT_SK_HEX, seed, 57);
    from_hex(ED448_1OCT_PK_HEX, pub, 57);
    key = ed25519_ed448_make_key("ED448", seed, 57, pub, 57);
    if (ed25519_ed448_ctx_accepts(key, "context-string")) {
        char ctxfoo[] = "foo";
        OSSL_PARAM params[2];
        size_t i = 0;

        params[i++] = OSSL_PARAM_construct_octet_string("context-string",
                                                        ctxfoo, 3);
        params[i] = OSSL_PARAM_construct_end();
        wantlen = from_hex(ED448_CTX_SIG_HEX, want, 256);
        CHECK(wantlen == 114, "Ed448 context KAT signature length mismatch");
        CHECK(ed25519_ed448_sign(key, params, msg, msglen, sig, &siglen) == 1,
              "Ed448 context signing failed");
        unpoison_all();
        CHECK(siglen == 114 && memcmp(sig, want, 114) == 0,
              "Ed448 context signature != RFC 8032 7.4 vector");
        repoison_all();
        CHECK(ed25519_ed448_verify(key, params, msg, msglen, want, wantlen) == 1,
              "Ed448 context RFC signature does not verify");
    }
    EVP_PKEY_free(key);

    free_poison_allocations();
}


int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    test_ed448();

    test_report_pass();
    return 0;
}

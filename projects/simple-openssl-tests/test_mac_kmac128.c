#include "test_common.h"

#include "test_mac_kmac_common.h"

#include <openssl/evp.h>

/*
 * test_mac_kmac128 -- EVP_MAC "KMAC-128" (NIST SP 800-185) known-answer
 * and behavior tests, one binary per MAC (the shared KMAC machinery, the
 * NIST sample provenance, and the "custom" OCTET_STRING portability note
 * live in test_mac_kmac_common.c).
 *
 * Known-answer provenance (KMAC-128): this MAC's rows of the NIST sample
 * table -- samples 1-3 (KMAC128/00010203/""/32, KMAC128/00010203/
 * "My Tagged Application"/32, and the 200-byte custom-message sample) and
 * the two KMAC-128 XOF samples 7-8 -- every one re-verified with
 * pizfix/bin/openssl, e.g.
 *
 *   printf '<hex msg>' | xxd -r -p | pizfix/bin/openssl mac \
 *       -macopt hexkey:404142...5f -macopt size:32 \
 *       -macopt custom:'My Tagged Application' KMAC-128
 *
 * Coverage for this MAC: the five KATs (XOF included); the absent-custom
 * == empty-custom equivalence over the sample 1 message; the default
 * output size 32; chunked updates ({7,rest} and {64,rest}) over the
 * 200-byte sample 3 message; and the requested output size (40) tracked
 * by EVP_MAC_CTX_get_mac_size with a determinism cross-check.
 */

/* The MAC under test and its provider default output size. */
static const struct kmac_family fam = { "KMAC-128", 32 };

/* This MAC's rows of the NIST sample table (the sample numbers of the
 * original nine-row table are preserved, for the failure messages). */
static const struct kmac_kat kats[] = {
    { 1, "00010203", 4, "", 0, 32,
      "e5780b0d3ea6f7d3a429c5706aa43a00fadbd7d49628839e3187243f456ee14e" },
    { 2, "00010203", 4, "My Tagged Application", 0, 32,
      "3b1fba963cd8b0b59e8c1a6d71888b7143651af8ba0a7070c0979e2811324aa5" },
    { 3, NULL, 200, "My Tagged Application", 0, 32,
      "1f5b4e6cca02209e0dcb5ca635b89a15e271ecc760071dfd805faa38f9729230" },
    { 7, "00010203", 4, NULL, 1, 32,
      "cd83740bbd92ccc8cf032b1481a0f4460e7ca9dd12b08a0c4031178bacd6ec35" },
    { 8, "00010203", 4, "My Tagged Application", 1, 32,
      "31a44527b4ed9f5c6101d11de6d26f0620aa5c341def41299657fe9df1a3b16c" },
};

/* The chunked-update first-splits over the 200-byte sample 3 message. */
static const size_t CHUNKED_FIRSTS[] = { 7, 64 };

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    kmac_test_kats(&fam, kats, sizeof(kats) / sizeof(kats[0]));
    /* kats[0] is the empty-custom sample 1 row the equivalence runs over. */
    kmac_test_absent_custom(&fam, &kats[0]);
    /* kats[2] is the 200-byte sample 3 row the chunked updates run over. */
    kmac_test_chunked(&fam, &kats[2], CHUNKED_FIRSTS,
                      sizeof(CHUNKED_FIRSTS) / sizeof(CHUNKED_FIRSTS[0]));
    kmac_test_size_param(&fam, 40);

    test_report_pass();
    return 0;
}

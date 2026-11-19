#include "test_common.h"

#include "test_mac_kmac_common.h"

#include <openssl/evp.h>

/*
 * test_mac_kmac256 -- EVP_MAC "KMAC-256" (NIST SP 800-185) known-answer
 * and behavior tests, one binary per MAC (the shared KMAC machinery, the
 * NIST sample provenance, and the "custom" OCTET_STRING portability note
 * live in test_mac_kmac_common.c).
 *
 * Known-answer provenance (KMAC-256): this MAC's rows of the NIST sample
 * table -- samples 4-6 (KMAC256/00010203/"My Tagged Application"/64, the
 * 200-byte empty-custom sample, and the 200-byte custom-message sample)
 * and the KMAC-256 XOF sample 9 -- every one re-verified with
 * pizfix/bin/openssl, e.g.
 *
 *   printf '<hex msg>' | xxd -r -p | pizfix/bin/openssl mac \
 *       -macopt hexkey:404142...5f -macopt size:64 \
 *       -macopt custom:'My Tagged Application' KMAC-256
 *
 * Coverage for this MAC: the four KATs (XOF included); the absent-custom
 * == empty-custom equivalence over the 200-byte sample 5 message; the
 * default output size 64; chunked updates ({7,rest}) over the 200-byte
 * sample 6 message; and the requested output size (40) tracked by
 * EVP_MAC_CTX_get_mac_size with a determinism cross-check.
 */

/* The MAC under test and its provider default output size. */
static const struct kmac_family fam = { "KMAC-256", 64 };

/* This MAC's rows of the NIST sample table (the sample numbers of the
 * original nine-row table are preserved, for the failure messages). */
static const struct kmac_kat kats[] = {
    { 4, "00010203", 4, "My Tagged Application", 0, 64,
      "20c570c31346f703c9ac36c61c03cb64c3970d0cfc787e9b79599d273a68d2f7f69d4cc3de9d104a351689f27cf6f5951f0103f33f4f24871024d9c27773a8dd" },
    { 5, NULL, 200, "", 0, 64,
      "75358cf39e41494e949707927cee0af20a3ff553904c86b08f21cc414bcfd691589d27cf5e15369cbbff8b9a4c2eb17800855d0235ff635da82533ec6b759b69" },
    { 6, NULL, 200, "My Tagged Application", 0, 64,
      "b58618f71f92e1d56c1b8c55ddd7cd188b97b4ca4d99831eb2699a837da2e4d970fbacfde50033aea585f1a2708510c32d07880801bd182898fe476876fc8965" },
    { 9, "00010203", 4, "My Tagged Application", 1, 64,
      "1755133f1534752aad0748f2c706fb5c784512cab835cd15676b16c0c6647fa96faa7af634a0bf8ff6df39374fa00fad9a39e322a7c92065a64eb1fb0801eb2b" },
};

/* The chunked-update first-splits over the 200-byte sample 6 message. */
static const size_t CHUNKED_FIRSTS[] = { 7 };

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    kmac_test_kats(&fam, kats, sizeof(kats) / sizeof(kats[0]));
    /* kats[1] is the empty-custom sample 5 row the equivalence runs over. */
    kmac_test_absent_custom(&fam, &kats[1]);
    /* kats[2] is the 200-byte sample 6 row the chunked updates run over. */
    kmac_test_chunked(&fam, &kats[2], CHUNKED_FIRSTS,
                      sizeof(CHUNKED_FIRSTS) / sizeof(CHUNKED_FIRSTS[0]));
    kmac_test_size_param(&fam, 40);

    test_report_pass();
    return 0;
}

#include "test_common.h"

#include "test_kdf_kbkdf_common.h"

#include <openssl/evp.h>

/*
 * test_kdf_kbkdf_sha1 -- SP 800-108 key-based KDF (KBKDF) through the
 * EVP_KDF public API for the SHA-1 HMAC PRF, one binary per digest (the
 * shared KBKDF machinery, the CAVS vector provenance, the parameter naming
 * quirk and the distribution decisions live in test_kdf_kbkdf_common.c).
 *
 * Known-answer provenance (SHA-1): the CAVS HMAC-SHA1 r=8 counter vector
 * below is a NIST CAVS value as distributed in OpenSSL's own test data.
 *
 * Parameterization notes for this digest: the monolith verified the switch
 * and feedback KAT values for SHA-256 only, so this binary checks the same
 * switch and feedback behavior for its own digest by difference and against
 * a reference derive (see test_kdf_kbkdf_common.c); the digest-independent
 * CMAC-AES128 and KMAC128 PRF checks run in the representative SHA-256
 * binary.
 */

/* This digest's family row: the HMAC PRF's digest param, its prose name for
 * the failure messages, and the PRF output size (the feedback-mode IV
 * length). */
static const struct kbkdf_family fam = { "SHA1", "SHA-1", 20 };

/* CAVS counter-mode vector, HMAC-SHA1, RLEN=8 bits (needs "r"). */
static const char KSHA1_HEX[] = "00a39bd547fb88b2d98727cf64c195c61e1cad6c";
static const char ISHA1_HEX[] =
    "98132c1ffaf59ae5cbc0a3133d84c551bb97e0c75ecaddfc30056f6876f598030"
    "09bffc7d75c4ed46f40b8f80426750d15bc1ddb14ac5dcb69a68242";
static const char OUTSHA1_HEX[] = "0611e1903609b47ad7a5fc2c82e47702";

/* This binary's r-width CAVS row. */
static const struct kbkdf_r_kat rkats[] = {
    /* CAVS HMAC-SHA1, r=8. */
    { "HMAC-SHA1", "HMAC", NULL, "SHA1", KSHA1_HEX, ISHA1_HEX,
      OUTSHA1_HEX, 8 },
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    /* No verified HMAC-SHA1 switch KATs: the switch behavior is checked by
     * difference over the shared small material. */
    kbkdf_test_counter_hmac(&fam, NULL, NULL, NULL, rkats,
                            sizeof(rkats) / sizeof(rkats[0]));
    /* No verified HMAC-SHA1 feedback KAT: the no-IV first block is checked
     * against a fresh counter derive, the IV must change the output, and a
     * one-byte-short IV must be rejected. */
    kbkdf_test_feedback_hmac(&fam, NULL, NULL, NULL);
    kbkdf_test_negative(&fam);

    test_report_pass();
    return 0;
}

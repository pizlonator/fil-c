#include "test_common.h"

#include "test_kdf_kbkdf_common.h"

#include <openssl/evp.h>

/*
 * test_kdf_kbkdf_sha512 -- SP 800-108 key-based KDF (KBKDF) through the
 * EVP_KDF public API for the SHA-512 HMAC PRF, one binary per digest (the
 * shared KBKDF machinery, the CAVS vector provenance, the parameter naming
 * quirk and the distribution decisions live in test_kdf_kbkdf_common.c).
 *
 * Known-answer provenance (SHA-512): the three-chunk info vector below is
 * CAVS-style HMAC-SHA512, RLEN=32 bits, counter-mode data whose single-
 * chunk result was verified with the authoritative pizfix OpenSSL 3.6.5 and
 * cross-checked under the system OpenSSL 3.0.13 (see
 * test_kdf_kbkdf_common.c for the command).
 *
 * Parameterization notes for this digest: the monolith verified the switch
 * and feedback KAT values for SHA-256 only, so this binary checks the same
 * switch and feedback behavior for its own digest by difference and against
 * a reference derive (see test_kdf_kbkdf_common.c); the digest-independent
 * CMAC-AES128 and KMAC128 PRF checks run in the representative SHA-256
 * binary, and the three-chunk concatenation vector -- whose PRF is this
 * binary's digest -- runs here, gated on the "r" parameter like the
 * monolith's.
 */

/* This digest's family row: the HMAC PRF's digest param, its prose name for
 * the failure messages, and the PRF output size (the feedback-mode IV
 * length). */
static const struct kbkdf_family fam = { "SHA512", "SHA-512", 64 };

/* CAVS-style counter-mode material, HMAC-SHA512, RLEN=32 bits: the key,
 * the 60-byte info (split into 28 + 28 + 4 chunks by the multi-info check)
 * and the single-chunk output the three chunks must reproduce. */
static const char K3INFO_HEX[] =
    "a9f4a2c5af839867f5db5a1e520ab3cca72a166ca60de512fd"
    "7fe7e64cf94f92cf1d8b636175f293e003275e021018c3f0ede"
    "495997a505ec9a2afeb0495be57";
static const char I3INFO_HEX[] =
    "8e9db3335779db688bcfe096668d9c3bc64e193e3529c430"
    "e68d09d56c837dd6c0f94678f121a68ee1feea4735da85a49"
    "d34a5290aa39f7b40de435f";
static const char OUT3INFO_HEX[] =
    "6db880daac98b078ee389a2164252ded61322d661e2b4924"
    "7ea921e544675d8f17af2bf66dd40d81af9388c6537bdee9";

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    /* No verified HMAC-SHA512 switch KATs: the switch behavior is checked
     * by difference over the shared small material.  This digest has no
     * r-width CAVS row of its own. */
    kbkdf_test_counter_hmac(&fam, NULL, NULL, NULL, NULL, 0);
    /* No verified HMAC-SHA512 feedback KAT: the no-IV first block is
     * checked against a fresh counter derive, the IV must change the
     * output, and a one-byte-short IV must be rejected. */
    kbkdf_test_feedback_hmac(&fam, NULL, NULL, NULL);
    /* The monolith's three-chunk HMAC-SHA512 r=32 concatenation vector. */
    kbkdf_test_multi_info3(K3INFO_HEX, I3INFO_HEX, OUT3INFO_HEX);
    kbkdf_test_negative(&fam);

    test_report_pass();
    return 0;
}

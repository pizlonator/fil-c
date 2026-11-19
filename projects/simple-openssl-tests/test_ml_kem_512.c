#include "test_common.h"
#include "test_ml_kem_common.h"

#include <openssl/evp.h>

/*
 * test_ml_kem_512 -- ML-KEM-512 (FIPS 203) tests: deterministic seed
 * keygen, encapsulation/decapsulation known answers, implicit rejection of
 * tampered ciphertexts, and raw private-key import/export -- one binary per
 * parameter set (the shared ML-KEM machinery, the determinism hooks, the
 * KAT provenance and the version gate live in test_ml_kem_common.c).
 *
 * Known-answer provenance (ML-KEM-512), from the same scratch probe as the
 * sibling binaries:
 *
 *   - SEED_512_HEX:     the key-derivation seed 0x00,0x01,...,0x3f (64
 *                       bytes, the FIPS 203 (d, z) seed length).
 *   - KAT_PUB_512_HEX:  the raw encapsulation key exported from that seed
 *                       (800 bytes, the FIPS 203 ek size).
 *   - KAT_CT_512_HEX:   the ciphertext produced by encapsulating against
 *                       that public key with the fixed ephemeral IKME
 *                       (768 bytes, the FIPS 203 ct size).
 *   - KAT_SS_512_HEX:   the shared secret paired with that ciphertext (32
 *                       bytes, the FIPS 203 ss size).
 *
 * The private key is not embedded as hex; its correctness is pinned by the
 * embedded (ct, ss) pair and its raw size is checked against FIPS 203
 * (1632 bytes for ML-KEM-512).
 */

/* This parameter set's KAT constants (the fixed IKME is family-wide and
 * lives in test_ml_kem_common.c). */
static const char SEED_512_HEX[] =
    "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
    "202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f";

static const char KAT_PUB_512_HEX[] =
    "3995815e597d104355cf29aa5333c93251869d5bcdbe487124f602b8b6a66c16"
    "c4761648ad765cf5d8006b515e905a7f0ac076b0c62efa328153e7ca5701699f"
    "1305f1e6bc6f90b0e49b693512b6ce992a8b8016ddfc1a662c7e3f9619cbd869"
    "dd771af30896ccd5918ac6cb77466c5e779996d67ff9aabc97503f2c7b7e2d00"
    "0d86450fb1807ca4cabda465825a31c789a1b7a491ab3872765d320d0b71920f"
    "a213c94093416b83b8124e69f65e62cb5000dcc37aa9a0fff73970c4772f357d"
    "24189ca6f5305568c0e2376a3762a68c605e563c5d209572e0fc7532ca294729"
    "535567b5fc413c5e8792d2464536cc808f98add74664f141566f9016a90a5418"
    "29a98a0464ce41a8bb44c2d4fa3c2c209460728ef14a1a7c4c9b98d12203b4cc"
    "3529160a9ab2d7838f7ff6b53ae05aa31a7d646b7afa6c45932526a3c3755619"
    "be994c211c2a31c05b3447836cb2150be1829dae6b04c5535cff546e392ba797"
    "411720f924f490a5ac5495f21356d550b782a64c1688b6b655bcc7842197a434"
    "c2f6563b5b7f09a78bcc488232783561d16f4cbab6755400050781570c66604b"
    "817ad1252294736e8b01861a4b5a74519b8b6fe51489a5072392e587626c7137"
    "76575d33806a1c8e2732af97c2680f51666331c4eb8bbc0431c4f96832daf1b3"
    "c45528fba153f6c78b1c198702947ccd337727a46fb53ba11de5cb4191346859"
    "516cb6ad72400f3cf209b236aef35a580ac87eb3e30fafd66973ca8a7dd2675a"
    "f41f7a17b61433cd1af80f7708869f665488497980b1ac10a0cdcb636a00ed86"
    "81b35e429124ca80350725b85f83a5eac3a4a3cc1600903e65293560b9b336e5"
    "af0d529dac1a048119302cb7a9bcc110b94851bf02117f199dc485a852b7473f"
    "09b831a6831d5b54c0b790d225cf6bb92d9462a26cdb33dda5123c7aaf0e26a0"
    "b83655eea28bf3a8074725018fd6bae4b601cf61baab71a7a3d35197a343e74b"
    "4a272c125d540896426d85b7958d3b38a6ba987ec37225c7b44cdb12dde4539b"
    "4ab082363683f04bf7a09cc5c41dfe830a1b162e0b324334362f084a14467723"
    "344badd000f8d8c537c48f998f05307cebd1ede0b81c3bc59a065a1b6d63b26c";

static const char KAT_CT_512_HEX[] =
    "d56f870a87d0da36e7a120f3d7e7947da9a63a53d4f99bad029beef9a90001b8"
    "a4e9378d8db946cbaffcd14669dcf1a188ed74cea4edf34def6577e2755fa7d2"
    "60d81c4938c5f0bf92b957ecc086253e1b5ef6208e3d01ffe2d40a689ea706ea"
    "f96e4e75adf2f7bb1df97a3e254574017688d0a4493c434a4b9fab54059c75f3"
    "a0ac772178637a231c5b176bb365c32a88a8769f2fb0408c31f62aa7955f0f28"
    "c695b3a06f8791319c0a22d54b1fa988c42dd3210c41dbf2bdddb8b4714680f6"
    "b6e5d544039d96dd8cdaeb3bd1e64e9433387479adacecf61d1f097012c7d1fd"
    "d4442130866a6212408fb40bb346df14ed9362303c3c7a1eea891251dd11ebf4"
    "f1e62d20f1d6897daf3f1f10a55d818d10b5da8b4bb5a9105b04cdacd949e0b5"
    "5ed4c0262078bfc916460ac646bebaebed3e4ecf66363e47220259eb9e5f7b40"
    "43df71732c61b2d968922cf4af28a9d5bcfd965cd9f17491ab48f6330612b9b2"
    "7a0e3413073540c1255079bc0480361f9ecdbf9348e3ddc6baff4823e594534e"
    "3c32f46309c52a7ce0bb64f70bee42010f970e22cf1b72775509ed664b53bcb2"
    "a7f6d771930a460ca05608f7ef421c094507d54eea820d93ea3c5bd65874e856"
    "aee57dcb781c76ef10585e519a385762fc8ee24eb4684c419a84cb42b5c76281"
    "03924165b0958ff766a1d697a309f243dd4e8104027436ffd014c52e5d555a04"
    "432956077c409b4ff754c6854165e12471999b89166346b6dc4044cedc885c49"
    "902f0598ad887332a0a1f03f2ff985bf0b01f4cac8d3c78a6949c9cebcda0679"
    "d150e6c65286a71ba8109ceb575d99499b33cc2d73d7f2d18f44688416a4cbdb"
    "4fac57fd6dfff537ad592b87b429bcfc93696cb78fac5dd1c44116fec0d83666"
    "37e3d19a8a836926ed0e474a9e76184124745eafef918b384530288152f4bea2"
    "c25ae2add04b70cd6c302c1138b8a2c9a9c4945b7ee73ca9465e847e0d19dd96"
    "af02b2b6c1ffce9ad8f6c2c064b02a45cc16a02d8c7d9bbac4f5c190c934cc30"
    "d20d29de7369e738dfe35b9bbf752a087dc2206987b78db778e40a03c517bc98";

static const char KAT_SS_512_HEX[] =
    "ee5f8f90fb6f15a5934504e1f65c23ad2d60964104bf42463876363a799dee4f";

/* This parameter set's FIPS 203 row. */
static const struct ml_kem_set set = {
    "ML-KEM-512", 64, 800, 1632, 768, 32,
    SEED_512_HEX, KAT_PUB_512_HEX, KAT_CT_512_HEX, KAT_SS_512_HEX,
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    /* ML-KEM is version-gated (OpenSSL 3.5+): if this parameter set is not
     * provided by this OpenSSL, take the SKIP path and exit 0. */
    ml_kem_test_gate(&set);

    ml_kem_test_seed_keygen_kat(&set);
    ml_kem_test_encap_kat(&set);
    ml_kem_test_tampered_ciphertext(&set);
    ml_kem_test_privkey_import(&set);

    test_report_pass();
    return 0;
}
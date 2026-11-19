#include "test_common.h"
#include "test_ml_kem_common.h"

#include <openssl/evp.h>

/*
 * test_ml_kem_768 -- ML-KEM-768 (FIPS 203) tests: deterministic seed
 * keygen, encapsulation/decapsulation known answers, implicit rejection of
 * tampered ciphertexts, and raw private-key import/export -- one binary per
 * parameter set (the shared ML-KEM machinery, the determinism hooks, the
 * KAT provenance and the version gate live in test_ml_kem_common.c).
 *
 * Known-answer provenance (ML-KEM-768), from the same scratch probe as the
 * sibling binaries:
 *
 *   - SEED_768_HEX:     the key-derivation seed 0x40,0x41,...,0x7f (64
 *                       bytes, the FIPS 203 (d, z) seed length).
 *   - KAT_PUB_768_HEX:  the raw encapsulation key exported from that seed
 *                       (1184 bytes, the FIPS 203 ek size).
 *   - KAT_CT_768_HEX:   the ciphertext produced by encapsulating against
 *                       that public key with the fixed ephemeral IKME
 *                       (1088 bytes, the FIPS 203 ct size).
 *   - KAT_SS_768_HEX:   the shared secret paired with that ciphertext (32
 *                       bytes, the FIPS 203 ss size).
 *
 * The private key is not embedded as hex; its correctness is pinned by the
 * embedded (ct, ss) pair and its raw size is checked against FIPS 203
 * (2400 bytes for ML-KEM-768).
 */

/* This parameter set's KAT constants (the fixed IKME is family-wide and
 * lives in test_ml_kem_common.c). */
static const char SEED_768_HEX[] =
    "404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f"
    "606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f";

static const char KAT_PUB_768_HEX[] =
    "8c3295d5ab9b708714c0d0792b1b1d2765affa8a09c2bc1c6b467ca534bc1fe6"
    "9b5a9a026fd80f82d69ce780626913918deba94bb208b61974f0d566f9c5b490"
    "164031773ce192b9be57c24dc995d560726f7961a6c0929ec4acc501c27b404a"
    "c2f3662d39a7fe5ac6b2d5ad73575810c97b4db0afa456c8d7d8bac6f26dce18"
    "b833553d5a7061eff8155be437aed3bd029625758bade7910b868167636ab8e2"
    "002ff7a508eb90644710008b5ab940d3bba87cb153e2b2f5d0cdda9c0dd7f66e"
    "c9f541a2d69045a63f59e13f2a8944f338b6ae0b72e0a6b51a060258798057db"
    "66df3ba3db876312a23572e7b948a8ae29f27a33f2805bb623a3022363233a42"
    "a16ac4bb3a9dc18dd1e7ad426c9919c555a56515eff0a689d468e24c2440589d"
    "94f569caea7990e1a719b019efc132ed31105e7135659669fcaa77624b79ce1c"
    "bd97f5cc85b590fbd6c922a52eb4a1c468d67f68c40a54260dbd4703e65372dc"
    "461752117264938d52f4971ef18a3f44b1f9a183493a509bbcbf57c63bac8b9c"
    "9de55f2255543b9c413a87941d0487c68a90a5ea662e912be876bc7579431066"
    "68fcb8bdd1cb628591ca2c86bd2ef839cd5405894bb068388073735da2172155"
    "6ab3fb3294dd632660056d9d970e1239945fb755b7429320f35cb9933c91b603"
    "b5591aaf8893ee0c4462180af3b2130401824dfb04bd3b0c1d9326754b5b0477"
    "0990f31cfee791bd0bab6eb7b8718955e720b02a13783c9293a723560f22c65d"
    "6173ccf967b29cb6603a450d13af1c872274992200d5639cdc19abe021ca849d"
    "60cbbe12b51de7d8cd89ecc565108fdc362a92e512d0f49c6e97beebd1c62525"
    "b8aa35290b9374aff3785af536038b1a6f3ba3e83b046353bca375b98517350d"
    "d3389205075d160de9678417c47da24094bb0a9af7f0916609bca260ba69ea4a"
    "01c0ab3f133d85483624311cd264cc5a0cb997272f30cb0b0583668b7c7391d1"
    "04f94265bab2b8a2e3b51263675a322256f04cfb170b70d162bbc601eb0c504d"
    "4bb90e26b1efb4afe50017a124ba919b297cf97dd3d24ff258657cd9cb121c3a"
    "ead01b92fb71db0c83775522a741077fb46a59ec338fcb0bc292383edc742a40"
    "5a7732b8e89a1894d06184f1134d630aac7b303a7cc9aaf62adfcc275b826995"
    "946c18c67ddca34318cb4526ea60d8982c516c2246d61b93e9781b16a46a3756"
    "0b409aae0ca4a59bb7299b6dbcb52c592256e5c36002bb37ea9c6401e4c20bab"
    "9579a06893e583f7b87ecf4c77fc3a9b288752bf030cf7e8305b8a1d710166cf"
    "388d2c2a7934704e5bf107eb6a0ffef191a4a39a1513824272ba139ac555cb3d"
    "ca2039e973aa2e296afb93788e2389fe7cb51ef671db370ba8ba35ccb55b1486"
    "25900c6aee3985df7b3fd3b4b14a23290f87cd1923223481a444b03e4b318a33"
    "d3499dc719d0d247f9a84a289cb74ce290cc682e65632481709ccc5bcf56595e"
    "dc7919bd2884658625a48b68733cb321666e468c48081667725c9a22fb4bc9b8"
    "848e123454254455c2cdeea0095513b2c8fb07dd8b4df5e2571a1b5ad431bd65"
    "083a050683accb559b1cc1794a76bc3586f7e18fcc6aabeaec06a581175b6850"
    "7444a85bf3292b276879ef19af6f1f7a7eee048be89994384927029f752f99fa";

static const char KAT_CT_768_HEX[] =
    "cfc9cdd7ed37610fda7a8dcedc9595661f01523befe4cbeed2256026472bac08"
    "622ec60edd3a93cb6229e73934fde9f4329ca74ebb958316a6c1ddd5b2e12275"
    "895216e3cbd53d68895bd1e981955120665f63c386b2abec8e0980ba9648b4d9"
    "6e8dabfd4d7324b1abff1774e4ae1d255760ad8cd39e066a684df62f562607b6"
    "e583718bf8a30dc643aa5776d994c11f778b07c7e6079f142d5672e378180ff5"
    "736e096ff5a3cc635a3d7dec5c5eeac0ac1a25f93da54b84f99990d9a03d6507"
    "d07283e2995a467f8940f106588fd0112ff7bf13089fa4b424f8fdbb186cc21b"
    "d0f7e85f79722c79e19ebfe693d6aa8558f9c228cf1c36afad8f75517afef3fd"
    "fec996b02acf4da0f8ed074eec0a5900271d7c6b5feaafa2de6034e4ba0bec1c"
    "8b5b5bbaa03d748c86f43577446c581865b89abce2f36da31783621c1d4a9b51"
    "f1064eb5b35b76734907f4f3e027e97a523372b3af020d34a71c05ba494017af"
    "e6ad5ab5dcd228b62b4efbb3ec21616290b2643ce44121fa96284fc1cfc0f122"
    "eafb926620c4582bbfbd3e23bd09f99906fd6df360e97422a1ab2d4c9b18109b"
    "806f4043289d886248a1105d5561fe1ea08d5b22b5ff494396ff7c17bb6d19be"
    "4925e19bd6ce2144e30a1bef23b4c72be5ee310b6892377c519d5c73506be2c3"
    "75919b0bf4647fda20656094823fb274ba01b7f55f0d02ed0cccb80da7cee3bf"
    "dc6d058c87751c91e2e039608db95483ee66256b6eb3c3396bfa280dabf44c4a"
    "cbd6857820f7932c45e04155ec2e1e6f9cdcc449ee2ba173f5da4d094559af47"
    "8e4e49ecd9b793012e8beab96ab6c3918d4328c71bf523b9855ff0cabdabcf8f"
    "02006779e0d94b8f0fdbf849e0f439b1292f334c1f7e0a4008cb9808974b9928"
    "33a41e0196dc54a4285da60716b1f97f9fc978461470a8141869f311724208f1"
    "482f79eef817793649586bc11f8f1014763a17d2f1de6f6b9d67eed2211cfc54"
    "0e0511dab95659204f0fac9bf8703e9b1f031e9a13970a86d979447049e8b199"
    "0e47914d9078908031683fe8fcab84450b635fdd2de739eea66e5e18d722d7ed"
    "654d6a78359b310dec405bb6d59052c8480e185099dd6dd14777ef16aceeb47a"
    "b2242be3e49f1bfa687e8456a0fa52a44aa7c7703bf5933816c8f206a512c569"
    "5ac323fa8c359e42c45c5f108e3cfbf3d0238eb6d56ff9e9f8de7fa66707111f"
    "db4d0169a763f5596b2d9b698d649e3180b0c44055409209fff6bbfe4eecacb1"
    "0e56b293e47669f76044721e03921c6534a4fd1c2376e0915da46aeeb4eb0d1b"
    "589279c773e29acff81d72be038e025d34e3bed715aba056850e758641ef369f"
    "fec74557d323887e15cc7b171b461900b6e4954a256acf1e1d0b73bd50ebdee7"
    "5f12d417ba03a90658a9beabe7da5c8b16710b9e55784176afffc76a5b19f7f8"
    "522eef9fe54757a3f56fb2ce1b2aef933e8d629de9b8cae44509e4cacf456387"
    "6669d5837a1ea928cfd499c28a909d8fac17338f077a43bb5d639acc1f037cfc";

static const char KAT_SS_768_HEX[] =
    "b1dac4eb1ef2322eabc817f512d0f2d92c9ceb9febed6dc8b14c00a18b98908e";

/* This parameter set's FIPS 203 row. */
static const struct ml_kem_set set = {
    "ML-KEM-768", 64, 1184, 2400, 1088, 32,
    SEED_768_HEX, KAT_PUB_768_HEX, KAT_CT_768_HEX, KAT_SS_768_HEX,
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
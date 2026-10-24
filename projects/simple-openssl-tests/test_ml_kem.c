#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>

/*
 * test_ml_kem -- ML-KEM-512/768/1024 (FIPS 203) tests: deterministic seed
 * keygen, encapsulation/decapsulation known answers, implicit rejection of
 * tampered ciphertexts, and raw private-key import/export.
 *
 * Determinism hooks used (all public OpenSSL 3 API):
 *
 *   - Key generation accepts the FIPS 203 (d, z) seed, 64 bytes, via the
 *     keymgmt "seed" parameter (OSSL_PKEY_PARAM_ML_KEM_SEED, new in 3.5);
 *     identical seeds derive identical key pairs.
 *   - Encapsulation accepts the 32-byte ephemeral randomness via the KEM
 *     "ikme" parameter (OSSL_KEM_PARAM_IKME, new in 3.2), which makes
 *     encapsulation deterministic.  The PQ-specific parameter names are
 *     spelled as string literals because their macros are absent from
 *     pre-3.5 headers; the program still compiles against OpenSSL 3.0
 *     (and takes the SKIP path there).
 *
 * Known-answer constants below were derived and cross-verified with a
 * scratch probe compiled against the authoritative pizfix OpenSSL 3.6.5
 * (build/bin/filcc), per README house rule 7:
 *
 *   - SEED_*:    the key-derivation seeds (0x00,0x01,...,0x3f for
 *                ML-KEM-512, 0x40,...,0x7f for ML-KEM-768, 0x80,...,0xbf
 *                for ML-KEM-1024).
 *   - KAT_PUB_*: the raw encapsulation keys exported from those seeds
 *                (800 / 1184 / 1568 bytes, the FIPS 203 ek sizes).
 *   - KAT_CT_*:  the ciphertexts produced by encapsulating against those
 *                public keys with the fixed ephemeral IKME (0xa0,...,0xbf)
 *                (768 / 1088 / 1568 bytes, the FIPS 203 ct sizes).
 *   - KAT_SS_*:  the shared secrets paired with those ciphertexts (32
 *                bytes, the FIPS 203 ss size for every parameter set).
 *
 * The same probe verified every behavior asserted below: decapsulating a
 * KAT ciphertext with the seed-derived private key yields the KAT shared
 * secret, a raw-imported private key does too, and a tampered ciphertext
 * decapsulates without error but yields a different shared secret (FIPS
 * 203 implicit rejection).  Private keys are not embedded as hex; their
 * correctness is pinned by the embedded (ct, ss) pairs and their raw
 * sizes are checked against FIPS 203 (1632 / 2400 / 3168 bytes).
 */

/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */
static void *ALLOCATE_KEY(size_t n)    { return poison_alloc(POISON_KEY, n); }
static void *ALLOCATE_INPUT(size_t n)  { return poison_alloc(POISON_INPUT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }
static void *ALLOCATE_OTHER(size_t n)  { return poison_alloc(POISON_OTHER, n); }

static unsigned g_checks = 0;

static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_ml_kem: %s\n", msg);
    exit(1);
}

#define CHECK(cond, msg)                      \
    do {                                      \
        if (!(cond))                          \
            test_failed(msg);                 \
        g_checks++;                           \
    } while (0)

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Copies a hex constant into a buffer allocated by an ALLOCATE_* function
 * (house rule: constants live in the source as hex strings, but are copied
 * into labeled working buffers before use).  Returns the byte length. */
static size_t from_hex(const char *hex, unsigned char *dst, size_t dstcap)
{
    size_t n = strlen(hex) / 2;
    size_t i;
    if (strlen(hex) % 2 != 0)
        test_failed("odd-length hex constant");
    if (n > dstcap)
        test_failed("hex constant larger than its destination buffer");
    for (i = 0; i < n; i++) {
        int hi = hexval(hex[2 * i]);
        int lo = hexval(hex[2 * i + 1]);
        if (hi < 0 || lo < 0)
            test_failed("bad hex digit in constant");
        dst[i] = (unsigned char)((hi << 4) | lo);
    }
    return n;
}

/* Length-and-content comparison of two working buffers. */
static void expect_equal(const char *what, const unsigned char *got, size_t gotlen,
                         const unsigned char *want, size_t wantlen)
{
    CHECK(gotlen == wantlen, what);
    if (wantlen > 0)
        CHECK(memcmp(got, want, wantlen) == 0, what);
}

static const char *const KAT_PUB_512_HEX =
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
static const char *const KAT_CT_512_HEX =
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
static const char *const KAT_SS_512_HEX =
    "ee5f8f90fb6f15a5934504e1f65c23ad2d60964104bf42463876363a799dee4f";

static const char *const KAT_PUB_768_HEX =
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
static const char *const KAT_CT_768_HEX =
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
static const char *const KAT_SS_768_HEX =
    "b1dac4eb1ef2322eabc817f512d0f2d92c9ceb9febed6dc8b14c00a18b98908e";

static const char *const KAT_PUB_1024_HEX =
    "3b5a9e810b752ddb869f2a75eeaba8fd12ae67e6702bb12c6b6a48cd65915ab4"
    "4d84d74f5d9b0a6179c4f8d795243932e5b3298e6043bbf21d7746cb94d04696"
    "2104e99555d653c5b5fcb4fbe072c6708d53b479e8a12400e3889a38951ecc86"
    "0f241aaf46266bd535de7b0fbd677bc4ec7497462687060eaf179d81a965079c"
    "3635c52c6795915e634e42ba14373c5d5f22bf8cd54ad7d94cf71a528d06c6aa"
    "5aabaa4a4dab217aa71b6d95d4074c32899066296f21b65b6038e9761875d52e"
    "d271ad127611a9f14eab1558bfd5a9e4b6849b54cedd5a2afbd6518af6ca5130"
    "0ee5f1bbeb7786e69c7cec338330e29c32a50c660ba44b773dff764c33f3505e"
    "502be0ebca96790b46fb40527129766802e5082ee8877437945e234cb1a39888"
    "3a98b1e70421bc4095e8a349010c12efd06d9b76ae67aa3e4835709014494318"
    "172b0c197718074dea49192a717106c862c854e4519727f7bcb40bb365824031"
    "d88bce84835eac834288a0b95366ecf859871845e587b33078053a0571caacb7"
    "3e2c173f7b5a8c05b3d5f874e6eab51943bee83b25d4bba8bf9a44eb90c26063"
    "99016c21c81cbe0415aacba08f4392a5761b423d0b03a3b15109e76dddb1865f"
    "45446bc914650353651b6228592c7fb515cc08ae794a44cb0b001f1c5c3392b2"
    "80e185321c1598ecaaeeec18d8662b8eba72272bb63f173d33c67dc0814bc9a2"
    "881258b60928274ba055a0e587399199927b1e76761b49440c891acb24db1680"
    "7b5872ca18ebd40de4288914e68a9c207e3285a57aa61ed8491a18681749e326"
    "dc20a20d552eeb532289cb0587e6b33071441f65b2636a332c32c4ca217557e6"
    "2dba7657aaf50ec063ab0d4c50fdf45abaa7076dc7595b4108ff8735c97880fd"
    "e34369829e9aa120ab564eef42b455b2787097bc262a5054529044373a76f76f"
    "07f40f28eb0bd9f1afb343beb22ba17b7aa59ab219f1f7cd6640a027620dcb7a"
    "45d8b19db123ab934aa3e9a22440723e61cacb4e38626f5b4fe1f041e5c5a923"
    "fc8041b36314611d862c362e2b85bc277897271e53b70badc08313b0c4535a6c"
    "40b7b4e3573bb9a08723c10bd0e748a6d45ad02022cb53194b814c07068704d1"
    "0d403b5122f98a3062628aa9336ae252a296a8ad9061957378deb99d3895a8a9"
    "242c0d14658143c9d34466fc4474ffeb28e3b89eadb76d287b6908cc8e9e8176"
    "4a668f3c0990699037d0d247d6ec4a04b8267e63327ae8567ada2e06e2b9dc91"
    "151c52c283e51a55e1887c69c463fc534a128fd6d55d8d6557f61b5ce3b1277f"
    "d53988fb240544bb668a29c6e9bbaa354fb72a8408f69a28278585471c87b513"
    "949a96ff87857c6a39fd173c1e782a65e1251dab21cc09725a2a9cf0844b2eb4"
    "05cde1022fa022208671ede258afe796a4c71c2a718aad26bfbfd09607416349"
    "6a8f5ed1416ad332fd31cb1b4ac218c74976b34e9af929088816dad887f6155b"
    "debaa282808ebbe754551084f1f47547308b66437ff8fb0871c75c6f27ad0179"
    "80fe9c2442f4c71c63ad542681bc263bd0820ba3fa0c05097ae663b4d48488ed"
    "706382a890a8a157deba58b9f59f0a30748b292c35a040ec9b0c30a05db18265"
    "a79989f2f72089041e9445b1e1737fdc206a75e648dd1b25412238a3581e2613"
    "99e51a71527c254bdc253c82af8c0c2a71d553e9aba02eb850c5e658742b02fe"
    "c6413bd94d26d41b34eb7bed821f6ec27f119907a6088cdd8c898b11100f9065"
    "4af6b64f76b2ef7389838bc35f81763af4778301a78f799a91ca1f82a0c70ab1"
    "75e5cb3c2ee31f84d6c78baa02051a38746c057b36735f9c62e1c790da7a857f"
    "368230662c19a98cd2d58b6abb010211b947298f421a0f7bd05d3a940a65f0cf"
    "895ca75e145d9e25639fdb2a1ba7397998b85cf75114203219895b47d25be6a0"
    "69c1a91ceb225954676925ea2c3fb26776dbbc23e38476ecb146d7507c8542c6"
    "480e7c850bf83aa1dc062a34441134fb837e2c06ff1190a95c6262f971ba2139"
    "fe920ad3ca2eb9d77e666402b3047ec0231548b04eae7a800d3470fb95700f59"
    "b0db125402998432a08f12f81f307422d04336ff1a66e5284d18a9945ee1ce3c"
    "d500127b7011c791fa04c5ed90ba7c90a728c955ab442aecd3ce3c0c8cb9787a"
    "0aebbae06f27696726ff083f8bde5c026fc329fb5156214cab0ded86d91d8c33";
static const char *const KAT_CT_1024_HEX =
    "bf3360b50390ae4a5fac6a48cc714a3cdd38a351f932dcea571d330f5bdf550c"
    "3bc927ee69d853d26393a2384784cd534466d01406deeb8a7290f2d79e28e3e5"
    "3c4126cc4e33703febe69de23bb582db63dd3fbcd26274a2ced48ef7eba8fd63"
    "884d4b67ed53311e99038ab318265eb52d1d7e30a1bfc63be3d0aa5e943bc126"
    "4ebfed38fe61a95a0399e9141aa88436a6511e02253105b6a828a5c4944965c7"
    "8450b2bbc5e4f09f8ad516d12aeed7d746c0994464f03035895ab6f105ba679b"
    "fc35c6ac1966a750fb74a8603ba3f673d75577f6804f2e845d125e81de43c008"
    "36a239d8f943fd416e1ecc5286e8db257d8e6c615e4fd149459d41f969672288"
    "e010acfc4684c37dedc2249102eae904c33bc3351e610945b902ff95f6921fa9"
    "85766e9eef3df25672e2633bd226ee3143919b3620d1b31f131258c69198697f"
    "17c9882195d61dfc70da531ff15a01166e330c9a28432318a61f8627fda652e1"
    "eddf1457a4f30c661bb0a17b175afa83175b5e90a281c685746c796f57563d81"
    "eb5fa5fd702f90bb4e8109ed671e59f9d798f35d6ed97b2d760149925656d349"
    "abccce6c104d9ad1970605f5ca5b87cd7f8c3092aaa6646ff8f16096fe8a5b43"
    "56f074245c1f8ff545bebd9b5905d045123dd8c13be8edd37c200e2e6a8786ce"
    "015722a286b0cb034476b6ea3dff5fb7a9c71554900ecb5f3aeec04e1771c361"
    "b92399c3e5ccc1fed0a32c4eae88678b84096e18596dc6d8d658bf03881345aa"
    "8a33e3ab50fb82d4595269b0700fb6e537d5e02c9f4e0e9f7747bb59c3a231d8"
    "f5f433688a83605f89f6577ceee9f79f70e93248371ccabefeb95980a2a8232f"
    "27e6a83afcdd849de7490ab08fa406ceba76c7ad2fa141f1a9c42aa307aa9c07"
    "123826158af86ba756cbaa9cf67f802ee0f9d51e546422e81498c74dc68eed3b"
    "3c457b2c8d68e91a59f4e64d449bb702cbac5c5e3b2c727577122efbb991bd73"
    "3988bb921e2f63c9fde8325146c0ee52ed4b282fbaf3ee473e13c24f7405e058"
    "9783e5f6a645e79628f546357582a84b8368c97a10fcb425f20ff74f528d30f8"
    "f1686f86bfa483bd6245402cd5c25ec902bbb330a2d922fc5670c55f277afbe9"
    "f5c58fc71fa1d10fb70e78a1003019cdd738aa63d90a9ebbc5f35c389260446c"
    "8c03cf28ddd26c1cad599de39cd58bfc94005a57cc4aaaf2bea159dd2ade6a29"
    "86a12971e7ca83ef19cc7dc8fd429116d618ddc4266bddd837152290083c7a71"
    "2a45c0ffa136b5ec25e2e3dd1b4a85973d0aa388b701645fb74b702f4b67ca67"
    "c6d9cf1951dd7099d84b9c0190632b41a024eb74b98450c2f80ebb24f4991cfa"
    "aa725d10ceedd7d1f94b5f1e02abfcfc4ffc6abb832f7417637fe8c9000a0393"
    "c489eaba5f046154253fa6a3e814b3a9c2ed34153e0898a9e3154b36cda74b3c"
    "d2bfe7b44a6fbcce79ad5eec34279a973507dadfbd7ac368399c83bb10a030c0"
    "f62691015d7eb4a7f9788c5dd56b3047bd774471687e31f266602dff387081cd"
    "43229867d20ab9c9f421f93bf85db62339d99f55494087516ea46028fcd39014"
    "6d27924087df628497b89e69f2770a2539998ec330b25ea7005c2fdd2900d678"
    "28503565f077410e40a342c4d7fce4d63283b04c72879625912a36e5bdba491c"
    "b8f0531b8478adb224c2af389ce3d01c7dd3a0138d8388f5cc798fc11c7f93ed"
    "401a1c441f58d501a9e5f1ae3da2f719142aa3d2bad9b3e8f7ea4541e01f518e"
    "0abbddad6c9440d7ce46a88ccd33f590ec66885cdf3cb3e43add0b96d9fd3563"
    "433dd661a18a5beae9f5b688a9fef5b21e01ebf61a3d6f6e8543f1a88be9ac81"
    "02e30db16e85dcb4f836b8677d0d009fd2cfbcb06cc0cb7b3262694766530fae"
    "e4e9b3e2f40ca50d2993d466fa78c86c4708863d04b9be728e36d8c57a8027c2"
    "5a1b6cad593adb4603985184d67171a7ecae1105924797cbc89ffd7531a5354a"
    "e60508bc8bbd9f75b9f856b2cba654fb5f228a4a4b6ec097154f735879972bbf"
    "b68947c243c75ab5e56504c8ad84250e1ee5db3e273ea22240952600159d404e"
    "273fdf4659298c68ad773de9fbf8cb350ee4be057c34e194c1332cc2fbc7d046"
    "d8fd9b3cbdb09bb96b1304f63a7903ce35a3ed9ffb7f46e7001357f5aeeb2865"
    "005a0669cf54c3a2db3f9aa06415c50d3c5dd8f635f9c929903bb7ad43d84351";
static const char *const KAT_SS_1024_HEX =
    "a1b75b4f796679173738226a65abdb0a360a7b027503dbb39f9af37be149627f";

/* The FIPS 203 (d, z) key-derivation seeds: 0x00,0x01,...,0x3f for
 * ML-KEM-512, 0x40,...,0x7f for ML-KEM-768, 0x80,...,0xbf for ML-KEM-1024
 * (64 bytes each, the ML_KEM_SEED_BYTES length in every parameter set). */
static const char *const SEED_512_HEX =
    "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
    "202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f";
static const char *const SEED_768_HEX =
    "404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f"
    "606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f";
static const char *const SEED_1024_HEX =
    "808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f"
    "a0a1a2a3a4a5a6a7a8a9aaabacadaeafb0b1b2b3b4b5b6b7b8b9babbbcbdbebf";

/* The three FIPS 203 parameter sets. */
static const struct ml_kem_set {
    const char *name;
    size_t seed_len;    /* (d, z) seed: 64 bytes for every set */
    size_t pub_len;     /* ek:  800 / 1184 / 1568 */
    size_t priv_len;    /* dk:  1632 / 2400 / 3168 */
    size_t ct_len;      /* ct:  768 / 1088 / 1568 */
    size_t ss_len;      /* ss:  32 for every set */
    const char *seedhex;
    const char *pubhex;
    const char *cthex;
    const char *sshex;
} ml_kem_sets[] = {
    { "ML-KEM-512",  64, 800,  1632, 768,  32,
      SEED_512_HEX, KAT_PUB_512_HEX, KAT_CT_512_HEX, KAT_SS_512_HEX },
    { "ML-KEM-768",  64, 1184, 2400, 1088, 32,
      SEED_768_HEX, KAT_PUB_768_HEX, KAT_CT_768_HEX, KAT_SS_768_HEX },
    { "ML-KEM-1024", 64, 1568, 3168, 1568, 32,
      SEED_1024_HEX, KAT_PUB_1024_HEX, KAT_CT_1024_HEX, KAT_SS_1024_HEX },
};

/* The fixed encapsulation ephemeral randomness ("ikmE"). */
static const char *const IKME_HEX =
    "a0a1a2a3a4a5a6a7a8a9aaabacadaeafb0b1b2b3b4b5b6b7b8b9babbbcbdbebf";

#define ML_KEM_MAX_PUB  1568
#define ML_KEM_MAX_PRIV 3168
#define ML_KEM_MAX_CT   1568
#define ML_KEM_SS_LEN   32

/* True when this OpenSSL provides the named algorithm (its keymgmt can be
 * instantiated and key generation initialized). */
static int pkey_alg_available(const char *name)
{
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(NULL, name, NULL);
    int ok = ctx != NULL && EVP_PKEY_keygen_init(ctx) == 1;
    EVP_PKEY_CTX_free(ctx);
    return ok;
}

/* Deterministic key generation from the FIPS 203 (d, z) seed via the
 * keymgmt "seed" parameter (OSSL_PKEY_PARAM_ML_KEM_SEED; spelled as a
 * literal because the macro is new in 3.5). */
static EVP_PKEY *keygen_from_seed(const struct ml_kem_set *set,
                                  unsigned char *seedbuf)
{
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(NULL, set->name, NULL);
    EVP_PKEY *pkey = NULL;
    OSSL_PARAM params[2];

    CHECK(ctx != NULL, "EVP_PKEY_CTX_new_from_name failed for an available ML-KEM");
    CHECK(EVP_PKEY_keygen_init(ctx) == 1, "EVP_PKEY_keygen_init failed");
    from_hex(set->seedhex, seedbuf, set->seed_len);
    params[0] = OSSL_PARAM_construct_octet_string("seed", seedbuf, set->seed_len);
    params[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_PKEY_CTX_set_params(ctx, params) == 1,
          "setting the ML-KEM keygen \"seed\" parameter failed");
    CHECK(EVP_PKEY_keygen(ctx, &pkey) == 1, "EVP_PKEY_keygen failed");
    CHECK(pkey != NULL, "EVP_PKEY_keygen produced no key");
    EVP_PKEY_CTX_free(ctx);
    return pkey;
}

/* Exports a raw key component.  Returns its length. */
static size_t export_raw(EVP_PKEY *pkey, const char *param_name,
                         unsigned char *out, size_t outcap)
{
    size_t len = 0;
    CHECK(EVP_PKEY_get_octet_string_param(pkey, param_name, out, outcap, &len) == 1,
          "raw key export failed");
    return len;
}

/* Builds a peer EVP_PKEY by importing a raw encapsulation key with
 * EVP_PKEY_fromdata. */
static EVP_PKEY *import_pub(const struct ml_kem_set *set,
                            const unsigned char *pub, size_t publen)
{
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(NULL, set->name, NULL);
    EVP_PKEY *pkey = NULL;
    OSSL_PARAM params[2];

    CHECK(ctx != NULL, "EVP_PKEY_CTX_new_from_name failed for an available ML-KEM");
    CHECK(EVP_PKEY_fromdata_init(ctx) == 1, "EVP_PKEY_fromdata_init failed");
    params[0] = OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_PUB_KEY,
                                                  (void *)pub, publen);
    params[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_PKEY_fromdata(ctx, &pkey, EVP_PKEY_PUBLIC_KEY, params) == 1,
          "EVP_PKEY_fromdata of the encapsulation key failed");
    CHECK(pkey != NULL, "EVP_PKEY_fromdata produced no key");
    EVP_PKEY_CTX_free(ctx);
    return pkey;
}

/* Encapsulates against `peer` with the fixed ephemeral randomness via the
 * KEM "ikme" parameter (OSSL_KEM_PARAM_IKME; spelled as a literal because
 * the macro is absent from pre-3.2 headers).  *ctlen and *sslen are in/out
 * capacities, per the EVP contract. */
static void encapsulate_with_ikme(EVP_PKEY *peer, const unsigned char *ikme,
                                  size_t ikmelen, unsigned char *ct, size_t *ctlen,
                                  unsigned char *ss, size_t *sslen)
{
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(peer, NULL);
    OSSL_PARAM params[2];

    CHECK(ctx != NULL, "EVP_PKEY_CTX_new failed for the peer key");
    CHECK(EVP_PKEY_encapsulate_init(ctx, NULL) == 1,
          "EVP_PKEY_encapsulate_init failed");
    params[0] = OSSL_PARAM_construct_octet_string("ikme", (void *)ikme, ikmelen);
    params[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_PKEY_CTX_set_params(ctx, params) == 1,
          "setting the encapsulation \"ikme\" parameter failed");
    CHECK(EVP_PKEY_encapsulate(ctx, ct, ctlen, ss, sslen) == 1,
          "EVP_PKEY_encapsulate failed");
    EVP_PKEY_CTX_free(ctx);
}

/* Decapsulates `ct` with `pkey` into `ss` (in/out capacity in *sslen).
 * Returns what EVP_PKEY_decapsulate returned (1 on success; ML-KEM
 * decapsulation reports implicit rejection through a success code and a
 * pseudo-random shared secret, never through an error). */
static int decapsulate(EVP_PKEY *pkey, const unsigned char *ct, size_t ctlen,
                       unsigned char *ss, size_t *sslen)
{
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(pkey, NULL);
    int rc;

    CHECK(ctx != NULL, "EVP_PKEY_CTX_new failed for the private key");
    CHECK(EVP_PKEY_decapsulate_init(ctx, NULL) == 1,
          "EVP_PKEY_decapsulate_init failed");
    rc = EVP_PKEY_decapsulate(ctx, ss, sslen, ct, ctlen);
    EVP_PKEY_CTX_free(ctx);
    return rc;
}

/* (1) Seed keygen reproduces the KAT encapsulation key for every parameter
 * set, twice in a row (key generation is deterministic), and exports a
 * private key of the FIPS 203 dk size. */
static void test_seed_keygen_kats(void)
{
    size_t s;
    for (s = 0; s < sizeof(ml_kem_sets) / sizeof(ml_kem_sets[0]); s++) {
        const struct ml_kem_set *set = &ml_kem_sets[s];
        unsigned char *seed = ALLOCATE_KEY(set->seed_len);
        unsigned char *want_pub = ALLOCATE_OTHER(ML_KEM_MAX_PUB);
        unsigned char *pub1 = ALLOCATE_OUTPUT(ML_KEM_MAX_PUB);
        unsigned char *pub2 = ALLOCATE_OUTPUT(ML_KEM_MAX_PUB);
        unsigned char *priv = ALLOCATE_OUTPUT(ML_KEM_MAX_PRIV);
        EVP_PKEY *pkey;
        size_t pub1len, pub2len, privlen, want_len;
        char msg[128];

        pkey = keygen_from_seed(set, seed);
        want_len = from_hex(set->pubhex, want_pub, ML_KEM_MAX_PUB);

        pub1len = export_raw(pkey, OSSL_PKEY_PARAM_PUB_KEY, pub1, ML_KEM_MAX_PUB);
        expect_equal("seed-derived encapsulation key does not match the KAT",
                     pub1, pub1len, want_pub, want_len);

        privlen = export_raw(pkey, OSSL_PKEY_PARAM_PRIV_KEY, priv, ML_KEM_MAX_PRIV);
        snprintf(msg, sizeof(msg), "%s private key export is %zu bytes, want %zu",
                 set->name, privlen, set->priv_len);
        CHECK(privlen == set->priv_len, msg);
        EVP_PKEY_free(pkey);

        /* Key generation from the same seed must be deterministic. */
        pkey = keygen_from_seed(set, seed);
        pub2len = export_raw(pkey, OSSL_PKEY_PARAM_PUB_KEY, pub2, ML_KEM_MAX_PUB);
        expect_equal("repeated seed keygen produced a different encapsulation key",
                     pub2, pub2len, want_pub, want_len);
        EVP_PKEY_free(pkey);
    }
}

/* (2) Encapsulation KAT: importing the KAT encapsulation key and
 * encapsulating with the fixed ephemeral randomness reproduces the KAT
 * ciphertext and shared secret; repeating the encapsulation with the same
 * randomness reproduces them exactly; and decapsulating the KAT ciphertext
 * with the seed-derived private key yields the KAT shared secret. */
static void test_encap_kats(void)
{
    size_t s;
    for (s = 0; s < sizeof(ml_kem_sets) / sizeof(ml_kem_sets[0]); s++) {
        const struct ml_kem_set *set = &ml_kem_sets[s];
        unsigned char *seed = ALLOCATE_KEY(set->seed_len);
        unsigned char *ikme = ALLOCATE_KEY(ML_KEM_SS_LEN);
        unsigned char *kat_pub = ALLOCATE_OTHER(ML_KEM_MAX_PUB);
        unsigned char *kat_ct = ALLOCATE_OUTPUT(ML_KEM_MAX_CT);
        unsigned char *kat_ss = ALLOCATE_OUTPUT(ML_KEM_SS_LEN);
        unsigned char *ct = ALLOCATE_OUTPUT(ML_KEM_MAX_CT);
        unsigned char *ss = ALLOCATE_OUTPUT(ML_KEM_SS_LEN);
        unsigned char *ss2 = ALLOCATE_OUTPUT(ML_KEM_SS_LEN);
        EVP_PKEY *pkey, *peer;
        size_t kat_publen, kat_ctlen, kat_sslen, ctlen, sslen, ss2len;

        pkey = keygen_from_seed(set, seed);
        kat_publen = from_hex(set->pubhex, kat_pub, ML_KEM_MAX_PUB);
        kat_ctlen = from_hex(set->cthex, kat_ct, ML_KEM_MAX_CT);
        kat_sslen = from_hex(set->sshex, kat_ss, ML_KEM_SS_LEN);
        from_hex(IKME_HEX, ikme, ML_KEM_SS_LEN);
        peer = import_pub(set, kat_pub, kat_publen);

        ctlen = ML_KEM_MAX_CT;
        sslen = ML_KEM_SS_LEN;
        encapsulate_with_ikme(peer, ikme, ML_KEM_SS_LEN, ct, &ctlen, ss, &sslen);
        expect_equal("encapsulation ciphertext does not match the KAT",
                     ct, ctlen, kat_ct, kat_ctlen);
        expect_equal("encapsulation shared secret does not match the KAT",
                     ss, sslen, kat_ss, kat_sslen);

        /* A fresh encapsulation context with the same ikme must produce the
         * same ciphertext and shared secret again. */
        ctlen = ML_KEM_MAX_CT;
        ss2len = ML_KEM_SS_LEN;
        encapsulate_with_ikme(peer, ikme, ML_KEM_SS_LEN, ct, &ctlen, ss2, &ss2len);
        expect_equal("repeated encapsulation with the same ikme did not reproduce "
                     "the KAT shared secret",
                     ss2, ss2len, kat_ss, kat_sslen);

        /* Decapsulation with the seed-derived private key returns the KAT
         * shared secret. */
        ss2len = ML_KEM_SS_LEN;
        CHECK(decapsulate(pkey, kat_ct, kat_ctlen, ss2, &ss2len) == 1,
              "decapsulation of the KAT ciphertext failed");
        expect_equal("decapsulated shared secret does not match the KAT",
                     ss2, ss2len, kat_ss, kat_sslen);

        EVP_PKEY_free(peer);
        EVP_PKEY_free(pkey);
    }
}

/* (3) Implicit rejection: a tampered ciphertext decapsulates without any
 * error but yields a shared secret different from the KAT one.  Both the
 * first (encapsulation-key) byte and the last (ciphertext) byte are
 * tampered, one at a time. */
static void test_tampered_ciphertext(void)
{
    size_t s;
    for (s = 0; s < sizeof(ml_kem_sets) / sizeof(ml_kem_sets[0]); s++) {
        const struct ml_kem_set *set = &ml_kem_sets[s];
        unsigned char *seed = ALLOCATE_KEY(set->seed_len);
        unsigned char *kat_ct = ALLOCATE_OUTPUT(ML_KEM_MAX_CT);
        unsigned char *kat_ss = ALLOCATE_OUTPUT(ML_KEM_SS_LEN);
        unsigned char *bad_ct = ALLOCATE_INPUT(ML_KEM_MAX_CT);
        unsigned char *ss = ALLOCATE_OUTPUT(ML_KEM_SS_LEN);
        EVP_PKEY *pkey;
        size_t kat_ctlen, kat_sslen, sslen;
        int rc;

        pkey = keygen_from_seed(set, seed);
        kat_ctlen = from_hex(set->cthex, kat_ct, ML_KEM_MAX_CT);
        kat_sslen = from_hex(set->sshex, kat_ss, ML_KEM_SS_LEN);

        memcpy(bad_ct, kat_ct, kat_ctlen);
        bad_ct[0] ^= 0x01;
        sslen = ML_KEM_SS_LEN;
        rc = decapsulate(pkey, bad_ct, kat_ctlen, ss, &sslen);
        CHECK(rc == 1, "decapsulation of a tampered ciphertext errored");
        CHECK(sslen == kat_sslen, "tampered-ciphertext shared secret size changed");
        CHECK(memcmp(ss, kat_ss, kat_sslen) != 0,
              "tampered ciphertext did not trigger implicit rejection");

        memcpy(bad_ct, kat_ct, kat_ctlen);
        bad_ct[kat_ctlen - 1] ^= 0x80;
        sslen = ML_KEM_SS_LEN;
        rc = decapsulate(pkey, bad_ct, kat_ctlen, ss, &sslen);
        CHECK(rc == 1, "decapsulation of a tampered ciphertext errored");
        CHECK(memcmp(ss, kat_ss, kat_sslen) != 0,
              "tampered ciphertext did not trigger implicit rejection");
        EVP_PKEY_free(pkey);
    }
}

/* (4) A raw private key exported from the seed-derived key pair can be
 * re-imported with EVP_PKEY_fromdata and decapsulates the KAT ciphertext
 * to the KAT shared secret. */
static void test_privkey_import(void)
{
    size_t s;
    for (s = 0; s < sizeof(ml_kem_sets) / sizeof(ml_kem_sets[0]); s++) {
        const struct ml_kem_set *set = &ml_kem_sets[s];
        unsigned char *seed = ALLOCATE_KEY(set->seed_len);
        unsigned char *kat_ct = ALLOCATE_OUTPUT(ML_KEM_MAX_CT);
        unsigned char *kat_ss = ALLOCATE_OUTPUT(ML_KEM_SS_LEN);
        unsigned char *priv = ALLOCATE_OUTPUT(ML_KEM_MAX_PRIV);
        unsigned char *ss = ALLOCATE_OUTPUT(ML_KEM_SS_LEN);
        EVP_PKEY *pkey, *impkey = NULL;
        EVP_PKEY_CTX *ctx;
        OSSL_PARAM params[2];
        size_t kat_ctlen, kat_sslen, privlen, sslen;

        pkey = keygen_from_seed(set, seed);
        kat_ctlen = from_hex(set->cthex, kat_ct, ML_KEM_MAX_CT);
        kat_sslen = from_hex(set->sshex, kat_ss, ML_KEM_SS_LEN);
        privlen = export_raw(pkey, OSSL_PKEY_PARAM_PRIV_KEY, priv, ML_KEM_MAX_PRIV);
        CHECK(privlen == set->priv_len, "private key export size mismatch");
        EVP_PKEY_free(pkey);

        ctx = EVP_PKEY_CTX_new_from_name(NULL, set->name, NULL);
        CHECK(ctx != NULL, "EVP_PKEY_CTX_new_from_name failed for an available ML-KEM");
        CHECK(EVP_PKEY_fromdata_init(ctx) == 1, "EVP_PKEY_fromdata_init failed");
        params[0] = OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_PRIV_KEY,
                                                      priv, privlen);
        params[1] = OSSL_PARAM_construct_end();
        CHECK(EVP_PKEY_fromdata(ctx, &impkey, EVP_PKEY_PRIVATE_KEY, params) == 1,
              "EVP_PKEY_fromdata of the raw private key failed");
        EVP_PKEY_CTX_free(ctx);

        sslen = ML_KEM_SS_LEN;
        CHECK(decapsulate(impkey, kat_ct, kat_ctlen, ss, &sslen) == 1,
              "decapsulation with the imported private key failed");
        expect_equal("imported private key decapsulated to the wrong shared secret",
                     ss, sslen, kat_ss, kat_sslen);
        EVP_PKEY_free(impkey);
    }
}

int main(int argc, char **argv)
{
    size_t s;

    poison_parse_args(argc, argv);

    /* ML-KEM is version-gated (OpenSSL 3.5+): if any parameter set is not
     * provided by this OpenSSL, take the SKIP path and exit 0. */
    for (s = 0; s < sizeof(ml_kem_sets) / sizeof(ml_kem_sets[0]); s++) {
        if (!pkey_alg_available(ml_kem_sets[s].name)) {
            printf("SKIP test_ml_kem: %s is unavailable\n", ml_kem_sets[s].name);
            return 0;
        }
    }

    test_seed_keygen_kats();
    test_encap_kats();
    test_tampered_ciphertext();
    test_privkey_import();

    printf("test_ml_kem: all %u checks passed\n", g_checks);
    return 0;
}

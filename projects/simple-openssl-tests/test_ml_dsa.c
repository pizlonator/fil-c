#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>

/*
 * test_ml_dsa -- ML-DSA-44/65/87 (FIPS 204) tests: deterministic seed
 * keygen, one-shot signature known answers, streaming (chunked) signing
 * and verification, tamper detection, and context-string binding.
 *
 * Determinism hooks used (all public OpenSSL 3 API):
 *
 *   - Key generation accepts the 32-byte FIPS 204 seed xi via the keymgmt
 *     "seed" parameter (OSSL_PKEY_PARAM_ML_DSA_SEED, new in 3.5); identical
 *     seeds derive identical key pairs.
 *   - Signing accepts the integer "deterministic" parameter
 *     (OSSL_SIGNATURE_PARAM_DETERMINISTIC, new in 3.5).  With it set, the
 *     32-byte signing randomizer is zeroed and signatures are fully
 *     deterministic given (key, message, context string).  Without it,
 *     ML-DSA uses randomized (hedged) signing.
 *   - Streaming uses EVP_PKEY_sign_message_init / _update / _final and
 *     EVP_PKEY_verify_message_init / _update / _final (new in 3.6): ML-DSA
 *     is a pure message-signature algorithm whose provider does not offer
 *     the digest_sign_update/digest_verify_update streaming hooks, so the
 *     EVP_DigestSignUpdate/EVP_DigestVerifyUpdate route does not apply.
 *     Those symbols first appear in OpenSSL 3.6, so the streaming section
 *     is compiled only against 3.6+ headers (the whole program skips on
 *     OpenSSL 3.0, which has no ML-DSA at all).
 *
 * Known-answer constants below were derived and cross-verified with a
 * scratch probe compiled against the authoritative pizfix OpenSSL 3.6.5
 * (build/bin/filcc), per README house rule 7:
 *
 *   - SEED_*:   the key-derivation seeds (0x00,0x01,...,0x1f for ML-DSA-44,
 *               0x40,...,0x5f for ML-DSA-65, 0x80,...,0x9f for ML-DSA-87).
 *   - KAT_SIG_*: the deterministic one-shot signatures of the 8-byte
 *               message 00 01 02 03 04 05 06 07 (2420 / 3309 / 4627 bytes,
 *               the FIPS 204 signature sizes).
 *
 * The public/secret key raw export sizes (1312/1952/2592 and
 * 2560/4032/4896 bytes) and the exact KAT signature bytes are asserted
 * below; keygen is additionally required to be reproducible run-to-run.
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
    fprintf(stderr, "FAIL test_ml_dsa: %s\n", msg);
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

static const char *const KAT_SIG_44_HEX =
    "c5ecadbf91016dc0a29703f1c82160df3e64fcbce645d762b2341579b6ac55d0"
    "2b556ed41cb9bea864e881184ff8216cf4882cbb2f727387a9fab13a2a461b6c"
    "1e26fa98848959989123fb00e3f183ce3409be713405181f43eff6a6cfaa446a"
    "e4f4ca0d95d463ac50d967acb30392abc007c4c2ce652bb7b870a49eea10718a"
    "cee5742eba314712e1abc12ae706a1ab4edd5fa1e1a4e35325753e3e90e3c5e7"
    "e10a44c3f0c39ee9f99a1c19e0d3ccda3b0c424b0f49af757dc97236cd7dc570"
    "cde16d2784d3fc8bcf6aab315ec69cd13a9c5a77047320ddab660649693c5f56"
    "a0ac9f1a8abdfb3a5c88ce01a90735b2689c661b7d1315fb680eb838c373e580"
    "8f28e26eaa99d3abb108dee559cdbe079e381a6bda7d8dcb3663e658d95b9dea"
    "75f2fa1c62e826a8df133c6a032c07e8e124905fba5c3a3e03af717b7bbde897"
    "513409fcc55463b631fb3951fdce47d03774d47b9924aa3a595c07f74dd01d06"
    "2c6c1a93a2c271e23c65a381d60637ef4fba6456144c6c4da45255d3b3a5cbb4"
    "02a5c60b20aa3ceaa9f20c765bc290e9dc1b488471242ed4a0fe12d3b05476ce"
    "cd6e1d054a3c8777cb5d077987d89c26906e73fc51056f6c8c9634be842d248f"
    "49390af82caeaf5f3381199b5baf56a6e48dc8fb225c13d7f6dfeb1a5afb2358"
    "8c5d778b65ce0d4ac6abba28d0cf92948fb792d31319bcc0141d1d37777e2987"
    "aac84afb34798722c872e122043b97669aff2f2aa559ec76eb83141cd3435cc8"
    "e91c40a9daa35f1b60c0fd32d9700e1ab9a27b5373877281319db8c6a09db980"
    "1e15b945cd800a2838a8988ed964bf2dfadc188576dc3bcb621ea56b37592d1b"
    "d38672c2f5c161241c2c70a7dabbd768b05b38645640a831b909cd97ab2ac4b3"
    "ba9c3b82ae0203cafee3df5f1939afd5dcdb94b98ae00c9b7fd5a2f1ba342cf5"
    "57a08199928bbf0b6f007953ecbdb85364014df2f551fef727986e528df2ad30"
    "072d562f7debe57f8c33e38d04de0a9b31e68edc039b83913ae12026eafc722f"
    "b7d4956b3dea82a62036b2ff847e9fba62dd6bfb40a86bc27181a2b07573bd5c"
    "03f838334ce11127b59e0f4e81de385c282d48364c2a48fda8e5f7b36a4dc153"
    "f31c7fe62f60981160f15958a9a7a8526705cd10c75177b5323c673797db6bce"
    "55a9adfa26f3a4cad69bb13401ed2965ec5a89514c7313b052530704856f2da0"
    "83273d4ddfc92a3689f6712f55630ef0588dafa8cd89e8e1d687357a1bcdab11"
    "3571a2202695c924a9be16a4920d6dd006ae61a951edb9d069912e5e65a31556"
    "fdc62fbb241954f4ca36ab9a776eead7ea018af7483ad68441887427c057e48f"
    "3d5a8e9c5d2089fc63d0ce676aa0e9dbded108a6dd05d6b3138932a70e9a0729"
    "ff3eb94100fa8b61f01f85cbdc2531854b55dd77e3adf732f4c3ce573d589532"
    "66fd19f05c8ade917d18ba99024d3d909ba42cdaeacb441638e2fc32b608a8a7"
    "fa793d8e7edb20cef4f9f868cac206efefedb9994c2e63670e42ee8b3dce193f"
    "8e1ecc6f480048678749bf707a739e95d66d7575d5b952ec5f9ae055736fe972"
    "54e7e20cb99a40cd6787df0a3c4f86479f53690654a3ef550f67d88b5b85f67c"
    "ef151a28a2f5df7887cf873284068b192cca7a71b31f28195f6b676c9046f3d2"
    "f1e937ad22147905e4c4c1a439393635c49e42a1f16291c27fd2484ae86a88c9"
    "a705a5f84bcfd6e0d84bb4a4a1c0b503d41dc3e3cd8d3a4ecefa241c78185024"
    "2ac94e52cfa53379778beb04ec8dffe0a37e8b4bcc58bb4328f37c8ee4d6e0bd"
    "16a5be3bc9140d9f99c309ffbd2eb4026474e4af04d65650b19a3ee265ee847e"
    "318b4a8fd90360fdfa93705f576cccca3403313382205861d5165d1ff25fecae"
    "7cd576b949e21fabc6729c684cefd32e8b43ca60c159d1ad5c9508d111d8c6da"
    "1b3baa2b3c2efae6156ccabb336c53a8c946e53bb0f01dab7a455fd253ec8bcf"
    "8934de688b667f646215e22050f49db84ed81be833d9f37b3d3eff78f55d5ff0"
    "190b68761d921799ea599109cf2b54a8e4ea511a1096e6d0df40c0f019734b3c"
    "3321daa60bf80d4a3ccbec4567befac12d25e4af9b1f571559d4505a82f0e331"
    "0d979a80c7d0abf661095e069e9fa7df72b672dccd39699dcdc273025c47290b"
    "042cdbcb8dd8b733fe9456e47f20bbceb503760fe86335061f514db5a24efdf4"
    "21e1e1f6f259473d8ab54104b5ee1b423edecc74486dfd828d08bba0c3ca5cc5"
    "07081835589783e3200174e7bce93272cc0f9628325bca5f45f7b409f7c029e2"
    "020416af3fe648b6eebb1170d524258cc51204d697c2a4403cca18ca1925b48d"
    "73f9de28356158e1df4581c08462bb123c0b966e2fcaf0aa5f22f15899112029"
    "aaf276029ccab33330fdf297c50ce8c87f58f98277f310abd443057272342840"
    "3955c1594ca52ef2de232957f2f22b24a55689f92cf7c4a15211e870e570f5c1"
    "6109027149720acf30ae023971f8f5b46c2ee4156e29c826f6b9897e1e5053f9"
    "ae46930a3c5fb9ffc0242344e0a09bbecd8c9ff7b810fe872fd0be1c00df3142"
    "0b50bfb7a5388af9593a36a068455da9b720eccad6174f120bcc5cc941272130"
    "4d632ac76726b06f7e65897c11d9237242dc682c02d4f970a34f3e2d7118b1ef"
    "934ed0436df20d4621e73b27139bd8bb6048f893c80f7e033780986b7ea373a3"
    "a81a294072b5c3499da8544af37b04ff11719c3f6f5825de10179a149c81d179"
    "f201dba08f5b0235ad6368ace07a8a13fab7cf9ba7c35f3141afcd5fe6484cfe"
    "3128ee464249dee061fd22d2071d0919a18adb6c10f7ebf6a2e4e6451ccc8028"
    "bca977de66af86a94c2e6c368d9f60ae05689775814145ecf04cf814e80e78f9"
    "10cd81d18616da59966a3667a35db871b2248bd8960a012efb6a7cadb82e6f94"
    "422bb1afcb454a3e069740a1fbc1acd205a0fc71e9cf08de3c3fab45e6825175"
    "75668227745931d540e5b4cbd629fba8e01abcdcb25ea633c05da13f8edbb28a"
    "b38a27e00b9b67810b5eef0f1b67dc86e45b55e398afff77ed923572dfacaf9f"
    "fc010ff4720f60158fe2fb09fe7dde0e4349e6b0e533656fa5fa22dae19e844b"
    "673a5fb97864405cc945843bf8d0b9581ddf9cfb4ee647eb95a097dd97c6019d"
    "f96e725498be1b1c3b903fc2fbe56bcd1e014f895e87bc36fcb1be2189ef7199"
    "e192e994408685d93b5f1a86004201ae32032a1dd9526608cc290afc8ff6c8e9"
    "81567bd713ae09b93cb5efe327a807be72fbeb076756a12df3230ba4dc251948"
    "001a1e1f2c42476d72828586b2bec9e4edf1fb2b38446d70bfcdcfd1d9e2e3fb"
    "fd2d3337687173d8e9f1f819282c3241516a70818a9196abbac6ccdbe7f3f600"
    "0000000000000000000000000000000013212b3f";

static const char *const KAT_SIG_65_HEX =
    "ad3757f5bf3143e9ee5e860baf3cbffbb80a6d953742675dd3f1bdfeb0cec59c"
    "673be8c163f1b284751bfba971dbf4f19a6d8c330e2680b554249eb8b7de74cf"
    "7fd4f8c537a650f898bf06bd8b668f7f881238e8a23ec7616f93c81d2b74b5a4"
    "12c7b9f51b85ee3b7fb6fe0ee90831ecdf53aab0bc18dacd6a24abfe65f2d4e4"
    "6bce91e3e615744b72471cb5075f02c7c7e6650ec743629319b3556316b8af87"
    "edffc682f698400531191f49718e87fe4f0071c9d24955ce08eef9ae503299ad"
    "1e502599d4c023a20c4ff4aa27ceb352a73a7c5d07b5d73b380ed750e48f2581"
    "b85de237dc5c4412890bde8e2c9c5f32f03a75ba1c5f1788859cde0971b25dcf"
    "0644048c5e6c7913b283a9c3909e50ba4f6f2a9e68818e1ef7a3bbcc1f8bf30b"
    "843389945755706075d06edb18436e93c68c4575808b769664e838cff43d5adf"
    "578e2cb4803e982ad230e8985a610dd5c5f8954ec74a018e80f9ee2c01d25994"
    "4788d3918b4cb8b442a4eb5dacaef6bf1533641eddaec97418111709b95952f7"
    "0329bd518d1ec005a722c99b3502a33e92e8e91396000432151085d7ff3cbb7b"
    "89062b7938a3692a5a4e7316ffb8132ae925f3f4065b998f316b7a249fb4cbb0"
    "643c10be381254ed80e353ac853aeb23e702b3a18a0a5d65742788982b90e043"
    "d0ed78b017944f1e1d47ec8870deb41f1c54b749b76ad6d5fecf1a7b0701baf5"
    "dc013539b245c14cb8b031c852f718d90704120ce9d6e4b2da873536600d761a"
    "2b41951d34a4bed96d11365155fb6eda13fe506093c65c163201cd683dcad2d0"
    "31511a0699a31117b0282bccb98982dffd5521700acf487e8c55249ff8e23cca"
    "4d3e8f871df67c507789c3025f11c30ac4b2f2f3e49dd7886281fc529404846c"
    "54d1e0b8ecd9af53ff891106ae8a22b887de7a0c9fb198f6d7885d6be35d9ff1"
    "b66abfff3ad4c8a4768ae949cb6acc1e89ed818d11fc77e42dd5b4dd2a9f72b7"
    "a6f38f4473b15e0313a7606261e08ea5eb5bf0ef11cdac628b136787d3d56d30"
    "65d24f155658dfbf048c0fdb04ae36f2e49bb297745a236e896e09604ed5a5d6"
    "cede9f6c1fb46c0c58c099e74aaf9ee5a7efab627082b0f0842417deecd48c9a"
    "97b55da1f402bc8719e479c36a0274719fdca4497dd33fa7c5d37e0853a5c545"
    "f8f7d994b9df2259ad6d6a956d3f9735138b9613f7bf10437800de35b17aba03"
    "84f8872bef43c08493d48d406ba3e68aa3e5a1ae3d69b201611e142cb7814a9f"
    "d09ba383ba2c988dd752ea2db9fa70ac43655db4bf1e5e7abc4005a1e68234ab"
    "f53818030e4cebb2c0ccb84a5fe021bb8cff94de53cfa347caa4e9bed0b51fda"
    "3518448def267c18a6a5b6e9f800d3ab75912fde9aaa251ad976e85942f672c0"
    "6c3882d1e587f5cf50b77c58f9e1b0593b0498800a860e80a99d267433df1205"
    "3f6fa447ef0117987a25b2002da75d68ebf99fd65bd4049c6b39a8a8dc1a79e0"
    "78d2fd545238fbf279ef7fab10e93894e80d1db99afd0fa8cb8bb23bc7ed248d"
    "74498aac3f566898753aa475cbe9e1b948e5bde99df5b557a4745ad32b753b3b"
    "263bdb46c5f217d69cda535ec54d08cc54d20f66f0c06b4f08938af3a72fcf05"
    "636df0671541e450c4e2667f4f6a3b7a570142239586e5a54c3e6bfbe7d7ca77"
    "cda7cdd4c974849fe7e15045271c46ede2d0f93363fbb1ba9921677ec1944c91"
    "4378b0a3fa55f8df21b9dffe91104206c381814bea00f0624e95b9bcf039c428"
    "0985579fe0b0f8d8ef37c8c43fcd625c2c774eae31cd344954bb357bce23d6d0"
    "8f7b0ef74c1742339ecacfa2af080d2cad1676e927283cb6cbdbc89bac28391f"
    "81e49328dfcbadebd42d83a716aa549b47b0c8e0d39212a2c5bad1b1a6a0be56"
    "d0ef99fa0aea6f15a291da235d62bff049ec0b955703a2cfcf4c19c22ac39311"
    "712691a3bad79b48d65de3f9a72c873e42f61da546fb4dfd78ccc0cbaef77da2"
    "97f1a4f74704ef1712204ab8daf9783ffcf5644c60e651285d5d2463384bc534"
    "bfc650a9794b0b99a48ae616339f16774c1c4896e4c96e3b7da4e1e6f077de74"
    "1dd8c85c54dc2fefbcbea725bc64e63cc22d9ed941fda78173a5167b086ddca2"
    "22b939496761d583954c35a196bc30dbc2a3af8437722a9f8f6b7fd3a1189ee4"
    "62a24b4f3aa8579b910edd7bc1e8fa43ec42a4ea824461fab1a719845835bd81"
    "cac261d13ff65ec1a511a4d8a2e9a79d530b81f6e3157793c8986b9deb6df7c5"
    "1ba79b9757301b687fe353a001768ab770b407d0a8e0613e3babd780ee893d3c"
    "f161b4a668a51e346b42d15f6b597bf4a60fe2537a7ac37b6ea57f286c8469b6"
    "0c5349a5a6bbdc52c75ae8cb145d4ce2be7deb2825367322533adc55ef3174f6"
    "13899a6fd317287bea8f0f9a8b90ec8436a50f961249e5193fcc2aa31be3b1b9"
    "4abd81c033a8d3ea432cec83e09e2db874b6448e730ac869983eb6fde289a776"
    "37ff7a01387cba62dafd6bf6b08cd56a3585760c4f593ff75db36e7b443fed9b"
    "5d1ba422e4a06186645c1e5d9d31ca22f309df1d48518a551cff5eb23eb8e09d"
    "eb184a6b1c6d5e8f6f5fbb04af6e516480b4067d95b9822e38abf28efa787cce"
    "9225e14876fb2fd4c3f6c7982b89729eae3163624ac44fe17e90cd64861359ef"
    "98ccee54f1d6da5776a1166fb0debe58828d403686a7db38ff1015b9089720b3"
    "b4c9ffcf224acab65e89c417d36ca745a3a250fb30b541de319863ad8439317d"
    "ecf99905b2c7a890ca3e8d047a4d453d3601164787847fe47496ff9c21d319e2"
    "7b09481833fd3232b4b30097e5fab9e019d4f30de7d619458f258a19f01f6109"
    "20d59ff6d43e0503f5c94bd7943dcd9ff85c385b83963ead458c940e57f56017"
    "6fa32ed9d1d75e69e345d1771f035ecb1998a8488aeb3fdfb0aa7207b133687e"
    "2fb821a33b08f1e4ed818b2c4c5f63d1dcd80f3aa4b49d954a77d6c864e196b9"
    "2d2cab13b66448275fc3222302ca8b5561e75dcf0796b95157bb710a642dfc50"
    "6e2336ed163298909614e110f9dec1253b6afa40eabbd5ca1cf8e66f789b3664"
    "12b9454e37e1b46695b85766d5f04b20f65b61993bfb7be6ddf27b84ecfd9470"
    "923ae7030f0be696766773e4f64935ac154acf084c6b1b667bb90928ddb6cdd4"
    "20f10feb6d9ec0c032cd642987a78eaa664bf730b19797e0f2ae64638525130a"
    "06f9dddd75947f90d6c1bc02ba524f552b927a500e5a7b3995ac1c9f734b78ae"
    "9b8954682f4698a886c6681848691b014bea8ce6bfa0ce20af9774b3ca707b7f"
    "f4a96892c756dba7c83961cb4bad071eee4daf22501967a4e212934fa9264f4d"
    "6d26df5396f341c3c910d80920d44ce08297f178eb6463a8f78bbd66628372af"
    "f64fae8abdd080abaf70bc455dda4f887c5237446813eb032257ab617cbc0f0d"
    "1b8f0ee1310b19dc6f76226731e764d9c1826314d737cfd26ca4713bd3b63d47"
    "faee44dc1b190cbe91d3966525a9a362dde5a63041e478aa347e8f1f708b4310"
    "2233f99c0d8954a52d6bdf6a36ca7199e7bdddde4406014476c1504f594f3b5a"
    "9a8181598af40c98eef369fa6afda34ad43a7b0a30adb959a3017e4be9195a78"
    "1052614980edbbe49c633fae05fbf3beda60f2148a82df93dec04f1879cdd0c4"
    "7f68b03a87fb74aa3f8df7e053950376e794eb57013369337e70bcf5bdffd1e7"
    "4b702adecd74bad76b76a146a0a6d02e5286b3f2cfa25dc632755b512af9d389"
    "93f4c4a5d16f7570fc394c2c19efcb711d15ea42ff604a93b82f4f6c4d3cab7d"
    "b7db5a0435b0a7cf1f995b06665d7405518f8ac39b8a810be77168f4bd47becd"
    "f0cc534781379fb38eff8e49a553fad739a51f5e5b28a66c0102ce11fa89839c"
    "15a1512b8a7f88173022042d97edb70c4cea64a5e04100b2d22ce5297751e17d"
    "1b22434dd744b9f6ca3c322e994ae12a1db9abe5071ac0f9da252b5340118bc9"
    "790d735235428a33832b70d954e20610ef2e033433f551c4d8fd74979be7fb40"
    "6298c0707d8e23bdc429165f220af55252c0ff3919dca586ecb562ad9cc22016"
    "8ff2526322b76e8ee9c10cfe36e882211f5ae2f6464e05a610bbe4a45a9ec78b"
    "e7c114fbcd8df8ba54fb59577eaad424ec71031ebe35e6292ca2366bdfd85fe8"
    "100c1db037b6a12a83c40465be4d44aafcfb8f58b866eca70c7adcb03fe687da"
    "aadd429df7b73bcddeb538f5d20e82656bc8ab8acbc7ba58dc3558747e24045a"
    "958a53c912ede50ef8c6b7011896c6ce5d41ef75ca16aea94246424b192c1e06"
    "8f070823d1bede47af9248e0b3d39a2b9043f9226ad3eb65087e3fef9f00da1a"
    "77784396fbb7a23724ea24dd47498cd07f0cf29d32b2c26c28246f8aefedeac3"
    "cf2331765c5390872d950c4c510b683a4ac515954a6457b1ffb1629a9950402a"
    "c61f3fff9c3c7d19d1df094a9f8f2cfd59f741d1e7f057023e20824930dd89b9"
    "95e265c8d66a1ea6f61410aca2e1e998ff2861742cd0d2c9371d994a914daeeb"
    "05573e5b95051bf032b35de9c3b85de0ef4a2aa648ef70bc98b087e0277a4ed5"
    "b4f9183563811e566137d1b19ce241a909152c45678cb60410525c696eb6e317"
    "1a3b3d71b3bdfb0e7da9b2d934634b52687498a5fe0000000000000000000000"
    "00000000000000070f171c1e25";

static const char *const KAT_SIG_87_HEX =
    "46c96c92d91303d4f0b292adfb0d0a741948d4e255d2ffcec9645c6bd3d54507"
    "67b3f3301fb1f149e7afe5afa6f7d02d541d7bdabb7cc9e32c00de0f68195cdd"
    "0f55b5e5965856e5b32c24cb43e9fdc8ec45bf8d52fb47dfc3ccc63e6fab69b5"
    "a142338640b93569b5b02c9ac1699842e8aa30dc2af14ba6a81317ba9bf3e0ac"
    "d20a4032089fc421539b3c7b7cc3a9b8b7d4091da94c043b1f3f1ee6030baefc"
    "aa71dba27aec161a3cfa77d3a732aed52302bed10553d3102241e840227a2b70"
    "69986d49bfa995e2c9a8e903ff6f860b91cd91878a32cf584034b99fd03ef2f2"
    "ad6a1b3ff162c488ad8a9ad2eb16a8cd7390f6cd7c57f6bc5b9bc1c1af5396ab"
    "2bc22c916d0d88065f736b867567b80b2a4269384f574a970514325b7e68b0eb"
    "4a594ab705fcf5ca5113c999f9ff6491e1695b8394b473fee86d1ebebbdda3d3"
    "09dcb2cb5053404375a6e878e06e5f45e73a87bbe37a263cc85ed72576eac15d"
    "5e889c30f66a07b7e233e87fb360e1ed7f346e753f946a39764be7728360aeb8"
    "d8bb04a99e3ed73c011fc800bfc5fecf97eda3dfd7357281a3d40d2ebfda713e"
    "6fc9e02c1541631f3a0cf35a0b81910cc49e450f4d7d45e8bc53b60327ddcb72"
    "dd1ba4f4b982335bb5d8933776862124335ee463bf9b9c24a91c4d68a3867831"
    "7210328896e2c449e4053ff47b751b369055ba92818ff915a2e57330206bf362"
    "0d9ed619f1cc45be46da117028c7e898dc3dcf10bc1ede7d4660c0ed510ad40f"
    "70be57177898b865143abcc3e2c5df85876f458f8373f58f628af9f9a8281201"
    "7c476fdc7d8eb311727ea8cc2e30b11316e8b0e90ff8978bd26fd638a1532679"
    "43329ab1acd2aac1631f8fdc9b9dc2c13346e5a5c57af48a88cf65d0dbfaaad3"
    "0de8ff4c3dac13ad71b534bdbe10fb0a61db23727376e14b5b66bca6aba7a445"
    "39c50d59337ad865cb1d1ba3598e1861c53536d889259a2de850e45bb871faf1"
    "5ddf34af35c41ff4d560b12d2d993bd206ced89dd56d9342cb415c8a8ee28e14"
    "6f32a3d9551cb2a420ed4635266167fff74993ab9dbbfe9bd6ba767396972b83"
    "5c852020c83782b4bd0f9f7cb69ed63cb6e04a2399ef9d0fb0cc8c66b0b52f5d"
    "28b15181987133352c96e0da6a7938dc4b944808c3ad2898748824c503b3a6e8"
    "fac73b4a6ba9da6f4554eb6d3ed89258d645637722f705eb47f3edc8af8a810d"
    "faa6e30008865602b53c00c0414d19b62607782d79e08a0d9a9b39a93d3279ca"
    "3f08f8cff0b35b1e4dbfbd4acec85c8cc831c6bc97b8f99eaf8c547a83401d68"
    "a3524567bf5fbbde19efbf4722f3f1875d072400ff1d4a79389d21a97952dd95"
    "927d8d14d6f6b3440cb3d6b3b49563055f21221c570b72744f586bfea834c92c"
    "622a5e91ed5c03b93cc3e338ce4edd29900f01be7add48a41fba41452da68dbc"
    "db06c55725c0698fb402f385c46aa484b0f9fddf57d1181064893937aceb0510"
    "42092e96419e695d6b5e549a74709f41b44ccfcc4c9dd35cd321526adb1859bc"
    "89973a14b34a0c9ee37e01755469f81954e4221961bc4882d8ff95cc07686814"
    "3f406c49f549607bd5d0d8d4937ffc1fc56b5fdf955ff8c522dcdd66e639a85e"
    "0cb9b6ad2fc9695e80e1257918948107f90eb1d0011e305d2899824cd20451f3"
    "3ffda1b59be960eb9a78b14d1af8fa1296ed1ad7ba0b11e3b7ac2f582375b720"
    "c966e31e8c70d0a66bbf00f2e37f4ea0b0ecb3b142af12a7a0749e024dab7daf"
    "e5763ec1bedb3da923f8793ee16d2c6069652d77b4c80a27223ac6eba1c06c75"
    "55eb1b20ba4dc867fd2060d459a612b1e0234ec85a6369dfdd8875bceaa2c5a6"
    "853e7f58690b43c2cdebdb6a30a6a8dcbfbdc4811213a2d6ae248f002bdc95e7"
    "b7d90480dcbfe6c61438395b67932dff410ce8d5a7d6af8b9f0c01cf7408d469"
    "2173ccf70ce4885c354912cd8e50bda0650277a255a6bd6f7ecc7880b08b9e11"
    "13102ca6686d837e8ba7f37ed1dcacea78b2a2ca1549015d3e2261aae0bb229f"
    "296b67583075fce66ca1cdbd64c99b1045812e3db2becf08faae1bb7c2a38c2f"
    "78eadc6f078faf0dd201a6242ca4b533bf070f99895bbc75af5d98bf3262d7e5"
    "1e312071487932dffdbd2dd61bddc635bac683df774faae7d89148c484185c42"
    "9efa42c4b427f16aa83a2f50addab9db3d8ee63727ea6ea644b97ae9fde099bd"
    "76b5f96a19c6a68ce7b3e9859a6c5400716f631781a612c9220d7ad977be75c2"
    "1e2d4979ae7590ce9f52ec9c6533044a8ae7b4fcccd42e382b3eece1cf73193a"
    "bdc7d0024e3d64ae674dc3aa400dccd2a9520ba301d951a43df8d18e9964ef6d"
    "4e4460e594b21c07376f460c4fd57d73a54ec6905630769ab5bf11567e70e07a"
    "13a5fc711958c26de2fa01acd6715e364922e20a0459c471caaf22bcf24c964d"
    "acb02b5e49c7ef70a0acd7e06e168b6959e68e43491ef8dccaf758c2b35a0682"
    "d94b3fbf99330bcb4c376c9679c3ad77f610bee089ae0218135bb5c2644ed162"
    "696628349e40c128783c8f8e5a8cb77031e864f63087be24d2bf071a47f06ab0"
    "797cfce491dce4cdf743f3cd4620efff3aeaba548bba5cc706fd2d26e253f513"
    "b1cbde3bdcdb7523b9e6ef7233b5f0b7a1c0ba585ea3a0b9ede73a1ae7bd66a0"
    "fa88ae3f1bf17e8c042c5c6c829ade2861b2a3c5dd32ea40414097f5d91b5b84"
    "5a665c023fe72b94bf8ab97b26050691428667df2ec7298dc61e61e923971ea2"
    "bdfbf76d4d335f74acf0ad78b130f9b5426e1a8162f6223f67065d7a7cb3e720"
    "9bfeb040076e32689191397f1dadbf6884b65446940ad5052c2d548afb922555"
    "16425eb1d47f2545408dad021d8ff7746e22a7810981ed92c9a04f149498cae2"
    "1915763e1e6511926ad4f228f042f2d8abdeb35d3d6e5fa6564ad02e467c1bf2"
    "4f093788cd8569d7a38094ec6cc7f331b42ad5094acd69360311927a6113e07d"
    "8577d482723e5ba3180128e9280f6da0312b892f07951774de95bbf0a4940ba6"
    "8d116eea85f387e98460e848a025106a4475e739d76f1f95605f665d8ca8a368"
    "f05f5bc523eb3693b66490d38cdede6a33e5029a8087f50f8c61871d955d538e"
    "77d3ce2d3644539aff7622db35a0e6adb3307921c2aa13bf1105bed82791fe04"
    "ee33f8ebd9fc94126b102c1d88441b4caa077afed23b6e1a631964c888ae3498"
    "703fa96336aa314573ce02fa9cc6c7d0949089d345e7ae1235559e6d9e3f6b59"
    "34254837c8fe9a181743c46ccbede457dfee02110bd5e02972f24b7fd736d09e"
    "5ec52ea017c629acfa76d7b595c9f153f90211d2aab0380fd6d6bb296a10644f"
    "35f979b732bc2b6c8e9aca186ad61f6a21bb2aaab07ca7e9afdbdbb27cf1a9ff"
    "7efab23300805ff31a02385024046a95da683b477ee0f4120c5371a8730cc702"
    "d0434463a54c04bcbd94da5b4ccc409f986ecdb62b7073863472c1c80397a7e4"
    "775d177e18246621504d7200053ead4d19090565d2d668b123352829565315ad"
    "3ec1762619160f2b01cbfb3e50fce387e2cc4af7dae9c0c5961aca9e5072e09c"
    "9c54dde83eb8ca11f3a8878dec5f1fc77859cdf7c8c6d6997eced5843b7a5460"
    "47ff18035231a13a2df7ca5721b417427f19cdf8862ddbba7f6549b3b9c2bcee"
    "4f8d17ec9853a93e92318082ee02dbc209ac973495d1685d799b977955f2cfab"
    "8146d4960b0009bfe423123b165a0aaebb28ac4a21a31d9b3ce65e65acca5b37"
    "7533a937f44df65ffc21e17317e3d5c8f410e78ae688a96e7464182ee6c95534"
    "60723dcedae9c2e2eca92e0444e94eea4ea5a08954cac0ac15e4d1f238b3dd10"
    "b1b1c15d2b138e4ccae6d30011f975dd9d9382d4bc13f9a1e87878dca3b32ecf"
    "18dbac2b416fae0002e743b3730464b73b7925ae5543f327521b0b8e8bad1155"
    "e85c599afec5732205f5ded6ac21c93ea119f1032c655c10f360c5026fca23dd"
    "45335f8bb9577df664095748e4200d62c3c15e00a5ef71dbc083b07f0ae4c67a"
    "d7f0483382db87efbf9137e2304b1fa9645e7b48314d5f6efc27b216564dd7b2"
    "1e1049b62af8f2937037657f19a070f264ec79a5cbd39562f95a52f2b212a66b"
    "f08434321db5d3259cc7e4ed7ef53814c6fb55a3a480265fd77a7205af312f63"
    "87001b53e5b570ccd61669bf28624fa7d4cc6a3676cb900848b86a08ec9af470"
    "810b15a1bab154c16f8db61e0d4e097fb7eb6306fa034228b2c264b6ba41a28c"
    "7c26fc27a0d7df5b34362da25dfedb2111730064542b371248ebae820cfcbe7d"
    "82336cc664577e7f1198d39799a79fa1ff234658b7aea4e885ec9b3aeaa0268c"
    "e89f8655e2c6bead3d943d78a1baf211e232f5e778280e23ae139814a83b793b"
    "821c6054ca629eaa05a5373f0d92d668921d7b7682f28df8be1e14c3713318ca"
    "49115e3de96fb9601cd96ed802755605322577a8bfef5df6870a6cee9da3af40"
    "d09ad12b5c7f8d135a8d3e786dcbff7d845b42ee899fa6794c85f779ace33473"
    "06cbefe701f1aa33836d20284b99b9d01a1ef2ba3d2e0ad19adff07d3a19294f"
    "2cb7973008ce307f7cf7039bab5611aac726a1565bb633f065ff69d9c727b2ca"
    "776d4afe75e1e78e263fcdb481c2ada06980a5da07623543fdfec3ae995933a8"
    "ccc197df548d2fba6b29642bc57b2a87c47bd1c0854def6b6b49e7466609c30d"
    "c465f143789fbf407145cfde9d717ac7263c50265ada7fc2343f44a47e22fba3"
    "1463ac536c05ee3a224abe6d183d5def402e4c15cd680a8b8d92e93438e1b063"
    "8f1f5e387a2bbb07becf8e80e34522b8bb75214ceee58b9de90e0ed1be2ad517"
    "2e293393c38c051f465eb4c634bfeffd88db203aea472914a7a63048bd2c60d2"
    "c95abd6a5893bf1a8173a1ebd7a5ab13bccffaf6cae63aa8ca5719cd0655d6ad"
    "5be13dc3278a5b5ca07dae215f31326331f1fdfbfd911dd60f428200f9afe048"
    "98e637d9b59c98f4a8a250606b1a2319da4a19cca7a7a1bbda2a7c29f9019fdf"
    "522899e83054308dd14f1f7e43067319a2b2db32170663bb4485e1fc3479354a"
    "b7348ffcb07a44d03ef7676c30d89d82d91c0ac219e4fccbef8343a1bdc8b52c"
    "893fbd2d0bf6f94ea60543940fcce35c27454bd1c50230015bf80c29898d1b75"
    "33ec880de556a4f88afc0ececed5814c114ed4525790b4089ab58f8df68088ba"
    "a5ee612eb183bf58b58ec3f7fb569c094f31b16633374add3ebbb1b0bcd7671e"
    "7c6e6d0634271899c05f197cf96bcbf1ce597772296bb9163d88a4d3414eefcf"
    "c04a69c67377c9eef75497ac909c8ef48819560c3bab2020fb69292839f7ef7e"
    "815da5f1103479f866d9069f848add1acb63487bfac58a8ec72d553fc06181be"
    "4bd1ec18c7e5a4d57f9639160335b44c17f9bfc842698c0fd560007e47980da4"
    "6aed882011ece9f58df1360368b98214adb8f0e35760435c556e93163cfbf648"
    "de8ba1072b2d33d1ddaf2cf36a1f3125f13cca1415f6e6bd00290fbea04433c0"
    "6791cc42fd38a2c86e916cee2ec3cae6e474ef9c56cf1e1a4aa476888772cd07"
    "e0f7f3f2aac8cf786cfcef77c927b156d388d49dc2b4dea84aa657384011bdd9"
    "a520cdcf26428771ebf68cc082630969eea720051648579fe9a2bcfdd34e91f3"
    "216e20f459c3cf33870771cd925448bedeee0c67d4c983cd6eff9b6a57de842e"
    "866dd32aca26ac8e04928d4c077ad828ea319e965f8ac11f8a2f199575eee463"
    "b9e18bdee216548f48a7a599d184e8d505186aff0ccfff7414ad258d5f09628f"
    "09d9967303c0926db53f80fb4295849f4dbee47c87412d888e266484f7903741"
    "97fad1b21b506675fbf4179ca79889428f5758e7fda38bc70ab5f90b77566204"
    "2f1519195c34460d45b5cef038bbf703688677eb014e243f8d7bb6e852877167"
    "0bf50f7f94295c3757bc75a22cbace9d137fa0c0dd1a06a168122492da68ff20"
    "44ca561ed88f469615c79bbd76baea39af97e2da906f73aec541fb63c401d230"
    "d2f3a6d975e13ab58e23a44f0f592300e6a14ba4d7dec70cd99cd7683c924ced"
    "89ed7db71054c418c69690b8f0e7e50bd1a70ea3c43d3bf410e386d96ff1aee3"
    "e78f3270f60587e72b91f0838da31285df4ffe90daf1d051a6e707bd6814f41a"
    "8d0333e0fad3cbfa26931bb331208be530407b07697f7344cf78188047c1c8b5"
    "0101c2262d7ab01742eb081761b6e279a4142b2841ef946b14fb51719ed31bf5"
    "022d7f7d462118f1f07260ee7afa5b8f90bce4101eb10903400464c4a63be208"
    "8610b7f5ee3f1d7a959002f24f5487bb58693a9a44841b30d42b8eb4ef060eba"
    "e91117286227bc33bf9ebd80f5e7d33b1b3d4f30feb9e5bd07379203765e3c46"
    "a999b3dd97d616f6f58d3db8862a7f692d70c3d99660d21e273cc21915e1b851"
    "2d3475b2bddefe62ae4b5d60829fa0cc07587c888ba6b5f00408606bb50a0e2c"
    "4a7495dbe7f607152f329cabadd5daf3016670727ed3fc000000000000000000"
    "0000000000000000000000070910181d263037";

/* The FIPS 204 key-derivation seeds: 0x00,0x01,...,0x1f for ML-DSA-44,
 * 0x40,...,0x5f for ML-DSA-65, 0x80,...,0x9f for ML-DSA-87 (32 bytes each,
 * the ML_DSA_SEED_BYTES length in every parameter set). */
static const char *const SEED_44_HEX =
    "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";
static const char *const SEED_65_HEX =
    "404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f";
static const char *const SEED_87_HEX =
    "808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f";

/* The three FIPS 204 parameter sets. */
static const struct ml_dsa_set {
    const char *name;
    size_t seed_len;    /* xi: 32 bytes for every set */
    size_t pub_len;     /* pk:  1312 / 1952 / 2592 */
    size_t priv_len;    /* sk:  2560 / 4032 / 4896 */
    size_t sig_len;     /* sig: 2420 / 3309 / 4627 */
    const char *seedhex;
    const char *sighex;
} ml_dsa_sets[] = {
    { "ML-DSA-44", 32, 1312, 2560, 2420, SEED_44_HEX, KAT_SIG_44_HEX },
    { "ML-DSA-65", 32, 1952, 4032, 3309, SEED_65_HEX, KAT_SIG_65_HEX },
    { "ML-DSA-87", 32, 2592, 4896, 4627, SEED_87_HEX, KAT_SIG_87_HEX },
};

/* The fixed 8-byte KAT message. */
static const char *const MSG8_HEX = "0001020304050607";

/* A non-empty signing/verification context string (FIPS 204 ctx). */
static const char CTXSTR[] = "FilC-context";

#define ML_DSA_MAX_PUB  2592
#define ML_DSA_MAX_PRIV 4896
#define ML_DSA_MAX_SIG  4627
#define ML_DSA_MSG_LEN  8

/* Size of the streaming message: 1 MiB signed in three chunks. */
#define BIG_LEN (1024u * 1024u)

/* The "deterministic" signature parameter (OSSL_SIGNATURE_PARAM_DETERMINISTIC;
 * spelled as a literal because the macro is new in 3.5). */
static void set_deterministic(EVP_PKEY_CTX *pctx)
{
    OSSL_PARAM params[2];
    int one = 1;

    params[0] = OSSL_PARAM_construct_int("deterministic", &one);
    params[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_PKEY_CTX_set_params(pctx, params) == 1,
          "setting the \"deterministic\" signature parameter failed");
}

/* True when this OpenSSL provides the named algorithm (its keymgmt can be
 * instantiated and key generation initialized). */
static int pkey_alg_available(const char *name)
{
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(NULL, name, NULL);
    int ok = ctx != NULL && EVP_PKEY_keygen_init(ctx) == 1;
    EVP_PKEY_CTX_free(ctx);
    return ok;
}

/* Deterministic key generation from the FIPS 204 seed xi via the keymgmt
 * "seed" parameter (OSSL_PKEY_PARAM_ML_DSA_SEED; spelled as a literal
 * because the macro is new in 3.5). */
static EVP_PKEY *keygen_from_seed(const struct ml_dsa_set *set,
                                  unsigned char *seedbuf)
{
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(NULL, set->name, NULL);
    EVP_PKEY *pkey = NULL;
    OSSL_PARAM params[2];

    CHECK(ctx != NULL, "EVP_PKEY_CTX_new_from_name failed for an available ML-DSA");
    CHECK(EVP_PKEY_keygen_init(ctx) == 1, "EVP_PKEY_keygen_init failed");
    from_hex(set->seedhex, seedbuf, set->seed_len);
    params[0] = OSSL_PARAM_construct_octet_string("seed", seedbuf, set->seed_len);
    params[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_PKEY_CTX_set_params(ctx, params) == 1,
          "setting the ML-DSA keygen \"seed\" parameter failed");
    CHECK(EVP_PKEY_keygen(ctx, &pkey) == 1, "EVP_PKEY_keygen failed");
    CHECK(pkey != NULL, "EVP_PKEY_keygen produced no key");
    EVP_PKEY_CTX_free(ctx);
    return pkey;
}

/* One-shot deterministic EVP_DigestSign of a message.  *siglen is an in/out
 * capacity per the EVP contract. */
static void sign_one_shot(EVP_PKEY *pkey, const unsigned char *msg, size_t msglen,
                          unsigned char *sig, size_t *siglen)
{
    EVP_MD_CTX *mctx = EVP_MD_CTX_new();
    EVP_PKEY_CTX *pctx;

    CHECK(mctx != NULL, "EVP_MD_CTX_new failed");
    CHECK(EVP_DigestSignInit(mctx, &pctx, NULL, NULL, pkey) == 1,
          "EVP_DigestSignInit failed");
    set_deterministic(pctx);
    CHECK(EVP_DigestSign(mctx, sig, siglen, msg, msglen) == 1,
          "EVP_DigestSign failed");
    EVP_MD_CTX_free(mctx);
}

/* One-shot deterministic verify.  Returns 1 only for a valid signature. */
static int verify_one_shot(EVP_PKEY *pkey, const unsigned char *sig, size_t siglen,
                           const unsigned char *msg, size_t msglen)
{
    EVP_MD_CTX *mctx = EVP_MD_CTX_new();
    EVP_PKEY_CTX *pctx;
    int rc;

    CHECK(mctx != NULL, "EVP_MD_CTX_new failed");
    CHECK(EVP_DigestVerifyInit(mctx, &pctx, NULL, NULL, pkey) == 1,
          "EVP_DigestVerifyInit failed");
    rc = EVP_DigestVerify(mctx, sig, siglen, msg, msglen);
    EVP_MD_CTX_free(mctx);
    return rc;
}

/* (1) Seed keygen: exports keys of the FIPS 204 pk/sk sizes and is
 * reproducible run-to-run for every parameter set. */
static void test_seed_keygen(void)
{
    size_t s;
    for (s = 0; s < sizeof(ml_dsa_sets) / sizeof(ml_dsa_sets[0]); s++) {
        const struct ml_dsa_set *set = &ml_dsa_sets[s];
        unsigned char *seed = ALLOCATE_KEY(set->seed_len);
        unsigned char *pub1 = ALLOCATE_OUTPUT(ML_DSA_MAX_PUB);
        unsigned char *pub2 = ALLOCATE_OUTPUT(ML_DSA_MAX_PUB);
        unsigned char *priv = ALLOCATE_OUTPUT(ML_DSA_MAX_PRIV);
        EVP_PKEY *pkey;
        size_t pub1len, pub2len, privlen;
        char msg[128];

        pkey = keygen_from_seed(set, seed);

        pub1len = 0;
        CHECK(EVP_PKEY_get_octet_string_param(pkey, OSSL_PKEY_PARAM_PUB_KEY,
                                              pub1, ML_DSA_MAX_PUB, &pub1len) == 1,
              "public key export failed");
        snprintf(msg, sizeof(msg), "%s public key export is %zu bytes, want %zu",
                 set->name, pub1len, set->pub_len);
        CHECK(pub1len == set->pub_len, msg);

        privlen = 0;
        CHECK(EVP_PKEY_get_octet_string_param(pkey, OSSL_PKEY_PARAM_PRIV_KEY,
                                              priv, ML_DSA_MAX_PRIV, &privlen) == 1,
              "private key export failed");
        snprintf(msg, sizeof(msg), "%s private key export is %zu bytes, want %zu",
                 set->name, privlen, set->priv_len);
        CHECK(privlen == set->priv_len, msg);
        EVP_PKEY_free(pkey);

        /* Key generation from the same seed must be deterministic. */
        pkey = keygen_from_seed(set, seed);
        pub2len = 0;
        CHECK(EVP_PKEY_get_octet_string_param(pkey, OSSL_PKEY_PARAM_PUB_KEY,
                                              pub2, ML_DSA_MAX_PUB, &pub2len) == 1,
              "public key export failed (second keygen)");
        expect_equal("repeated seed keygen produced a different public key",
                     pub2, pub2len, pub1, pub1len);
        EVP_PKEY_free(pkey);
    }
}

/* (2) One-shot signing KAT: the deterministic signature of the 8-byte KAT
 * message matches the embedded bytes (FIPS 204 signature size), signs
 * identically when repeated, verifies, and rejects tampered signatures and
 * wrong messages.  The default (randomized) mode differs from the
 * deterministic signature but still verifies. */
static void test_oneshot_kats(void)
{
    size_t s;
    for (s = 0; s < sizeof(ml_dsa_sets) / sizeof(ml_dsa_sets[0]); s++) {
        const struct ml_dsa_set *set = &ml_dsa_sets[s];
        unsigned char *seed = ALLOCATE_KEY(set->seed_len);
        unsigned char *msg = ALLOCATE_INPUT(ML_DSA_MSG_LEN);
        unsigned char *want_sig = ALLOCATE_OTHER(ML_DSA_MAX_SIG);
        unsigned char *sig = ALLOCATE_OUTPUT(ML_DSA_MAX_SIG);
        unsigned char *sig2 = ALLOCATE_OUTPUT(ML_DSA_MAX_SIG);
        EVP_PKEY *pkey;
        size_t msglen, wantlen, siglen, sig2len;
        char msgbuf[128];

        pkey = keygen_from_seed(set, seed);
        msglen = from_hex(MSG8_HEX, msg, ML_DSA_MSG_LEN);
        wantlen = from_hex(set->sighex, want_sig, ML_DSA_MAX_SIG);

        /* The one-shot NULL-buffer call reports the exact FIPS 204 size. */
        siglen = 0;
        {
            EVP_MD_CTX *mctx = EVP_MD_CTX_new();
            EVP_PKEY_CTX *pctx;
            CHECK(mctx != NULL, "EVP_MD_CTX_new failed");
            CHECK(EVP_DigestSignInit(mctx, &pctx, NULL, NULL, pkey) == 1,
                  "EVP_DigestSignInit failed");
            set_deterministic(pctx);
            CHECK(EVP_DigestSign(mctx, NULL, &siglen, msg, msglen) == 1,
                  "EVP_DigestSign (size query) failed");
            snprintf(msgbuf, sizeof(msgbuf),
                     "%s signature size is %zu, want %zu", set->name, siglen,
                     set->sig_len);
            CHECK(siglen == set->sig_len, msgbuf);
            EVP_MD_CTX_free(mctx);
        }

        siglen = ML_DSA_MAX_SIG;
        sign_one_shot(pkey, msg, msglen, sig, &siglen);
        expect_equal("deterministic one-shot signature does not match the KAT",
                     sig, siglen, want_sig, wantlen);

        /* Signing the same message again must reproduce the signature. */
        sig2len = ML_DSA_MAX_SIG;
        sign_one_shot(pkey, msg, msglen, sig2, &sig2len);
        expect_equal("repeated one-shot signing produced a different signature",
                     sig2, sig2len, want_sig, wantlen);

        /* The KAT signature verifies. */
        CHECK(verify_one_shot(pkey, want_sig, wantlen, msg, msglen) == 1,
              "the KAT signature does not verify");

        /* A tampered signature must fail verification. */
        sig2[0] ^= 0x01;
        CHECK(verify_one_shot(pkey, sig2, sig2len, msg, msglen) != 1,
              "tampered signature verified");

        /* A wrong message must fail verification. */
        sig2[0] ^= 0x01; /* restore */
        msg[0] ^= 0x80;
        CHECK(verify_one_shot(pkey, sig, siglen, msg, msglen) != 1,
              "signature verified over the wrong message");
        msg[0] ^= 0x80;

        /* Default (randomized/hedged) signing differs from the deterministic
         * signature but must still verify. */
        {
            EVP_MD_CTX *mctx = EVP_MD_CTX_new();
            EVP_PKEY_CTX *pctx;
            CHECK(mctx != NULL, "EVP_MD_CTX_new failed");
            CHECK(EVP_DigestSignInit(mctx, &pctx, NULL, NULL, pkey) == 1,
                  "EVP_DigestSignInit failed");
            sig2len = ML_DSA_MAX_SIG;
            CHECK(EVP_DigestSign(mctx, sig2, &sig2len, msg, msglen) == 1,
                  "randomized EVP_DigestSign failed");
            EVP_MD_CTX_free(mctx);
            CHECK(sig2len == siglen, "randomized signature size changed");
            CHECK(memcmp(sig2, sig, siglen) != 0,
                  "randomized signing did not randomize");
            CHECK(verify_one_shot(pkey, sig2, sig2len, msg, msglen) == 1,
                  "randomized signature does not verify");
        }

        EVP_PKEY_free(pkey);
    }
}

/* (3) Context-string binding: signatures made with a context string verify
 * only under the same context string. */
static void test_context_string(void)
{
    size_t s;
    for (s = 0; s < sizeof(ml_dsa_sets) / sizeof(ml_dsa_sets[0]); s++) {
        const struct ml_dsa_set *set = &ml_dsa_sets[s];
        unsigned char *seed = ALLOCATE_KEY(set->seed_len);
        unsigned char *msg = ALLOCATE_INPUT(ML_DSA_MSG_LEN);
        unsigned char *ctxstr = ALLOCATE_INPUT(sizeof(CTXSTR) - 1);
        unsigned char *sig = ALLOCATE_OUTPUT(ML_DSA_MAX_SIG);
        unsigned char *plain = ALLOCATE_OUTPUT(ML_DSA_MAX_SIG);
        EVP_PKEY *pkey;
        size_t msglen, siglen, plainlen;

        pkey = keygen_from_seed(set, seed);
        msglen = from_hex(MSG8_HEX, msg, ML_DSA_MSG_LEN);
        memcpy(ctxstr, CTXSTR, sizeof(CTXSTR) - 1);

        /* Sign with the context string. */
        {
            EVP_MD_CTX *mctx = EVP_MD_CTX_new();
            EVP_PKEY_CTX *pctx;
            OSSL_PARAM params[2];
            CHECK(mctx != NULL, "EVP_MD_CTX_new failed");
            CHECK(EVP_DigestSignInit(mctx, &pctx, NULL, NULL, pkey) == 1,
                  "EVP_DigestSignInit failed");
            set_deterministic(pctx);
            params[0] = OSSL_PARAM_construct_octet_string("context-string",
                                                          ctxstr,
                                                          sizeof(CTXSTR) - 1);
            params[1] = OSSL_PARAM_construct_end();
            CHECK(EVP_PKEY_CTX_set_params(pctx, params) == 1,
                  "setting the \"context-string\" parameter failed");
            siglen = ML_DSA_MAX_SIG;
            CHECK(EVP_DigestSign(mctx, sig, &siglen, msg, msglen) == 1,
                  "EVP_DigestSign with a context string failed");
            EVP_MD_CTX_free(mctx);
        }

        /* Same context string: verifies. */
        {
            EVP_MD_CTX *mctx = EVP_MD_CTX_new();
            EVP_PKEY_CTX *pctx;
            OSSL_PARAM params[2];
            CHECK(mctx != NULL, "EVP_MD_CTX_new failed");
            CHECK(EVP_DigestVerifyInit(mctx, &pctx, NULL, NULL, pkey) == 1,
                  "EVP_DigestVerifyInit failed");
            params[0] = OSSL_PARAM_construct_octet_string("context-string",
                                                          ctxstr,
                                                          sizeof(CTXSTR) - 1);
            params[1] = OSSL_PARAM_construct_end();
            CHECK(EVP_PKEY_CTX_set_params(pctx, params) == 1,
                  "setting the \"context-string\" parameter failed");
            CHECK(EVP_DigestVerify(mctx, sig, siglen, msg, msglen) == 1,
                  "context-string signature does not verify under its context");
            EVP_MD_CTX_free(mctx);
        }

        /* Without the context string: must fail. */
        CHECK(verify_one_shot(pkey, sig, siglen, msg, msglen) != 1,
              "context-string signature verified without the context");

        /* And it must differ from the no-context signature. */
        plainlen = ML_DSA_MAX_SIG;
        sign_one_shot(pkey, msg, msglen, plain, &plainlen);
        CHECK(siglen == plainlen && memcmp(sig, plain, plainlen) != 0,
              "context string did not change the signature");

        EVP_PKEY_free(pkey);
    }
}

/*
 * (4) Streaming signing and verification of a ~1MB message fed in three
 * chunks, via EVP_PKEY_sign_message_* and EVP_PKEY_verify_message_* (3.6+;
 * see the file comment).  The streaming signature must equal the one-shot
 * EVP_DigestSign signature over the same message (deterministic mode), the
 * streaming verification must accept it, and a one-shot verification of the
 * streaming signature over the full message must also accept it, while a
 * tampered streaming signature is rejected.  Compiled only against 3.6+
 * headers: these symbols do not exist in OpenSSL 3.0 (the program skips
 * there long before reaching this section).
 */
#if OPENSSL_VERSION_NUMBER >= 0x30600000L
static void test_streaming(void)
{
    static const size_t chunks[3] = { 400000, 500000, BIG_LEN - 900000 };
    unsigned char *big = ALLOCATE_INPUT(BIG_LEN);
    size_t s, i;

    for (i = 0; i < BIG_LEN; i++)
        big[i] = (unsigned char)(i * 7 + 3);

    for (s = 0; s < sizeof(ml_dsa_sets) / sizeof(ml_dsa_sets[0]); s++) {
        const struct ml_dsa_set *set = &ml_dsa_sets[s];
        unsigned char *seed = ALLOCATE_KEY(set->seed_len);
        unsigned char *stream_sig = ALLOCATE_OUTPUT(ML_DSA_MAX_SIG);
        unsigned char *oneshot_sig = ALLOCATE_OUTPUT(ML_DSA_MAX_SIG);
        EVP_PKEY *pkey;
        size_t stream_len, oneshot_len, off;

        pkey = keygen_from_seed(set, seed);

        /* Streaming signature over the 1MB message, three updates. */
        {
            EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new(pkey, NULL);
            CHECK(pctx != NULL, "EVP_PKEY_CTX_new failed");
            CHECK(EVP_PKEY_sign_message_init(pctx, NULL, NULL) == 1,
                  "EVP_PKEY_sign_message_init failed");
            set_deterministic(pctx);
            off = 0;
            for (i = 0; i < 3; i++) {
                CHECK(EVP_PKEY_sign_message_update(pctx, big + off,
                                                   chunks[i]) == 1,
                      "EVP_PKEY_sign_message_update failed");
                off += chunks[i];
            }
            CHECK(off == BIG_LEN, "streaming chunks did not cover the message");
            stream_len = ML_DSA_MAX_SIG;
            CHECK(EVP_PKEY_sign_message_final(pctx, stream_sig,
                                              &stream_len) == 1,
                  "EVP_PKEY_sign_message_final failed");
            EVP_PKEY_CTX_free(pctx);
        }

        /* One-shot signature over the same message must be identical. */
        oneshot_len = ML_DSA_MAX_SIG;
        sign_one_shot(pkey, big, BIG_LEN, oneshot_sig, &oneshot_len);
        expect_equal("streaming signature differs from the one-shot signature",
                     stream_sig, stream_len, oneshot_sig, oneshot_len);

        /* Streaming verification of the streaming signature. */
        {
            EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new(pkey, NULL);
            OSSL_PARAM params[2];
            CHECK(pctx != NULL, "EVP_PKEY_CTX_new failed");
            CHECK(EVP_PKEY_verify_message_init(pctx, NULL, NULL) == 1,
                  "EVP_PKEY_verify_message_init failed");
            params[0] = OSSL_PARAM_construct_octet_string("signature",
                                                          stream_sig,
                                                          stream_len);
            params[1] = OSSL_PARAM_construct_end();
            CHECK(EVP_PKEY_CTX_set_params(pctx, params) == 1,
                  "setting the \"signature\" parameter failed");
            off = 0;
            for (i = 0; i < 3; i++) {
                CHECK(EVP_PKEY_verify_message_update(pctx, big + off,
                                                     chunks[i]) == 1,
                      "EVP_PKEY_verify_message_update failed");
                off += chunks[i];
            }
            CHECK(EVP_PKEY_verify_message_final(pctx) == 1,
                  "streaming verification of a valid signature failed");
            EVP_PKEY_CTX_free(pctx);
        }

        /* One-shot verification of the streaming signature. */
        CHECK(verify_one_shot(pkey, stream_sig, stream_len, big, BIG_LEN) == 1,
              "one-shot verification of the streaming signature failed");

        /* A tampered streaming signature must be rejected. */
        stream_sig[0] ^= 0x01;
        {
            EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new(pkey, NULL);
            OSSL_PARAM params[2];
            int vrc;
            CHECK(pctx != NULL, "EVP_PKEY_CTX_new failed");
            CHECK(EVP_PKEY_verify_message_init(pctx, NULL, NULL) == 1,
                  "EVP_PKEY_verify_message_init failed");
            params[0] = OSSL_PARAM_construct_octet_string("signature",
                                                          stream_sig,
                                                          stream_len);
            params[1] = OSSL_PARAM_construct_end();
            CHECK(EVP_PKEY_CTX_set_params(pctx, params) == 1,
                  "setting the \"signature\" parameter failed");
            off = 0;
            for (i = 0; i < 3; i++) {
                CHECK(EVP_PKEY_verify_message_update(pctx, big + off,
                                                     chunks[i]) == 1,
                      "EVP_PKEY_verify_message_update failed");
                off += chunks[i];
            }
            vrc = EVP_PKEY_verify_message_final(pctx);
            CHECK(vrc != 1, "tampered streaming signature verified");
            EVP_PKEY_CTX_free(pctx);
        }
        EVP_PKEY_free(pkey);
    }
}
#endif /* OPENSSL_VERSION_NUMBER >= 0x30600000L */


int main(int argc, char **argv)
{
    size_t s;

    poison_parse_args(argc, argv);

    /* ML-DSA is version-gated (OpenSSL 3.5+): if any parameter set is not
     * provided by this OpenSSL, take the SKIP path and exit 0. */
    for (s = 0; s < sizeof(ml_dsa_sets) / sizeof(ml_dsa_sets[0]); s++) {
        if (!pkey_alg_available(ml_dsa_sets[s].name)) {
            printf("SKIP test_ml_dsa: %s is unavailable\n", ml_dsa_sets[s].name);
            return 0;
        }
    }

    test_seed_keygen();
    test_oneshot_kats();
    test_context_string();
#if OPENSSL_VERSION_NUMBER >= 0x30600000L
    test_streaming();
#endif

    printf("test_ml_dsa: all %u checks passed\n", g_checks);
    return 0;
}

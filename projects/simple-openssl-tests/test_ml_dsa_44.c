#include "test_common.h"
#include "test_ml_dsa_common.h"

#include <openssl/evp.h>

/*
 * test_ml_dsa_44 -- ML-DSA-44 (FIPS 204) tests: deterministic seed keygen,
 * one-shot signature known answers, streaming (chunked) signing and
 * verification, tamper detection, and context-string binding -- one binary
 * per parameter set (the shared ML-DSA machinery, the determinism hooks,
 * the KAT provenance and the version gate live in test_ml_dsa_common.c).
 *
 * Known-answer provenance (ML-DSA-44), from the same scratch probe as the
 * sibling binaries:
 *
 *   - SEED_44_HEX:   the key-derivation seed 0x00,0x01,...,0x1f (32 bytes,
 *                    the FIPS 204 xi seed length).
 *   - KAT_SIG_44_HEX: the deterministic one-shot signature of the shared
 *                    8-byte KAT message 00 01 02 03 04 05 06 07 (2420
 *                    bytes, the FIPS 204 signature size).
 *
 * The public/secret key raw export sizes (1312 and 2560 bytes) are asserted
 * in the shared routines; keygen is additionally required to be
 * reproducible run-to-run.
 */

/* This parameter set's KAT constants (the 8-byte KAT message and the
 * context string are family-wide and live in test_ml_dsa_common.c). */
static const char SEED_44_HEX[] =
    "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";

static const char KAT_SIG_44_HEX[] =
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

/* This parameter set's FIPS 204 row. */
static const struct ml_dsa_set set = {
    "ML-DSA-44", 32, 1312, 2560, 2420, SEED_44_HEX, KAT_SIG_44_HEX,
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    /* ML-DSA is version-gated (OpenSSL 3.5+): if this parameter set is not
     * provided by this OpenSSL, take the SKIP path and exit 0. */
    ml_dsa_test_gate(&set);

    ml_dsa_test_seed_keygen(&set);
    ml_dsa_test_oneshot_kats(&set);
    ml_dsa_test_context_string(&set);
#if OPENSSL_VERSION_NUMBER >= 0x30600000L
    ml_dsa_test_streaming(&set);
#endif

    test_report_pass();
    return 0;
}
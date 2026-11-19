#include "test_common.h"

#include "test_null_common.h"

#include <openssl/evp.h>

/*

 * test_null_cipher -- the NULL identity cipher: the smallest possible EVP
 * cipher, useful for probing the API contract itself (the shared
 * commentary lives in test_null_common.c).

 * The NULL cipher ("NULL", also available statically as EVP_enc_null())
 * has a 0-byte key, a 0-byte IV and a block size of 1: every
 * Encrypt/DecryptUpdate passes its input through unchanged and Final
 * emits nothing.  EVP_get_cipherbyname("NULL") returns NULL on the
 * OpenSSL 3.x builds exercised here (3.0.13 and 3.6.5) -- the identity
 * cipher has no OBJ name -- so only the fetch and static routes are
 * checked.  Everything is verified behaviorally (identity roundtrips and
 * zero lengths) -- there are no KAT hex constants to embed for an object
 * whose entire output is empty.
 */

/* One encrypt or decrypt pass through the NULL cipher.  key and iv are
 * NULL: the cipher's key and IV lengths are 0.  Returns the number of
 * bytes emitted (update + final). */
static size_t null_cipher_pass(const EVP_CIPHER *cipher, int encrypt,
                               const unsigned char *in, size_t inlen,
                               unsigned char *out, size_t outcap)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, total = 0;
    int ok;

    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    /* Block size from the cipher object: the context cannot be queried
     * before it has been initialized. */
    CHECK((size_t)EVP_CIPHER_get_block_size(cipher) + inlen <= outcap,
          "output buffer smaller than the EVP update contract requires");
    if (encrypt)
        ok = EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL);
    else
        ok = EVP_DecryptInit_ex(ctx, cipher, NULL, NULL, NULL);
    CHECK(ok == 1, "NULL cipher init failed (key and IV must be NULL)");
    if (encrypt)
        ok = EVP_EncryptUpdate(ctx, out, &len, in, (int)inlen);
    else
        ok = EVP_DecryptUpdate(ctx, out, &len, in, (int)inlen);
    CHECK(ok == 1, "NULL cipher update failed");
    CHECK(len == (int)inlen,
          "NULL cipher update did not pass the input through byte for byte");
    total = len;
    if (encrypt)
        ok = EVP_EncryptFinal_ex(ctx, out + total, &len);
    else
        ok = EVP_DecryptFinal_ex(ctx, out + total, &len);
    CHECK(ok == 1, "NULL cipher final failed");
    CHECK(len == 0, "NULL cipher final emitted unexpected bytes");
    EVP_CIPHER_CTX_free(ctx);
    return (size_t)total;
}

/* (1) The NULL cipher through EVP_CIPHER_fetch: lengths, block size, and
 * an encrypt/decrypt roundtrip of the 64-byte pattern. */
static void test_null_cipher_fetch(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "NULL", NULL);
    unsigned char *in = ALLOCATE_INPUT(NULL_PATTERN_LEN);
    unsigned char *ct = ALLOCATE_OUTPUT(NULL_PATTERN_LEN + 1);
    unsigned char *back = ALLOCATE_OUTPUT(NULL_PATTERN_LEN + 1);
    size_t ctlen, backlen;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"NULL\") failed");
    CHECK(EVP_CIPHER_get_key_length(cipher) == 0,
          "the NULL cipher key length is not 0");
    CHECK(EVP_CIPHER_get_iv_length(cipher) == 0,
          "the NULL cipher IV length is not 0");
    CHECK(EVP_CIPHER_get_block_size(cipher) == 1,
          "the NULL cipher block size is not 1");

    null_fill_pattern(in, NULL_PATTERN_LEN);
    ctlen = null_cipher_pass(cipher, 1, in, NULL_PATTERN_LEN, ct,
                             NULL_PATTERN_LEN + 1);
    CHECK(ctlen == NULL_PATTERN_LEN, "NULL cipher encryption changed the length");
    unpoison_all();
    CHECK(memcmp(ct, in, NULL_PATTERN_LEN) == 0,
          "NULL cipher encryption did not pass the pattern through");
    repoison_all();

    backlen = null_cipher_pass(cipher, 0, ct, ctlen, back,
                               NULL_PATTERN_LEN + 1);
    CHECK(backlen == NULL_PATTERN_LEN, "NULL cipher decryption changed the length");
    unpoison_all();
    CHECK(memcmp(back, in, NULL_PATTERN_LEN) == 0,
          "NULL cipher roundtrip did not recover the pattern");
    repoison_all();
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (2) The static route EVP_enc_null(): same roundtrip through a second,
 * independent cipher object. */
static void test_null_cipher_static(void)
{
    const EVP_CIPHER *cipher = EVP_enc_null();
    unsigned char *in = ALLOCATE_INPUT(NULL_PATTERN_LEN);
    unsigned char *ct = ALLOCATE_OUTPUT(NULL_PATTERN_LEN + 1);
    unsigned char *back = ALLOCATE_OUTPUT(NULL_PATTERN_LEN + 1);
    size_t ctlen, backlen;

    CHECK(cipher != NULL, "EVP_enc_null() returned NULL");
    null_fill_pattern(in, NULL_PATTERN_LEN);
    ctlen = null_cipher_pass(cipher, 1, in, NULL_PATTERN_LEN, ct,
                             NULL_PATTERN_LEN + 1);
    backlen = null_cipher_pass(cipher, 0, ct, ctlen, back,
                               NULL_PATTERN_LEN + 1);
    unpoison_all();
    CHECK(backlen == NULL_PATTERN_LEN && memcmp(back, in, NULL_PATTERN_LEN) == 0,
          "EVP_enc_null() roundtrip did not recover the pattern");
    repoison_all();

    free_poison_allocations();
}

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    test_null_cipher_fetch();
    test_null_cipher_static();

    test_report_pass();
    return 0;
}

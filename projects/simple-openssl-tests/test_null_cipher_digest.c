#include "poisoning.h"

#include <openssl/evp.h>

/*
 * test_null_cipher_digest -- the NULL identity cipher and the NULL
 * identity digest: the smallest possible EVP objects, useful for probing
 * the API contract itself.
 *
 * The NULL cipher ("NULL", also available statically as EVP_enc_null())
 * has a 0-byte key, a 0-byte IV and a block size of 1: every Encrypt/
 * DecryptUpdate passes its input through unchanged and Final emits
 * nothing.  EVP_get_cipherbyname("NULL") returns NULL on the OpenSSL 3.x
 * builds exercised here (3.0.13 and 3.6.5) -- the identity cipher has no
 * OBJ name -- so only the fetch and static routes are checked.
 *
 * The NULL digest ("NULL", also available statically as EVP_md_null())
 * produces a 0-byte digest of any input.  It is present in both of the
 * OpenSSL 3.x builds exercised here; if some build lacks it, the fetch
 * fails and the program prints SKIP and exits 0 (a genuinely optional
 * digest, not a core one).
 *
 * Everything is verified behaviorally (identity roundtrips and zero
 * lengths) -- there are no KAT hex constants to embed for objects whose
 * entire output is empty.
 */

/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */
static void *ALLOCATE_INPUT(size_t n)  { return poison_alloc(POISON_INPUT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }

static unsigned g_checks = 0;

/* Set when the NULL digest is unavailable in this OpenSSL build; main()
 * then prints the SKIP line and exits 0. */
static int g_no_null_digest = 0;

static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_null_cipher_digest: %s\n", msg);
    exit(1);
}

#define CHECK(cond, msg)                      \
    do {                                      \
        if (!(cond))                          \
            test_failed(msg);                 \
        g_checks++;                           \
    } while (0)

/* A 64-byte deterministic pattern and its length. */
#define PATTERN_LEN 64

static void fill_pattern(unsigned char *buf, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
        buf[i] = (unsigned char)(i * 3 + 1);
}

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
    unsigned char *in = ALLOCATE_INPUT(PATTERN_LEN);
    unsigned char *ct = ALLOCATE_OUTPUT(PATTERN_LEN + 1);
    unsigned char *back = ALLOCATE_OUTPUT(PATTERN_LEN + 1);
    size_t ctlen, backlen;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"NULL\") failed");
    CHECK(EVP_CIPHER_get_key_length(cipher) == 0,
          "the NULL cipher key length is not 0");
    CHECK(EVP_CIPHER_get_iv_length(cipher) == 0,
          "the NULL cipher IV length is not 0");
    CHECK(EVP_CIPHER_get_block_size(cipher) == 1,
          "the NULL cipher block size is not 1");

    fill_pattern(in, PATTERN_LEN);
    ctlen = null_cipher_pass(cipher, 1, in, PATTERN_LEN, ct, PATTERN_LEN + 1);
    CHECK(ctlen == PATTERN_LEN, "NULL cipher encryption changed the length");
    CHECK(memcmp(ct, in, PATTERN_LEN) == 0,
          "NULL cipher encryption did not pass the pattern through");

    backlen = null_cipher_pass(cipher, 0, ct, ctlen, back, PATTERN_LEN + 1);
    CHECK(backlen == PATTERN_LEN, "NULL cipher decryption changed the length");
    CHECK(memcmp(back, in, PATTERN_LEN) == 0,
          "NULL cipher roundtrip did not recover the pattern");
    EVP_CIPHER_free(cipher);
}

/* (2) The static route EVP_enc_null(): same roundtrip through a second,
 * independent cipher object. */
static void test_null_cipher_static(void)
{
    const EVP_CIPHER *cipher = EVP_enc_null();
    unsigned char *in = ALLOCATE_INPUT(PATTERN_LEN);
    unsigned char *ct = ALLOCATE_OUTPUT(PATTERN_LEN + 1);
    unsigned char *back = ALLOCATE_OUTPUT(PATTERN_LEN + 1);
    size_t ctlen, backlen;

    CHECK(cipher != NULL, "EVP_enc_null() returned NULL");
    fill_pattern(in, PATTERN_LEN);
    ctlen = null_cipher_pass(cipher, 1, in, PATTERN_LEN, ct, PATTERN_LEN + 1);
    backlen = null_cipher_pass(cipher, 0, ct, ctlen, back, PATTERN_LEN + 1);
    CHECK(backlen == PATTERN_LEN && memcmp(back, in, PATTERN_LEN) == 0,
          "EVP_enc_null() roundtrip did not recover the pattern");
}

/* (3) The NULL digest through EVP_MD_fetch: size 0, empty one-shot
 * output, empty streaming output, and input-independence. */
static void test_null_digest(void)
{
    EVP_MD *md = EVP_MD_fetch(NULL, "NULL", NULL);
    unsigned char *in = ALLOCATE_INPUT(PATTERN_LEN);
    unsigned char *out = ALLOCATE_OUTPUT(0);
    unsigned char *out2 = ALLOCATE_OUTPUT(0);
    EVP_MD_CTX *ctx = NULL;
    unsigned int olen = 9999;

    if (md == NULL) {
        g_no_null_digest = 1;
        return;
    }
    CHECK(EVP_MD_get_size(md) == 0, "the NULL digest size is not 0");
    fill_pattern(in, PATTERN_LEN);

    CHECK(EVP_Digest(in, PATTERN_LEN, out, &olen, md, NULL) == 1,
          "EVP_Digest with the NULL digest failed");
    CHECK(olen == 0, "the NULL digest one-shot output is not empty");

    ctx = EVP_MD_CTX_new();
    CHECK(ctx != NULL, "EVP_MD_CTX_new failed");
    CHECK(EVP_DigestInit_ex(ctx, md, NULL) == 1, "EVP_DigestInit_ex failed");
    CHECK(EVP_DigestUpdate(ctx, in, PATTERN_LEN) == 1,
          "EVP_DigestUpdate failed");
    olen = 9999;
    CHECK(EVP_DigestFinal_ex(ctx, out, &olen) == 1,
          "EVP_DigestFinal_ex failed");
    CHECK(olen == 0, "the NULL digest streaming output is not empty");
    EVP_MD_CTX_free(ctx);

    /* Any input produces the same (empty) output. */
    olen = 0;
    CHECK(EVP_Digest(in, 1, out2, &olen, md, NULL) == 1,
          "EVP_Digest with a 1-byte input failed");
    CHECK(olen == 0, "the NULL digest output for a 1-byte input is not empty");
    EVP_MD_free(md);
}

/* (4) The static route EVP_md_null(): identical behavior. */
static void test_md_null_static(void)
{
    const EVP_MD *md = EVP_md_null();
    unsigned char *in = ALLOCATE_INPUT(PATTERN_LEN);
    unsigned char *out = ALLOCATE_OUTPUT(0);
    EVP_MD_CTX *ctx = NULL;
    unsigned int olen = 9999;

    CHECK(md != NULL, "EVP_md_null() returned NULL");
    CHECK(EVP_MD_get_size(md) == 0, "EVP_md_null() size is not 0");
    fill_pattern(in, PATTERN_LEN);
    ctx = EVP_MD_CTX_new();
    CHECK(ctx != NULL, "EVP_MD_CTX_new failed");
    CHECK(EVP_DigestInit_ex(ctx, md, NULL) == 1, "EVP_DigestInit_ex failed");
    CHECK(EVP_DigestUpdate(ctx, in, PATTERN_LEN) == 1,
          "EVP_DigestUpdate failed");
    CHECK(EVP_DigestFinal_ex(ctx, out, &olen) == 1,
          "EVP_DigestFinal_ex failed");
    CHECK(olen == 0, "EVP_md_null() output is not empty");
    EVP_MD_CTX_free(ctx);
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_null_cipher_fetch();
    test_null_cipher_static();
    test_null_digest();
    if (!g_no_null_digest)
        test_md_null_static();

    if (g_no_null_digest) {
        printf("SKIP test_null_cipher_digest: the NULL digest is unavailable\n");
        return 0;
    }
    printf("test_null_cipher_digest: all %u checks passed\n", g_checks);
    return 0;
}

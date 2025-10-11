#include "internal/deprecated.h"

#include <assert.h>

#include <stdlib.h>
#include <openssl/crypto.h>
#include <openssl/aes.h>
#include "crypto/modes.h"
#include "crypto/aes_platform.h"
#include <stdfil.h>

void ossl_bsaes_cbc_encrypt(const unsigned char *in, unsigned char *out,
                            size_t length, const AES_KEY *key,
                            unsigned char ivec[16], int enc)
{
    zcheck_readonly(in, length);
    zcheck(out, length);
    zcheck_readonly(key, sizeof(AES_KEY));
    zcheck(ivec, 16);
    zunsafe_buf_call(length, "ossl_bsaes_cbc_encrypt", in, out, length, key, ivec, enc);
}

void ossl_bsaes_ctr32_encrypt_blocks(const unsigned char *in,
                                     unsigned char *out, size_t len,
                                     const AES_KEY *key,
                                     const unsigned char ivec[16])
{
    zcheck_readonly(in, zchecked_mul(len, 16));
    zcheck(out, zchecked_mul(len, 16));
    zcheck_readonly(key, sizeof(AES_KEY));
    zcheck_readonly(ivec, 16);
    zunsafe_buf_call(len, "ossl_bsaes_ctr32_encrypt_blocks", in, out, len, key, ivec);
}

void ossl_bsaes_xts_encrypt(const unsigned char *inp, unsigned char *out,
                            size_t len, const AES_KEY *key1,
                            const AES_KEY *key2, const unsigned char iv[16])
{
    zcheck_readonly(inp, len);
    zcheck(out, len);
    zcheck_readonly(key1, sizeof(AES_KEY));
    zcheck_readonly(key2, sizeof(AES_KEY));
    zcheck_readonly(iv, 16);
    zunsafe_buf_call(len, "ossl_bsaes_xts_encrypt", inp, out, len, key1, key2, iv);
}

void ossl_bsaes_xts_decrypt(const unsigned char *inp, unsigned char *out,
                            size_t len, const AES_KEY *key1,
                            const AES_KEY *key2, const unsigned char iv[16])
{
    zcheck_readonly(inp, len);
    zcheck(out, len);
    zcheck_readonly(key1, sizeof(AES_KEY));
    zcheck_readonly(key2, sizeof(AES_KEY));
    zcheck_readonly(iv, 16);
    zunsafe_buf_call(len, "ossl_bsaes_xts_decrypt", inp, out, len, key1, key2, iv);
}


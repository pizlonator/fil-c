#include "internal/deprecated.h"

#include <assert.h>

#include <stdlib.h>
#include <openssl/crypto.h>
#include <openssl/aes.h>
#include "crypto/modes.h"
#include "crypto/aes_platform.h"
#include <stdfil.h>

int aesni_set_encrypt_key(const unsigned char *userKey, int bits,
                          AES_KEY *key)
{
    zcheck_readonly(userKey, zchecked_add(bits, 7) / 8);
    zcheck(key, sizeof(AES_KEY));
    return zunsafe_buf_call(bits, "aesni_set_encrypt_key", userKey, bits, key);
}

int aesni_set_decrypt_key(const unsigned char *userKey, int bits,
                          AES_KEY *key)
{
    zcheck_readonly(userKey, zchecked_add(bits, 7) / 8);
    zcheck(key, sizeof(AES_KEY));
    return zunsafe_buf_call(bits, "aesni_set_decrypt_key", userKey, bits, key);
}

void aesni_encrypt(const unsigned char *in, unsigned char *out,
                   const AES_KEY *key)
{
    zcheck_readonly(in, 16);
    zcheck(out, 16);
    zcheck_readonly(key, sizeof(AES_KEY));
    zunsafe_fast_call("aesni_encrypt", in, out, key);
}

void aesni_decrypt(const unsigned char *in, unsigned char *out,
                   const AES_KEY *key)
{
    zcheck_readonly(in, 16);
    zcheck(out, 16);
    zcheck_readonly(key, sizeof(AES_KEY));
    zunsafe_fast_call("aesni_decrypt", in, out, key);
}

void aesni_ecb_encrypt(const unsigned char *in,
                       unsigned char *out,
                       size_t length, const AES_KEY *key, int enc)
{
    zcheck_readonly(in, length);
    zcheck(out, length);
    zcheck_readonly(key, sizeof(AES_KEY));
    zunsafe_buf_call(length, "aesni_ecb_encrypt", in, out, length, key, enc);
}

void aesni_cbc_encrypt(const unsigned char *in,
                       unsigned char *out,
                       size_t length,
                       const AES_KEY *key, unsigned char *ivec, int enc)
{
    zcheck_readonly(in, length);
    zcheck(out, length);
    zcheck_readonly(key, sizeof(AES_KEY));
    zcheck(ivec, 16);
    zunsafe_buf_call(length, "aesni_cbc_encrypt", in, out, length, key, ivec, enc);
}

#  ifndef OPENSSL_NO_OCB
static size_t l_size(size_t blocks, size_t start_block_num)
{
    size_t blocks_processed = start_block_num - 1;
    size_t all_num_blocks = zchecked_add(blocks, blocks_processed);
    size_t result = 0;
    while (all_num_blocks >>= 1)
        result = zchecked_add(result, 1);
    return result;
}

void aesni_ocb_encrypt(const unsigned char *in, unsigned char *out,
                       size_t blocks, const void *key,
                       size_t start_block_num,
                       unsigned char offset_i[16],
                       const unsigned char L_[][16],
                       unsigned char checksum[16])
{
    zcheck_readonly(in, zchecked_mul(blocks, 16));
    zcheck(out, zchecked_mul(blocks, 16));
    zcheck_readonly(key, sizeof(AES_KEY));
    zcheck(offset_i, 16);
    zcheck_readonly(L_, zchecked_mul(16, l_size(blocks, start_block_num)));
    zcheck(checksum, 16);
    zunsafe_buf_call(blocks, "aesni_ocb_encrypt", in, out, blocks, key, start_block_num, offset_i, L_, checksum);
}

void aesni_ocb_decrypt(const unsigned char *in, unsigned char *out,
                       size_t blocks, const void *key,
                       size_t start_block_num,
                       unsigned char offset_i[16],
                       const unsigned char L_[][16],
                       unsigned char checksum[16])
{
    zcheck_readonly(in, zchecked_mul(blocks, 16));
    zcheck(out, zchecked_mul(blocks, 16));
    zcheck_readonly(key, sizeof(AES_KEY));
    zcheck(offset_i, 16);
    zcheck_readonly(L_, zchecked_mul(16, l_size(blocks, start_block_num)));
    zcheck(checksum, 16);
    zunsafe_buf_call(blocks, "aesni_ocb_decrypt", in, out, blocks, key, start_block_num, offset_i, L_, checksum);
}
#  endif /* OPENSSL_NO_OCB */

void aesni_ctr32_encrypt_blocks(const unsigned char *in,
                                unsigned char *out,
                                size_t blocks,
                                const void *key, const unsigned char *ivec)
{
    zcheck_readonly(in, zchecked_mul(blocks, 16));
    zcheck(out, zchecked_mul(blocks, 16));
    zcheck_readonly(key, sizeof(AES_KEY));
    zcheck_readonly(ivec, 16);
    zunsafe_buf_call(blocks, "aesni_ctr32_encrypt_blocks", in, out, blocks, key, ivec);
}

void aesni_xts_encrypt(const unsigned char *in,
                       unsigned char *out,
                       size_t length,
                       const AES_KEY *key1, const AES_KEY *key2,
                       const unsigned char iv[16])
{
    zcheck_readonly(in, length);
    zcheck(out, length);
    zcheck_readonly(key1, sizeof(AES_KEY));
    zcheck_readonly(key2, sizeof(AES_KEY));
    zcheck_readonly(iv, 16);
    zunsafe_buf_call(length, "aesni_xts_encrypt", in, out, length, key1, key2, iv);
}

void aesni_xts_decrypt(const unsigned char *in,
                       unsigned char *out,
                       size_t length,
                       const AES_KEY *key1, const AES_KEY *key2,
                       const unsigned char iv[16])
{
    zcheck_readonly(in, length);
    zcheck(out, length);
    zcheck_readonly(key1, sizeof(AES_KEY));
    zcheck_readonly(key2, sizeof(AES_KEY));
    zcheck_readonly(iv, 16);
    zunsafe_buf_call(length, "aesni_xts_decrypt", in, out, length, key1, key2, iv);
}

void aesni_ccm64_encrypt_blocks(const unsigned char *in,
                                unsigned char *out,
                                size_t blocks,
                                const void *key,
                                const unsigned char ivec[16],
                                unsigned char cmac[16])
{
    zcheck_readonly(in, zchecked_mul(blocks, 16));
    zcheck(out, zchecked_mul(blocks, 16));
    zcheck_readonly(key, sizeof(AES_KEY));
    zcheck_readonly(ivec, 16);
    zcheck(cmac, 16);
    zunsafe_buf_call(blocks, "aesni_ccm64_encrypt_blocks", in, out, blocks, key, ivec, cmac);
}

void aesni_ccm64_decrypt_blocks(const unsigned char *in,
                                unsigned char *out,
                                size_t blocks,
                                const void *key,
                                const unsigned char ivec[16],
                                unsigned char cmac[16])
{
    zcheck_readonly(in, zchecked_mul(blocks, 16));
    zcheck(out, zchecked_mul(blocks, 16));
    zcheck_readonly(key, sizeof(AES_KEY));
    zcheck_readonly(ivec, 16);
    zcheck(cmac, 16);
    zunsafe_buf_call(blocks, "aesni_ccm64_decrypt_blocks", in, out, blocks, key, ivec, cmac);
}


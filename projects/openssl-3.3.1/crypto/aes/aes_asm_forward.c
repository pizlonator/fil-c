#include "internal/deprecated.h"

#include <assert.h>

#include <stdlib.h>
#include <openssl/crypto.h>
#include <openssl/aes.h>
#include "aes_local.h"
#include <stdfil.h>

int AES_set_encrypt_key(const unsigned char *userKey, const int bits,
                        AES_KEY *key)
{
    zcheck_readonly(userKey, zchecked_add(bits, 7) / 8);
    zcheck(key, sizeof(AES_KEY));
    return zunsafe_buf_call(bits, "AES_set_encrypt_key", userKey, bits, key);
}

int AES_set_decrypt_key(const unsigned char *userKey, const int bits,
                        AES_KEY *key)
{
    zcheck_readonly(userKey, zchecked_add(bits, 7) / 8);
    zcheck(key, sizeof(AES_KEY));
    return zunsafe_buf_call(bits, "AES_set_decrypt_key", userKey, bits, key);
}

void AES_encrypt(const unsigned char *in, unsigned char *out,
                 const AES_KEY *key)
{
    zcheck_readonly(in, 16);
    zcheck(out, 16);
    zcheck_readonly(key, sizeof(AES_KEY));
    zunsafe_fast_call("AES_encrypt", in, out, key);
}

void AES_decrypt(const unsigned char *in, unsigned char *out,
                 const AES_KEY *key)
{
    zcheck_readonly(in, 16);
    zcheck(out, 16);
    zcheck_readonly(key, sizeof(AES_KEY));
    zunsafe_fast_call("AES_decrypt", in, out, key);
}

void AES_cbc_encrypt(const unsigned char *in, unsigned char *out,
                     size_t length, const AES_KEY *key,
                     unsigned char *ivp, const int enc)
{
    zcheck_readonly(in, length);
    zcheck(out, length);
    zcheck_readonly(key, sizeof(AES_KEY));
    zcheck(ivp, 16);
    zunsafe_buf_call(length, "AES_cbc_encrypt", in, out, length, key, ivp, enc);
}

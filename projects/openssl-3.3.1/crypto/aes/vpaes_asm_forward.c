#include "internal/deprecated.h"

#include <assert.h>

#include <stdlib.h>
#include <openssl/crypto.h>
#include <openssl/aes.h>
#include "crypto/modes.h"
#include "crypto/aes_platform.h"
#include <stdfil.h>

int vpaes_set_encrypt_key(const unsigned char *userKey, const int bits,
                          AES_KEY *key)
{
    zcheck_readonly(userKey, (bits + 7) / 8);
    zcheck(key, sizeof(AES_KEY));
    return zunsafe_buf_call(bits, "vpaes_set_encrypt_key", userKey, bits, key);
}

int vpaes_set_decrypt_key(const unsigned char *userKey, const int bits,
                          AES_KEY *key)
{
    zcheck_readonly(userKey, (bits + 7) / 8);
    zcheck(key, sizeof(AES_KEY));
    return zunsafe_buf_call(bits, "vpaes_set_decrypt_key", userKey, bits, key);
}

void vpaes_encrypt(const unsigned char *in, unsigned char *out,
                   const AES_KEY *key)
{
    zcheck_readonly(in, 16);
    zcheck(out, 16);
    zcheck_readonly(key, sizeof(AES_KEY));
    zunsafe_fast_call("vpaes_encrypt", in, out, key);
}

void vpaes_decrypt(const unsigned char *in, unsigned char *out,
                   const AES_KEY *key)
{
    zcheck_readonly(in, 16);
    zcheck(out, 16);
    zcheck_readonly(key, sizeof(AES_KEY));
    zunsafe_fast_call("vpaes_decrypt", in, out, key);
}

void vpaes_cbc_encrypt(const unsigned char *in, unsigned char *out,
                       size_t length, const AES_KEY *key,
                       unsigned char *ivp, const int enc)
{
    zcheck_readonly(in, length);
    zcheck(out, length);
    zcheck_readonly(key, sizeof(AES_KEY));
    zcheck(ivp, 16);
    zunsafe_buf_call(length, "vpaes_cbc_encrypt", in, out, length, key, ivp, enc);
}

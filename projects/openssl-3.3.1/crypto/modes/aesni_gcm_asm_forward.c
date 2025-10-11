#include "internal/deprecated.h"

#include <assert.h>

#include <stdlib.h>
#include <openssl/crypto.h>
#include <openssl/aes.h>
#include "crypto/modes.h"
#include "crypto/aes_platform.h"
#include <stdfil.h>
#include <stddef.h>

size_t aesni_gcm_encrypt(const unsigned char *in, unsigned char *out, size_t len,
                         const void *key, unsigned char ivec[16], u64 *Xi)
{
    zcheck_readonly(in, len);
    zcheck(out, len);
    zcheck_readonly(key, sizeof(AES_KEY));
    zcheck(ivec, 16);
    zcheck(Xi, sizeof(GCM128_CONTEXT) - offsetof(GCM128_CONTEXT, Xi));
    return zunsafe_buf_call(len, "aesni_gcm_encrypt", in, out, len, key, ivec, Xi);
}

size_t aesni_gcm_decrypt(const unsigned char *in, unsigned char *out, size_t len,
                         const void *key, unsigned char ivec[16], u64 *Xi)
{
    zcheck_readonly(in, len);
    zcheck(out, len);
    zcheck_readonly(key, sizeof(AES_KEY));
    zcheck(ivec, 16);
    zcheck(Xi, sizeof(GCM128_CONTEXT) - offsetof(GCM128_CONTEXT, Xi));
    return zunsafe_buf_call(len, "aesni_gcm_decrypt", in, out, len, key, ivec, Xi);
}


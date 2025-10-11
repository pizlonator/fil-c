#include "internal/deprecated.h"

#include <assert.h>

#include <stdlib.h>
#include <openssl/crypto.h>
#include <openssl/aes.h>
#include <openssl/sha.h>
#include "crypto/modes.h"
#include "crypto/aes_platform.h"
#include <stdfil.h>

int aesni_cbc_sha256_enc(const void *inp, void *out, size_t blocks,
                         const AES_KEY *key, unsigned char iv[16],
                         SHA256_CTX *ctx, const void *in0)
{
    zcheck_readonly(inp, zchecked_mul(blocks, 16));
    zcheck(out, zchecked_mul(blocks, 16));
    if (key)
        zcheck_readonly(key, sizeof(AES_KEY));
    if (iv)
        zcheck(iv, 16);
    if (ctx)
        zcheck(ctx, sizeof(SHA_CTX));
    zcheck_readonly(in0, zchecked_mul(blocks, 16));
    return zunsafe_call("aesni_cbc_sha256_enc", inp, out, blocks, key, iv, ctx, in0);
}



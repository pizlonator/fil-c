#include "internal/deprecated.h"

#include <assert.h>

#include <stdlib.h>
#include <openssl/crypto.h>
#include <openssl/aes.h>
#include <openssl/sha.h>
#include "crypto/modes.h"
#include "crypto/aes_platform.h"
#include <stdfil.h>

void aesni_cbc_sha1_enc(const void *inp, void *out, size_t blocks,
                        const AES_KEY *key, unsigned char iv[16],
                        SHA_CTX *ctx, const void *in0)
{
    zcheck_readonly(inp, zchecked_mul(blocks, 16));
    zcheck(out, zchecked_mul(blocks, 16));
    zcheck_readonly(key, sizeof(AES_KEY));
    zcheck(iv, 16);
    zcheck(ctx, sizeof(SHA_CTX));
    zcheck_readonly(in0, zchecked_mul(blocks, 16));
    zunsafe_call("aesni_cbc_sha1_enc", inp, out, blocks, key, iv, ctx, in0);
}

/* The stitched decrypt thing seems to be implemented by disabled. */
#if 0
void aesni256_cbc_sha1_dec(const void *inp, void *out, size_t blocks,
                           const AES_KEY *key, unsigned char iv[16],
                           SHA_CTX *ctx, const void *in0)
{
    zcheck_readonly(inp, zchecked_mul(blocks, 16));
    zcheck(out, zchecked_mul(blocks, 16));
    zcheck_readonly(key, sizeof(AES_KEY));
    zcheck(iv, 16);
    zcheck(ctx, sizeof(SHA_CTX));
    zcheck_readonly(in0, zchecked_mul(blocks, 16));
    zunsafe_call("aesni256_cbc_sha1_dec", inp, out, blocks, key, iv, ctx, in0);
}
#endif


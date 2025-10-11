#include "internal/deprecated.h"

#include <openssl/rc4.h>
#include <openssl/md5.h>
#include "rc4_local.h"
#include <openssl/opensslv.h>
#include <stdfil.h>

void rc4_md5_enc(RC4_KEY *key, const void *in0, void *out,
                 MD5_CTX *ctx, const void *inp, size_t blocks)
{
    zcheck(key, sizeof(RC4_KEY));
    zcheck(ctx, sizeof(MD5_CTX));
    zcheck_readonly(in0, zchecked_mul(blocks, MD5_CBLOCK));
    zcheck(out, zchecked_mul(blocks, MD5_CBLOCK));
    zcheck_readonly(inp, zchecked_mul(blocks, MD5_CBLOCK));
    zunsafe_call("rc4_md5_enc", key, in0, out, ctx, inp, blocks);
}


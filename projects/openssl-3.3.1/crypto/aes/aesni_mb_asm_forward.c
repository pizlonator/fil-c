#include "internal/deprecated.h"

#include <assert.h>

#include <stdlib.h>
#include <openssl/crypto.h>
#include <openssl/aes.h>
#include "crypto/modes.h"
#include "crypto/aes_platform.h"
#include <stdfil.h>

typedef struct {
    const unsigned char *inp;
    unsigned char *out;
    int blocks;
    u64 iv[2];
} CIPH_DESC;

void aesni_multi_cbc_encrypt(CIPH_DESC *desc, void *key, int n4x)
{
    ZSAFETY_CHECK(n4x == 1 || n4x == 2);
    unsigned n = n4x * 4;
    zcheck(desc, zchecked_mul(sizeof(CIPH_DESC), n));
    unsigned i;
    unsigned total = 0;
    for (i = n; i--;) {
        zcheck_readonly(desc[i].inp, zchecked_mul(desc[i].blocks, 16));
        zcheck(desc[i].out, zchecked_mul(desc[i].blocks, 16));
        total += zchecked_mul(desc[i].blocks, 16);
    }
    zcheck(key, sizeof(AES_KEY));
    zunsafe_buf_call(total, "aesni_multi_cbc_encrypt", desc, key, n4x);
}


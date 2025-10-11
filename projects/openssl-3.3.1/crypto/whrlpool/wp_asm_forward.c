#include "internal/deprecated.h"

#include "internal/cryptlib.h"
#include "wp_local.h"
#include <string.h>

#include <stdfil.h>

void whirlpool_block(WHIRLPOOL_CTX *ctx, const void *inp, size_t n)
{
    zcheck(ctx, sizeof(WHIRLPOOL_CTX));
    zcheck_readonly(inp, zchecked_mul(n, WHIRLPOOL_BBLOCK / 8));
    zunsafe_buf_call(zchecked_mul(n, WHIRLPOOL_BBLOCK / 8), "whirlpool_block", ctx, inp, n);
}

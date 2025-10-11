#include <string.h>

#include "internal/endian.h"
#include "crypto/chacha.h"
#include "crypto/ctype.h"
#include <stdfil.h>

void ChaCha20_ctr32(unsigned char *out, const unsigned char *inp, size_t len,
                    const unsigned int key[8], const unsigned int counter[4])
{
    zcheck(out, len);
    zcheck_readonly(inp, len);
    zcheck_readonly(key, 8 * sizeof(unsigned int));
    zcheck_readonly(counter, 4 * sizeof(unsigned int));
    zunsafe_buf_call(len, "ChaCha20_ctr32", out, inp, len, key, counter);
}

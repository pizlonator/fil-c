#include <openssl/e_os2.h>
#include <string.h>
#include <assert.h>
#include <stdfil.h>

size_t SHA3_absorb(uint64_t A[5][5], const unsigned char *inp, size_t len,
                   size_t r)
{
    ZSAFETY_CHECK(r < (25 * sizeof(A[0][0])));
    ZSAFETY_CHECK((r % 8) == 0);
    zcheck(A, 25 * sizeof(A[0][0]));
    zcheck_readonly(inp, len);
    return zunsafe_buf_call(len, "SHA3_absorb", A, inp, len, r);
}

void SHA3_squeeze(uint64_t A[5][5], unsigned char *out, size_t len, size_t r, int next)
{
    ZSAFETY_CHECK(r < (25 * sizeof(A[0][0])));
    ZSAFETY_CHECK((r % 8) == 0);
    zcheck(A, 25 * sizeof(A[0][0]));
    zcheck(out, len);
    zunsafe_buf_call(len, "SHA3_squeeze", A, out, len, r, next);
}



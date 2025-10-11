#include "internal/deprecated.h"

#include <openssl/rc4.h>
#include "rc4_local.h"
#include <openssl/opensslv.h>
#include <stdfil.h>

void RC4_set_key(RC4_KEY *key, int len, const unsigned char *data)
{
    zcheck(key, sizeof(RC4_KEY));
    zcheck_readonly(data, len);
    zunsafe_buf_call(len, "RC4_set_key", key, len, data);
}

void RC4(RC4_KEY *key, size_t len, const unsigned char *indata,
         unsigned char *outdata)
{
    zcheck(key, sizeof(RC4_KEY));
    zcheck_readonly(indata, len);
    zcheck(outdata, len);
    zunsafe_buf_call(len, "RC4", key, len, indata, outdata);
}

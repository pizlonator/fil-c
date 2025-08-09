// Taken form OpenSSL 3.4-dev, for benchmarking purposes.
// See
// https://github.com/openssl/openssl/blob/0285160ffa3b8c2b5491222243042593808298c4/crypto/evp/encode.c#L303
// and
// https://github.com/openssl/openssl/blob/0285160ffa3b8c2b5491222243042593808298c4/include/openssl/evp.h#L871-L875
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
namespace openssl3 {
struct evp_Encode_Ctx_st {
  /* number saved in a partial encode/decode */
  int num;
  /*
   * The length is either the output line length (in input bytes) or the
   * shortest input line length that is ok.  Once decoding begins, the
   * length is adjusted up each time a longer line is decoded
   */
  int length;
  /* data to encode */
  unsigned char enc_data[80];
  /* number read on current line */
  int line_num;
  unsigned int flags;
};
typedef struct evp_Encode_Ctx_st EVP_ENCODE_CTX;

struct buf_mem_st {
  size_t length; /* current number of bytes */
  char *data;
  size_t max; /* size of buffer */
  unsigned long flags;
};
typedef struct buf_mem_st BUF_MEM;

static int evp_decodeblock_int(EVP_ENCODE_CTX *ctx, unsigned char *t,
                               const unsigned char *f, int n);

template <typename TypeName>
size_t base64_decode(char *const dst, const size_t dstlen,
                     const TypeName *const src, const size_t srclen);

void EVP_DecodeInit(EVP_ENCODE_CTX *ctx);

int EVP_DecodeUpdate(EVP_ENCODE_CTX *ctx, unsigned char *out, int *outl,
                     const unsigned char *in, int inl);

int EVP_DecodeFinal(EVP_ENCODE_CTX *ctx, unsigned char *out, int *outl);
} // namespace openssl3

namespace openssl3 {
//// Base 64 ////

#define EVP_ENCODE_CTX_USE_SRP_ALPHABET 2

#define B64_EOF 0xF2
#define B64_WS 0xE0
#define B64_ERROR 0xFF
#define B64_NOT_BASE64(a) (((a) | 0x13) == 0xF3)
#define B64_BASE64(a) (!B64_NOT_BASE64(a))

static const unsigned char data_ascii2bin[128 * 2] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xE0, 0xF0, 0xFF,
    0xFF, 0xF1, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xE0, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x3E, 0xFF, 0xF2, 0xFF, 0x3F,
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0xFF, 0xFF,
    0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
    0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12,
    0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24,
    0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30,
    0x31, 0x32, 0x33, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF,
};

static const unsigned char srpdata_ascii2bin[128] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xE0, 0xF0, 0xFF,
    0xFF, 0xF1, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xE0, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF2, 0x3E, 0x3F,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xFF, 0xFF,
    0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
    0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C,
    0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E,
    0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A,
    0x3B, 0x3C, 0x3D, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};

inline static unsigned char conv_ascii2bin(unsigned char a,
                                           const unsigned char *table) {
  if (a & 0x80)
    return B64_ERROR;
  return table[a];
}

void OPENSSL_die(const char *message, const char *file, int line) {
  printf("%s:%d: OpenSSL internal error: %s\n", file, line, message);
  abort();
}

#define OPENSSL_assert(e)                                                      \
  (void)((e) ? 0                                                               \
             : (OPENSSL_die("assertion failed: " #e, __FILE__, __LINE__), 1))

void EVP_DecodeInit(EVP_ENCODE_CTX *ctx) {
  /* Only ctx->num and ctx->flags are used during decoding. */
  ctx->num = 0;
  ctx->length = 0;
  ctx->line_num = 0;
  ctx->flags = 0;
}

/*-
 * -1 for error
 *  0 for last line
 *  1 for full line
 *
 * Note: even though EVP_DecodeUpdate attempts to detect and report end of
 * content, the context doesn't currently remember it and will accept more data
 * in the next call. Therefore, the caller is responsible for checking and
 * rejecting a 0 return value in the middle of content.
 *
 * Note: even though EVP_DecodeUpdate has historically tried to detect end of
 * content based on line length, this has never worked properly. Therefore,
 * we now return 0 when one of the following is true:
 *   - Padding or B64_EOF was detected and the last block is complete.
 *   - Input has zero-length.
 * -1 is returned if:
 *   - Invalid characters are detected.
 *   - There is extra trailing padding, or data after padding.
 *   - B64_EOF is detected after an incomplete base64 block.
 */
int EVP_DecodeUpdate(EVP_ENCODE_CTX *ctx, unsigned char *out, int *outl,
                     const unsigned char *in, int inl) {
  int seof = 0, eof = 0, rv = -1, ret = 0, i, v, tmp, n, decoded_len;
  unsigned char *d;
  const unsigned char *table;

  n = ctx->num;
  d = ctx->enc_data;

  if (n > 0 && d[n - 1] == '=') {
    eof++;
    if (n > 1 && d[n - 2] == '=')
      eof++;
  }

  /* Legacy behaviour: an empty input chunk signals end of input. */
  if (inl == 0) {
    rv = 0;
    goto end;
  }

  if ((ctx->flags & EVP_ENCODE_CTX_USE_SRP_ALPHABET) != 0)
    table = srpdata_ascii2bin;
  else
    table = data_ascii2bin;

  for (i = 0; i < inl; i++) {
    tmp = *(in++);
    v = conv_ascii2bin(tmp, table);
    if (v == B64_ERROR) {
      rv = -1;
      goto end;
    }

    if (tmp == '=') {
      eof++;
    } else if (eof > 0 && B64_BASE64(v)) {
      /* More data after padding. */
      rv = -1;
      goto end;
    }

    if (eof > 2) {
      rv = -1;
      goto end;
    }

    if (v == B64_EOF) {
      seof = 1;
      goto tail;
    }

    /* Only save valid base64 characters. */
    if (B64_BASE64(v)) {
      if (n >= 64) {
        /*
         * We increment n once per loop, and empty the buffer as soon as
         * we reach 64 characters, so this can only happen if someone's
         * manually messed with the ctx. Refuse to write any more data.
         */
        rv = -1;
        goto end;
      }
      OPENSSL_assert(n < (int)sizeof(ctx->enc_data));
      d[n++] = tmp;
    }

    if (n == 64) {
      decoded_len = evp_decodeblock_int(ctx, out, d, n);
      n = 0;
      if (decoded_len < 0 || eof > decoded_len) {
        rv = -1;
        goto end;
      }
      ret += decoded_len - eof;
      out += decoded_len - eof;
    }
  }

  /*
   * Legacy behaviour: if the current line is a full base64-block (i.e., has
   * 0 mod 4 base64 characters), it is processed immediately. We keep this
   * behaviour as applications may not be calling EVP_DecodeFinal properly.
   */
tail:
  if (n > 0) {
    if ((n & 3) == 0) {
      decoded_len = evp_decodeblock_int(ctx, out, d, n);
      n = 0;
      if (decoded_len < 0 || eof > decoded_len) {
        rv = -1;
        goto end;
      }
      ret += (decoded_len - eof);
    } else if (seof) {
      /* EOF in the middle of a base64 block. */
      rv = -1;
      goto end;
    }
  }

  rv = seof || (n == 0 && eof) ? 0 : 1;
end:
  /* Legacy behaviour. This should probably rather be zeroed on error. */
  *outl = ret;
  ctx->num = n;
  return rv;
}

// use mega SIMD fast base64 decode.
static int evp_decodeblock_int(EVP_ENCODE_CTX *ctx, unsigned char *t,
                               const unsigned char *f, int n) {
  int i, ret = 0, a, b, c, d;
  unsigned long l;
  const unsigned char *table;

  if (ctx != NULL && (ctx->flags & EVP_ENCODE_CTX_USE_SRP_ALPHABET) != 0)
    table = srpdata_ascii2bin;
  else
    table = data_ascii2bin;

  /* trim whitespace from the start of the line. */
  while ((n > 0) && (conv_ascii2bin(*f, table) == B64_WS)) {
    f++;
    n--;
  }

  /*
   * strip off stuff at the end of the line ascii2bin values B64_WS,
   * B64_EOLN, B64_EOLN and B64_EOF
   */
  while ((n > 3) && (B64_NOT_BASE64(conv_ascii2bin(f[n - 1], table))))
    n--;

  if (n % 4 != 0)
    return -1;

  for (i = 0; i < n; i += 4) {
    a = conv_ascii2bin(*(f++), table);
    b = conv_ascii2bin(*(f++), table);
    c = conv_ascii2bin(*(f++), table);
    d = conv_ascii2bin(*(f++), table);
    if ((a & 0x80) || (b & 0x80) || (c & 0x80) || (d & 0x80))
      return -1;
    l = ((((unsigned long)a) << 18L) | (((unsigned long)b) << 12L) |
         (((unsigned long)c) << 6L) | (((unsigned long)d)));
    *(t++) = (unsigned char)(l >> 16L) & 0xff;
    *(t++) = (unsigned char)(l >> 8L) & 0xff;
    *(t++) = (unsigned char)(l) & 0xff;
    ret += 3;
  }
  return ret;
}

int EVP_DecodeFinal(EVP_ENCODE_CTX *ctx, unsigned char *out, int *outl) {
  int i;

  *outl = 0;
  if (ctx->num != 0) {
    i = evp_decodeblock_int(ctx, out, ctx->enc_data, ctx->num);
    if (i < 0)
      return -1;
    ctx->num = 0;
    *outl = i;
    return 1;
  } else
    return 1;
}

template <typename TypeName>
size_t base64_decode(char *const dst, const size_t dstlen,
                     const TypeName *const src, const size_t srclen) {
  EVP_ENCODE_CTX *ctx = new EVP_ENCODE_CTX();
  int len = dstlen;
  int taillen = 0;

  EVP_DecodeInit(ctx);
  if (EVP_DecodeUpdate(ctx, (unsigned char *)dst, &len, (unsigned char *)src,
                       srclen) < 0 ||
      EVP_DecodeFinal(ctx, (unsigned char *)&(dst[len]), &taillen) < 0) {
    fprintf(stderr, "Invalid input for openssl base64 decode.\n");
    exit(1);
  }
  delete ctx;

  len += taillen;
  return len;
}

} // namespace openssl3
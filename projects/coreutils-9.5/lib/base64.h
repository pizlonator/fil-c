/* base64.h -- Encode binary data using printable characters.
   Copyright (C) 2004-2006, 2009-2024 Free Software Foundation, Inc.
   Written by Simon Josefsson.

   This file is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   This file is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

#ifndef BASE64_H
#define BASE64_H

/* This file uses _GL_INLINE_HEADER_BEGIN.  */
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

/* Get idx_t.  */
#include <idx.h>

/* Pacify GCC in isubase64.  */
#if defined __GNUC__ && 4 < __GNUC__ + (3 <= __GNUC_MINOR__)
# pragma GCC diagnostic ignored "-Wtype-limits"
#endif

_GL_INLINE_HEADER_BEGIN
#ifndef BASE64_INLINE
# define BASE64_INLINE _GL_INLINE
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* This uses that the expression (n+(k-1))/k means the smallest
   integer >= n/k, i.e., the ceiling of n/k.  */
#define BASE64_LENGTH(inlen) ((((inlen) + 2) / 3) * 4)

struct base64_decode_context
{
  int i;
  char buf[4];
};

extern signed char const base64_to_int[256];

BASE64_INLINE bool
isubase64 (unsigned char ch)
{
  return ch < sizeof base64_to_int && 0 <= base64_to_int[ch];
}

BASE64_INLINE bool
isbase64 (char ch)
{
  return isubase64 (ch);
}

extern void base64_encode (const char *restrict in, idx_t inlen,
                           char *restrict out, idx_t outlen);

extern idx_t base64_encode_alloc (const char *in, idx_t inlen, char **out);

/* Initialize decode-context buffer, CTX.  */
BASE64_INLINE void
base64_decode_ctx_init (struct base64_decode_context *ctx)
{
  ctx->i = 0;
}

extern bool base64_decode_ctx (struct base64_decode_context *ctx,
                               const char *restrict in, idx_t inlen,
                               char *restrict out, idx_t *outlen);

extern bool base64_decode_alloc_ctx (struct base64_decode_context *ctx,
                                     const char *in, idx_t inlen,
                                     char **out, idx_t *outlen);

#define base64_decode(in, inlen, out, outlen) \
        base64_decode_ctx (NULL, in, inlen, out, outlen)

#define base64_decode_alloc(in, inlen, out, outlen) \
        base64_decode_alloc_ctx (NULL, in, inlen, out, outlen)

#ifdef __cplusplus
}
#endif

_GL_INLINE_HEADER_END

#endif /* BASE64_H */

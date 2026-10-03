/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│ vi: set et ft=c ts=2 sts=2 sw=8 fenc=utf-8                               :vi │
╞═════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2022 Justine Alexandra Roberts Tunney                              │
│ Fil-C port additions 2026                                                    │
│                                                                              │
│ Permission to use, copy, modify, and/or distribute this software for         │
│ any purpose with or without fee is hereby granted, provided that the         │
│ above copyright notice and this permission notice appear in all copies.      │
│                                                                              │
│ THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL                │
│ WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED                │
│ WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE             │
│ AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL         │
│ DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR        │
│ PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER               │
│ TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR             │
│ PERFORMANCE OF THIS SOFTWARE.                                                │
╚─────────────────────────────────────────────────────────────────────────────*/
/* Fil-C port: implement swprintf().
 *
 * Upstream cosmo stubs swprintf() out with a kprintf + _Exit, because cosmo's
 * printf machinery only formats narrow strings.  That stub kills every
 * program that uses libc++'s std::to_wstring(float/double/long double) (they
 * format through swprintf), and libc++/libc++abi are the reason this libc
 * needs to be complete, so implement the real thing here:
 *
 * 1. translate the wide format to an equivalent narrow format, and
 * 2. run vsnprintf() (which works), then
 * 3. widen the result back with mbstowcs().
 *
 * The translation is exact for the flags/width/precision/length-modifier
 * grammar, whose characters are all ASCII: %s and %ls mean the same thing to
 * both printf families, and the output of the numeric conversions is pure
 * ASCII, so the narrow byte count is the wide character count.  Non-ASCII
 * formats (and non-ASCII output, e.g. a %c argument outside ASCII) fail with
 * EILSEQ instead of misprinting. */
#include "libc/errno.h"
#include "libc/mem/mem.h"
#include "libc/stdio/stdio.h"
#include "libc/str/str.h"

#define KMAXFMT 256

static int translate_format(const wchar_t *wfmt, char **out) {
  size_t n = 0;
  while (wfmt[n]) {
    if ((unsigned)wfmt[n] > 126) {
      errno = EILSEQ;
      return -1;
    }
    ++n;
  }
  if (n > KMAXFMT - 1) {
    errno = EINVAL;
    return -1;
  }
  char *fmt = *out;
  /* verbatim copy with validation: every ASCII format character means the
   * same thing to the wide and narrow printf families */
  for (size_t i = 0; i < n; ++i) {
    wchar_t w = wfmt[i];
    char c = (char)w;
    fmt[i] = c;
    if (c == '%') {
      /* validate flags, width, precision and length modifiers */
      for (++i; i < n; ++i) {
        w = wfmt[i];
        if ((unsigned)w > 126) {
          errno = EILSEQ;
          return -1;
        }
        char s = (char)w;
        fmt[i] = s;
        if (s == 'C' || s == 'S') {
          /* wide-only conversions with no narrow equivalent */
          errno = EINVAL;
          return -1;
        }
        if (strchr("diouxXfFeEgGaAcspn%", s))
          break;
        if (!strchr("-+ #0123456789.hljztL", s)) {
          errno = EINVAL;
          return -1;
        }
      }
      if (i >= n) {
        errno = EINVAL;
        return -1;
      }
    }
  }
  fmt[n] = 0;
  return (int)n;
}

int vswprintf(wchar_t *ws, size_t n, const wchar_t *format, va_list va) {
  char fmt[KMAXFMT];
  char *fmtp = fmt;
  int rc = translate_format(format, &fmtp);
  if (rc == -1)
    return -1;

  /* measure, then render (the incoming va_list may only be walked once, so
   * each pass gets its own copy) */
  va_list va1;
  va_copy(va1, va);
  int len = vsnprintf(0, 0, fmtp, va1);
  va_end(va1);
  if (len < 0)
    return -1;
  /* C11: n or more wide characters => negative return (truncation) */
  if ((size_t)len >= n)
    return -1;
  if ((size_t)len >= KMAXFMT - 1) {
    fmtp = (char *)malloc(len + 1);
    if (!fmtp)
      return -1;
    rc = translate_format(format, &fmtp);
    if (rc == -1) {
      free(fmtp);
      return -1;
    }
  }

  char buf[KMAXFMT];
  char *bufp = buf;
  char *heap = 0;
  if ((size_t)len >= KMAXFMT - 1) {
    heap = (char *)malloc(len + 1);
    if (!heap) {
      if (fmtp != fmt)
        free(fmtp);
      return -1;
    }
    bufp = heap;
  }
  va_list va2;
  va_copy(va2, va);
  vsnprintf(bufp, len + 1, fmtp, va2);
  va_end(va2);

  /* widen; the narrow result is treated as UTF-8, matching what cosmo's
   * printf emits for %s arguments and what callers should be handing over */
  size_t wlen = mbstowcs(ws, bufp, n);
  if (fmtp != fmt)
    free(fmtp);
  if (heap)
    free(heap);
  if (wlen == (size_t)-1) {
    errno = EILSEQ;
    return -1;
  }
  return (int)wlen;
}

int swprintf(wchar_t *ws, size_t n, const wchar_t *format, ...) {
  va_list va;
  va_start(va, format);
  int rc = vswprintf(ws, n, format, va);
  va_end(va);
  return rc;
}
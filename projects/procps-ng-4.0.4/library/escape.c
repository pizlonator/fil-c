/*
 * escape.c - printing handling
 *
 * Copyright © 2011-2023 Jim Warner <james.warner@comcast.net>
 * Copyright © 2016-2023 Craig Small <csmall@dropbear.xyz>
 * Copyright © 1998-2005 Albert Cahalan
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "escape.h"
#include "readproc.h"
#include "nls.h"

#define SECURE_ESCAPE_ARGS(dst, bytes) do { \
  if ((bytes) <= 0) return 0; \
  *(dst) = '\0'; \
  if ((bytes) >= INT_MAX) return 0; \
} while (0)

static const char UTF_tab[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x00 - 0x0F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x10 - 0x1F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x20 - 0x2F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x30 - 0x3F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x40 - 0x4F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x50 - 0x5F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x60 - 0x6F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x70 - 0x7F
   -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, // 0x80 - 0x8F
   -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, // 0x90 - 0x9F
   -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, // 0xA0 - 0xAF
   -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, // 0xB0 - 0xBF
   -1,-1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // 0xC0 - 0xCF
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // 0xD0 - 0xDF
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, // 0xE0 - 0xEF
    4, 4, 4, 4, 4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, // 0xF0 - 0xFF
};

static const unsigned char ESC_tab[] = {
   "@..............................." // 0x00 - 0x1F
   "||||||||||||||||||||||||||||||||" // 0x20 - 0x3F
   "||||||||||||||||||||||||||||||||" // 0x40 - 0x5f
   "|||||||||||||||||||||||||||||||." // 0x60 - 0x7F
   "????????????????????????????????" // 0x80 - 0x9F
   "????????????????????????????????" // 0xA0 - 0xBF
   "????????????????????????????????" // 0xC0 - 0xDF
   "????????????????????????????????" // 0xE0 - 0xFF
};

static inline void esc_all (unsigned char *str) {
   unsigned char c;

   // if bad locale/corrupt str, replace non-printing stuff
   while (*str) {
      if ((c = ESC_tab[*str]) != '|')
         *str = c;
      ++str;
   }
}

static inline void esc_ctl (unsigned char *str, int len) {
 #define setQ  { *str = '?'; n = 1; goto next_up; }
   int i, n, x;

   for (i = 0; i < len; ) {
      n = UTF_tab[*str];
      /* even with a proper locale, strings might be corrupt or we
         might encounter one of those 32 unicode multibyte control
         characters which begin at U+0080 (0xc280) */
      if (n < 0 || i + n > len
      || (*str == 0xc2 && str[1] >= 0x80 && str[1] <= 0x9f)) {
         setQ
      }
      /* let's validate those utf-8 continuation bytes too, all of
         which must take the binary form of 10xxxxxx */
      for (x = 1; x < n; x++) {
         if (str[x] < 0x80 || str[x] > 0xbf) {
            setQ
         }
      }
      // and eliminate those non-printing control characters
      if (*str < 0x20 || *str == 0x7f)
         *str = '?';
next_up:
      str += n;
      i += n;
   }
 #undef setQ
}

int escape_str (char *dst, const char *src, int bufsize) {
   static __thread int utf_sw = 0;
   int n;

   if (utf_sw == 0) {
      char *enc = nl_langinfo(CODESET);
      utf_sw = enc && strcasecmp(enc, "UTF-8") == 0 ? 1 : -1;
   }
   SECURE_ESCAPE_ARGS(dst, bufsize);
   n = snprintf(dst, bufsize, "%s", src);
   if (n < 0) {
      *dst = '\0';
      return 0;
   }
   if (n >= bufsize) n = bufsize-1;
   if (utf_sw < 0)
      esc_all((unsigned char *)dst);
   else
      esc_ctl((unsigned char *)dst, n);
   return n;
}

int escape_command (char *outbuf, const proc_t *pp, int bytes, unsigned flags) {
   int overhead = 0;
   int end = 0;

   if (flags & ESC_BRACKETS)
      overhead += 2;
   if (flags & ESC_DEFUNCT) {
      if (pp->state == 'Z') overhead += 10;    // chars in " <defunct>"
      else flags &= ~ESC_DEFUNCT;
   }
   if (overhead + 1 >= bytes) {
      // if no room for even one byte of the command name
      outbuf[0] = '\0';
      return 0;
   }
   if (flags & ESC_BRACKETS)
      outbuf[end++] = '[';
   end += escape_str(outbuf+end, pp->cmd, bytes-overhead);
   // we want "[foo] <defunct>", not "[foo <defunct>]"
   if (flags & ESC_BRACKETS)
      outbuf[end++] = ']';
   if (flags & ESC_DEFUNCT) {
      memcpy(outbuf+end, " <defunct>", 10);
      end += 10;
   }
   outbuf[end] = '\0';
   return end;  // bytes, not including the NUL
}

/*
 * Copyright (c) 2004
 * Kevin Atkinson
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation.  Kevin Atkinson makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 */

/*
 * Format:
 *   <data> ::= 0x02 <line>+ 0x1F 0xFF
 *   <line> ::= <prefix> <rest>*
 *   <prefix> ::= 0x00..0x1D | 0x1E 0xFF* 0x00..0xFE
 *   <rest> ::= 0x20..0xFF | <escape>
 *   <escape> ::= 0x1F 0x20..0x3F
 *
 * To decompress:
 *   Take the first PREFIX_LEN characters from the previous line
 *   and concatenate that with the rest, unescaping as necessary.
 *   The PREFIX_LEN is the sum of the characters in <prefix>.
 *   To unescape take the second character of <escape> and subtract 0x20.
 *   If the prefix length is computed before unescaping characters.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#if defined(__CYGWIN__) || defined (_WIN32)

#  include <io.h>
#  include <fcntl.h>

#  define SETBIN(fno)  _setmode( _fileno( fno ), _O_BINARY )

#else

#  define SETBIN(fno)

#endif

#define HEAD "prezip, a prefix delta compressor. Version 0.1.1, 2004-11-06"

typedef struct Word {
  char * str;
  size_t alloc;
} Word;

#define INSURE_SPACE(cur,p,need)\
  do {\
    size_t pos = p - (cur)->str;\
    if (pos + need + 1 < (cur)->alloc) break;\
    (cur)->alloc = (cur)->alloc*3/2;\
    (cur)->str = (char *)realloc((cur)->str, (cur)->alloc);\
    p = (cur)->str + pos;\
  } while (0)

#define ADV(w, c) do {char * s = w + c;\
                      while(w != s) {\
                        if (*w == 0) ret = 3;\
                        ++w;}} while (0)

int main (int argc, const char *argv[]) {

  if (argc < 2) {

    goto usage;

  } else if (strcmp(argv[1], "-z") == 0) {

    Word w1,w2;
    Word * prev = &w1;
    Word * cur  = &w2;
    char * w = 0;
    char * p = 0;
    int c,l;

    w1.str = (char *)malloc(256);
    w1.str[0] = '\0';
    w1.alloc = 256;
    w2.str = (char *)malloc(256);
    w2.alloc = 256;

    SETBIN (stdout);

    putc(2, stdout);

    c = 0;
    while (c != EOF)
    {
      /* get next word */
      w = cur->str;
      while (c = getc(stdin), c != EOF && c != '\n') {
        if (c >= 32) {
          INSURE_SPACE(cur, w, 1);
          *w++ = c;
        } else {
          INSURE_SPACE(cur, w, 2);
          *w++ = 31;
          *w++ = c + 32;
        }
      }

      *w = 0;
      p = prev->str;
      w = cur->str;

      /* get the length of the prefix */
      l = 0;
      while (p[l] != '\0' && p[l] == w[l]) ++l;

      /* prefix compress, and write word */
      if (l < 30) {
        putc(l, stdout);
      } else {
        int i = l - 30;
        putc(30, stdout);
        while (i >= 255) {putc(255, stdout); i -= 255;}
	putc(i, stdout);
      }
      fputs(w+l, stdout);

      /* swap prev and next */
      {
        Word * tmp = cur;
        cur = prev;
        prev = tmp;
      }
    }

    putc(31, stdout);
    putc(255, stdout);

    free(w1.str);
    free(w2.str);

  } else if (strcmp(argv[1], "-d") == 0) {

    int ret = 0;

    Word cur;
    int c;
    char * w;
    unsigned char ch;

    cur.str = (char *)malloc(256);
    cur.alloc = 256;
    w = cur.str;

    SETBIN (stdin);

    c = getc(stdin);

    if (c == 2)
    {
      *w = '\0';
      while (c != EOF && ret <= 0) {
        ret = -1;
        if (c != 2) {ret = 3; break;}
        c = getc(stdin);
        while (ret < 0) {
          w = cur.str;
          ADV(w, c);
          if (c == 30) {
            while (c = getc(stdin), c == 255) ADV(w, 255);
            ADV(w, c);
          }
          while (c = getc(stdin), c > 30) {
            INSURE_SPACE(&cur,w,1);
            *w++ = (char)c;
          }
          *w = '\0';
          for (w = cur.str; *w; w++) {
            if (*w != 31) {
              putc(*w, stdout);
            } else {
              ++w;
              ch = *w;
              if (32 <= ch && ch < 64) {
                putc(ch - 32, stdout);
              } else if (ch == 255) {
                if (w[1] != '\0') ret = 3;
                else              ret = 0;
              } else {
                ret = 3;
              }
            }
          }
          if (ret < 0 && c == EOF) ret = 4;
          if (ret != 0)
            putc('\n', stdout);
        }
      }
    }
    else if (c == 1)
    {
      int last_max = 0;
      while (c != -1) {
        if (c == 0)
          c = getc(stdin);
        --c;
        if (c < 0 || c > last_max) {ret = 3; break;}
        w = cur.str + c;
        while (c = getc(stdin), c > 32) {
          INSURE_SPACE(&cur,w,1);
          *w++ = (char)c;
        }
        *w = '\0';
        last_max = w - cur.str;
        fputs(cur.str, stdout);
        putc('\n', stdout);
      }
    }
    else
    {
      ret = 2;
    }

    assert(ret >= 0);
    if (ret > 0 && argc > 2)
      fputs(argv[2], stderr);
    if (ret == 2)
      fputs("unknown format\n", stderr);
    else if (ret == 3)
      fputs("corrupt input\n", stderr);
    else if (ret == 4)
      fputs("unexpected EOF\n", stderr);

    free (cur.str);

    return ret;

  } else if (strcmp(argv[1], "-V") == 0) {

    printf("%s\n", HEAD);

  } else {

    goto usage;

  }

  return 0;

  usage:

  printf("%s\n"
         "Usage:\n"
         "  To Compress:   %s -z\n"
         "  To Decompress: %s -d\n", HEAD, argv[0], argv[0]);
  return 1;
}

/* Test some emoji transliterations

   Copyright (C) 2019-2024 Free Software Foundation, Inc.
   Copyright The GNU Toolchain Authors.

   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include <iconv.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <support/check.h>

static int
do_test (void)
{
  iconv_t cd;

  const int num_emojis = 70;

  const char str[] = "♡ ♥ ❤ 💙 💓 "
                     "💔 💖 💗 💚 💛 "
                     "💜 🖤 🧡 🤍 🤎 "
                     "😀 😁 😂 😃 😄 "
                     "😅 😆 😇 😈 😉 "
                     "😊 😋 😌 😍 😎 "
                     "😏 😐 😑 😒 😓 "
                     "😔 😕 😖 😗 😘 "
                     "😙 😚 😛 😜 😝 "
                     "😞 😟 😠 😡 😢 "
                     "😣 😦 😧 😨 😩 "
                     "😭 😮 😯 😰 😱 "
                     "😲 😸 😹 😺 😻 "
                     "😼 😽 🙁 🙂 🙃";

  const char expected[] = "<3 <3 <3 <3 <3 "
                          "</3 <3 <3 <3 <3 "
                          "<3 <3 <3 <3 <3 "
                          ":-D :-D :'D :-D :-D "
                          ":-D :-D O:-) >:) ;-) "
                          ":-) :-P :-) :-* B-) "
                          ";-) :-| :-| :-| :'-| "
                          ":-| :-/ :-S :-* :-* "
                          ":-* :-* :-P ;-P X-P "
                          ":-( :-( >:-( :-( :'-( "
                          "X-( :-O :-O :-O :-O "
                          ":\"-( :-O :-O :'-O :-O "
                          ":-O :-3 :'-3 :-3 :-3 "
                          ";-3 :-3 :-( :-) (-:";

  char *inptr = (char *) str;
  size_t inlen = strlen (str) + 1;
  char outbuf[500];
  char *outptr = outbuf;
  size_t outlen = sizeof (outbuf);
  int result = 0;
  size_t n;

  if (setlocale (LC_ALL, "en_US.UTF-8") == NULL)
    FAIL_EXIT1 ("setlocale failed");

  cd = iconv_open ("ASCII//TRANSLIT", "UTF-8");
  if (cd == (iconv_t) -1)
    FAIL_EXIT1 ("iconv_open failed");

  n = iconv (cd, &inptr, &inlen, &outptr, &outlen);
  if (n != num_emojis)
    {
      if (n == (size_t) -1)
        printf ("iconv() returned error: %m\n");
      else
        printf ("iconv() returned %zd, expected %d\n", n, num_emojis);
      result = 1;
    }
  if (inlen != 0)
    {
      puts ("not all input consumed");
      result = 1;
    }
  else if (inptr - str != strlen (str) + 1)
    {
      printf ("inptr wrong, advanced by %td\n", inptr - str);
      result = 1;
    }
  if (memcmp (outbuf, expected, sizeof (expected)) != 0)
    {
      printf ("result wrong: \"%.*s\", expected: \"%s\"\n",
              (int) (sizeof (outbuf) - outlen), outbuf, expected);
      result = 1;
    }
  else if (outlen != sizeof (outbuf) - sizeof (expected))
    {
      printf ("outlen wrong: %zd, expected %zd\n", outlen,
              sizeof (outbuf) - sizeof (expected));
      result = 1;
    }
  else
    printf ("output is \"%s\" which is OK\n", outbuf);

  return result;
}

#include <support/test-driver.c>

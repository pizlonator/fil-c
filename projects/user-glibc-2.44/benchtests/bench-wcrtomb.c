/* Measure wcrtomb function.
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

#include <array_length.h>
#include <limits.h>
#include <locale.h>
#include <string.h>
#include <wchar.h>

#include "bench-timing.h"
#include "json-lib.h"

#define NITERS 100000

struct test_inputs
{
  const char *locale;
  const wchar_t *input_chars;
};

/* The inputs represent different types of characters, e.g. RTL, 1 byte, 2
   byte, 3 byte and 4 byte chars.  The exact number of inputs per locale
   doesn't really matter because we're not looking to compare performance
   between locales.  */
struct test_inputs inputs[] =
{
  /* RTL.  */
  {"ar_SA.UTF-8",
   L",-.،؟ـًُّ٠٢٣٤ءآأؤإئابةتثجحخدذرزسشصضطظعغفقكلمنهوىي"},

  /* Various mixes of 1 and 2 byte chars.  */
  {"cs_CZ.UTF-8",
   L",.aAábcCčdDďeEéÉěĚfFghHiIíJlLmMnNňŇoóÓpPqQrřsSšŠTťuUúÚůŮvVWxyýz"},

  {"el_GR.UTF-8",
   L",.αΑβγδΔεΕζηΗθΘιΙκΚλμΜνΝξοΟπΠρΡσΣςτυΥφΦχψω"},

  {"en_GB.UTF-8",
   L",.aAāĀæÆǽǣǢbcCċdDðÐeEēĒfFgGġhHiIīĪlLmMnNoōpPqQrsSTuUūŪvVwxyȝzþÞƿǷ"},

  {"fr_FR.UTF-8",
   L",.aAàâbcCçdDeEéèêëfFghHiIîïjlLmMnNoOôœpPqQrRsSTuUùûvVwxyz"},

  {"he_IL.UTF-8",
   L"',.ִאבגדהוזחטיכךלמםנןסעפףצץקרשת"},

  /* Devanagari, Japanese, 3-byte chars.  */
  {"hi_IN.UTF-8",
   L"(।ं०४५७अआइईउऎएओऔकखगघचछजञटडढणतथदधनपफ़बभमयरलवशषसहािीुूृेैोौ्"},

  {"ja_JP.UTF-8",
   L".ー0123456789あアいイうウえエおオかカがきキぎくクぐけケげこコごさサざ"},

  /* More mixtures of 1 and 2 byte chars.  */
  {"ru_RU.UTF-8",
   L",.аАбвВгдДеЕёЁжЖзЗийЙкКлЛмМнНоОпПрстТуУфФхХЦчшШщъыЫьэЭюЮя"},

  {"sr_RS.UTF-8",
   L",.aAbcCćčdDđĐeEfgGhHiIlLmMnNoOpPqQrsSšŠTuUvVxyzZž"},

  {"sv_SE.UTF-8",
   L",.aAåÅäÄæÆbBcCdDeEfFghHiIjlLmMnNoOöÖpPqQrsSTuUvVwxyz"},

  /* Chinese, 3-byte chars  */
  {"zh_CN.UTF-8",
   L"一七三下不与世両並中串主乱予事二五亡京人今仕付以任企伎会伸住佐体作使"},

  /* 4-byte chars, because smileys are the universal language and we want to
     ensure optimal performance with them 😊.  */
  {"en_US.UTF-8",
   L"😀😁😂😃😄😅😆😇😈😉😊😋😌😍😎😏😐😑😒😓😔😕😖😗😘😙😚😛😜😝😞😟😠😡"}
};

char buf[MB_LEN_MAX];
size_t ret;

int
main (int argc, char **argv)
{
  json_ctx_t json_ctx;
  json_init (&json_ctx, 0, stdout);
  json_document_begin (&json_ctx);

  json_attr_string (&json_ctx, "timing_type", TIMING_TYPE);
  json_attr_object_begin (&json_ctx, "functions");
  json_attr_object_begin (&json_ctx, "wcrtomb");

  for (size_t i = 0; i < array_length (inputs); i++)
    {
      json_attr_object_begin (&json_ctx, inputs[i].locale);
      setlocale (LC_ALL, inputs[i].locale);

      timing_t min = 0x7fffffffffffffff, max = 0, total = 0;
      const wchar_t *inp = inputs[i].input_chars;
      const size_t len = wcslen (inp);
      mbstate_t s;

      memset (&s, '\0', sizeof (s));

      for (size_t n = 0; n < NITERS; n++)
	{
	  timing_t start, end, elapsed;

	  TIMING_NOW (start);
	  for (size_t j = 0; j < len; j++)
	    ret = wcrtomb (buf, inp[j], &s);
	  TIMING_NOW (end);
	  TIMING_DIFF (elapsed, start, end);
	  if (min > elapsed)
	    min = elapsed;
	  if (max < elapsed)
	    max = elapsed;
	  TIMING_ACCUM (total, elapsed);
	}
      json_attr_double (&json_ctx, "max", max);
      json_attr_double (&json_ctx, "min", min);
      json_attr_double (&json_ctx, "mean", total / NITERS);
      json_attr_object_end (&json_ctx);
    }

  json_attr_object_end (&json_ctx);
  json_attr_object_end (&json_ctx);
  json_document_end (&json_ctx);
}

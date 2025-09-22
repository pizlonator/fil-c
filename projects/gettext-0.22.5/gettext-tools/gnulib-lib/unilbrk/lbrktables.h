/* Line breaking auxiliary tables.
   Copyright (C) 2001-2003, 2006-2024 Free Software Foundation, Inc.
   Written by Bruno Haible <bruno@clisp.org>, 2001.

   This file is free software.
   It is dual-licensed under "the GNU LGPLv3+ or the GNU GPLv2+".
   You can redistribute it and/or modify it under either
     - the terms of the GNU Lesser General Public License as published
       by the Free Software Foundation, either version 3, or (at your
       option) any later version, or
     - the terms of the GNU General Public License as published by the
       Free Software Foundation; either version 2, or (at your option)
       any later version, or
     - the same dual license "the GNU LGPLv3+ or the GNU GPLv2+".

   This file is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License and the GNU General Public License
   for more details.

   You should have received a copy of the GNU Lesser General Public
   License and of the GNU General Public License along with this
   program.  If not, see <https://www.gnu.org/licenses/>.  */

#include "unitypes.h"

/* Line breaking classification.  */

enum
{
  /* Values >= 40 are resolved at run time. */
  LBP_BK  = 40, /* mandatory break */
  LBP_CR  = 41, /* carriage return */
  LBP_LF  = 42, /* line feed */
  LBP_CM  = 43, /* attached characters and combining marks */
/*LBP_NL,          next line - not used here because it's equivalent to LBP_BK */
/*LBP_SG,          surrogates - not used here because they are not characters */
  LBP_WJ  =  0, /* word joiner */
  LBP_ZW  = 44, /* zero width space */
  LBP_GL  =  1, /* non-breaking (glue) */
  LBP_SP  = 45, /* space */
  LBP_B2  =  2, /* break opportunity before and after */
  LBP_BA  =  3, /* break opportunity after */
  LBP_BB  =  4, /* break opportunity before */
  LBP_HY  =  5, /* hyphen */
  LBP_CB  = 46, /* contingent break opportunity */
  LBP_CL  =  6, /* closing punctuation */
  LBP_CP1 =  7, /* closing parenthesis, non-EastAsian character */
  LBP_CP2 =  8, /* closing parenthesis, EastAsian character */
  LBP_EX  =  9, /* exclamation/interrogation */
  LBP_IN  = 10, /* inseparable */
  LBP_NS  = 11, /* non starter */
  LBP_OP1 = 12, /* opening punctuation, non-EastAsian character */
  LBP_OP2 = 13, /* opening punctuation, EastAsian character */
  LBP_QU1 = 14, /* ambiguous quotation, neither initial nor final punctuation */
  LBP_QU2 = 15, /* ambiguous quotation, initial punctuation */
  LBP_QU3 = 16, /* ambiguous quotation, final punctuation */
  LBP_IS  = 17, /* infix separator (numeric) */
  LBP_NU  = 18, /* numeric */
  LBP_PO  = 19, /* postfix (numeric) */
  LBP_PR  = 20, /* prefix (numeric) */
  LBP_SY  = 21, /* symbols allowing breaks */
  LBP_AI  = 47, /* ambiguous (alphabetic or ideograph) */
  LBP_AL  = 22, /* ordinary alphabetic and symbol characters */
/*LBP_CJ,          conditional Japanese starter, resolved to NS */
  LBP_H2  = 23, /* Hangul LV syllable */
  LBP_H3  = 24, /* Hangul LVT syllable */
  LBP_HL  = 30, /* Hebrew letter */
  LBP_ID1 = 25, /* ideographic */
  LBP_ID2 = 26, /* ideographic and potential future emoji */
  LBP_JL  = 27, /* Hangul L Jamo */
  LBP_JV  = 28, /* Hangul V Jamo */
  LBP_JT  = 29, /* Hangul T Jamo */
  LBP_AP  = 31, /* Brahmic scripts: pre-base repha */
  LBP_AK  = 32, /* Brahmic scripts: consonants */
  LBP_AS  = 33, /* Brahmic scripts: independent vowels */
  LBP_VI  = 34, /* Brahmic scripts: conjoining viramas */
  LBP_VF  = 35, /* Brahmic scripts: viramas for final consonants */
  LBP_RI  = 36, /* regional indicator */
  LBP_SA  = 48, /* complex context (South East Asian) */
  LBP_ZWJ = 37, /* zero width joiner */
  LBP_EB  = 38, /* emoji base */
  LBP_EM  = 39, /* emoji modifier */
  LBP_XX  = 49, /* unknown */
  /* Artificial values that exist only at runtime, not in the tables. */
  LBP_HL_BA = 100
};

#include "lbrkprop1.h"

static inline unsigned char
unilbrkprop_lookup (ucs4_t uc)
{
  unsigned int index1 = uc >> lbrkprop_header_0;
  if (index1 < lbrkprop_header_1)
    {
      int lookup1 = unilbrkprop.level1[index1];
      if (lookup1 >= 0)
        {
          unsigned int index2 = (uc >> lbrkprop_header_2) & lbrkprop_header_3;
          int lookup2 = unilbrkprop.level2[lookup1 + index2];
          if (lookup2 >= 0)
            {
              unsigned int index3 = uc & lbrkprop_header_4;
              return unilbrkprop.level3[lookup2 + index3];
            }
        }
    }
  return LBP_XX;
}

/* Table indexed by two line breaking classifications.  */
#define D 1  /* direct break opportunity, empty in table 7.3 of UTR #14 */
#define I 2  /* indirect break opportunity, '%' in table 7.3 of UTR #14 */
#define P 3  /* prohibited break,           '^' in table 7.3 of UTR #14 */

extern const unsigned char unilbrk_table[40][40];

/* We don't support line breaking of complex-context dependent characters
   (Thai, Lao, Myanmar, Khmer) yet, because it requires dictionary lookup. */

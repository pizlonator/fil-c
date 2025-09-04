/* Test of stable-sorting of an array using mergesort.
   Copyright (C) 2009-2024 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as published
   by the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

#include <config.h>

#include <stddef.h>

struct foo { double x; double index; };
#define ELEMENT struct foo
#define COMPARE(a,b) ((a)->x < (b)->x ? -1 : (a)->x > (b)->x ? 1 : 0)
#define STATIC static
#include "array-mergesort.h"

#include <stdlib.h>

#include "macros.h"

#define NMAX 257
static const struct foo data[NMAX] =
{
  {  2, 0 },
  { 28, 1 },
  { 36, 2 },
  { 43, 3 },
  { 20, 4 },
  { 37, 5 },
  { 19, 6 },
  { 37, 7 },
  { 30, 8 },
  { 18, 9 },
  { 30, 10 },
  { 49, 11 },
  { 16, 12 },
  { 22, 13 },
  { 23, 14 },
  {  3, 15 },
  { 39, 16 },
  { 48, 17 },
  { 18, 18 },
  { 18, 19 },
  { 45, 20 },
  { 39, 21 },
  {  1, 22 },
  { 44, 23 },
  { 24, 24 },
  { 21, 25 },
  { 29, 26 },
  {  3, 27 },
  { 34, 28 },
  { 15, 29 },
  { 39, 30 },
  { 11, 31 },
  { 29, 32 },
  { 27, 33 },
  { 43, 34 },
  { 31, 35 },
  { 28, 36 },
  { 12, 37 },
  { 16, 38 },
  { 34, 39 },
  { 25, 40 },
  { 31, 41 },
  { 29, 42 },
  { 36, 43 },
  { 17, 44 },
  { 18, 45 },
  { 44, 46 },
  { 22, 47 },
  { 23, 48 },
  { 32, 49 },
  { 16, 50 },
  { 47, 51 },
  { 28, 52 },
  { 46, 53 },
  { 49, 54 },
  { 24, 55 },
  {  0, 56 },
  { 20, 57 },
  { 25, 58 },
  { 42, 59 },
  { 48, 60 },
  { 16, 61 },
  { 26, 62 },
  { 32, 63 },
  { 24, 64 },
  { 17, 65 },
  { 47, 66 },
  { 47, 67 },
  { 12, 68 },
  { 33, 69 },
  { 41, 70 },
  { 36, 71 },
  {  8, 72 },
  { 15, 73 },
  {  0, 74 },
  { 32, 75 },
  { 28, 76 },
  { 11, 77 },
  { 46, 78 },
  { 34, 79 },
  {  5, 80 },
  { 20, 81 },
  { 47, 82 },
  { 25, 83 },
  {  7, 84 },
  { 29, 85 },
  { 40, 86 },
  {  5, 87 },
  { 12, 88 },
  { 30, 89 },
  {  1, 90 },
  { 22, 91 },
  { 29, 92 },
  { 42, 93 },
  { 49, 94 },
  { 30, 95 },
  { 40, 96 },
  { 33, 97 },
  { 36, 98 },
  { 12, 99 },
  {  8, 100 },
  { 33, 101 },
  {  5, 102 },
  { 31, 103 },
  { 27, 104 },
  { 19, 105 },
  { 43, 106 },
  { 37, 107 },
  {  9, 108 },
  { 40, 109 },
  {  0, 110 },
  { 35, 111 },
  { 32, 112 },
  {  6, 113 },
  { 27, 114 },
  { 28, 115 },
  { 30, 116 },
  { 37, 117 },
  { 32, 118 },
  { 41, 119 },
  { 14, 120 },
  { 44, 121 },
  { 22, 122 },
  { 26, 123 },
  {  2, 124 },
  { 43, 125 },
  { 20, 126 },
  { 32, 127 },
  { 24, 128 },
  { 33, 129 },
  {  7, 130 },
  { 17, 131 },
  { 10, 132 },
  { 47, 133 },
  { 14, 134 },
  { 29, 135 },
  { 19, 136 },
  { 25, 137 },
  { 25, 138 },
  { 13, 139 },
  { 25, 140 },
  { 32, 141 },
  {  8, 142 },
  { 37, 143 },
  { 31, 144 },
  { 32, 145 },
  {  5, 146 },
  { 45, 147 },
  { 35, 148 },
  { 47, 149 },
  {  3, 150 },
  {  4, 151 },
  { 37, 152 },
  { 43, 153 },
  { 39, 154 },
  { 18, 155 },
  { 13, 156 },
  { 15, 157 },
  { 41, 158 },
  { 34, 159 },
  {  4, 160 },
  { 33, 161 },
  { 20, 162 },
  {  4, 163 },
  { 38, 164 },
  { 47, 165 },
  { 30, 166 },
  { 41, 167 },
  { 23, 168 },
  { 40, 169 },
  { 23, 170 },
  { 35, 171 },
  { 47, 172 },
  { 32, 173 },
  { 15, 174 },
  { 15, 175 },
  { 41, 176 },
  { 35, 177 },
  {  6, 178 },
  { 18, 179 },
  { 35, 180 },
  { 39, 181 },
  { 34, 182 },
  {  6, 183 },
  { 34, 184 },
  { 37, 185 },
  { 15, 186 },
  {  6, 187 },
  { 12, 188 },
  { 39, 189 },
  {  9, 190 },
  { 48, 191 },
  { 37, 192 },
  { 28, 193 },
  { 32, 194 },
  {  1, 195 },
  { 45, 196 },
  { 21, 197 },
  { 11, 198 },
  { 32, 199 },
  { 43, 200 },
  { 35, 201 },
  { 25, 202 },
  {  4, 203 },
  { 20, 204 },
  { 10, 205 },
  { 22, 206 },
  { 44, 207 },
  { 30, 208 },
  { 16, 209 },
  { 42, 210 },
  { 13, 211 },
  { 29, 212 },
  { 23, 213 },
  { 30, 214 },
  { 25, 215 },
  { 49, 216 },
  {  0, 217 },
  { 49, 218 },
  { 29, 219 },
  { 37, 220 },
  {  6, 221 },
  { 27, 222 },
  { 31, 223 },
  { 17, 224 },
  { 45, 225 },
  { 25, 226 },
  { 15, 227 },
  { 34, 228 },
  {  7, 229 },
  {  7, 230 },
  {  4, 231 },
  { 31, 232 },
  { 40, 233 },
  { 17, 234 },
  {  2, 235 },
  { 34, 236 },
  { 17, 237 },
  { 25, 238 },
  {  5, 239 },
  { 48, 240 },
  { 31, 241 },
  { 41, 242 },
  { 45, 243 },
  { 33, 244 },
  { 46, 245 },
  { 19, 246 },
  { 17, 247 },
  { 38, 248 },
  { 43, 249 },
  { 16, 250 },
  {  5, 251 },
  { 21, 252 },
  {  0, 253 },
  { 47, 254 },
  { 40, 255 },
  { 22, 256 }
};

static int
cmp_double (const void *a, const void *b)
{
  return (*(const double *)a < *(const double *)b ? -1 :
          *(const double *)a > *(const double *)b ? 1 :
          0);
}

int
main ()
{
  size_t n;

  /* Test merge_sort_fromto.  */
  for (n = 1; n <= NMAX; n++)
    {
      struct foo *dst;
      struct foo *tmp;
      double *qsort_result;
      size_t i;

      dst = (struct foo *) malloc ((n + 1) * sizeof (struct foo));
      dst[n].x = 0x4A6A71FE; /* canary */
      tmp = (struct foo *) malloc ((n / 2 + 1) * sizeof (struct foo));
      tmp[n / 2].x = 0x587EF149; /* canary */

      merge_sort_fromto (data, dst, n, tmp);

      /* Verify the canaries.  */
      ASSERT (dst[n].x == 0x4A6A71FE);
      ASSERT (tmp[n / 2].x == 0x587EF149);

      /* Verify the result.  */
      qsort_result = (double *) malloc (n * sizeof (double));
      for (i = 0; i < n; i++)
        qsort_result[i] = data[i].x;
      qsort (qsort_result, n, sizeof (double), cmp_double);
      for (i = 0; i < n; i++)
        ASSERT (dst[i].x == qsort_result[i]);

      /* Verify the stability.  */
      for (i = 0; i < n; i++)
        if (i > 0 && dst[i - 1].x == dst[i].x)
          ASSERT (dst[i - 1].index < dst[i].index);

      free (qsort_result);
      free (tmp);
      free (dst);
    }

  /* Test merge_sort_inplace.  */
  for (n = 1; n <= NMAX; n++)
    {
      struct foo *src;
      struct foo *tmp;
      double *qsort_result;
      size_t i;

      src = (struct foo *) malloc ((n + 1) * sizeof (struct foo));
      src[n].x = 0x4A6A71FE; /* canary */
      tmp = (struct foo *) malloc ((n + 1) * sizeof (struct foo));
      tmp[n].x = 0x587EF149; /* canary */

      for (i = 0; i < n; i++)
        src[i] = data[i];

      merge_sort_inplace (src, n, tmp);

      /* Verify the canaries.  */
      ASSERT (src[n].x == 0x4A6A71FE);
      ASSERT (tmp[n].x == 0x587EF149);

      /* Verify the result.  */
      qsort_result = (double *) malloc (n * sizeof (double));
      for (i = 0; i < n; i++)
        qsort_result[i] = data[i].x;
      qsort (qsort_result, n, sizeof (double), cmp_double);
      for (i = 0; i < n; i++)
        ASSERT (src[i].x == qsort_result[i]);

      /* Verify the stability.  */
      for (i = 0; i < n; i++)
        if (i > 0 && src[i - 1].x == src[i].x)
          ASSERT (src[i - 1].index < src[i].index);

      free (qsort_result);
      free (tmp);
      free (src);
    }

  return 0;
}

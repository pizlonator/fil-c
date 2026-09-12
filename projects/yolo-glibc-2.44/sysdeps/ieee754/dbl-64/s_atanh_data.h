/* Internal data for atanh implementation.

Copyright (c) 2023 Alexei Sibidanov.

This file is part of the CORE-MATH project
(https://core-math.gitlabpages.inria.fr/).

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE. */

#ifndef _S_ATANH_DATA_H
#define _S_ATANH_DATA_H

#include <stdint.h>

extern const double __atanh_ch[][2] attribute_hidden;
extern const double __atanh_cl[] attribute_hidden;
extern const double __atan_c[] attribute_hidden;
extern const struct __atanh_B_t
{
  uint16_t c0;
  short c1;
} __atanh_B[] attribute_hidden;
extern const double __atanh_r1[] attribute_hidden;
extern const double __atanh_r2[] attribute_hidden;
extern const double __atanh_l1[][2] attribute_hidden;
extern const double __atanh_l2[][2] attribute_hidden;

#define CH __atanh_ch
#define CL __atanh_cl
#define B __atanh_B
#define R1 __atanh_r1
#define R2 __atanh_r2
#define L1 __atanh_l1
#define L2 __atanh_l2

extern const double __atanh_t1[] attribute_hidden;
extern const double __atanh_t2[] attribute_hidden;
extern const double __atanh_t3[] attribute_hidden;
extern const double __atanh_t4[] attribute_hidden;
extern const double __atanh_LL[4][17][3] attribute_hidden;
extern const double __atanh_ch2[][2] attribute_hidden;

#define T1 __atanh_t1
#define T2 __atanh_t2
#define T3 __atanh_t3
#define T4 __atanh_t4
#define LL __atanh_LL
#define CH2 __atanh_ch2

#endif

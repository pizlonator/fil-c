/* Common data for asinh/acosh implementations.

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

#ifndef _S_ASINCOSH_DATA_H
#define _S_ASINCOSH_DATA_H

#include <stdint.h>

struct __asincosh_B_t
{
  uint16_t c0;
  short c1;
};
extern const struct __asincosh_B_t __asincosh_B[] attribute_hidden;
extern const double __asincosh_r1[] attribute_hidden;
extern const double __asincosh_r2[] attribute_hidden;
extern const double __asincosh_l1[][2] attribute_hidden;
extern const double __asincosh_l2[][2] attribute_hidden;
extern const double __asincosh_c[] attribute_hidden;

#define B __asincosh_B
#define r1 __asincosh_r1
#define r2 __asincosh_r2
#define l1 __asincosh_l1
#define l2 __asincosh_l2
#define c __asincosh_c

#endif

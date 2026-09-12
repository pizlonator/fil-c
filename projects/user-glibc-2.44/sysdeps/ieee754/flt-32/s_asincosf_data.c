/* Common data for asinf/acosf implementations.

Copyright (c) 2023-2024 Alexei Sibidanov.

The original version of this file was copied from the CORE-MATH
project (file src/binary32/asin/asinf.c, revision bc385c2).

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
SOFTWARE.
*/

#include "s_asincosf_data.h"

const double __asincosf_c0[] =
  {
    0x1.555555555529cp-3,  0x1.333333337e0ddp-4, 0x1.6db6db3b4465ep-5,
    0x1.f1c72e13ac306p-6,  0x1.6e89cebe06bc4p-6, 0x1.1c6dcf5289094p-6,
    0x1.c6dbbcc7c6315p-7,  0x1.8f8dc2615e996p-7, 0x1.a5833b7bf15e8p-8,
    0x1.43f44ace1665cp-6, -0x1.0fb17df881c73p-6, 0x1.07520c026b2d6p-5
  };

const double __asincosf_c1[] =
  {
    0x1.6a09e667f3bcbp+0,   0x1.e2b7dddff2db9p-4,  0x1.b27247ab42dbcp-6,
    0x1.02995cc4e0744p-7,   0x1.5ffb0276ec8eap-9,  0x1.033885a928decp-10,
    0x1.911f2be23f8c7p-12,  0x1.4c3c55d2437fdp-13, 0x1.af477e1d7b461p-15,
    0x1.abd6bdff67dcbp-15, -0x1.1717e86d0fa28p-16, 0x1.6ff526de46023p-16
  };

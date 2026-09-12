/* Data definitions used by tanh implementation.

Copyright (c) 2023-2026 Alexei Sibidanov, Cyprien Peignier, Paul Zimmermann.

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
SOFTWARE.
*/

#include "e_tanh_data.h"

const double __tanh_data_ch[9][2] =
  {
    { -0x1.5555555555555p-2, -0x1.5555555554cc4p-56 },
    { 0x1.1111111111111p-3, 0x1.111110f8c0178p-59 },
    { -0x1.ba1ba1ba1ba1cp-5, 0x1.7917c1d676ff5p-59 },
    { 0x1.664f4882c10fap-6, -0x1.9d5cb27c0af28p-63 },
    { -0x1.226e355e6c23cp-7, -0x1.c9674586913f3p-61 },
    { 0x1.d6d3d0e157db3p-9, -0x1.71376fa06ce94p-65 },
    { -0x1.7da36452b5e46p-10, -0x1.aba8d51bd9cp-65 },
    { 0x1.3558247faa32dp-11, -0x1.e0cfb423aedfdp-65 },
    { -0x1.f57d76ea30928p-13, -0x1.c30601213cae9p-67 }
  };

const double __tanh_data_cl[5] =
  {
    0x1.967e0a63ca836p-14,  -0x1.497b99d2a77d1p-15, 0x1.0ae346258cbdep-16,
    -0x1.aade68fb2f076p-18, 0x1.22e609bf8671fp-19
  };

const double __tanh_data_db[12][3] =
  {
    { 0x1.ac343b179fec4p-3, 0x1.a612499c53078p-3, 0x1p-57 },
    { 0x1.00764a988bf73p-2, 0x1.f676484c0703bp-3, -0x1p-104 },
    { 0x1.17d1e8a63711fp-2, 0x1.110e96a6c2d96p-2, -0x1p-105 },
    { 0x1.291c601a05276p-2, 0x1.210b7d0c03743p-2, 0x1p-56 },
    { 0x1.36f33d51c264dp-2, 0x1.2dbb7b1c91363p-2, -0x1p-106 },
    { 0x1.43eaea23649c3p-2, 0x1.39877ed028641p-2, -0x1p-55 },
    { 0x1.d88d7550b2826p-2, 0x1.b9a3637366afdp-2, 0x1p-56 },
    { 0x1.e611aa58ab608p-2, 0x1.c493dc899e4a6p-2, -0x1p-55 },
    { 0x1.01efe7ac8c15dp-1, 0x1.dc3fe1b524821p-2, -0x1p-104 },
    { 0x1.1005ec0bccabbp-1, 0x1.f20b1c8557dedp-2, -0x1p-55 },
    { 0x1.33dfeb0fa4bfep-1, 0x1.1372f9ee76e99p-1, 0x1p-55 },
    { 0x1.49f24ac5cac35p-1, 0x1.22c495ff06104p-1, -0x1p-104 },
  };

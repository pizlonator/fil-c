/* Common data for sinhf/coshf implementations.

Copyright (c) 2022-2025 Alexei Sibidanov.

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

#include "e_sincoshf_data.h"

const double __sincoshf_c[] =
  {
    1, 0x1.62e42fef4c4e7p-6, 0x1.ebfd1b232f475p-13, 0x1.c6b19384ecd93p-20
  };
const double __sincoshf_ch[] =
  {
    1,                     0x1.62e42fefa39efp-6,  0x1.ebfbdff82c58fp-13,
    0x1.c6b08d702e0edp-20, 0x1.3b2ab6fb92e5ep-27, 0x1.5d886e6d54203p-35,
    0x1.430976b8ce6efp-43
  };
const uint64_t __sincoshf_tb[] =
  {
    0x3fe0000000000000, 0x3fe059b0d3158574, 0x3fe0b5586cf9890f,
    0x3fe11301d0125b51, 0x3fe172b83c7d517b, 0x3fe1d4873168b9aa,
    0x3fe2387a6e756238, 0x3fe29e9df51fdee1, 0x3fe306fe0a31b715,
    0x3fe371a7373aa9cb, 0x3fe3dea64c123422, 0x3fe44e086061892d,
    0x3fe4bfdad5362a27, 0x3fe5342b569d4f82, 0x3fe5ab07dd485429,
    0x3fe6247eb03a5585, 0x3fe6a09e667f3bcd, 0x3fe71f75e8ec5f74,
    0x3fe7a11473eb0187, 0x3fe82589994cce13, 0x3fe8ace5422aa0db,
    0x3fe93737b0cdc5e5, 0x3fe9c49182a3f090, 0x3fea5503b23e255d,
    0x3feae89f995ad3ad, 0x3feb7f76f2fb5e47, 0x3fec199bdd85529c,
    0x3fecb720dcef9069, 0x3fed5818dcfba487, 0x3fedfc97337b9b5f,
    0x3feea4afa2a490da, 0x3fef50765b6e4540
  };

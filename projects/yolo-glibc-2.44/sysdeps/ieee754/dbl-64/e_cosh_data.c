/* Data definitions used by cosh implementation.

Copyright (c) 2023-2026 Alexei Sibidanov.

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

#include "e_cosh_data.h"

const double __cosh_data_ch[][2] =
  {
    { 0x1p-1, -0x1.c7e8db669f624p-111 },
    { 0x1.5555555555555p-5, 0x1.5555555556135p-59 },
    { 0x1.6c16c16c16c17p-10, -0x1.f49f4a6e838f2p-65 },
    { 0x1.a01a01a01a01ap-16, 0x1.a4ffbe15316aap-76 }
  };
const double __cosh_data_cl[] =
  {
    0x1.27e4fb7789f5cp-22, 0x1.1eed8eff9089cp-29, 0x1.939749ce13dadp-37,
    0x1.ae9891efb6691p-45
  };

const double __cosh_data_db[21][3] =
  {
    { 0x1.9a5e3cbe1985ep-4, 0x1.01492f72f984bp+0, -0x1p-107 },
    { 0x1.52a11832e847dp-3, 0x1.0381e68cac923p+0, 0x1p-104 },
    { 0x1.bf0305e2c6c37p-3, 0x1.061f4c39e16f2p+0, 0x1p-107 },
    { 0x1.17326ffc09f68p-2, 0x1.099318a43ac8p+0, 0x1p-104 },
    { 0x1.3d27bf16d8bdbp-2, 0x1.0c6091056e06ap+0, -0x1p-107 },
    { 0x1.03923f2b47c07p-1, 0x1.219c1989e3373p+0, -0x1p-54 },
    { 0x1.a6031cd5f93bap-1, 0x1.5bff041b260fep+0, -0x1p-107 },
    { 0x1.104b648f113a1p+0, 0x1.9efdca62b700ap+0, -0x1p-109 },
    { 0x1.1585720f35cd9p+0, 0x1.a5bf3acfde4b2p+0, 0x1p-105 },
    { 0x1.e9cc7ed2e1a7ep+0, 0x1.bb0ff220d8eb5p+1, -0x1p-53 },
    { 0x1.43180ea854696p+1, 0x1.91f1122b6b63ap+2, 0x1p-102 },
    { 0x1.725811dcf6782p+2, 0x1.45ea160ddc71fp+7, -0x1p-100 },
    { 0x1.5afd56f7d565bp+3, 0x1.8ff8e0ccea7cp+14, 0x1p-90 },
    { 0x1.759a2ad4c4d56p+3, 0x1.cb62eec26bd78p+15, -0x1p-92 },
    { 0x1.7fce95ea5c653p+3, 0x1.3bf8009648dcp+16, 0x1p-88 },
    { 0x1.743d5609348acp+4, 0x1.7a87a8bb7fa28p+32, -0x1p-22 },
    { 0x1.e07e71bfcf06fp+5, 0x1.91ec4412c344fp+85, 0x1p-24 },
    { 0x1.6474c604cc0d7p+6, 0x1.7a8f65ad009bdp+127, -0x1p+20 },
    { 0x1.54cd1fea7663ap+7, 0x1.c90810d354618p+244, 0x1p+135 },
    { 0x1.2da9e5e6af0bp+8, 0x1.27d6fe867d6f6p+434, 0x1p+329 },
    { 0x1.d6479eba7c971p+8, 0x1.62a88613629b6p+677, -0x1p+568 },
  };

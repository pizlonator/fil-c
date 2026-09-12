/* Common data for asinh/acosh implementations.

Copyright (c) 2023-2025 Alexei Sibidanov.

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

#include "s_asincosh_data.h"

/* coefficient of a linear piece-wise approximation of log2(x) in [1,2]
   range with an error ~< 5.8e-4 */
const struct __asincosh_B_t __asincosh_B[] = {
  { 301, 27565 },    { 7189, 24786 },	{ 13383, 22167 },  { 18923, 19696 },
  { 23845, 17361 },  { 28184, 15150 },	{ 31969, 13054 },  { 35231, 11064 },
  { 37996, 9173 },   { 40288, 7372 },	{ 42129, 5657 },   { 43542, 4020 },
  { 44546, 2457 },   { 45160, 962 },	{ 45399, -468 },   { 45281, -1838 },
  { 44821, -3151 },  { 44032, -4412 },	{ 42929, -5622 },  { 41522, -6786 },
  { 39825, -7905 },  { 37848, -8982 },	{ 35602, -10020 }, { 33097, -11020 },
  { 30341, -11985 }, { 27345, -12916 }, { 24115, -13816 }, { 20661, -14685 },
  { 16989, -15526 }, { 13107, -16339 }, { 9022, -17126 },  { 4740, -17889 }
};
// 20-bit approximation of 2^(-i/32)
const double __asincosh_r1[]
    = { 0x1p+0,	      0x1.f5076p-1, 0x1.ea4bp-1,  0x1.dfc98p-1, 0x1.d5818p-1,
	0x1.cb72p-1,  0x1.c199cp-1, 0x1.b7f76p-1, 0x1.ae8ap-1,	0x1.a5504p-1,
	0x1.9c492p-1, 0x1.93738p-1, 0x1.8ace6p-1, 0x1.8258ap-1, 0x1.7a114p-1,
	0x1.71f76p-1, 0x1.6a09ep-1, 0x1.6247ep-1, 0x1.5ab08p-1, 0x1.5342cp-1,
	0x1.4bfdap-1, 0x1.44e08p-1, 0x1.3dea6p-1, 0x1.371a8p-1, 0x1.306fep-1,
	0x1.29e9ep-1, 0x1.2387ap-1, 0x1.1d488p-1, 0x1.172b8p-1, 0x1.11302p-1,
	0x1.0b558p-1, 0x1.059bp-1,  0x1p-1 };
// 20-bit approximation of 2^(-i/1024)
const double __asincosh_r2[]
    = { 0x1p+0,	      0x1.ffa74p-1, 0x1.ff4eap-1, 0x1.fef62p-1, 0x1.fe9dap-1,
	0x1.fe452p-1, 0x1.fdeccp-1, 0x1.fd946p-1, 0x1.fd3c2p-1, 0x1.fce3ep-1,
	0x1.fc8bcp-1, 0x1.fc33ap-1, 0x1.fbdbap-1, 0x1.fb83ap-1, 0x1.fb2bcp-1,
	0x1.fad3ep-1, 0x1.fa7c2p-1, 0x1.fa246p-1, 0x1.f9ccap-1, 0x1.f975p-1,
	0x1.f91d8p-1, 0x1.f8c6p-1,  0x1.f86e8p-1, 0x1.f8172p-1, 0x1.f7bfep-1,
	0x1.f768ap-1, 0x1.f7116p-1, 0x1.f6ba4p-1, 0x1.f6632p-1, 0x1.f60c2p-1,
	0x1.f5b52p-1, 0x1.f55e4p-1, 0x1.f5076p-1 };
// natural logarithm of 1/r1[i]
const double __asincosh_l1[][2]
    = { { 0x0p+0, 0x0p+0 },
	{ -0x1.269e2038315b3p-46, 0x1.62e4eacd4p-6 },
	{ -0x1.3f2558bddfc47p-45, 0x1.62e3ce7218p-5 },
	{ 0x1.07ea13c34efb5p-45, 0x1.0a2ab6d3ecp-4 },
	{ 0x1.8f3e77084d3bap-44, 0x1.62e4a86d8cp-4 },
	{ -0x1.8d92a005f1a7ep-46, 0x1.bb9db7062cp-4 },
	{ 0x1.58239e799bfe5p-44, 0x1.0a2b1a22ccp-3 },
	{ -0x1.a93fcf5f593b7p-44, 0x1.3687f0a298p-3 },
	{ -0x1.db4cac32fd2b5p-46, 0x1.62e4116b64p-3 },
	{ -0x1.0e65a92ee0f3bp-46, 0x1.8f409e4df6p-3 },
	{ -0x1.8261383d475f1p-44, 0x1.bb9d15001cp-3 },
	{ -0x1.359886207513bp-44, 0x1.e7f9a8c94p-3 },
	{ 0x1.811f87496ceb7p-44, 0x1.0a2b052ddbp-2 },
	{ 0x1.4991ec6cb435cp-44, 0x1.205955ef73p-2 },
	{ -0x1.4581abfeb8927p-44, 0x1.3687bd9121p-2 },
	{ 0x1.cab48f6942703p-44, 0x1.4cb5e8f2b5p-2 },
	{ -0x1.df2c452fde132p-47, 0x1.62e4420e2p-2 },
	{ 0x1.6109f4fdb74bdp-45, 0x1.791292c46ap-2 },
	{ -0x1.6b95fbdac7696p-44, 0x1.8f40af84e7p-2 },
	{ 0x1.7394fa880cbdap-46, 0x1.a56ed8f865p-2 },
	{ -0x1.50b06a94eccabp-46, 0x1.bb9d6505b4p-2 },
	{ -0x1.be2abf0b38989p-44, 0x1.d1cb91e728p-2 },
	{ -0x1.7d6bf1e34da04p-44, 0x1.e7f9d139e2p-2 },
	{ -0x1.423c1e14de6edp-44, 0x1.fe27db9b0ep-2 },
	{ 0x1.c46f1a0efbbc2p-44, 0x1.0a2b25060a8p-1 },
	{ 0x1.834fe4e3e6018p-45, 0x1.154244482ap-1 },
	{ 0x1.6a03d0f02b65p-46, 0x1.20597312988p-1 },
	{ 0x1.d437056526f3p-44, 0x1.2b707145dep-1 },
	{ -0x1.a0233728405c5p-45, 0x1.3687b0e0b28p-1 },
	{ -0x1.4dbdda10d2bf1p-45, 0x1.419ec5d3f68p-1 },
	{ 0x1.f7d0a25d154f2p-44, 0x1.4cb5f9fc02p-1 },
	{ 0x1.15ede4d803b18p-44, 0x1.57cd28421a8p-1 },
	{ 0x1.ef35793c7673p-45, 0x1.62e42fefa38p-1 } };
// natural logarithm of 1/r2[i]
const double __asincosh_l2[][2]
    = { { 0x0p+0, 0x0p+0 },
	{ 0x1.5abdac3638e99p-44, 0x1.631ec81ep-11 },
	{ -0x1.16b8be9bbe239p-45, 0x1.62fd8127p-10 },
	{ -0x1.364c6315542ebp-44, 0x1.0a2520508p-9 },
	{ 0x1.734abe459c9p-45, 0x1.62dadc1dp-9 },
	{ 0x1.0cf8a761431bfp-44, 0x1.bb9ff94dp-9 },
	{ 0x1.da2718eb78708p-45, 0x1.0a2a2def8p-8 },
	{ 0x1.34ada62c59b93p-44, 0x1.368c0fae4p-8 },
	{ 0x1.d09ab376682d4p-44, 0x1.62e58e4f8p-8 },
	{ -0x1.3cb7b94329211p-45, 0x1.8f46bd28cp-8 },
	{ -0x1.eec5c297c41dp-45, 0x1.bb9f8312p-8 },
	{ -0x1.6411b9395d15p-44, 0x1.e7fff8f3p-8 },
	{ -0x1.1c0e59a43053cp-44, 0x1.0a2c0006ep-7 },
	{ 0x1.6506596e077b6p-46, 0x1.205bdb6fp-7 },
	{ 0x1.e256bce6faa27p-44, 0x1.36877c86ep-7 },
	{ 0x1.bd42467b0c8d1p-51, 0x1.4cb6f5578p-7 },
	{ -0x1.c4f92132ff0fp-44, 0x1.62e230e8cp-7 },
	{ -0x1.80be08bfab39p-44, 0x1.7911440f6p-7 },
	{ -0x1.f0b1319ceb1f7p-44, 0x1.8f443020ap-7 },
	{ 0x1.a65fcfb8de99bp-45, 0x1.a572dbef4p-7 },
	{ 0x1.4233885d3779cp-46, 0x1.bb9d449a6p-7 },
	{ 0x1.f46a59e646edbp-44, 0x1.d1cb8491cp-7 },
	{ -0x1.c3d2f11c11446p-44, 0x1.e7fd9d2aap-7 },
	{ 0x1.7763f78a1e0ccp-45, 0x1.fe2b6f978p-7 },
	{ 0x1.b4c37fc60c043p-44, 0x1.0a2a7c7a5p-6 },
	{ -0x1.5b8a822859be3p-46, 0x1.15412ca86p-6 },
	{ -0x1.f2d8c9fc064p-44, 0x1.2059c9005p-6 },
	{ -0x1.e80e79c20378dp-44, 0x1.2b703f49bp-6 },
	{ 0x1.68256e4329bdbp-44, 0x1.3688a1a8dp-6 },
	{ 0x1.7e9741da248c3p-44, 0x1.419edc7bap-6 },
	{ 0x1.e330dccce602bp-45, 0x1.4cb7034fap-6 },
	{ 0x1.2f32b5d18eefbp-49, 0x1.57cd01187p-6 },
	{ -0x1.269e2038315b3p-46, 0x1.62e4eacd4p-6 } };
/* x+c[0]*x^2+c[1]*x^3+...+c[4]*x^6 is a degree-6 approximation of log(1+x),
   with absolute error bounded by 2^-85.878 for |x| < 2^-11.296 */
const double __asincosh_c[]
    = { -0x1p-1, 0x1.555555555553p-2, -0x1.fffffffffffap-3,
	0x1.99999e33a6366p-3, -0x1.555559ef9525fp-3 };

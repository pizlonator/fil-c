/* Internal data for atanh implementation.

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

#include "s_atanh_data.h"

const double __atanh_ch[][2]
    = { { 0x1.5555555555555p-2, 0x1.5555555555555p-56 },   /* degree 3 */
	{ 0x1.999999999999ap-3, -0x1.999999999611cp-57 },  /* degree 5 */
	{ 0x1.2492492492492p-3, 0x1.2492490f76b25p-57 },   /* degree 7 */
	{ 0x1.c71c71c71c71cp-4, 0x1.c71cd5c38a112p-58 },   /* degree 9 */
	{ 0x1.745d1745d1746p-4, -0x1.7556c4165f4cap-59 },  /* degree 11 */
	{ 0x1.3b13b13b13b14p-4, -0x1.b893c3b36052ep-59 },  /* degree 13 */
	{ 0x1.1111111111105p-4, 0x1.4e1afd723ed1fp-59 },   /* degree 15 */
	{ 0x1.e1e1e1e1e2678p-5, -0x1.f86ea96fb1435p-59 },  /* degree 17 */
	{ 0x1.af286bc9f90ccp-5, 0x1.1e51a6e54fde9p-60 },   /* degree 19 */
	{ 0x1.8618618c779b6p-5, -0x1.ab913de95c3bfp-61 },  /* degree 21 */
	{ 0x1.642c84aa383ebp-5, 0x1.632e747641b12p-59 },   /* degree 23 */
	{ 0x1.47ae2d205013cp-5, -0x1.0c9617e7bcff2p-60 },  /* degree 25 */
	{ 0x1.2f664d60473f9p-5, 0x1.3adb3e2b7f35ep-61 } }; /* degree 27 */

const double __atanh_cl[]
    = { 0x1.1a9a91fd692afp-5, /* degree 29 */
	0x1.06dfbb35e7f44p-5, /* degree 31 */
	0x1.037bed4d7588fp-5, /* degree 33 */
	0x1.5aca6d6d720d6p-6, /* degree 35 */
	0x1.99ea5700d53a5p-5  /* degree 37 */ };

const struct __atanh_B_t __atanh_B[] = {
  { 301, 27565 },    { 7189, 24786 },	{ 13383, 22167 },  { 18923, 19696 },
  { 23845, 17361 },  { 28184, 15150 },	{ 31969, 13054 },  { 35231, 11064 },
  { 37996, 9173 },   { 40288, 7372 },	{ 42129, 5657 },   { 43542, 4020 },
  { 44546, 2457 },   { 45160, 962 },	{ 45399, -468 },   { 45281, -1838 },
  { 44821, -3151 },  { 44032, -4412 },	{ 42929, -5622 },  { 41522, -6786 },
  { 39825, -7905 },  { 37848, -8982 },	{ 35602, -10020 }, { 33097, -11020 },
  { 30341, -11985 }, { 27345, -12916 }, { 24115, -13816 }, { 20661, -14685 },
  { 16989, -15526 }, { 13107, -16339 }, { 9022, -17126 },  { 4740, -17889 }
};
const double __atanh_r1[]
    = { 0x1p+0,	      0x1.f5076p-1, 0x1.ea4bp-1,  0x1.dfc98p-1, 0x1.d5818p-1,
	0x1.cb72p-1,  0x1.c199cp-1, 0x1.b7f76p-1, 0x1.ae8ap-1,	0x1.a5504p-1,
	0x1.9c492p-1, 0x1.93738p-1, 0x1.8ace6p-1, 0x1.8258ap-1, 0x1.7a114p-1,
	0x1.71f76p-1, 0x1.6a09ep-1, 0x1.6247ep-1, 0x1.5ab08p-1, 0x1.5342cp-1,
	0x1.4bfdap-1, 0x1.44e08p-1, 0x1.3dea6p-1, 0x1.371a8p-1, 0x1.306fep-1,
	0x1.29e9ep-1, 0x1.2387ap-1, 0x1.1d488p-1, 0x1.172b8p-1, 0x1.11302p-1,
	0x1.0b558p-1, 0x1.059bp-1,  0x1p-1 };
const double __atanh_r2[]
    = { 0x1p+0,	      0x1.ffa74p-1, 0x1.ff4eap-1, 0x1.fef62p-1, 0x1.fe9dap-1,
	0x1.fe452p-1, 0x1.fdeccp-1, 0x1.fd946p-1, 0x1.fd3c2p-1, 0x1.fce3ep-1,
	0x1.fc8bcp-1, 0x1.fc33ap-1, 0x1.fbdbap-1, 0x1.fb83ap-1, 0x1.fb2bcp-1,
	0x1.fad3ep-1, 0x1.fa7c2p-1, 0x1.fa246p-1, 0x1.f9ccap-1, 0x1.f975p-1,
	0x1.f91d8p-1, 0x1.f8c6p-1,  0x1.f86e8p-1, 0x1.f8172p-1, 0x1.f7bfep-1,
	0x1.f768ap-1, 0x1.f7116p-1, 0x1.f6ba4p-1, 0x1.f6632p-1, 0x1.f60c2p-1,
	0x1.f5b52p-1, 0x1.f55e4p-1, 0x1.f5076p-1 };
const double __atanh_l1[][2] = { { 0x0p+0, 0x0p+0 },
				 { -0x1.532c1269e2038p-27, 0x1.62e5p-7 },
				 { 0x1.ce42d81b54e84p-27, 0x1.62e3cp-6 },
				 { -0x1.25826f815ec3dp-26, 0x1.0a2acp-5 },
				 { 0x1.0db1b1e7cee11p-26, 0x1.62e4ap-5 },
				 { -0x1.1f3a8c6c95003p-26, 0x1.bb9dcp-5 },
				 { -0x1.774cd4fb8c30dp-26, 0x1.0a2b2p-4 },
				 { 0x1.452e56c030a0ap-29, 0x1.3687fp-4 },
				 { 0x1.6b63c4966a79ap-28, 0x1.62e41p-4 },
				 { -0x1.b20a21ccb525ep-28, 0x1.8f40ap-4 },
				 { 0x1.4006cfb3d8f85p-26, 0x1.bb9d1p-4 },
				 { -0x1.cdb026b310c41p-26, 0x1.e7f9bp-4 },
				 { -0x1.69124fdc0f16dp-26, 0x1.0a2b08p-3 },
				 { -0x1.084656cdc2727p-26, 0x1.205958p-3 },
				 { -0x1.376fa8b0357fdp-26, 0x1.3687cp-3 },
				 { 0x1.e56ae55a47b4ap-28, 0x1.4cb5e8p-3 },
				 { 0x1.070ff8834eeb4p-26, 0x1.62e44p-3 },
				 { 0x1.623516109f4fep-26, 0x1.79129p-3 },
				 { -0x1.ec656b95fbdacp-29, 0x1.8f40bp-3 },
				 { 0x1.f0ca2e729f51p-28, 0x1.a56ed8p-3 },
				 { -0x1.7d260a858354ap-26, 0x1.bb9d68p-3 },
				 { 0x1.e7279075503d3p-27, 0x1.d1cb9p-3 },
				 { 0x1.39e1a0a503873p-27, 0x1.e7f9dp-3 },
				 { 0x1.cd86d7b87c3d6p-26, 0x1.fe27d8p-3 },
				 { 0x1.060ab88de341ep-26, 0x1.0a2b24p-2 },
				 { 0x1.20a860d3f939p-28, 0x1.154244p-2 },
				 { -0x1.dacee95fc2f1p-27, 0x1.205974p-2 },
				 { 0x1.45de3a86e0acap-26, 0x1.2b707p-2 },
				 { 0x1.c164cbfb991afp-27, 0x1.3687bp-2 },
				 { 0x1.d3f66b24225efp-26, 0x1.419ec4p-2 },
				 { 0x1.fc023efa144bap-26, 0x1.4cb5f8p-2 },
				 { 0x1.086a8af6f26cp-28, 0x1.57cd28p-2 },
				 { -0x1.05c610ca86c39p-30, 0x1.62e43p-2 } };
const double __atanh_l2[][2] = { { 0x0p+0, 0x0p+0 },
				 { -0x1.37e152a129e4ep-28, 0x1.632p-12 },
				 { -0x1.3f6c916b8be9cp-26, 0x1.63p-11 },
				 { 0x1.20505936739d5p-26, 0x1.0a24p-10 },
				 { -0x1.23e2e8cb541bap-26, 0x1.62dcp-10 },
				 { -0x1.acb7983ac4f5ep-32, 0x1.bbap-10 },
				 { 0x1.6f7c7689c63aep-28, 0x1.0a2ap-9 },
				 { 0x1.f5ca695b4c58bp-30, 0x1.368cp-9 },
				 { -0x1.c6c18bd953226p-27, 0x1.62e6p-9 },
				 { 0x1.7a516c34846bdp-26, 0x1.8f46p-9 },
				 { -0x1.f3b83dd8b853p-27, 0x1.bbap-9 },
				 { -0x1.c3459046e4e57p-31, 0x1.e8p-9 },
				 { 0x1.b5c7e34cb79f6p-38, 0x1.0a2cp-8 },
				 { -0x1.2487e9af9a692p-27, 0x1.205cp-8 },
				 { 0x1.f21bbc4ad79cep-26, 0x1.3687p-8 },
				 { -0x1.550ffc857b731p-29, 0x1.4cb7p-8 },
				 { 0x1.87458ec1b7b34p-27, 0x1.62e2p-8 },
				 { 0x1.103d4fe83ee81p-26, 0x1.7911p-8 },
				 { 0x1.810483d3b398cp-27, 0x1.8f44p-8 },
				 { -0x1.2085cb340608ep-27, 0x1.a573p-8 },
				 { 0x1.12698a119c42fp-26, 0x1.bb9dp-8 },
				 { -0x1.edb8c172b4c33p-26, 0x1.d1ccp-8 },
				 { -0x1.8b55b87a5e238p-26, 0x1.e7fep-8 },
				 { 0x1.be5e17763f78ap-26, 0x1.fe2bp-8 },
				 { -0x1.c2d496790073ep-30, 0x1.0a2a8p-7 },
				 { 0x1.6542f523abeecp-26, 0x1.1541p-7 },
				 { -0x1.b7fdbe5b193f8p-26, 0x1.205ap-7 },
				 { 0x1.fa4d42fe30c7cp-26, 0x1.2b7p-7 },
				 { 0x1.0d46ad04adc86p-26, 0x1.36888p-7 },
				 { -0x1.1c22d02d17c4cp-26, 0x1.419fp-7 },
				 { 0x1.a7d1e330dcccep-30, 0x1.4cb7p-7 },
				 { 0x1.187025e656ba3p-31, 0x1.57cdp-7 },
				 { -0x1.532c1269e2038p-27, 0x1.62e5p-7 } };

const double __atanh_t1[]
    = { 0x1p+0,		0x1.ea4afap-1, 0x1.d5818ep-1,  0x1.c199bep-1,
	0x1.ae89f98p-1, 0x1.9c4918p-1, 0x1.8ace54p-1,  0x1.7a1147p-1,
	0x1.6a09e68p-1, 0x1.5ab07ep-1, 0x1.4bfdad8p-1, 0x1.3dea65p-1,
	0x1.306fe08p-1, 0x1.2387a7p-1, 0x1.172b84p-1,  0x1.0b5587p-1,
	0x1p-1 };
const double __atanh_t2[]
    = { 0x1p+0,		0x1.fe9d968p-1, 0x1.fd3c228p-1, 0x1.fbdba38p-1,
	0x1.fa7c18p-1,	0x1.f91d8p-1,	0x1.f7bfdbp-1,	0x1.f663278p-1,
	0x1.f507658p-1, 0x1.f3ac948p-1, 0x1.f252b38p-1, 0x1.f0f9c2p-1,
	0x1.efa1bfp-1,	0x1.ee4aaap-1,	0x1.ecf483p-1,	0x1.eb9f488p-1 };
const double __atanh_t3[]
    = { 0x1p+0,		0x1.ffe9d2p-1,	0x1.ffd3a58p-1, 0x1.ffbd798p-1,
	0x1.ffa74e8p-1, 0x1.ff91248p-1, 0x1.ff7afb8p-1, 0x1.ff64d38p-1,
	0x1.ff4eac8p-1, 0x1.ff38868p-1, 0x1.ff22618p-1, 0x1.ff0c3dp-1,
	0x1.fef61ap-1,	0x1.fedff78p-1, 0x1.fec9d68p-1, 0x1.feb3b6p-1 };
const double __atanh_t4[]
    = { 0x1p+0,		0x1.fffe9dp-1,	0x1.fffd3ap-1,	0x1.fffbd78p-1,
	0x1.fffa748p-1, 0x1.fff9118p-1, 0x1.fff7ae8p-1, 0x1.fff64cp-1,
	0x1.fff4e9p-1,	0x1.fff386p-1,	0x1.fff2238p-1, 0x1.fff0c08p-1,
	0x1.ffef5d8p-1, 0x1.ffedfa8p-1, 0x1.ffec98p-1,	0x1.ffeb35p-1 };
const double __atanh_LL[4][17][3] = {
  {
      { 0x0p+0, 0x0p+0, 0x0p+0 },
      { 0x1.62e432b24p-6, -0x1.745af34bb54b8p-42, -0x1.17e3ec05cde7p-97 },
      { 0x1.62e42e4a8p-5, 0x1.111a4eadf312p-44, 0x1.cff3027abb119p-93 },
      { 0x1.0a2b233f1p-4, -0x1.88ac4ec78af8p-42, 0x1.4fa087ca75dfdp-93 },
      { 0x1.62e43056cp-4, 0x1.6bd65e8b0b7p-46, -0x1.b18e160362c24p-95 },
      { 0x1.bb9d3cbd6p-4, 0x1.de14aa55ec2bp-42, -0x1.c6ac3f1862a6bp-94 },
      { 0x1.0a2b244dap-3, 0x1.94def487fea7p-42, -0x1.dead1a4581acfp-94 },
      { 0x1.3687aa9b78p-3, 0x1.9cec9a50db22p-43, 0x1.34a70684f8e0ep-93 },
      { 0x1.62e42fabap-3, -0x1.d69047a3aebp-44, -0x1.4e061f79144e2p-95 },
      { 0x1.8f40b56d28p-3, 0x1.de7d755fd2e2p-42, 0x1.bdc7ecf001489p-94 },
      { 0x1.bb9d3b61fp-3, 0x1.c14f1445b12p-46, 0x1.a1d78cbdc5b58p-93 },
      { 0x1.e7f9c11f08p-3, -0x1.6e3e0000dae7p-43, 0x1.6a4559fadde98p-94 },
      { 0x1.0a2b242ec4p-2, 0x1.bb7cf852a5fe8p-42, 0x1.a6aef11ee43bdp-93 },
      { 0x1.205966c764p-2, 0x1.ad3a5f214294p-45, 0x1.5cc344fa10652p-93 },
      { 0x1.3687a98aacp-2, 0x1.1623671842fp-45, -0x1.0b428fe1f9e43p-94 },
      { 0x1.4cb5ec93f4p-2, 0x1.3d50980ea513p-42, 0x1.67f0ea083b1c4p-93 },
      { 0x1.62e42fefa4p-2, -0x1.8432a1b0e264p-44, 0x1.803f2f6af40f3p-93 },
  },
  {
      { 0x0p+0, 0x0p+0, 0x0p+0 },
      { 0x1.62e462b4p-10, 0x1.061d003b97318p-42, 0x1.d7faee66a2e1ep-93 },
      { 0x1.62e44c92p-9, 0x1.95a7bff5e239p-42, -0x1.f7e788a87135p-95 },
      { 0x1.0a2b1e33p-8, 0x1.2a3a1a65aa3ap-43, -0x1.54599c9605442p-93 },
      { 0x1.62e4367cp-8, -0x1.4a995b6d9ddcp-45, -0x1.56bb79b254f33p-100 },
      { 0x1.bb9d449ap-8, 0x1.8a119c42e9bcp-42, -0x1.8ecf7d8d661f1p-93 },
      { 0x1.0a2b1f19p-7, 0x1.8863771bd10a8p-42, 0x1.e9731de7f0155p-94 },
      { 0x1.3687ad11p-7, 0x1.e026a347ca1c8p-42, 0x1.fadc62522444dp-97 },
      { 0x1.62e436f28p-7, 0x1.25b84f71b70b8p-42, -0x1.fcb3f98612d27p-96 },
      { 0x1.8f40b7b38p-7, -0x1.62a0a4fd4758p-43, 0x1.3cb3c35d9f6a1p-93 },
      { 0x1.bb9d3abbp-7, -0x1.0ec48f94d786p-42, -0x1.6b47d410e4cc7p-93 },
      { 0x1.e7f9bb23p-7, 0x1.e4415cbc97ap-43, -0x1.3729fdb677231p-93 },
      { 0x1.0a2b22478p-6, -0x1.cb73f4505b03p-42, -0x1.1b3b3a3bc370ap-93 },
      { 0x1.2059691e8p-6, -0x1.abcc3412f264p-43, -0x1.fe6e998e48673p-95 },
      { 0x1.3687a768p-6, -0x1.43901e5c97a9p-42, 0x1.b54cdd52a5d88p-96 },
      { 0x1.4cb5eb5d8p-6, -0x1.8f106f00f13b8p-42, -0x1.8f793f5fce148p-93 },
      { 0x1.62e432b24p-6, -0x1.745af34bb54b8p-42, -0x1.17e3ec05cde7p-97 },
  },
  {
      { 0x0p+0, 0x0p+0, 0x0p+0 },
      { 0x1.62e7bp-14, -0x1.868625640a68p-44, -0x1.34bf0db910f65p-93 },
      { 0x1.62e35f6p-13, -0x1.2ee3d96b696ap-43, 0x1.a2948cd558655p-94 },
      { 0x1.0a2b4b2p-12, 0x1.53edbcf1165p-47, -0x1.cfc26ccf6d0e4p-97 },
      { 0x1.62e4be1p-12, 0x1.783e334614p-52, -0x1.04b96da30e63ap-93 },
      { 0x1.bb9e085p-12, -0x1.60785f20acb2p-43, -0x1.f33369bf7dff1p-96 },
      { 0x1.0a2b94dp-11, 0x1.fd4b3a273353p-42, -0x1.685a35575eff1p-96 },
      { 0x1.368810f8p-11, 0x1.7ded26dc813p-47, -0x1.4c4d1abca79bfp-96 },
      { 0x1.62e47878p-11, 0x1.7d2bee9a1f63p-42, 0x1.860233b7ad13p-93 },
      { 0x1.8f40cb48p-11, -0x1.af034eaf471cp-42, 0x1.ae748822d57b7p-94 },
      { 0x1.bb9d094p-11, -0x1.7a223013a20fp-42, -0x1.1e499087075b6p-93 },
      { 0x1.e7fa32c8p-11, -0x1.b2e67b1b59bdp-43, -0x1.54a41eda30fa6p-93 },
      { 0x1.0a2b237p-10, -0x1.7ad97ff4ac7ap-44, 0x1.f932da91371ddp-93 },
      { 0x1.2059a338p-10, -0x1.96422d90df4p-44, -0x1.90800fbbf2ed3p-94 },
      { 0x1.36879824p-10, 0x1.0f9054001812p-44, 0x1.9567e01e48f9ap-93 },
      { 0x1.4cb602cp-10, -0x1.0d709a5ec0b5p-43, 0x1.253dfd44635d2p-94 },
      { 0x1.62e462b4p-10, 0x1.061d003b97318p-42, 0x1.d7faee66a2e1ep-93 },
  },
  {
      { 0x0p+0, 0x0p+0, 0x0p+0 },
      { 0x1.63007cp-18, -0x1.db0e38e5aaaap-43, 0x1.259a7b94815b9p-93 },
      { 0x1.6300f6p-17, 0x1.2b1c75580438p-44, 0x1.78cabba01e3e4p-93 },
      { 0x1.0a2115p-16, -0x1.5ff223730759p-42, 0x1.8074feacfe49dp-95 },
      { 0x1.62e1ecp-16, -0x1.85d6f6487ce4p-45, 0x1.05485074b9276p-93 },
      { 0x1.bba301p-16, -0x1.af5d58a7c921p-43, -0x1.30a8c0fd2ff5fp-93 },
      { 0x1.0a32298p-15, 0x1.590faa0883bdp-43, 0x1.95e9bda999947p-93 },
      { 0x1.3682f1p-15, 0x1.f0224376efaf8p-42, -0x1.5843c0db50d1p-93 },
      { 0x1.62e3d8p-15, -0x1.142c13daed4ap-43, 0x1.c68a61183ce87p-93 },
      { 0x1.8f44dd8p-15, -0x1.aa489f399931p-43, 0x1.11c5c376854eap-94 },
      { 0x1.bb9601p-15, 0x1.9904d8b6a3638p-42, 0x1.8c89554493c8fp-93 },
      { 0x1.e7f744p-15, 0x1.5785ddbe7cba8p-42, 0x1.e7ff3cde7d70cp-94 },
      { 0x1.0a2c53p-14, -0x1.6d9e8780d0d5p-43, 0x1.ad9c178106693p-94 },
      { 0x1.205d134p-14, -0x1.214a2e893fccp-43, 0x1.548a9500c9822p-93 },
      { 0x1.3685e28p-14, 0x1.e23588646103p-43, 0x1.2a97b26da2d88p-94 },
      { 0x1.4cb6c18p-14, 0x1.2b7cfcea9e0d8p-42, -0x1.5095048a6b824p-93 },
      { 0x1.62e7bp-14, -0x1.868625640a68p-44, -0x1.34bf0db910f65p-93 },
  },
};
const double __atanh_ch2[][2] = {
  { 0x1p-1, 0x1.24b67ee516e3bp-111 },
  { -0x1p-2, -0x1.932ce43199a8dp-110 },
  { 0x1.5555555555555p-3, 0x1.55540c15cf91fp-57 },
};

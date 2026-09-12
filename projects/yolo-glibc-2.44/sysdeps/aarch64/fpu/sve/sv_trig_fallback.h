/* Vectorised fallback for Double-Precision SVE trig functions.

   Copyright (C) 2026 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

static const double ONE_TWENTY_EIGHT_OVER_PI_0[64] = {
  0x1.0000000000014p5,	 0x1.0000000145f3p5,   0x1.000145f306dc8p5,
  0x1.45f306dc9c884p5,	 -0x1.f246c6efab58p4,  0x1.391054a7f09d4p4,
  0x1.529fc2757d1f4p2,	 -0x1.ec5417056591p-1, -0x1.505c1596447e4p5,
  -0x1.596447e493ad4p1,	 0x1.bb81b6c52b328p5,  0x1.b6c52b3278874p5,
  0x1.2b3278872084p5,	 -0x1.8778df7c035d4p5, -0x1.bef806ba71508p4,
  -0x1.ae9c5421443a8p-2, -0x1.38a84288753c8p5, -0x1.0a21d4f246dc8p3,
  -0x1.d4f246dc8e2ep3,	 -0x1.236e4716f6c8cp4, 0x1.b8e909374b8p4,
  0x1.09374b801924cp4,	 -0x1.68ffcdb688afcp3, 0x1.924bba8274648p0,
  -0x1.a22bec5cdbc6p5,	 0x1.3a32439fc3bd8p1,  -0x1.b78c0788538d4p4,
  0x1.fc3bd63962534p5,	 -0x1.4e34ed658c118p2, 0x1.62534e7dd1048p5,
  -0x1.63045df7282b4p4,	 0x1.d1046bea5d768p5,  0x1.afa975da24274p3,
  -0x1.a28976f62cc7p5,	 -0x1.76f62cc71fb28p5, 0x1.d338e04d68bfp5,
  0x1.c09ad17df905p4,	 0x1.68befc827323cp5,  -0x1.037d8cdc538dp5,
  -0x1.8cdc538cf9598p5,	 -0x1.4e33e566305bp3,  -0x1.f2b3182d8dee8p4,
  -0x1.8c16c6f740e88p5,	 0x1.3908bf177bf24p5,  0x1.7e2ef7e4a0ec8p4,
  -0x1.081b5f13801dcp4,	 -0x1.af89c00ed0004p5, -0x1.c00ed00043f4cp5,
  0x1.2fffbc0b301fcp5,	 -0x1.0fd33f8086878p3, -0x1.9fc04343b9d28p4,
  -0x1.0d0ee74a5f594p2,	 -0x1.dce94beb25c14p5, -0x1.4beb25c12593p5,
  0x1.b47db4d9fb3c8p4,	 -0x1.25930261b06ap5,  0x1.fb3c9f2c26dd4p4,
  -0x1.b069ec9161738p5,	 -0x1.ec9161738132cp5, -0x1.61738132c3404p5,
  0x1.fb34f2ff516bcp3,	 0x1.3cbfd45aea4f8p5,  -0x1.5d28ad8453814p2,
  -0x1.15b08a702834p5,
};

static const double ONE_TWENTY_EIGHT_OVER_PI_1[64] = {
  0x1.7cc1b727220a8p-49,  0x1.b727220a94fep-49,	  0x1.c882a53f84ebp-47,
  -0x1.5ac07b1505c14p-47, -0x1.ec5417056591p-49,  0x1.f47d4d377036cp-48,
  0x1.34ddc0db62958p-50,  -0x1.f924eb53361ep-53,  -0x1.275a99b0ef1cp-48,
  -0x1.9b0ef1bef806cp-52, -0x1.de37df00d74e4p-49, -0x1.f7c035d38a844p-47,
  -0x1.ae9c5421443a8p-50, 0x1.d5ef5de2b0db8p-49,  -0x1.443a9e48db91cp-50,
  -0x1.e48db91c5bdb4p-54, -0x1.1b7238b7b645cp-47, -0x1.c5bdb22d1ff9cp-50,
  0x1.26e9700324978p-49,  0x1.700324977505p-49,	  0x1.924bba8274648p-48,
  -0x1.15f62e6de302p-50,  0x1.d1921cfe1debp-50,	  0x1.cfe1deb1cb128p-54,
  -0x1.e214e34ed658cp-50, -0x1.c69dacb1822fp-51,  0x1.29a73ee88236p-50,
  0x1.cfba208d7d4bcp-48,  0x1.046bea5d7689p-51,	  -0x1.415a28976f62cp-47,
  -0x1.44bb7b16638fcp-50, 0x1.213a671c09adp-48,	  0x1.9c7026b45f7e4p-50,
  -0x1.fb29741037d8cp-47, -0x1.741037d8cdc54p-47, -0x1.bec66e29c67ccp-50,
  -0x1.9b8a719f2b318p-48, -0x1.38cf9598c16c8p-47, 0x1.a99cfa4e422fcp-49,
  -0x1.82d8dee81d108p-48, -0x1.bdd03a21036cp-49,  -0x1.d1081b5f138p-52,
  -0x1.036be27003b4p-49,  0x1.0763ff12fffbcp-47,  -0x1.da00087e99fcp-56,
  0x1.fff7816603fbcp-48,  -0x1.fa67f010d0ee8p-50, -0x1.fc04343b9d298p-48,
  0x1.e5e2316b414dcp-47,  0x1.8b5a0a6d1f6d4p-50,  -0x1.7d64b824b2604p-48,
  0x1.1f6d367ecf27cp-50,  0x1.a6cfd9e4f9614p-47,  -0x1.30d834f648b0cp-50,
  0x1.f2c26dd3d18fcp-48,  0x1.36e9e8c7ecd3cp-47,  -0x1.738132c3402bcp-51,
  -0x1.32c3402ba515cp-51, -0x1.a015d28ad8454p-50, 0x1.45aea4f758fd8p-47,
  -0x1.6c229c0a0d074p-49, -0x1.4e050683a130cp-48, -0x1.a0e84c2f8c608p-54,
  -0x1.d0985f18c10ecp-47,
};

static const double ONE_TWENTY_EIGHT_OVER_PI_2[64] = {
  0x1.4fe13abe8fa9cp-101,  0x1.3abe8fa9a6eep-101,   -0x1.70565911f925p-101,
  -0x1.96447e493ad4cp-99,  -0x1.f924eb53361ep-101,  0x1.8a5664f10e41p-100,
  0x1.93c439041fe5p-102,   0x1.c820ff28b1d6p-105,   0x1.07f9458eaf7bp-100,
  0x1.63abdebbc561cp-106,  0x1.5ef5de2b0db94p-101,  0x1.778ac36e48dc8p-99,
  -0x1.e48db91c5bdb4p-102, 0x1.2371d2126e97p-101,   -0x1.6f6c8b47fe6dcp-104,
  0x1.d2e006492eeap-106,   0x1.c00c925dd413cp-99,   0x1.25dd413a32438p-103,
  -0x1.5f62e6de301e4p-102, -0x1.736f180f10a7p-101,  0x1.cfe1deb1cb128p-102,
  0x1.deb1cb129a74p-102,   0x1.cb129a73ee884p-102,  0x1.a73ee88235f54p-106,
  -0x1.177dca0ad144cp-106, 0x1.1afa975da2428p-105,  -0x1.5a28976f62cc8p-103,
  -0x1.12edec598e3f8p-100, 0x1.3a671c09ad17cp-104,  -0x1.8e3f652e8207p-100,
  -0x1.94ba081bec67p-102,  0x1.7df904e64759p-100,   0x1.3991d63983534p-106,
  -0x1.b8a719f2b3184p-100, 0x1.cc1a99cfa4e44p-101,  0x1.339f49c845f8cp-102,
  -0x1.6c6f740e8840cp-103, 0x1.08bf177bf2508p-99,   0x1.77bf250763ffp-103,
  -0x1.b5f13801dap-104,	   0x1.d8ffc4bffef04p-101,  -0x1.da00087e99fcp-104,
  -0x1.0fd33f8086878p-109, 0x1.6603fbcbc462cp-104,  -0x1.0d0ee74a5f594p-110,
  0x1.788c5ad05369p-101,   0x1.6b414da3eda6cp-103,  0x1.4da3eda6cfdap-103,
  -0x1.c125930261b08p-99,  -0x1.30261b069ec9p-103,  -0x1.86c1a7b24585cp-101,
  0x1.6136e9e8c7eccp-103,  -0x1.22c2e70265868p-100, 0x1.8fd9a797fa8b4p-104,
  0x1.9a797fa8b5d48p-100,  0x1.7fa8b5d49eebp-100,   0x1.aea4f758fd7ccp-103,
  0x1.eeb1faf97c5ecp-104,  0x1.faf97c5ecf41cp-104,  -0x1.a0e84c2f8c608p-102,
  -0x1.30be31821d6b4p-104, 0x1.ce7de294a4ba8p-104,  -0x1.d6b5b45650128p-108,
  0x1.4a4ba9afed7ecp-100,
};

static const double ONE_TWENTY_EIGHT_OVER_PI_3[64] = {
  -0x1.911f924eb5336p-153, 0x1.b6c52b3278872p-155,  0x1.4acc9e21c821p-153,
  -0x1.b0ef1bef806bap-152, 0x1.c820ff28b1d5fp-153,  0x1.fe5163abdebbcp-154,
  0x1.63abdebbc561bp-154,  -0x1.0a21d4f246dc9p-157, -0x1.0ea79236e4717p-152,
  -0x1.1b7238b7b645ap-159, -0x1.c8e2ded9169p-153,   -0x1.6f6c8b47fe6dbp-152,
  0x1.d2e006492eea1p-154,  0x1.924bba8274648p-160,  0x1.77504e8c90e7fp-157,
  0x1.3a32439fc3bd6p-159,  -0x1.cdbc603c429c7p-151, 0x1.fc3bd63962535p-155,
  0x1.eb1cb129a73efp-154,  -0x1.a76b2c608bbeep-153, 0x1.a73ee88235f53p-154,
  -0x1.177dca0ad144cp-154, -0x1.ca0ad144bb7b1p-154, -0x1.144bb7b16639p-158,
  0x1.213a671c09ad1p-160,  -0x1.6638fd94ba082p-158, 0x1.c09ad17df904ep-156,
  0x1.ad17df904e647p-152,  0x1.f904e64758e61p-156,  0x1.3991d63983534p-154,
  0x1.d639835339f4ap-154,  -0x1.9f2b3182d8defp-152, -0x1.82d8dee81d108p-160,
  0x1.272117e2ef7e5p-152,  -0x1.d03a21036be27p-153, -0x1.081b5f13801dap-156,
  -0x1.af89c00ed0004p-155, -0x1.3801da00087eap-152, 0x1.2fffbc0b301fep-155,
  -0x1.0fd33f8086877p-157, -0x1.33f80868773a5p-153, -0x1.0d0ee74a5f593p-158,
  0x1.8b5a0a6d1f6d3p-162,  0x1.6829b47db4dap-156,   0x1.1f6d367ecf27dp-162,
  -0x1.25930261b069fp-155, 0x1.fb3c9f2c26dd4p-156,  -0x1.b069ec9161738p-155,
  0x1.6136e9e8c7ecdp-151,  -0x1.61738132c3403p-155, -0x1.c09961a015d29p-154,
  0x1.3cbfd45aea4f7p-155,  -0x1.5d28ad8453814p-158, 0x1.d49eeb1faf97cp-156,
  0x1.eeb1faf97c5edp-152,  0x1.faf97c5ecf41dp-152,  -0x1.d0985f18c10ebp-159,
  0x1.e839cfbc52949p-157,  0x1.cfbc529497536p-157,  -0x1.d6b5b45650128p-156,
  -0x1.b4565012813b8p-156, 0x1.afed7ec47e357p-156,  -0x1.3b81ca8bdea7fp-164,
  0x1.1f8d5d0856033p-154,
};

static const double COS_K_PI_OVER_128[256] = {
  0x1.0000000000000p+0,	 0x1.ffd886084cd0dp-1,	0x1.ff621e3796d7ep-1,
  0x1.fe9cdad01883ap-1,	 0x1.fd88da3d12526p-1,	0x1.fc26470e19fd3p-1,
  0x1.fa7557f08a517p-1,	 0x1.f8764fa714ba9p-1,	0x1.f6297cff75cb0p-1,
  0x1.f38f3ac64e589p-1,	 0x1.f0a7efb9230d7p-1,	0x1.ed740e7684963p-1,
  0x1.e9f4156c62ddap-1,	 0x1.e6288ec48e112p-1,	0x1.e212104f686e5p-1,
  0x1.ddb13b6ccc23cp-1,	 0x1.d906bcf328d46p-1,	0x1.d4134d14dc93ap-1,
  0x1.ced7af43cc773p-1,	 0x1.c954b213411f5p-1,	0x1.c38b2f180bdb1p-1,
  0x1.bd7c0ac6f952ap-1,	 0x1.b728345196e3ep-1,	0x1.b090a58150200p-1,
  0x1.a9b66290ea1a3p-1,	 0x1.a29a7a0462782p-1,	0x1.9b3e047f38741p-1,
  0x1.93a22499263fbp-1,	 0x1.8bc806b151741p-1,	0x1.83b0e0bff976ep-1,
  0x1.7b5df226aafafp-1,	 0x1.72d0837efff96p-1,	0x1.6a09e667f3bcdp-1,
  0x1.610b7551d2cdfp-1,	 0x1.57d69348ceca0p-1,	0x1.4e6cabbe3e5e9p-1,
  0x1.44cf325091dd6p-1,	 0x1.3affa292050b9p-1,	0x1.30ff7fce17035p-1,
  0x1.26d054cdd12dfp-1,	 0x1.1c73b39ae68c8p-1,	0x1.11eb3541b4b23p-1,
  0x1.073879922ffeep-1,	 0x1.f8ba4dbf89abap-2,	0x1.e2b5d3806f63bp-2,
  0x1.cc66e9931c45ep-2,	 0x1.b5d1009e15cc0p-2,	0x1.9ef7943a8ed8ap-2,
  0x1.87de2a6aea963p-2,	 0x1.7088530fa459fp-2,	0x1.58f9a75ab1fddp-2,
  0x1.4135c94176601p-2,	 0x1.294062ed59f06p-2,	0x1.111d262b1f677p-2,
  0x1.f19f97b215f1bp-3,	 0x1.c0b826a7e4f63p-3,	0x1.8f8b83c69a60bp-3,
  0x1.5e214448b3fc6p-3,	 0x1.2c8106e8e613ap-3,	0x1.f564e56a9730ep-4,
  0x1.917a6bc29b42cp-4,	 0x1.2d52092ce19f6p-4,	0x1.91f65f10dd814p-5,
  0x1.92155f7a3667ep-6,	 0x0.0000000000000p-0,	-0x1.92155f7a3667ep-6,
  -0x1.91f65f10dd814p-5, -0x1.2d52092ce19f6p-4, -0x1.917a6bc29b42cp-4,
  -0x1.f564e56a9730ep-4, -0x1.2c8106e8e613ap-3, -0x1.5e214448b3fc6p-3,
  -0x1.8f8b83c69a60bp-3, -0x1.c0b826a7e4f63p-3, -0x1.f19f97b215f1bp-3,
  -0x1.111d262b1f677p-2, -0x1.294062ed59f06p-2, -0x1.4135c94176601p-2,
  -0x1.58f9a75ab1fddp-2, -0x1.7088530fa459fp-2, -0x1.87de2a6aea963p-2,
  -0x1.9ef7943a8ed8ap-2, -0x1.b5d1009e15cc0p-2, -0x1.cc66e9931c45ep-2,
  -0x1.e2b5d3806f63bp-2, -0x1.f8ba4dbf89abap-2, -0x1.073879922ffeep-1,
  -0x1.11eb3541b4b23p-1, -0x1.1c73b39ae68c8p-1, -0x1.26d054cdd12dfp-1,
  -0x1.30ff7fce17035p-1, -0x1.3affa292050b9p-1, -0x1.44cf325091dd6p-1,
  -0x1.4e6cabbe3e5e9p-1, -0x1.57d69348ceca0p-1, -0x1.610b7551d2cdfp-1,
  -0x1.6a09e667f3bcdp-1, -0x1.72d0837efff96p-1, -0x1.7b5df226aafafp-1,
  -0x1.83b0e0bff976ep-1, -0x1.8bc806b151741p-1, -0x1.93a22499263fbp-1,
  -0x1.9b3e047f38741p-1, -0x1.a29a7a0462782p-1, -0x1.a9b66290ea1a3p-1,
  -0x1.b090a58150200p-1, -0x1.b728345196e3ep-1, -0x1.bd7c0ac6f952ap-1,
  -0x1.c38b2f180bdb1p-1, -0x1.c954b213411f5p-1, -0x1.ced7af43cc773p-1,
  -0x1.d4134d14dc93ap-1, -0x1.d906bcf328d46p-1, -0x1.ddb13b6ccc23cp-1,
  -0x1.e212104f686e5p-1, -0x1.e6288ec48e112p-1, -0x1.e9f4156c62ddap-1,
  -0x1.ed740e7684963p-1, -0x1.f0a7efb9230d7p-1, -0x1.f38f3ac64e589p-1,
  -0x1.f6297cff75cb0p-1, -0x1.f8764fa714ba9p-1, -0x1.fa7557f08a517p-1,
  -0x1.fc26470e19fd3p-1, -0x1.fd88da3d12526p-1, -0x1.fe9cdad01883ap-1,
  -0x1.ff621e3796d7ep-1, -0x1.ffd886084cd0dp-1, -0x1.0000000000000p+0,
  -0x1.ffd886084cd0dp-1, -0x1.ff621e3796d7ep-1, -0x1.fe9cdad01883ap-1,
  -0x1.fd88da3d12526p-1, -0x1.fc26470e19fd3p-1, -0x1.fa7557f08a517p-1,
  -0x1.f8764fa714ba9p-1, -0x1.f6297cff75cb0p-1, -0x1.f38f3ac64e589p-1,
  -0x1.f0a7efb9230d7p-1, -0x1.ed740e7684963p-1, -0x1.e9f4156c62ddap-1,
  -0x1.e6288ec48e112p-1, -0x1.e212104f686e5p-1, -0x1.ddb13b6ccc23cp-1,
  -0x1.d906bcf328d46p-1, -0x1.d4134d14dc93ap-1, -0x1.ced7af43cc773p-1,
  -0x1.c954b213411f5p-1, -0x1.c38b2f180bdb1p-1, -0x1.bd7c0ac6f952ap-1,
  -0x1.b728345196e3ep-1, -0x1.b090a58150200p-1, -0x1.a9b66290ea1a3p-1,
  -0x1.a29a7a0462782p-1, -0x1.9b3e047f38741p-1, -0x1.93a22499263fbp-1,
  -0x1.8bc806b151741p-1, -0x1.83b0e0bff976ep-1, -0x1.7b5df226aafafp-1,
  -0x1.72d0837efff96p-1, -0x1.6a09e667f3bcdp-1, -0x1.610b7551d2cdfp-1,
  -0x1.57d69348ceca0p-1, -0x1.4e6cabbe3e5e9p-1, -0x1.44cf325091dd6p-1,
  -0x1.3affa292050b9p-1, -0x1.30ff7fce17035p-1, -0x1.26d054cdd12dfp-1,
  -0x1.1c73b39ae68c8p-1, -0x1.11eb3541b4b23p-1, -0x1.073879922ffeep-1,
  -0x1.f8ba4dbf89abap-2, -0x1.e2b5d3806f63bp-2, -0x1.cc66e9931c45ep-2,
  -0x1.b5d1009e15cc0p-2, -0x1.9ef7943a8ed8ap-2, -0x1.87de2a6aea963p-2,
  -0x1.7088530fa459fp-2, -0x1.58f9a75ab1fddp-2, -0x1.4135c94176601p-2,
  -0x1.294062ed59f06p-2, -0x1.111d262b1f677p-2, -0x1.f19f97b215f1bp-3,
  -0x1.c0b826a7e4f63p-3, -0x1.8f8b83c69a60bp-3, -0x1.5e214448b3fc6p-3,
  -0x1.2c8106e8e613ap-3, -0x1.f564e56a9730ep-4, -0x1.917a6bc29b42cp-4,
  -0x1.2d52092ce19f6p-4, -0x1.91f65f10dd814p-5, -0x1.92155f7a3667ep-6,
  0x0.0000000000000p-0,	 0x1.92155f7a3667ep-6,	0x1.91f65f10dd814p-5,
  0x1.2d52092ce19f6p-4,	 0x1.917a6bc29b42cp-4,	0x1.f564e56a9730ep-4,
  0x1.2c8106e8e613ap-3,	 0x1.5e214448b3fc6p-3,	0x1.8f8b83c69a60bp-3,
  0x1.c0b826a7e4f63p-3,	 0x1.f19f97b215f1bp-3,	0x1.111d262b1f677p-2,
  0x1.294062ed59f06p-2,	 0x1.4135c94176601p-2,	0x1.58f9a75ab1fddp-2,
  0x1.7088530fa459fp-2,	 0x1.87de2a6aea963p-2,	0x1.9ef7943a8ed8ap-2,
  0x1.b5d1009e15cc0p-2,	 0x1.cc66e9931c45ep-2,	0x1.e2b5d3806f63bp-2,
  0x1.f8ba4dbf89abap-2,	 0x1.073879922ffeep-1,	0x1.11eb3541b4b23p-1,
  0x1.1c73b39ae68c8p-1,	 0x1.26d054cdd12dfp-1,	0x1.30ff7fce17035p-1,
  0x1.3affa292050b9p-1,	 0x1.44cf325091dd6p-1,	0x1.4e6cabbe3e5e9p-1,
  0x1.57d69348ceca0p-1,	 0x1.610b7551d2cdfp-1,	0x1.6a09e667f3bcdp-1,
  0x1.72d0837efff96p-1,	 0x1.7b5df226aafafp-1,	0x1.83b0e0bff976ep-1,
  0x1.8bc806b151741p-1,	 0x1.93a22499263fbp-1,	0x1.9b3e047f38741p-1,
  0x1.a29a7a0462782p-1,	 0x1.a9b66290ea1a3p-1,	0x1.b090a58150200p-1,
  0x1.b728345196e3ep-1,	 0x1.bd7c0ac6f952ap-1,	0x1.c38b2f180bdb1p-1,
  0x1.c954b213411f5p-1,	 0x1.ced7af43cc773p-1,	0x1.d4134d14dc93ap-1,
  0x1.d906bcf328d46p-1,	 0x1.ddb13b6ccc23cp-1,	0x1.e212104f686e5p-1,
  0x1.e6288ec48e112p-1,	 0x1.e9f4156c62ddap-1,	0x1.ed740e7684963p-1,
  0x1.f0a7efb9230d7p-1,	 0x1.f38f3ac64e589p-1,	0x1.f6297cff75cb0p-1,
  0x1.f8764fa714ba9p-1,	 0x1.fa7557f08a517p-1,	0x1.fc26470e19fd3p-1,
  0x1.fd88da3d12526p-1,	 0x1.fe9cdad01883ap-1,	0x1.ff621e3796d7ep-1,
  0x1.ffd886084cd0dp-1,
};

static const double SIN_K_PI_OVER_128[256] = {
  0x0.0000000000000p-0,	 0x1.92155f7a3667ep-6,	0x1.91f65f10dd814p-5,
  0x1.2d52092ce19f6p-4,	 0x1.917a6bc29b42cp-4,	0x1.f564e56a9730ep-4,
  0x1.2c8106e8e613ap-3,	 0x1.5e214448b3fc6p-3,	0x1.8f8b83c69a60bp-3,
  0x1.c0b826a7e4f63p-3,	 0x1.f19f97b215f1bp-3,	0x1.111d262b1f677p-2,
  0x1.294062ed59f06p-2,	 0x1.4135c94176601p-2,	0x1.58f9a75ab1fddp-2,
  0x1.7088530fa459fp-2,	 0x1.87de2a6aea963p-2,	0x1.9ef7943a8ed8ap-2,
  0x1.b5d1009e15cc0p-2,	 0x1.cc66e9931c45ep-2,	0x1.e2b5d3806f63bp-2,
  0x1.f8ba4dbf89abap-2,	 0x1.073879922ffeep-1,	0x1.11eb3541b4b23p-1,
  0x1.1c73b39ae68c8p-1,	 0x1.26d054cdd12dfp-1,	0x1.30ff7fce17035p-1,
  0x1.3affa292050b9p-1,	 0x1.44cf325091dd6p-1,	0x1.4e6cabbe3e5e9p-1,
  0x1.57d69348ceca0p-1,	 0x1.610b7551d2cdfp-1,	0x1.6a09e667f3bcdp-1,
  0x1.72d0837efff96p-1,	 0x1.7b5df226aafafp-1,	0x1.83b0e0bff976ep-1,
  0x1.8bc806b151741p-1,	 0x1.93a22499263fbp-1,	0x1.9b3e047f38741p-1,
  0x1.a29a7a0462782p-1,	 0x1.a9b66290ea1a3p-1,	0x1.b090a58150200p-1,
  0x1.b728345196e3ep-1,	 0x1.bd7c0ac6f952ap-1,	0x1.c38b2f180bdb1p-1,
  0x1.c954b213411f5p-1,	 0x1.ced7af43cc773p-1,	0x1.d4134d14dc93ap-1,
  0x1.d906bcf328d46p-1,	 0x1.ddb13b6ccc23cp-1,	0x1.e212104f686e5p-1,
  0x1.e6288ec48e112p-1,	 0x1.e9f4156c62ddap-1,	0x1.ed740e7684963p-1,
  0x1.f0a7efb9230d7p-1,	 0x1.f38f3ac64e589p-1,	0x1.f6297cff75cb0p-1,
  0x1.f8764fa714ba9p-1,	 0x1.fa7557f08a517p-1,	0x1.fc26470e19fd3p-1,
  0x1.fd88da3d12526p-1,	 0x1.fe9cdad01883ap-1,	0x1.ff621e3796d7ep-1,
  0x1.ffd886084cd0dp-1,	 0x1.0000000000000p+0,	0x1.ffd886084cd0dp-1,
  0x1.ff621e3796d7ep-1,	 0x1.fe9cdad01883ap-1,	0x1.fd88da3d12526p-1,
  0x1.fc26470e19fd3p-1,	 0x1.fa7557f08a517p-1,	0x1.f8764fa714ba9p-1,
  0x1.f6297cff75cb0p-1,	 0x1.f38f3ac64e589p-1,	0x1.f0a7efb9230d7p-1,
  0x1.ed740e7684963p-1,	 0x1.e9f4156c62ddap-1,	0x1.e6288ec48e112p-1,
  0x1.e212104f686e5p-1,	 0x1.ddb13b6ccc23cp-1,	0x1.d906bcf328d46p-1,
  0x1.d4134d14dc93ap-1,	 0x1.ced7af43cc773p-1,	0x1.c954b213411f5p-1,
  0x1.c38b2f180bdb1p-1,	 0x1.bd7c0ac6f952ap-1,	0x1.b728345196e3ep-1,
  0x1.b090a58150200p-1,	 0x1.a9b66290ea1a3p-1,	0x1.a29a7a0462782p-1,
  0x1.9b3e047f38741p-1,	 0x1.93a22499263fbp-1,	0x1.8bc806b151741p-1,
  0x1.83b0e0bff976ep-1,	 0x1.7b5df226aafafp-1,	0x1.72d0837efff96p-1,
  0x1.6a09e667f3bcdp-1,	 0x1.610b7551d2cdfp-1,	0x1.57d69348ceca0p-1,
  0x1.4e6cabbe3e5e9p-1,	 0x1.44cf325091dd6p-1,	0x1.3affa292050b9p-1,
  0x1.30ff7fce17035p-1,	 0x1.26d054cdd12dfp-1,	0x1.1c73b39ae68c8p-1,
  0x1.11eb3541b4b23p-1,	 0x1.073879922ffeep-1,	0x1.f8ba4dbf89abap-2,
  0x1.e2b5d3806f63bp-2,	 0x1.cc66e9931c45ep-2,	0x1.b5d1009e15cc0p-2,
  0x1.9ef7943a8ed8ap-2,	 0x1.87de2a6aea963p-2,	0x1.7088530fa459fp-2,
  0x1.58f9a75ab1fddp-2,	 0x1.4135c94176601p-2,	0x1.294062ed59f06p-2,
  0x1.111d262b1f677p-2,	 0x1.f19f97b215f1bp-3,	0x1.c0b826a7e4f63p-3,
  0x1.8f8b83c69a60bp-3,	 0x1.5e214448b3fc6p-3,	0x1.2c8106e8e613ap-3,
  0x1.f564e56a9730ep-4,	 0x1.917a6bc29b42cp-4,	0x1.2d52092ce19f6p-4,
  0x1.91f65f10dd814p-5,	 0x1.92155f7a3667ep-6,	0x0.0000000000000p-0,
  -0x1.92155f7a3667ep-6, -0x1.91f65f10dd814p-5, -0x1.2d52092ce19f6p-4,
  -0x1.917a6bc29b42cp-4, -0x1.f564e56a9730ep-4, -0x1.2c8106e8e613ap-3,
  -0x1.5e214448b3fc6p-3, -0x1.8f8b83c69a60bp-3, -0x1.c0b826a7e4f63p-3,
  -0x1.f19f97b215f1bp-3, -0x1.111d262b1f677p-2, -0x1.294062ed59f06p-2,
  -0x1.4135c94176601p-2, -0x1.58f9a75ab1fddp-2, -0x1.7088530fa459fp-2,
  -0x1.87de2a6aea963p-2, -0x1.9ef7943a8ed8ap-2, -0x1.b5d1009e15cc0p-2,
  -0x1.cc66e9931c45ep-2, -0x1.e2b5d3806f63bp-2, -0x1.f8ba4dbf89abap-2,
  -0x1.073879922ffeep-1, -0x1.11eb3541b4b23p-1, -0x1.1c73b39ae68c8p-1,
  -0x1.26d054cdd12dfp-1, -0x1.30ff7fce17035p-1, -0x1.3affa292050b9p-1,
  -0x1.44cf325091dd6p-1, -0x1.4e6cabbe3e5e9p-1, -0x1.57d69348ceca0p-1,
  -0x1.610b7551d2cdfp-1, -0x1.6a09e667f3bcdp-1, -0x1.72d0837efff96p-1,
  -0x1.7b5df226aafafp-1, -0x1.83b0e0bff976ep-1, -0x1.8bc806b151741p-1,
  -0x1.93a22499263fbp-1, -0x1.9b3e047f38741p-1, -0x1.a29a7a0462782p-1,
  -0x1.a9b66290ea1a3p-1, -0x1.b090a58150200p-1, -0x1.b728345196e3ep-1,
  -0x1.bd7c0ac6f952ap-1, -0x1.c38b2f180bdb1p-1, -0x1.c954b213411f5p-1,
  -0x1.ced7af43cc773p-1, -0x1.d4134d14dc93ap-1, -0x1.d906bcf328d46p-1,
  -0x1.ddb13b6ccc23cp-1, -0x1.e212104f686e5p-1, -0x1.e6288ec48e112p-1,
  -0x1.e9f4156c62ddap-1, -0x1.ed740e7684963p-1, -0x1.f0a7efb9230d7p-1,
  -0x1.f38f3ac64e589p-1, -0x1.f6297cff75cb0p-1, -0x1.f8764fa714ba9p-1,
  -0x1.fa7557f08a517p-1, -0x1.fc26470e19fd3p-1, -0x1.fd88da3d12526p-1,
  -0x1.fe9cdad01883ap-1, -0x1.ff621e3796d7ep-1, -0x1.ffd886084cd0dp-1,
  -0x1.0000000000000p+0, -0x1.ffd886084cd0dp-1, -0x1.ff621e3796d7ep-1,
  -0x1.fe9cdad01883ap-1, -0x1.fd88da3d12526p-1, -0x1.fc26470e19fd3p-1,
  -0x1.fa7557f08a517p-1, -0x1.f8764fa714ba9p-1, -0x1.f6297cff75cb0p-1,
  -0x1.f38f3ac64e589p-1, -0x1.f0a7efb9230d7p-1, -0x1.ed740e7684963p-1,
  -0x1.e9f4156c62ddap-1, -0x1.e6288ec48e112p-1, -0x1.e212104f686e5p-1,
  -0x1.ddb13b6ccc23cp-1, -0x1.d906bcf328d46p-1, -0x1.d4134d14dc93ap-1,
  -0x1.ced7af43cc773p-1, -0x1.c954b213411f5p-1, -0x1.c38b2f180bdb1p-1,
  -0x1.bd7c0ac6f952ap-1, -0x1.b728345196e3ep-1, -0x1.b090a58150200p-1,
  -0x1.a9b66290ea1a3p-1, -0x1.a29a7a0462782p-1, -0x1.9b3e047f38741p-1,
  -0x1.93a22499263fbp-1, -0x1.8bc806b151741p-1, -0x1.83b0e0bff976ep-1,
  -0x1.7b5df226aafafp-1, -0x1.72d0837efff96p-1, -0x1.6a09e667f3bcdp-1,
  -0x1.610b7551d2cdfp-1, -0x1.57d69348ceca0p-1, -0x1.4e6cabbe3e5e9p-1,
  -0x1.44cf325091dd6p-1, -0x1.3affa292050b9p-1, -0x1.30ff7fce17035p-1,
  -0x1.26d054cdd12dfp-1, -0x1.1c73b39ae68c8p-1, -0x1.11eb3541b4b23p-1,
  -0x1.073879922ffeep-1, -0x1.f8ba4dbf89abap-2, -0x1.e2b5d3806f63bp-2,
  -0x1.cc66e9931c45ep-2, -0x1.b5d1009e15cc0p-2, -0x1.9ef7943a8ed8ap-2,
  -0x1.87de2a6aea963p-2, -0x1.7088530fa459fp-2, -0x1.58f9a75ab1fddp-2,
  -0x1.4135c94176601p-2, -0x1.294062ed59f06p-2, -0x1.111d262b1f677p-2,
  -0x1.f19f97b215f1bp-3, -0x1.c0b826a7e4f63p-3, -0x1.8f8b83c69a60bp-3,
  -0x1.5e214448b3fc6p-3, -0x1.2c8106e8e613ap-3, -0x1.f564e56a9730ep-4,
  -0x1.917a6bc29b42cp-4, -0x1.2d52092ce19f6p-4, -0x1.91f65f10dd814p-5,
  -0x1.92155f7a3667ep-6,
};

/* Error-free multiplication using double-double computation
   using the TwoProd algorithm.
   hi is the rounded product, lo is the exact FMA residual.  */
static inline svfloat64x2_t
two_prod (svfloat64_t a, svfloat64_t b)
{
  svfloat64_t hi = svmul_x (svptrue_b64 (), a, b);
  svfloat64_t lo = svnmls_x (svptrue_b64 (), hi, a, b);
  return svcreate2 (hi, lo);
}

/* Error-free sum using double-double computation.
   using the FastTwoSum algorithm, which requires |a| >= |b|.
   hi is the rounded sum and lo recovers the low-order
   bits lost by that rounding.  */
static inline svfloat64x2_t
fast_two_sum (svfloat64_t a, svfloat64_t b)
{
  svfloat64_t hi = svadd_x (svptrue_b64 (), a, b);
  svfloat64_t tmp = svsub_x (svptrue_b64 (), hi, a);
  svfloat64_t lo = svsub_x (svptrue_b64 (), b, tmp);
  return svcreate2 (hi, lo);
}

static inline svfloat64x4_t
load_datablock (svuint64_t idx)
{
  idx = svand_x (svptrue_b64 (), idx, sv_u64 (63));

  svfloat64_t D0, D1, D2, D3;
  D0 = svld1_gather_index (svptrue_b64 (), ONE_TWENTY_EIGHT_OVER_PI_0, idx);
  D1 = svld1_gather_index (svptrue_b64 (), ONE_TWENTY_EIGHT_OVER_PI_1, idx);
  D2 = svld1_gather_index (svptrue_b64 (), ONE_TWENTY_EIGHT_OVER_PI_2, idx);
  D3 = svld1_gather_index (svptrue_b64 (), ONE_TWENTY_EIGHT_OVER_PI_3, idx);

  return svcreate4 (D0, D1, D2, D3);
}

/* Reduce a large finite x modulo 2*pi, such that:
     x = k * (pi / 128) + remainder

   Returns an svfloat64x2_t containing:
   An svfloat64_t remainder.
   The integer k.

   Note that k is reinterpreted as an svfloat64_t in order to be packed into
   the second element of the return type, since SVE types cannot be struct
   elements.  */
static inline svfloat64x2_t
sv_large_range_reduction (svfloat64_t x)
{
  svbool_t ptrue = svptrue_b64 ();

  /* First, |x| is reduced into the range [2^62, 2^78), by directly
     adjusting the exponent. This ensures the rounded high part of the
     leading product contributes only multiples of 2^8, so the useful bits
     of k mod 256 are entirely contained within the lower product terms.  */
  svuint64_t ix = svreinterpret_u64 (x);
  svuint64_t exponent = svlsr_x (ptrue, ix, 52);
  svint64_t x_e_m62
      = svreinterpret_s64 (svsub_x (ptrue, exponent, sv_u64 ((1023 + 62))));

  /* We can then use the adjusted exponent to gather from the 128/pi
     tables.  */
  svuint64_t idx = svadd_x (
      ptrue, svreinterpret_u64 (svasr_x (ptrue, x_e_m62, 4)), sv_u64 (3));
  svfloat64x4_t data = load_datablock (idx);

  /* x_e_m62 has already been split into:
      x_e_m62 = 16 * row + offset
    where row selected the 128/pi table row above.

    We want to keep the offset (x_e_m62 mod 16), and use it to produce
    a new exponent (62 + offset) so that x_reduced is within our intended
    [62, 77] exponent window.  */
  svint64_t masked = svand_x (ptrue, x_e_m62, sv_s64 (15));
  svint64_t added = svadd_x (ptrue, masked, sv_s64 (1023 + 62));
  svint64_t shifted = svlsl_x (ptrue, added, 52);
  svuint64_t new_exponent = svreinterpret_u64 (shifted);

  /* Finally, we get our reduced x value, by reinserting the new exponent into
     the original input sign + mantissa.  */
  svuint64_t signed_mantissa
      = svand_x (ptrue, ix, sv_u64 (0x800fffffffffffff));
  ix = svorr_x (ptrue, new_exponent, signed_mantissa);
  svfloat64_t x_reduced = svreinterpret_f64 (ix);

  /* We now use the reduced x to calculate x * 128/pi ~= k + y.
     First, we multiply x_reduced by the first three chunks of the 128/pi
     table, using double-double arithmetic to maintain a high precision
     intermediate.  */
  svfloat64x2_t ph = two_prod (x_reduced, svget4 (data, 0));
  svfloat64x2_t pm = two_prod (x_reduced, svget4 (data, 1));
  svfloat64x2_t pl = two_prod (x_reduced, svget4 (data, 2));

  svfloat64_t ph_lo = svget2 (ph, 1);
  svfloat64_t pm_hi = svget2 (pm, 0);
  svfloat64_t pm_lo = svget2 (pm, 1);
  svfloat64_t pl_hi = svget2 (pl, 0);
  svfloat64_t pl_lo = svget2 (pl, 1);

  /* Next, accumulate the terms needed to get the integer k.
     However, ph_hi will always be a multiple of 2^8, so it cannot affect
     k mod 256. pm_lo and all lower terms will always be sufficiently small
     that they cannot affect the rounded integer result. Therefore we only
     need to sum ph_lo and pm_hi when computing k mod 256.  */
  svfloat64_t sum_hi = svadd_x (ptrue, ph_lo, pm_hi);
  svfloat64_t kd = svrintn_x (ptrue, sum_hi);

  /* To compute the remainder, we need to remove the rounded integer k and
     accumulate the remaining terms as a two-part remainder.  */
  svfloat64_t y_hi = svadd_x (ptrue, svsub_x (ptrue, ph_lo, kd), pm_hi);
  svfloat64x2_t y_mid = fast_two_sum (pm_lo, pl_hi);

  svfloat64_t y_mid_hi = svget2 (y_mid, 0);
  svfloat64_t y_mid_lo = svget2 (y_mid, 1);

  /* The low portion of x_reduced * D3 has no meaningful contribution to the
    result, so a simple FMA is sufficient.  */
  svfloat64_t y_l = svmla_x (ptrue, pl_lo, x_reduced, svget4 (data, 3));

  /* Our final remainder is (y_hi + y_mid_hi + y_mid_lo + y_l) * pi / 128.
     We can split the multiplication into two parts to maintain accuracy.  */
  svfloat64_t y = svadd_x (ptrue, y_hi, y_mid_hi);

  svfloat64_t pio128 = sv_f64 (0x1.921fb54442d18p-6);
  y_l = svmul_x (ptrue, svadd_x (ptrue, y_mid_lo, y_l), pio128);

  svfloat64_t remainder = svmla_x (ptrue, y_l, y, pio128);

  /* Convert k into integer bits to pack into the return tuple.  */
  svfloat64_t quadrant = svreinterpret_f64 (svcvt_s64_x (ptrue, kd));

  return svcreate2 (remainder, quadrant);
}

static inline svfloat64x2_t
sv_sincos_eval (svfloat64_t r)
{
  svfloat64_t s2 = sv_f64 (0x1.11111110efcdap-7);
  svfloat64_t s3 = sv_f64 (-0x1.a019249e23866p-13);
  svfloat64_t c0 = sv_f64 (-0x1p-1);
  svfloat64_t c1 = sv_f64 (0x1.5555555549e76p-5);
  svfloat64_t c2 = sv_f64 (-0x1.6c165d6952a53p-10);

  svfloat64_t r2 = svmul_x (svptrue_b64 (), r, r);
  svfloat64_t r3 = svmul_x (svptrue_b64 (), r2, r);

  svfloat64_t sin = svmla_x (svptrue_b64 (), s2, r2, s3);
  /* This term is exactly 1/3!, so svtmad is slightly faster.  */
  sin = svtmad (sin, r2, 1);
  sin = svmad_x (svptrue_b64 (), sin, r3, r);

  svfloat64_t cosm1 = svmla_x (svptrue_b64 (), c1, r2, c2);
  cosm1 = svmad_x (svptrue_b64 (), cosm1, r2, c0);
  cosm1 = svmul_x (svptrue_b64 (), cosm1, r2);

  return svcreate2 (sin, cosm1);
}

static inline svfloat64x2_t
sv_sin_cos_lookup (svuint64_t idx)
{
  idx = svand_x (svptrue_b64 (), idx, sv_u64 (255));

  svfloat64_t sin_k, cos_k;
  sin_k = svld1_gather_index (svptrue_b64 (), SIN_K_PI_OVER_128, idx);
  cos_k = svld1_gather_index (svptrue_b64 (), COS_K_PI_OVER_128, idx);

  return svcreate2 (sin_k, cos_k);
}

/* Vectorised fallback for double-precision AdvSIMD trig functions.

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

static const double ONE_TWENTY_EIGHT_OVER_PI[64][4] = {
  { 0x1.0000000000014p5, 0x1.7cc1b727220a8p-49, 0x1.4fe13abe8fa9cp-101,
    -0x1.911f924eb5336p-153 },
  { 0x1.0000000145f3p5, 0x1.b727220a94fep-49, 0x1.3abe8fa9a6eep-101,
    0x1.b6c52b3278872p-155 },
  { 0x1.000145f306dc8p5, 0x1.c882a53f84ebp-47, -0x1.70565911f925p-101,
    0x1.4acc9e21c821p-153 },
  { 0x1.45f306dc9c884p5, -0x1.5ac07b1505c14p-47, -0x1.96447e493ad4cp-99,
    -0x1.b0ef1bef806bap-152 },
  { -0x1.f246c6efab58p4, -0x1.ec5417056591p-49, -0x1.f924eb53361ep-101,
    0x1.c820ff28b1d5fp-153 },
  { 0x1.391054a7f09d4p4, 0x1.f47d4d377036cp-48, 0x1.8a5664f10e41p-100,
    0x1.fe5163abdebbcp-154 },
  { 0x1.529fc2757d1f4p2, 0x1.34ddc0db62958p-50, 0x1.93c439041fe5p-102,
    0x1.63abdebbc561bp-154 },
  { -0x1.ec5417056591p-1, -0x1.f924eb53361ep-53, 0x1.c820ff28b1d6p-105,
    -0x1.0a21d4f246dc9p-157 },
  { -0x1.505c1596447e4p5, -0x1.275a99b0ef1cp-48, 0x1.07f9458eaf7bp-100,
    -0x1.0ea79236e4717p-152 },
  { -0x1.596447e493ad4p1, -0x1.9b0ef1bef806cp-52, 0x1.63abdebbc561cp-106,
    -0x1.1b7238b7b645ap-159 },
  { 0x1.bb81b6c52b328p5, -0x1.de37df00d74e4p-49, 0x1.5ef5de2b0db94p-101,
    -0x1.c8e2ded9169p-153 },
  { 0x1.b6c52b3278874p5, -0x1.f7c035d38a844p-47, 0x1.778ac36e48dc8p-99,
    -0x1.6f6c8b47fe6dbp-152 },
  { 0x1.2b3278872084p5, -0x1.ae9c5421443a8p-50, -0x1.e48db91c5bdb4p-102,
    0x1.d2e006492eea1p-154 },
  { -0x1.8778df7c035d4p5, 0x1.d5ef5de2b0db8p-49, 0x1.2371d2126e97p-101,
    0x1.924bba8274648p-160 },
  { -0x1.bef806ba71508p4, -0x1.443a9e48db91cp-50, -0x1.6f6c8b47fe6dcp-104,
    0x1.77504e8c90e7fp-157 },
  { -0x1.ae9c5421443a8p-2, -0x1.e48db91c5bdb4p-54, 0x1.d2e006492eeap-106,
    0x1.3a32439fc3bd6p-159 },
  { -0x1.38a84288753c8p5, -0x1.1b7238b7b645cp-47, 0x1.c00c925dd413cp-99,
    -0x1.cdbc603c429c7p-151 },
  { -0x1.0a21d4f246dc8p3, -0x1.c5bdb22d1ff9cp-50, 0x1.25dd413a32438p-103,
    0x1.fc3bd63962535p-155 },
  { -0x1.d4f246dc8e2ep3, 0x1.26e9700324978p-49, -0x1.5f62e6de301e4p-102,
    0x1.eb1cb129a73efp-154 },
  { -0x1.236e4716f6c8cp4, 0x1.700324977505p-49, -0x1.736f180f10a7p-101,
    -0x1.a76b2c608bbeep-153 },
  { 0x1.b8e909374b8p4, 0x1.924bba8274648p-48, 0x1.cfe1deb1cb128p-102,
    0x1.a73ee88235f53p-154 },
  { 0x1.09374b801924cp4, -0x1.15f62e6de302p-50, 0x1.deb1cb129a74p-102,
    -0x1.177dca0ad144cp-154 },
  { -0x1.68ffcdb688afcp3, 0x1.d1921cfe1debp-50, 0x1.cb129a73ee884p-102,
    -0x1.ca0ad144bb7b1p-154 },
  { 0x1.924bba8274648p0, 0x1.cfe1deb1cb128p-54, 0x1.a73ee88235f54p-106,
    -0x1.144bb7b16639p-158 },
  { -0x1.a22bec5cdbc6p5, -0x1.e214e34ed658cp-50, -0x1.177dca0ad144cp-106,
    0x1.213a671c09ad1p-160 },
  { 0x1.3a32439fc3bd8p1, -0x1.c69dacb1822fp-51, 0x1.1afa975da2428p-105,
    -0x1.6638fd94ba082p-158 },
  { -0x1.b78c0788538d4p4, 0x1.29a73ee88236p-50, -0x1.5a28976f62cc8p-103,
    0x1.c09ad17df904ep-156 },
  { 0x1.fc3bd63962534p5, 0x1.cfba208d7d4bcp-48, -0x1.12edec598e3f8p-100,
    0x1.ad17df904e647p-152 },
  { -0x1.4e34ed658c118p2, 0x1.046bea5d7689p-51, 0x1.3a671c09ad17cp-104,
    0x1.f904e64758e61p-156 },
  { 0x1.62534e7dd1048p5, -0x1.415a28976f62cp-47, -0x1.8e3f652e8207p-100,
    0x1.3991d63983534p-154 },
  { -0x1.63045df7282b4p4, -0x1.44bb7b16638fcp-50, -0x1.94ba081bec67p-102,
    0x1.d639835339f4ap-154 },
  { 0x1.d1046bea5d768p5, 0x1.213a671c09adp-48, 0x1.7df904e64759p-100,
    -0x1.9f2b3182d8defp-152 },
  { 0x1.afa975da24274p3, 0x1.9c7026b45f7e4p-50, 0x1.3991d63983534p-106,
    -0x1.82d8dee81d108p-160 },
  { -0x1.a28976f62cc7p5, -0x1.fb29741037d8cp-47, -0x1.b8a719f2b3184p-100,
    0x1.272117e2ef7e5p-152 },
  { -0x1.76f62cc71fb28p5, -0x1.741037d8cdc54p-47, 0x1.cc1a99cfa4e44p-101,
    -0x1.d03a21036be27p-153 },
  { 0x1.d338e04d68bfp5, -0x1.bec66e29c67ccp-50, 0x1.339f49c845f8cp-102,
    -0x1.081b5f13801dap-156 },
  { 0x1.c09ad17df905p4, -0x1.9b8a719f2b318p-48, -0x1.6c6f740e8840cp-103,
    -0x1.af89c00ed0004p-155 },
  { 0x1.68befc827323cp5, -0x1.38cf9598c16c8p-47, 0x1.08bf177bf2508p-99,
    -0x1.3801da00087eap-152 },
  { -0x1.037d8cdc538dp5, 0x1.a99cfa4e422fcp-49, 0x1.77bf250763ffp-103,
    0x1.2fffbc0b301fep-155 },
  { -0x1.8cdc538cf9598p5, -0x1.82d8dee81d108p-48, -0x1.b5f13801dap-104,
    -0x1.0fd33f8086877p-157 },
  { -0x1.4e33e566305bp3, -0x1.bdd03a21036cp-49, 0x1.d8ffc4bffef04p-101,
    -0x1.33f80868773a5p-153 },
  { -0x1.f2b3182d8dee8p4, -0x1.d1081b5f138p-52, -0x1.da00087e99fcp-104,
    -0x1.0d0ee74a5f593p-158 },
  { -0x1.8c16c6f740e88p5, -0x1.036be27003b4p-49, -0x1.0fd33f8086878p-109,
    0x1.8b5a0a6d1f6d3p-162 },
  { 0x1.3908bf177bf24p5, 0x1.0763ff12fffbcp-47, 0x1.6603fbcbc462cp-104,
    0x1.6829b47db4dap-156 },
  { 0x1.7e2ef7e4a0ec8p4, -0x1.da00087e99fcp-56, -0x1.0d0ee74a5f594p-110,
    0x1.1f6d367ecf27dp-162 },
  { -0x1.081b5f13801dcp4, 0x1.fff7816603fbcp-48, 0x1.788c5ad05369p-101,
    -0x1.25930261b069fp-155 },
  { -0x1.af89c00ed0004p5, -0x1.fa67f010d0ee8p-50, 0x1.6b414da3eda6cp-103,
    0x1.fb3c9f2c26dd4p-156 },
  { -0x1.c00ed00043f4cp5, -0x1.fc04343b9d298p-48, 0x1.4da3eda6cfdap-103,
    -0x1.b069ec9161738p-155 },
  { 0x1.2fffbc0b301fcp5, 0x1.e5e2316b414dcp-47, -0x1.c125930261b08p-99,
    0x1.6136e9e8c7ecdp-151 },
  { -0x1.0fd33f8086878p3, 0x1.8b5a0a6d1f6d4p-50, -0x1.30261b069ec9p-103,
    -0x1.61738132c3403p-155 },
  { -0x1.9fc04343b9d28p4, -0x1.7d64b824b2604p-48, -0x1.86c1a7b24585cp-101,
    -0x1.c09961a015d29p-154 },
  { -0x1.0d0ee74a5f594p2, 0x1.1f6d367ecf27cp-50, 0x1.6136e9e8c7eccp-103,
    0x1.3cbfd45aea4f7p-155 },
  { -0x1.dce94beb25c14p5, 0x1.a6cfd9e4f9614p-47, -0x1.22c2e70265868p-100,
    -0x1.5d28ad8453814p-158 },
  { -0x1.4beb25c12593p5, -0x1.30d834f648b0cp-50, 0x1.8fd9a797fa8b4p-104,
    0x1.d49eeb1faf97cp-156 },
  { 0x1.b47db4d9fb3c8p4, 0x1.f2c26dd3d18fcp-48, 0x1.9a797fa8b5d48p-100,
    0x1.eeb1faf97c5edp-152 },
  { -0x1.25930261b06ap5, 0x1.36e9e8c7ecd3cp-47, 0x1.7fa8b5d49eebp-100,
    0x1.faf97c5ecf41dp-152 },
  { 0x1.fb3c9f2c26dd4p4, -0x1.738132c3402bcp-51, 0x1.aea4f758fd7ccp-103,
    -0x1.d0985f18c10ebp-159 },
  { -0x1.b069ec9161738p5, -0x1.32c3402ba515cp-51, 0x1.eeb1faf97c5ecp-104,
    0x1.e839cfbc52949p-157 },
  { -0x1.ec9161738132cp5, -0x1.a015d28ad8454p-50, 0x1.faf97c5ecf41cp-104,
    0x1.cfbc529497536p-157 },
  { -0x1.61738132c3404p5, 0x1.45aea4f758fd8p-47, -0x1.a0e84c2f8c608p-102,
    -0x1.d6b5b45650128p-156 },
  { 0x1.fb34f2ff516bcp3, -0x1.6c229c0a0d074p-49, -0x1.30be31821d6b4p-104,
    -0x1.b4565012813b8p-156 },
  { 0x1.3cbfd45aea4f8p5, -0x1.4e050683a130cp-48, 0x1.ce7de294a4ba8p-104,
    0x1.afed7ec47e357p-156 },
  { -0x1.5d28ad8453814p2, -0x1.a0e84c2f8c608p-54, -0x1.d6b5b45650128p-108,
    -0x1.3b81ca8bdea7fp-164 },
  { -0x1.15b08a702834p5, -0x1.d0985f18c10ecp-47, 0x1.4a4ba9afed7ecp-100,
    0x1.1f8d5d0856033p-154 },
};

/* 256 values of a { sin, cos } in increments of pi/128.  */
static const double SIN_COS_K_PI_OVER_128[256][2] = {
  { 0, 1 },
  { 0x1.92155f7a3667ep-6, 0x1.ffd886084cd0dp-1 },
  { 0x1.91f65f10dd814p-5, 0x1.ff621e3796d7ep-1 },
  { 0x1.2d52092ce19f6p-4, 0x1.fe9cdad01883ap-1 },
  { 0x1.917a6bc29b42cp-4, 0x1.fd88da3d12526p-1 },
  { 0x1.f564e56a9730ep-4, 0x1.fc26470e19fd3p-1 },
  { 0x1.2c8106e8e613ap-3, 0x1.fa7557f08a517p-1 },
  { 0x1.5e214448b3fc6p-3, 0x1.f8764fa714ba9p-1 },
  { 0x1.8f8b83c69a60bp-3, 0x1.f6297cff75cb0p-1 },
  { 0x1.c0b826a7e4f63p-3, 0x1.f38f3ac64e589p-1 },
  { 0x1.f19f97b215f1bp-3, 0x1.f0a7efb9230d7p-1 },
  { 0x1.111d262b1f677p-2, 0x1.ed740e7684963p-1 },
  { 0x1.294062ed59f06p-2, 0x1.e9f4156c62ddap-1 },
  { 0x1.4135c94176601p-2, 0x1.e6288ec48e112p-1 },
  { 0x1.58f9a75ab1fddp-2, 0x1.e212104f686e5p-1 },
  { 0x1.7088530fa459fp-2, 0x1.ddb13b6ccc23cp-1 },
  { 0x1.87de2a6aea963p-2, 0x1.d906bcf328d46p-1 },
  { 0x1.9ef7943a8ed8ap-2, 0x1.d4134d14dc93ap-1 },
  { 0x1.b5d1009e15cc0p-2, 0x1.ced7af43cc773p-1 },
  { 0x1.cc66e9931c45ep-2, 0x1.c954b213411f5p-1 },
  { 0x1.e2b5d3806f63bp-2, 0x1.c38b2f180bdb1p-1 },
  { 0x1.f8ba4dbf89abap-2, 0x1.bd7c0ac6f952ap-1 },
  { 0x1.073879922ffeep-1, 0x1.b728345196e3ep-1 },
  { 0x1.11eb3541b4b23p-1, 0x1.b090a58150200p-1 },
  { 0x1.1c73b39ae68c8p-1, 0x1.a9b66290ea1a3p-1 },
  { 0x1.26d054cdd12dfp-1, 0x1.a29a7a0462782p-1 },
  { 0x1.30ff7fce17035p-1, 0x1.9b3e047f38741p-1 },
  { 0x1.3affa292050b9p-1, 0x1.93a22499263fbp-1 },
  { 0x1.44cf325091dd6p-1, 0x1.8bc806b151741p-1 },
  { 0x1.4e6cabbe3e5e9p-1, 0x1.83b0e0bff976ep-1 },
  { 0x1.57d69348ceca0p-1, 0x1.7b5df226aafafp-1 },
  { 0x1.610b7551d2cdfp-1, 0x1.72d0837efff96p-1 },
  { 0x1.6a09e667f3bcdp-1, 0x1.6a09e667f3bcdp-1 },
  { 0x1.72d0837efff96p-1, 0x1.610b7551d2cdfp-1 },
  { 0x1.7b5df226aafafp-1, 0x1.57d69348ceca0p-1 },
  { 0x1.83b0e0bff976ep-1, 0x1.4e6cabbe3e5e9p-1 },
  { 0x1.8bc806b151741p-1, 0x1.44cf325091dd6p-1 },
  { 0x1.93a22499263fbp-1, 0x1.3affa292050b9p-1 },
  { 0x1.9b3e047f38741p-1, 0x1.30ff7fce17035p-1 },
  { 0x1.a29a7a0462782p-1, 0x1.26d054cdd12dfp-1 },
  { 0x1.a9b66290ea1a3p-1, 0x1.1c73b39ae68c8p-1 },
  { 0x1.b090a58150200p-1, 0x1.11eb3541b4b23p-1 },
  { 0x1.b728345196e3ep-1, 0x1.073879922ffeep-1 },
  { 0x1.bd7c0ac6f952ap-1, 0x1.f8ba4dbf89abap-2 },
  { 0x1.c38b2f180bdb1p-1, 0x1.e2b5d3806f63bp-2 },
  { 0x1.c954b213411f5p-1, 0x1.cc66e9931c45ep-2 },
  { 0x1.ced7af43cc773p-1, 0x1.b5d1009e15cc0p-2 },
  { 0x1.d4134d14dc93ap-1, 0x1.9ef7943a8ed8ap-2 },
  { 0x1.d906bcf328d46p-1, 0x1.87de2a6aea963p-2 },
  { 0x1.ddb13b6ccc23cp-1, 0x1.7088530fa459fp-2 },
  { 0x1.e212104f686e5p-1, 0x1.58f9a75ab1fddp-2 },
  { 0x1.e6288ec48e112p-1, 0x1.4135c94176601p-2 },
  { 0x1.e9f4156c62ddap-1, 0x1.294062ed59f06p-2 },
  { 0x1.ed740e7684963p-1, 0x1.111d262b1f677p-2 },
  { 0x1.f0a7efb9230d7p-1, 0x1.f19f97b215f1bp-3 },
  { 0x1.f38f3ac64e589p-1, 0x1.c0b826a7e4f63p-3 },
  { 0x1.f6297cff75cb0p-1, 0x1.8f8b83c69a60bp-3 },
  { 0x1.f8764fa714ba9p-1, 0x1.5e214448b3fc6p-3 },
  { 0x1.fa7557f08a517p-1, 0x1.2c8106e8e613ap-3 },
  { 0x1.fc26470e19fd3p-1, 0x1.f564e56a9730ep-4 },
  { 0x1.fd88da3d12526p-1, 0x1.917a6bc29b42cp-4 },
  { 0x1.fe9cdad01883ap-1, 0x1.2d52092ce19f6p-4 },
  { 0x1.ff621e3796d7ep-1, 0x1.91f65f10dd814p-5 },
  { 0x1.ffd886084cd0dp-1, 0x1.92155f7a3667ep-6 },
  { 1, 0 },
  { 0x1.ffd886084cd0dp-1, -0x1.92155f7a3667ep-6 },
  { 0x1.ff621e3796d7ep-1, -0x1.91f65f10dd814p-5 },
  { 0x1.fe9cdad01883ap-1, -0x1.2d52092ce19f6p-4 },
  { 0x1.fd88da3d12526p-1, -0x1.917a6bc29b42cp-4 },
  { 0x1.fc26470e19fd3p-1, -0x1.f564e56a9730ep-4 },
  { 0x1.fa7557f08a517p-1, -0x1.2c8106e8e613ap-3 },
  { 0x1.f8764fa714ba9p-1, -0x1.5e214448b3fc6p-3 },
  { 0x1.f6297cff75cb0p-1, -0x1.8f8b83c69a60bp-3 },
  { 0x1.f38f3ac64e589p-1, -0x1.c0b826a7e4f63p-3 },
  { 0x1.f0a7efb9230d7p-1, -0x1.f19f97b215f1bp-3 },
  { 0x1.ed740e7684963p-1, -0x1.111d262b1f677p-2 },
  { 0x1.e9f4156c62ddap-1, -0x1.294062ed59f06p-2 },
  { 0x1.e6288ec48e112p-1, -0x1.4135c94176601p-2 },
  { 0x1.e212104f686e5p-1, -0x1.58f9a75ab1fddp-2 },
  { 0x1.ddb13b6ccc23cp-1, -0x1.7088530fa459fp-2 },
  { 0x1.d906bcf328d46p-1, -0x1.87de2a6aea963p-2 },
  { 0x1.d4134d14dc93ap-1, -0x1.9ef7943a8ed8ap-2 },
  { 0x1.ced7af43cc773p-1, -0x1.b5d1009e15cc0p-2 },
  { 0x1.c954b213411f5p-1, -0x1.cc66e9931c45ep-2 },
  { 0x1.c38b2f180bdb1p-1, -0x1.e2b5d3806f63bp-2 },
  { 0x1.bd7c0ac6f952ap-1, -0x1.f8ba4dbf89abap-2 },
  { 0x1.b728345196e3ep-1, -0x1.073879922ffeep-1 },
  { 0x1.b090a58150200p-1, -0x1.11eb3541b4b23p-1 },
  { 0x1.a9b66290ea1a3p-1, -0x1.1c73b39ae68c8p-1 },
  { 0x1.a29a7a0462782p-1, -0x1.26d054cdd12dfp-1 },
  { 0x1.9b3e047f38741p-1, -0x1.30ff7fce17035p-1 },
  { 0x1.93a22499263fbp-1, -0x1.3affa292050b9p-1 },
  { 0x1.8bc806b151741p-1, -0x1.44cf325091dd6p-1 },
  { 0x1.83b0e0bff976ep-1, -0x1.4e6cabbe3e5e9p-1 },
  { 0x1.7b5df226aafafp-1, -0x1.57d69348ceca0p-1 },
  { 0x1.72d0837efff96p-1, -0x1.610b7551d2cdfp-1 },
  { 0x1.6a09e667f3bcdp-1, -0x1.6a09e667f3bcdp-1 },
  { 0x1.610b7551d2cdfp-1, -0x1.72d0837efff96p-1 },
  { 0x1.57d69348ceca0p-1, -0x1.7b5df226aafafp-1 },
  { 0x1.4e6cabbe3e5e9p-1, -0x1.83b0e0bff976ep-1 },
  { 0x1.44cf325091dd6p-1, -0x1.8bc806b151741p-1 },
  { 0x1.3affa292050b9p-1, -0x1.93a22499263fbp-1 },
  { 0x1.30ff7fce17035p-1, -0x1.9b3e047f38741p-1 },
  { 0x1.26d054cdd12dfp-1, -0x1.a29a7a0462782p-1 },
  { 0x1.1c73b39ae68c8p-1, -0x1.a9b66290ea1a3p-1 },
  { 0x1.11eb3541b4b23p-1, -0x1.b090a58150200p-1 },
  { 0x1.073879922ffeep-1, -0x1.b728345196e3ep-1 },
  { 0x1.f8ba4dbf89abap-2, -0x1.bd7c0ac6f952ap-1 },
  { 0x1.e2b5d3806f63bp-2, -0x1.c38b2f180bdb1p-1 },
  { 0x1.cc66e9931c45ep-2, -0x1.c954b213411f5p-1 },
  { 0x1.b5d1009e15cc0p-2, -0x1.ced7af43cc773p-1 },
  { 0x1.9ef7943a8ed8ap-2, -0x1.d4134d14dc93ap-1 },
  { 0x1.87de2a6aea963p-2, -0x1.d906bcf328d46p-1 },
  { 0x1.7088530fa459fp-2, -0x1.ddb13b6ccc23cp-1 },
  { 0x1.58f9a75ab1fddp-2, -0x1.e212104f686e5p-1 },
  { 0x1.4135c94176601p-2, -0x1.e6288ec48e112p-1 },
  { 0x1.294062ed59f06p-2, -0x1.e9f4156c62ddap-1 },
  { 0x1.111d262b1f677p-2, -0x1.ed740e7684963p-1 },
  { 0x1.f19f97b215f1bp-3, -0x1.f0a7efb9230d7p-1 },
  { 0x1.c0b826a7e4f63p-3, -0x1.f38f3ac64e589p-1 },
  { 0x1.8f8b83c69a60bp-3, -0x1.f6297cff75cb0p-1 },
  { 0x1.5e214448b3fc6p-3, -0x1.f8764fa714ba9p-1 },
  { 0x1.2c8106e8e613ap-3, -0x1.fa7557f08a517p-1 },
  { 0x1.f564e56a9730ep-4, -0x1.fc26470e19fd3p-1 },
  { 0x1.917a6bc29b42cp-4, -0x1.fd88da3d12526p-1 },
  { 0x1.2d52092ce19f6p-4, -0x1.fe9cdad01883ap-1 },
  { 0x1.91f65f10dd814p-5, -0x1.ff621e3796d7ep-1 },
  { 0x1.92155f7a3667ep-6, -0x1.ffd886084cd0dp-1 },
  { 0, -1 },
  { -0x1.92155f7a3667ep-6, -0x1.ffd886084cd0dp-1 },
  { -0x1.91f65f10dd814p-5, -0x1.ff621e3796d7ep-1 },
  { -0x1.2d52092ce19f6p-4, -0x1.fe9cdad01883ap-1 },
  { -0x1.917a6bc29b42cp-4, -0x1.fd88da3d12526p-1 },
  { -0x1.f564e56a9730ep-4, -0x1.fc26470e19fd3p-1 },
  { -0x1.2c8106e8e613ap-3, -0x1.fa7557f08a517p-1 },
  { -0x1.5e214448b3fc6p-3, -0x1.f8764fa714ba9p-1 },
  { -0x1.8f8b83c69a60bp-3, -0x1.f6297cff75cb0p-1 },
  { -0x1.c0b826a7e4f63p-3, -0x1.f38f3ac64e589p-1 },
  { -0x1.f19f97b215f1bp-3, -0x1.f0a7efb9230d7p-1 },
  { -0x1.111d262b1f677p-2, -0x1.ed740e7684963p-1 },
  { -0x1.294062ed59f06p-2, -0x1.e9f4156c62ddap-1 },
  { -0x1.4135c94176601p-2, -0x1.e6288ec48e112p-1 },
  { -0x1.58f9a75ab1fddp-2, -0x1.e212104f686e5p-1 },
  { -0x1.7088530fa459fp-2, -0x1.ddb13b6ccc23cp-1 },
  { -0x1.87de2a6aea963p-2, -0x1.d906bcf328d46p-1 },
  { -0x1.9ef7943a8ed8ap-2, -0x1.d4134d14dc93ap-1 },
  { -0x1.b5d1009e15cc0p-2, -0x1.ced7af43cc773p-1 },
  { -0x1.cc66e9931c45ep-2, -0x1.c954b213411f5p-1 },
  { -0x1.e2b5d3806f63bp-2, -0x1.c38b2f180bdb1p-1 },
  { -0x1.f8ba4dbf89abap-2, -0x1.bd7c0ac6f952ap-1 },
  { -0x1.073879922ffeep-1, -0x1.b728345196e3ep-1 },
  { -0x1.11eb3541b4b23p-1, -0x1.b090a58150200p-1 },
  { -0x1.1c73b39ae68c8p-1, -0x1.a9b66290ea1a3p-1 },
  { -0x1.26d054cdd12dfp-1, -0x1.a29a7a0462782p-1 },
  { -0x1.30ff7fce17035p-1, -0x1.9b3e047f38741p-1 },
  { -0x1.3affa292050b9p-1, -0x1.93a22499263fbp-1 },
  { -0x1.44cf325091dd6p-1, -0x1.8bc806b151741p-1 },
  { -0x1.4e6cabbe3e5e9p-1, -0x1.83b0e0bff976ep-1 },
  { -0x1.57d69348ceca0p-1, -0x1.7b5df226aafafp-1 },
  { -0x1.610b7551d2cdfp-1, -0x1.72d0837efff96p-1 },
  { -0x1.6a09e667f3bcdp-1, -0x1.6a09e667f3bcdp-1 },
  { -0x1.72d0837efff96p-1, -0x1.610b7551d2cdfp-1 },
  { -0x1.7b5df226aafafp-1, -0x1.57d69348ceca0p-1 },
  { -0x1.83b0e0bff976ep-1, -0x1.4e6cabbe3e5e9p-1 },
  { -0x1.8bc806b151741p-1, -0x1.44cf325091dd6p-1 },
  { -0x1.93a22499263fbp-1, -0x1.3affa292050b9p-1 },
  { -0x1.9b3e047f38741p-1, -0x1.30ff7fce17035p-1 },
  { -0x1.a29a7a0462782p-1, -0x1.26d054cdd12dfp-1 },
  { -0x1.a9b66290ea1a3p-1, -0x1.1c73b39ae68c8p-1 },
  { -0x1.b090a58150200p-1, -0x1.11eb3541b4b23p-1 },
  { -0x1.b728345196e3ep-1, -0x1.073879922ffeep-1 },
  { -0x1.bd7c0ac6f952ap-1, -0x1.f8ba4dbf89abap-2 },
  { -0x1.c38b2f180bdb1p-1, -0x1.e2b5d3806f63bp-2 },
  { -0x1.c954b213411f5p-1, -0x1.cc66e9931c45ep-2 },
  { -0x1.ced7af43cc773p-1, -0x1.b5d1009e15cc0p-2 },
  { -0x1.d4134d14dc93ap-1, -0x1.9ef7943a8ed8ap-2 },
  { -0x1.d906bcf328d46p-1, -0x1.87de2a6aea963p-2 },
  { -0x1.ddb13b6ccc23cp-1, -0x1.7088530fa459fp-2 },
  { -0x1.e212104f686e5p-1, -0x1.58f9a75ab1fddp-2 },
  { -0x1.e6288ec48e112p-1, -0x1.4135c94176601p-2 },
  { -0x1.e9f4156c62ddap-1, -0x1.294062ed59f06p-2 },
  { -0x1.ed740e7684963p-1, -0x1.111d262b1f677p-2 },
  { -0x1.f0a7efb9230d7p-1, -0x1.f19f97b215f1bp-3 },
  { -0x1.f38f3ac64e589p-1, -0x1.c0b826a7e4f63p-3 },
  { -0x1.f6297cff75cb0p-1, -0x1.8f8b83c69a60bp-3 },
  { -0x1.f8764fa714ba9p-1, -0x1.5e214448b3fc6p-3 },
  { -0x1.fa7557f08a517p-1, -0x1.2c8106e8e613ap-3 },
  { -0x1.fc26470e19fd3p-1, -0x1.f564e56a9730ep-4 },
  { -0x1.fd88da3d12526p-1, -0x1.917a6bc29b42cp-4 },
  { -0x1.fe9cdad01883ap-1, -0x1.2d52092ce19f6p-4 },
  { -0x1.ff621e3796d7ep-1, -0x1.91f65f10dd814p-5 },
  { -0x1.ffd886084cd0dp-1, -0x1.92155f7a3667ep-6 },
  { -1, 0 },
  { -0x1.ffd886084cd0dp-1, 0x1.92155f7a3667ep-6 },
  { -0x1.ff621e3796d7ep-1, 0x1.91f65f10dd814p-5 },
  { -0x1.fe9cdad01883ap-1, 0x1.2d52092ce19f6p-4 },
  { -0x1.fd88da3d12526p-1, 0x1.917a6bc29b42cp-4 },
  { -0x1.fc26470e19fd3p-1, 0x1.f564e56a9730ep-4 },
  { -0x1.fa7557f08a517p-1, 0x1.2c8106e8e613ap-3 },
  { -0x1.f8764fa714ba9p-1, 0x1.5e214448b3fc6p-3 },
  { -0x1.f6297cff75cb0p-1, 0x1.8f8b83c69a60bp-3 },
  { -0x1.f38f3ac64e589p-1, 0x1.c0b826a7e4f63p-3 },
  { -0x1.f0a7efb9230d7p-1, 0x1.f19f97b215f1bp-3 },
  { -0x1.ed740e7684963p-1, 0x1.111d262b1f677p-2 },
  { -0x1.e9f4156c62ddap-1, 0x1.294062ed59f06p-2 },
  { -0x1.e6288ec48e112p-1, 0x1.4135c94176601p-2 },
  { -0x1.e212104f686e5p-1, 0x1.58f9a75ab1fddp-2 },
  { -0x1.ddb13b6ccc23cp-1, 0x1.7088530fa459fp-2 },
  { -0x1.d906bcf328d46p-1, 0x1.87de2a6aea963p-2 },
  { -0x1.d4134d14dc93ap-1, 0x1.9ef7943a8ed8ap-2 },
  { -0x1.ced7af43cc773p-1, 0x1.b5d1009e15cc0p-2 },
  { -0x1.c954b213411f5p-1, 0x1.cc66e9931c45ep-2 },
  { -0x1.c38b2f180bdb1p-1, 0x1.e2b5d3806f63bp-2 },
  { -0x1.bd7c0ac6f952ap-1, 0x1.f8ba4dbf89abap-2 },
  { -0x1.b728345196e3ep-1, 0x1.073879922ffeep-1 },
  { -0x1.b090a58150200p-1, 0x1.11eb3541b4b23p-1 },
  { -0x1.a9b66290ea1a3p-1, 0x1.1c73b39ae68c8p-1 },
  { -0x1.a29a7a0462782p-1, 0x1.26d054cdd12dfp-1 },
  { -0x1.9b3e047f38741p-1, 0x1.30ff7fce17035p-1 },
  { -0x1.93a22499263fbp-1, 0x1.3affa292050b9p-1 },
  { -0x1.8bc806b151741p-1, 0x1.44cf325091dd6p-1 },
  { -0x1.83b0e0bff976ep-1, 0x1.4e6cabbe3e5e9p-1 },
  { -0x1.7b5df226aafafp-1, 0x1.57d69348ceca0p-1 },
  { -0x1.72d0837efff96p-1, 0x1.610b7551d2cdfp-1 },
  { -0x1.6a09e667f3bcdp-1, 0x1.6a09e667f3bcdp-1 },
  { -0x1.610b7551d2cdfp-1, 0x1.72d0837efff96p-1 },
  { -0x1.57d69348ceca0p-1, 0x1.7b5df226aafafp-1 },
  { -0x1.4e6cabbe3e5e9p-1, 0x1.83b0e0bff976ep-1 },
  { -0x1.44cf325091dd6p-1, 0x1.8bc806b151741p-1 },
  { -0x1.3affa292050b9p-1, 0x1.93a22499263fbp-1 },
  { -0x1.30ff7fce17035p-1, 0x1.9b3e047f38741p-1 },
  { -0x1.26d054cdd12dfp-1, 0x1.a29a7a0462782p-1 },
  { -0x1.1c73b39ae68c8p-1, 0x1.a9b66290ea1a3p-1 },
  { -0x1.11eb3541b4b23p-1, 0x1.b090a58150200p-1 },
  { -0x1.073879922ffeep-1, 0x1.b728345196e3ep-1 },
  { -0x1.f8ba4dbf89abap-2, 0x1.bd7c0ac6f952ap-1 },
  { -0x1.e2b5d3806f63bp-2, 0x1.c38b2f180bdb1p-1 },
  { -0x1.cc66e9931c45ep-2, 0x1.c954b213411f5p-1 },
  { -0x1.b5d1009e15cc0p-2, 0x1.ced7af43cc773p-1 },
  { -0x1.9ef7943a8ed8ap-2, 0x1.d4134d14dc93ap-1 },
  { -0x1.87de2a6aea963p-2, 0x1.d906bcf328d46p-1 },
  { -0x1.7088530fa459fp-2, 0x1.ddb13b6ccc23cp-1 },
  { -0x1.58f9a75ab1fddp-2, 0x1.e212104f686e5p-1 },
  { -0x1.4135c94176601p-2, 0x1.e6288ec48e112p-1 },
  { -0x1.294062ed59f06p-2, 0x1.e9f4156c62ddap-1 },
  { -0x1.111d262b1f677p-2, 0x1.ed740e7684963p-1 },
  { -0x1.f19f97b215f1bp-3, 0x1.f0a7efb9230d7p-1 },
  { -0x1.c0b826a7e4f63p-3, 0x1.f38f3ac64e589p-1 },
  { -0x1.8f8b83c69a60bp-3, 0x1.f6297cff75cb0p-1 },
  { -0x1.5e214448b3fc6p-3, 0x1.f8764fa714ba9p-1 },
  { -0x1.2c8106e8e613ap-3, 0x1.fa7557f08a517p-1 },
  { -0x1.f564e56a9730ep-4, 0x1.fc26470e19fd3p-1 },
  { -0x1.917a6bc29b42cp-4, 0x1.fd88da3d12526p-1 },
  { -0x1.2d52092ce19f6p-4, 0x1.fe9cdad01883ap-1 },
  { -0x1.91f65f10dd814p-5, 0x1.ff621e3796d7ep-1 },
  { -0x1.92155f7a3667ep-6, 0x1.ffd886084cd0dp-1 },
};

struct reduction_result_t
{
  uint64x2_t quadrant;
  float64x2_t remainder;
};

/* Error-free multiplication using double-double computation
   using the TwoProd algorithm.
   hi is the rounded product, lo is the exact FMA residual.  */
static inline VPCS_ATTR float64x2x2_t
two_prod (float64x2_t a, float64x2_t b)
{
  float64x2x2_t result;
  result.val[0] = vmulq_f64 (a, b);
  result.val[1] = vfmaq_f64 (-result.val[0], a, b);
  return result;
}

/* Error-free sum using double-double computation.
   using the FastTwoSum algorithm, which requires |a| >= |b|.
   hi is the rounded sum and lo recovers the low-order
   bits lost by that rounding.  */
static inline VPCS_ATTR float64x2x2_t
fast_two_sum (float64x2_t a, float64x2_t b)
{
  float64x2x2_t result;
  result.val[0] = vaddq_f64 (a, b);
  float64x2_t t = vsubq_f64 (result.val[0], a);
  result.val[1] = vsubq_f64 (b, t);
  return result;
}

static inline VPCS_ATTR float64x2x4_t
load_datablock (uint64x2_t idx)
{
  uint64_t idx0 = vgetq_lane_u64 (idx, 0) & 63;
  uint64_t idx1 = vgetq_lane_u64 (idx, 1) & 63;

  float64x2_t temp0 = vld1q_f64 (&ONE_TWENTY_EIGHT_OVER_PI[idx0][0]);
  float64x2_t temp1 = vld1q_f64 (&ONE_TWENTY_EIGHT_OVER_PI[idx0][2]);

  float64x2_t temp2 = vld1q_f64 (&ONE_TWENTY_EIGHT_OVER_PI[idx1][0]);
  float64x2_t temp3 = vld1q_f64 (&ONE_TWENTY_EIGHT_OVER_PI[idx1][2]);

  float64x2x4_t data;
  data.val[0] = vzip1q_f64 (temp0, temp2);
  data.val[1] = vzip2q_f64 (temp0, temp2);
  data.val[2] = vzip1q_f64 (temp1, temp3);
  data.val[3] = vzip2q_f64 (temp1, temp3);

  return data;
}

static const struct reduction_data
{
  double s3, c2;
  float64x2_t s1, s2, c0, c1;
  float64x2_t pio128;
} reduction_data = {
  .s1 = V2 (-0x1.5555555555555p-3),
  .s2 = V2 (0x1.11111110efcdap-7),
  .s3 = -0x1.a019249e23866p-13,
  .c0 = V2 (-0x1p-1),
  .c1 = V2 (0x1.5555555549e76p-5),
  .c2 = -0x1.6c165d6952a53p-10,
  .pio128 = V2 (0x1.921fb54442d18p-6),
};
/* Reduce a large finite x modulo 2*pi, such that:
     x = k * (pi / 128) + remainder

   Returns a reduction_result_t containing:
   A uint64x2_t containing the integer k.
   A float64x2_t containing the remainder.  */
static inline VPCS_ATTR struct reduction_result_t
v_large_range_reduction (float64x2_t x, const struct reduction_data *d)
{
  /* First, |x| is reduced into the range [2^62, 2^78), by directly
    adjusting the exponent. This ensures the rounded high part of the
    leading product contributes only multiples of 2^8, so the useful bits
    of k mod 256 are entirely contained within the lower product terms.  */
  uint64x2_t ix = vreinterpretq_u64_f64 (x);
  int64x2_t x_e_m62
      = vreinterpretq_s64_u64 (vsraq_n_u64 (v_u64 ((-(1023 + 62))), ix, 52));

  /* We can then use the adjusted exponent to gather from the 128/pi
     tables.  */
  uint64x2_t idx = vreinterpretq_u64_s64 (vsraq_n_s64 (v_s64 (3), x_e_m62, 4));
  float64x2x4_t data = load_datablock (idx);

  /* x_e_m62 has already been split into:
    x_e_m62 = 16 * row + offset
  where row selected the 128/pi table row above.

  We want to keep the offset (x_e_m62 mod 16), and use it to produce
  a new exponent (62 + offset) so that x_reduced is within our intended
  [62, 77] exponent window.  */
  int64x2_t masked = vandq_s64 (x_e_m62, v_s64 (15));
  int64x2_t added = vaddq_s64 (masked, v_s64 (1023 + 62));
  int64x2_t shifted = vshlq_n_s64 (added, 52);
  uint64x2_t new_exponent = vreinterpretq_u64_s64 (shifted);

  uint64x2_t x_mantissa = vandq_u64 (ix, v_u64 (0x800fffffffffffff));
  ix = vorrq_u64 (new_exponent, x_mantissa);
  float64x2_t x_reduced = vreinterpretq_f64_u64 (ix);

  float64x2x2_t ph = two_prod (x_reduced, data.val[0]);
  float64x2x2_t pm = two_prod (x_reduced, data.val[1]);
  float64x2x2_t pl = two_prod (x_reduced, data.val[2]);

  /* Next, accumulate the terms needed to get the integer k.
     However, ph_hi will always be a multiple of 2^8, so it cannot affect
     k mod 256. pm_lo and all lower terms will always be sufficiently small
     that they cannot affect the rounded integer result. Therefore we only
     need to sum ph_lo and pm_hi when computing k mod 256.  */
  float64x2_t sum_hi = vaddq_f64 (ph.val[1], pm.val[0]);
  float64x2_t kd = vrndnq_f64 (sum_hi);

  /* To compute the remainder, we need to remove the rounded integer k and
     accumulate the remaining terms as a two-part remainder.  */
  float64x2_t y_hi = vaddq_f64 (vsubq_f64 (ph.val[1], kd), pm.val[0]);
  float64x2x2_t y_mid = fast_two_sum (pm.val[1], pl.val[0]);

  /* The low portion of x_reduced * D3 has no meaningful contribution to the
     result, so a simple FMA is sufficient.  */
  float64x2_t y_l = vfmaq_f64 (pl.val[1], x_reduced, data.val[3]);

  /* Our final remainder is (y_hi + y_mid_hi + y_mid_lo + y_l) * pi / 128.
     We can split the multiplication into two parts to maintain accuracy.  */
  float64x2_t y = vaddq_f64 (y_hi, y_mid.val[0]);

  y_l = vmulq_f64 (vaddq_f64 (y_mid.val[1], y_l), d->pio128);

  struct reduction_result_t result;
  result.remainder = vfmaq_f64 (y_l, y, d->pio128);
  result.quadrant = vreinterpretq_u64_s64 (vcvtpq_s64_f64 (kd));
  return result;
}

static inline VPCS_ATTR float64x2x2_t
v_sincos_eval (float64x2_t r, const struct reduction_data *d)
{
  float64x2_t s3c2 = vld1q_f64 (&d->s3);

  float64x2_t r2 = vmulq_f64 (r, r);
  float64x2_t r3 = vmulq_f64 (r2, r);

  float64x2_t sin = vfmaq_laneq_f64 (d->s2, r2, s3c2, 0);
  sin = vfmaq_f64 (d->s1, r2, sin);
  sin = vfmaq_f64 (r, r3, sin);

  float64x2_t cosm1 = vfmaq_laneq_f64 (d->c1, r2, s3c2, 1);
  cosm1 = vfmaq_f64 (d->c0, r2, cosm1);
  cosm1 = vmulq_f64 (cosm1, r2);

  float64x2x2_t result;
  result.val[0] = sin;
  result.val[1] = cosm1;
  return result;
}

static inline VPCS_ATTR float64x2x2_t
v_sincos_lookup (uint64x2_t k)
{
  unsigned idx0 = vgetq_lane_u64 (k, 0);
  unsigned idx1 = vgetq_lane_u64 (k, 1);

  float64x2_t sin_cos_k0 = vld1q_f64 (&SIN_COS_K_PI_OVER_128[idx0 & 255][0]);
  float64x2_t sin_cos_k1 = vld1q_f64 (&SIN_COS_K_PI_OVER_128[idx1 & 255][0]);

  float64x2x2_t result;
  result.val[0] = vzip1q_f64 (sin_cos_k0, sin_cos_k1);
  result.val[1] = vzip2q_f64 (sin_cos_k0, sin_cos_k1);

  return result;
}

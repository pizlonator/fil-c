/* Common data for asinhf/acoshf implementations.

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

#include "s_asincoshf_data.h"

const double __asincoshf_ix[] =
  {
    0x1p+0,	        0x1.fc07f01fcp-1, 0x1.f81f81f82p-1, 0x1.f44659e4ap-1,
    0x1.f07c1f07cp-1, 0x1.ecc07b302p-1, 0x1.e9131abfp-1,  0x1.e573ac902p-1,
    0x1.e1e1e1e1ep-1, 0x1.de5d6e3f8p-1, 0x1.dae6076bap-1, 0x1.d77b654b8p-1,
    0x1.d41d41d42p-1, 0x1.d0cb58f6ep-1, 0x1.cd8568904p-1, 0x1.ca4b3055ep-1,
    0x1.c71c71c72p-1, 0x1.c3f8f01c4p-1, 0x1.c0e070382p-1, 0x1.bdd2b8994p-1,
    0x1.bacf914c2p-1, 0x1.b7d6c3ddap-1, 0x1.b4e81b4e8p-1, 0x1.b2036406cp-1,
    0x1.af286bca2p-1, 0x1.ac5701ac6p-1, 0x1.a98ef606ap-1, 0x1.a6d01a6dp-1,
    0x1.a41a41a42p-1, 0x1.a16d3f97ap-1, 0x1.9ec8e951p-1,  0x1.9c2d14ee4p-1,
    0x1.99999999ap-1, 0x1.970e4f80cp-1, 0x1.948b0fcd6p-1, 0x1.920fb49dp-1,
    0x1.8f9c18f9cp-1, 0x1.8d3018d3p-1,  0x1.8acb90f6cp-1, 0x1.886e5f0acp-1,
    0x1.861861862p-1, 0x1.83c977ab2p-1, 0x1.818181818p-1, 0x1.7f405fd02p-1,
    0x1.7d05f417ep-1, 0x1.7ad2208ep-1,  0x1.78a4c8178p-1, 0x1.767dce434p-1,
    0x1.745d1745ep-1, 0x1.724287f46p-1, 0x1.702e05c0cp-1, 0x1.6e1f76b44p-1,
    0x1.6c16c16c2p-1, 0x1.6a13cd154p-1, 0x1.681681682p-1, 0x1.661ec6a52p-1,
    0x1.642c8590cp-1, 0x1.623fa7702p-1, 0x1.605816058p-1, 0x1.5e75bb8dp-1,
    0x1.5c9882b94p-1, 0x1.5ac056b02p-1, 0x1.58ed23082p-1, 0x1.571ed3c5p-1,
    0x1.555555556p-1, 0x1.5390948f4p-1, 0x1.51d07eae2p-1, 0x1.501501502p-1,
    0x1.4e5e0a73p-1,  0x1.4cab88726p-1, 0x1.4afd6a052p-1, 0x1.49539e3b2p-1,
    0x1.47ae147aep-1, 0x1.460cbc7f6p-1, 0x1.446f86562p-1, 0x1.42d6625d6p-1,
    0x1.414141414p-1, 0x1.3fb013fbp-1,  0x1.3e22cbce4p-1, 0x1.3c995a47cp-1,
    0x1.3b13b13b2p-1, 0x1.3991c2c18p-1, 0x1.381381382p-1, 0x1.3698df3dep-1,
    0x1.3521cfb2cp-1, 0x1.33ae45b58p-1, 0x1.323e34a2cp-1, 0x1.30d19013p-1,
    0x1.2f684bda2p-1, 0x1.2e025c04cp-1, 0x1.2c9fb4d82p-1, 0x1.2b404ad02p-1,
    0x1.29e4129e4p-1, 0x1.288b01288p-1, 0x1.27350b882p-1, 0x1.25e22708p-1,
    0x1.24924924ap-1, 0x1.23456789ap-1, 0x1.21fb78122p-1, 0x1.20b470c68p-1,
    0x1.1f7047dc2p-1, 0x1.1e2ef3b4p-1,  0x1.1cf06ada2p-1, 0x1.1bb4a4046p-1,
    0x1.1a7b9611ap-1, 0x1.19453808cp-1, 0x1.181181182p-1, 0x1.16e068942p-1,
    0x1.15b1e5f76p-1, 0x1.1485f0e0ap-1, 0x1.135c81136p-1, 0x1.12358e75ep-1,
    0x1.111111112p-1, 0x1.0fef010fep-1, 0x1.0ecf56be6p-1, 0x1.0db20a89p-1,
    0x1.0c9714fbcp-1, 0x1.0b7e6ec26p-1, 0x1.0a6810a68p-1, 0x1.0953f3902p-1,
    0x1.084210842p-1, 0x1.073260a48p-1, 0x1.0624dd2f2p-1, 0x1.05197f7d8p-1,
    0x1.041041042p-1, 0x1.03091b52p-1,  0x1.020408102p-1, 0x1.01010101p-1,
    0x1p-1
  };

const double __asincoshf_lix[] =
  {
    0x0p+0,               0x1.fe02a6b146789p-8, 0x1.fc0a8b0fa03e4p-7,
    0x1.7b91b07de311bp-6, 0x1.f829b0e7c33p-6,   0x1.39e87b9fd7d6p-5,
    0x1.77458f63edcfcp-5, 0x1.b42dd7117b1bfp-5, 0x1.f0a30c01362a6p-5,
    0x1.16536eea7fae1p-4, 0x1.341d7961791d1p-4, 0x1.51b073f07983fp-4,
    0x1.6f0d28ae3eb4cp-4, 0x1.8c345d6383b21p-4, 0x1.a926d3a475563p-4,
    0x1.c5e548f63a743p-4, 0x1.e27076e28f2e6p-4, 0x1.fec9131dbaabbp-4,
    0x1.0d77e7ccf6e59p-3, 0x1.1b72ad52f87ap-3,  0x1.29552f81eb523p-3,
    0x1.371fc201f7f74p-3, 0x1.44d2b6ccbfd1ep-3, 0x1.526e5e3a41438p-3,
    0x1.5ff3070a613d4p-3, 0x1.6d60fe717221dp-3, 0x1.7ab890212b909p-3,
    0x1.87fa065214911p-3, 0x1.9525a9cf296b4p-3, 0x1.a23bc1fe42563p-3,
    0x1.af3c94e81bff3p-3, 0x1.bc2867430acd6p-3, 0x1.c8ff7c7989a22p-3,
    0x1.d5c216b535b91p-3, 0x1.e27076e2f92e6p-3, 0x1.ef0adcbe0d936p-3,
    0x1.fb9186d5ebe2bp-3, 0x1.0402594b51041p-2, 0x1.0a324e27370e3p-2,
    0x1.1058bf9ad7ad5p-2, 0x1.1675cabaa660ep-2, 0x1.1c898c16b91fbp-2,
    0x1.22941fbcfb966p-2, 0x1.2895a13dd2ea3p-2, 0x1.2e8e2bade7d31p-2,
    0x1.347dd9a9afd55p-2, 0x1.3a64c556b05eap-2, 0x1.40430868877e4p-2,
    0x1.4618bc219dec2p-2, 0x1.4be5f9579e0a1p-2, 0x1.51aad872c982dp-2,
    0x1.5767717432a6cp-2, 0x1.5d1bdbf5669cap-2, 0x1.62c82f2b83795p-2,
    0x1.686c81e9964afp-2, 0x1.6e08eaa2929e4p-2, 0x1.739d7f6b95007p-2,
    0x1.792a55fdb7fa2p-2, 0x1.7eaf83b82efc3p-2, 0x1.842d1da1ecb17p-2,
    0x1.89a3386be825bp-2, 0x1.8f11e87347ac7p-2, 0x1.947941c1f26fbp-2,
    0x1.99d958119208bp-2, 0x1.9f323ecbd984cp-2, 0x1.a484090e5eb0ap-2,
    0x1.a9cec9a9cf84ap-2, 0x1.af1293245606bp-2, 0x1.b44f77bc98f63p-2,
    0x1.b9858969218fbp-2, 0x1.beb4d9da96b7cp-2, 0x1.c3dd7a7d0354dp-2,
    0x1.c8ff7c79ada22p-2, 0x1.ce1af0b855bebp-2, 0x1.d32fe7e039bd5p-2,
    0x1.d83e72587673ep-2, 0x1.dd46a04c204a1p-2, 0x1.e24881a7cac26p-2,
    0x1.e744261d8a788p-2, 0x1.ec399d2457ccp-2,  0x1.f128f5fac86edp-2,
    0x1.f6123fa71c8acp-2, 0x1.faf588f76631fp-2, 0x1.ffd2e08580c98p-2,
    0x1.02552a5a4f0ffp-1, 0x1.04bdf9da8b6d2p-1, 0x1.0723e5c1b4f4p-1,
    0x1.0986f4f589521p-1, 0x1.0be72e423ca83p-1, 0x1.0e44985d0f48cp-1,
    0x1.109f39e2be497p-1, 0x1.12f71959283bcp-1, 0x1.154c3d2f4f5eap-1,
    0x1.179eabbd9c9a1p-1, 0x1.19ee6b466516fp-1, 0x1.1c3b81f723c25p-1,
    0x1.1e85f5e6ec0dp-1,  0x1.20cdcd193f76ep-1, 0x1.23130d7beb743p-1,
    0x1.2555bce9887cbp-1, 0x1.2795e1288211bp-1, 0x1.29d37fec2308bp-1,
    0x1.2c0e9ed45768cp-1, 0x1.2e47436e5ae68p-1, 0x1.307d7334ff0bep-1,
    0x1.32b1339134571p-1, 0x1.34e289d9b39d3p-1, 0x1.37117b5481bb6p-1,
    0x1.393e0d3549a1ap-1, 0x1.3b6844a017823p-1, 0x1.3d9026a70eefbp-1,
    0x1.3fb5b84cfeb42p-1, 0x1.41d8fe844b2aep-1, 0x1.43f9fe2fb9267p-1,
    0x1.4618bc21d86c2p-1, 0x1.48353d1e928dfp-1, 0x1.4a4f85db1debbp-1,
    0x1.4c679afcc323ap-1, 0x1.4e7d811b77bb1p-1, 0x1.50913cbff8c6bp-1,
    0x1.52a2d265be5abp-1, 0x1.54b2467998498p-1, 0x1.56bf9d5b34b99p-1,
    0x1.58cadb5cbe989p-1, 0x1.5ad404c33af2dp-1, 0x1.5cdb1dc6ad765p-1,
    0x1.5ee02a9241e75p-1, 0x1.60e32f447a8d9p-1, 0x1.62e42fefa39efp-1
  };

const double __asincoshf_c[] =
  {
    0x1.0000000066947p+0, -0x1.00007f053d8cbp-1, 0x1.555280111d914p-2
  };


const double __asincoshf_cp[] =
  {
     0x1p+0,               -0x1p-1,
     0x1.55555555030bcp-2, -0x1.ffffffff2b4e5p-3,
     0x1.999b5076a42f2p-3, -0x1.55570c45a647dp-3
  };

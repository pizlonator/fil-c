/* Correctly-rounded error function for binary32 value.

Copyright (c) 2022-2024 Alexei Sibidanov.

This file is part of the CORE-MATH project
project (file src/binary32/erf/erff.c revision bc385c2).

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

#include <math.h>
#include <stdint.h>
#include <libm-alias-float.h>
#include "math_config.h"

float
__erff (float x)
{
  /* for 7 <= i < 63, C[i-7] is a degree-7 polynomial approximation of
     erf(i/16+1/32+x) for -1/32 <= x <= 1/32 */
  static const double C[56][8] = {
    {  0x1.f86faa9428f9cp-2,   0x1.cfc41e36c7dfap-1,  -0x1.b2c7dc53508b9p-2,
      -0x1.5a9de93fa556ep-3,   0x1.731793dbb01b5p-3,   0x1.133e06426cf18p-6,
      -0x1.a12a6289cafd8p-5,   0x1.717d6f1d6f557p-9 },
    {  0x1.1855a5fd3dd50p-1,   0x1.b3aafcc27502fp-1,  -0x1.cee5ac8e92bb2p-2,
      -0x1.fa02983ca2d79p-4,   0x1.77cd746cb1922p-3,  -0x1.fa6f277886487p-10,
      -0x1.8de75458db416p-5,   0x1.00899c98551c9p-7 },
    {  0x1.32a54cb8db67ap-1,   0x1.96164fafd8de5p-1,  -0x1.e23a7ea0c9ad3p-2,
      -0x1.3f5ee15671cf4p-4,   0x1.70e468a3d72d9p-3,  -0x1.3da68037cfc99p-6,
      -0x1.69ed9ba1f9839p-5,   0x1.8cab9244a4ff4p-7 },
    {  0x1.4b13713ad3513p-1,   0x1.7791b886e7405p-1,  -0x1.ecef423109bf5p-2,
      -0x1.15c3c5cec6847p-5,   0x1.5f688fc931ba6p-3,  -0x1.1da63ed190037p-5,
      -0x1.38427ca63cca4p-5,   0x1.fa00e52525e17p-7 },
    {  0x1.61955607dd15dp-1,   0x1.58a445da7c74ep-1,  -0x1.ef6c246a0f66cp-2,
       0x1.e83e0d9d61330p-8,   0x1.44cc65535bc9fp-3,  -0x1.87d3c4860435dp-5,
      -0x1.f90b10501169bp-6,   0x1.22295856d427ap-6 },
    {  0x1.762870f720c6fp-1,   0x1.39ccc1b136d5cp-1,  -0x1.ea4feea4e4744p-2,
       0x1.715e5952ebfbap-5,   0x1.22cdbd83c75c4p-3,  -0x1.da50aa1d925b6p-5,
      -0x1.754dc0a29b4ddp-6,   0x1.350b6bef9392cp-6 },
    {  0x1.88d1cd474a2e0p-1,   0x1.1b7e98fe26219p-1,  -0x1.de65a22ce1419p-2,
       0x1.40686a3f16400p-4,   0x1.f6b0cbb216b2bp-4,  -0x1.09c7c903edd57p-4,
      -0x1.da7529fde641p-7,    0x1.362a7a0588eabp-6 },
    {  0x1.999d4192a5717p-1,   0x1.fc3ee5d1524b3p-2,  -0x1.cc990045b55c8p-2,
       0x1.b37338e68b37dp-4,   0x1.a0d120c872ea7p-4,  -0x1.19bb2b07ecff6p-4,
      -0x1.a110f5f593aafp-8,   0x1.272c15a57720ep-6 },
    {  0x1.a89c850b7d54dp-1,   0x1.c40b0729ed54ap-2,  -0x1.b5eaaef0a2346p-2,
       0x1.0847c7dacbae1p-3,   0x1.47de0ba6d18fbp-4,  -0x1.1d9de77a4b648p-4,
       0x1.30ffbe56f0726p-10,  0x1.0a9cb99feea01p-6 },
    {  0x1.b5e62fce16096p-1,   0x1.8eed36b886d95p-2,  -0x1.9b64a06e50705p-2,
       0x1.2bb6e2c744df5p-3,   0x1.dee3261ca61bcp-5,  -0x1.16996004f7da5p-4,
       0x1.fdff37bae983ep-8,   0x1.c750083e65f9ap-7 },
    {  0x1.c194b1d49a184p-1,   0x1.5d4fd33729015p-2,  -0x1.7e0f4f045addbp-2,
       0x1.444bc66c31a1bp-3,   0x1.356dbf16ec8f1p-5,  -0x1.0643de0906cd8p-4,
       0x1.b281af7bd3a2cp-7,   0x1.6b97eaa2c6abdp-7 },
    {  0x1.cbc54b476248ep-1,   0x1.2f7cc3fe6f423p-2,  -0x1.5ee8429e36de8p-2,
       0x1.52a8395f96177p-3,   0x1.313761ba257dcp-6,  -0x1.dcf844d5fed8fp-5,
       0x1.1e1420f475fa9p-6,   0x1.091c7dc1e18b2p-7 },
    {  0x1.d4970f9ce00d9p-1,   0x1.059f59af7a905p-2,  -0x1.3eda354de36c3p-2,
       0x1.57b85ad439779p-3,   0x1.8e913b9778136p-10, -0x1.a2893bd3435f4p-5,
       0x1.4d3a90e37164ap-6,   0x1.4ce7f6e19a902p-8 },
    {  0x1.dc29fb60715b0p-1,   0x1.bf8e1b1ca2277p-3,  -0x1.1eb7095e5d6d2p-2,
       0x1.549ea6f7a64f4p-3,  -0x1.b10f12f3877a3p-7,  -0x1.61420c8f7156ap-5,
       0x1.674f1f92a8812p-6,   0x1.25543ffd74d52p-9 },
    {  0x1.e29e22a89d767p-1,   0x1.7bd5c7df3fe99p-3,  -0x1.fe674494077bfp-3,
       0x1.4a9feacf86578p-3,  -0x1.a008269076644p-6,  -0x1.1cf0e8fb4f1cbp-5,
       0x1.6e0d2ef105fb3p-6,  -0x1.367205fbd7876p-12 },
    {  0x1.e812fc64db36ap-1,   0x1.3fda6bc016991p-3,  -0x1.c1cb278627920p-3,
       0x1.3b10512314f1ep-3,  -0x1.1e6457bb1b9a9p-5,  -0x1.b1f6474e2388cp-6,
       0x1.640a5345f7ec7p-6,  -0x1.3dae5a997fdbp-9 },
    {  0x1.eca6ccd709544p-1,   0x1.0b3f52ce8c380p-3,  -0x1.8885019f63c6dp-3,
       0x1.274275fc91a05p-3,  -0x1.57f73699a8372p-5,  -0x1.3076a305fc7cep-6,
       0x1.4c6ae04843a41p-6,  -0x1.0be5fcf5ecc91p-8 },
    {  0x1.f0762fde45ee7p-1,   0x1.bb1c972f23e4ap-4,  -0x1.5341e3c01b58dp-3,
       0x1.107929f6f0b60p-3,  -0x1.7e1b34f976c02p-5,  -0x1.73b62589c234ap-7,
       0x1.2a97ee1876486p-6,  -0x1.595f40a3150fep-8 },
    {  0x1.f39bc242e43e6p-1,   0x1.6c7e64e7281c5p-4,  -0x1.2274b86835fd3p-3,
       0x1.efb890e5c770dp-4,  -0x1.92c7db16847e0p-5,  -0x1.45477db5e2dd4p-8,
       0x1.01fc6165fc866p-6,  -0x1.8845509030c2cp-8 },
    {  0x1.f62fe80272419p-1,   0x1.297db960e4f5dp-4,  -0x1.ecb83b087c04fp-4,
       0x1.bce18363ca3d1p-4,  -0x1.985aaf776482cp-5,   0x1.cd953efdae886p-12,
       0x1.ab9a0b89b54ffp-7,  -0x1.9b5e576ccc31cp-8 },
    {  0x1.f848acb544e95p-1,   0x1.e1d4cf1e24501p-5,  -0x1.9e12e1fde5552p-4,
       0x1.8a27806df3d1bp-4,  -0x1.91674e5eb3319p-5,   0x1.3bc75595b2db8p-8,
       0x1.51bc537ac61afp-7,  -0x1.96b23b19ea04dp-8 },
    {  0x1.f9f9ba8d3c733p-1,   0x1.83298d7172108p-5,  -0x1.58d101f905a75p-4,
       0x1.58f1456f8639bp-4,  -0x1.808d1850b8231p-5,   0x1.0c1bd99c348a7p-7,
       0x1.f61e9d7bc48cap-8,  -0x1.7f07c13441774p-8 },
    {  0x1.fb54641aebbc9p-1,   0x1.34ac36ad8dafap-5,  -0x1.1c8ec267f9405p-4,
       0x1.2a52c5d841848p-4,  -0x1.68541c02b3b6bp-5,   0x1.5afe400196379p-7,
       0x1.565b2d6eda3d6p-8,  -0x1.596aaff29e739p-8 },
    {  0x1.fc67bcf2d7b8fp-1,   0x1.e85c449e377efp-6,  -0x1.d177f166c07c6p-5,
       0x1.fe23b7584b504p-5,  -0x1.4b12109613313p-5,   0x1.8d9905c0acf7dp-7,
       0x1.9265032a669dap-9,  -0x1.2ac4a6dbcbf3ep-8 },
    {  0x1.fd40bd6d7a785p-1,   0x1.7f5188610ddc7p-6,  -0x1.7954423f7c998p-5,
       0x1.af5baae33887fp-5,  -0x1.2ad77c7cbc474p-5,   0x1.a7b8c47ec2a51p-7,
       0x1.46646ee094bccp-10, -0x1.ef19d8db8673p-9 },
    {  0x1.fdea6e062d0c9p-1,   0x1.2a875b5ffab58p-6,  -0x1.2f3178cd6dcd5p-5,
       0x1.68d1c45b94182p-5,  -0x1.09648ed3aeaefp-5,   0x1.ad8b150d38164p-7,
      -0x1.e9a6023d9429fp-13, -0x1.8722d19ee2e8ep-9 },
    {  0x1.fe6e1742f7cf5p-1,   0x1.cd5ec93c1243ap-7,  -0x1.e2ff3aaacb386p-6,
       0x1.2aa4e5823cc89p-5,  -0x1.d049842dbe399p-6,   0x1.a34edb21ab302p-7,
      -0x1.676e5996c7f9bp-10, -0x1.23b01a35140bfp-9 },
    {  0x1.fed37386190fbp-1,   0x1.61beae53b72c2p-7,  -0x1.7d6193f22c3c1p-6,
       0x1.e947279e3bb7dp-6,  -0x1.906031b97ca97p-6,   0x1.8d14d62561755p-7,
      -0x1.1f245e7178882p-9,  -0x1.9257d4eb47685p-10 },
    {  0x1.ff20e0a7ba8c2p-1,   0x1.0d1d69569b839p-7,  -0x1.2a8ca0dc02752p-6,
       0x1.8cc071b709751p-6,  -0x1.54a149f1b070cp-6,   0x1.6e9137b13412cp-7,
      -0x1.6577ed3d8e83bp-9,  -0x1.e9c1a5178a289p-11 },
    {  0x1.ff5b8fb26f5f6p-1,   0x1.9646f35a7663cp-8,  -0x1.cf68ed9311b0bp-7,
       0x1.3e8735b5a694fp-6,  -0x1.1e1612d026fdfp-6,   0x1.4afd8e6ca636dp-7,
      -0x1.8c375170ccb22p-9,  -0x1.c799443c4fd3bp-12 },
    {  0x1.ff87b1913e853p-1,   0x1.30499b5039596p-8,  -0x1.64964201ec8bap-7,
       0x1.fa73d7eafba98p-7,  -0x1.daa3022141fbbp-7,   0x1.2509444c063b7p-7,
      -0x1.99482a2f8a0a1p-9,  -0x1.403d1f76c9454p-15 },
    {  0x1.ffa89fe5b3625p-1,   0x1.c4412bf4b8f35p-9,  -0x1.100f347126cf0p-7,
       0x1.8ebda07671d40p-7,  -0x1.850c6a31c98c1p-7,   0x1.fdac860c67d21p-8,
      -0x1.927d03d2ba12cp-9,   0x1.0ff620b4190fep-12 },
    {  0x1.ffc10194fcb64p-1,   0x1.4d78bba8ca621p-9,  -0x1.9ba107a443e02p-8,
       0x1.36f273fbc04ccp-7,  -0x1.3b38716ac7e6fp-7,   0x1.b3fe0181914acp-8,
      -0x1.7d3fe7de98c5cp-9,   0x1.ea31f8e5317f7p-12 },
    {  0x1.ffd2eae369a07p-1,   0x1.e7f232d9e266cp-10, -0x1.34c7442dd48d9p-8,
       0x1.e066bed070a0bp-8,  -0x1.f914f3c42fc0dp-8,   0x1.6f4664ed2260fp-8,
      -0x1.5e59910761d24p-9,   0x1.39cbb6e84c126p-11 },
    {  0x1.ffdff92db56e5p-1,   0x1.6235fbd7a4373p-10, -0x1.cb5e029b9e56ap-9,
       0x1.6fa4c7ef274dap-8,  -0x1.903a089a835f3p-8,   0x1.30f12e0ca1901p-8,
      -0x1.39d21b6957f99p-9,   0x1.5d3f8495a703cp-11 },
    {  0x1.ffe96a78a04a9p-1,   0x1.fe41cd9bb4f2cp-11, -0x1.52d7b28966c0cp-9,
       0x1.16c192d86a1a7p-8,  -0x1.39bfce951100cp-8,   0x1.f376a7869f9e3p-9,
      -0x1.12e6cef999c4fp-9,   0x1.66acd4d667b5p-11 },
    {  0x1.fff0312b010b5p-1,   0x1.6caa0d3583018p-11, -0x1.efb729f4cf75bp-10,
       0x1.a2da7cebe12acp-9,  -0x1.e6c27a24bc759p-9,   0x1.93b1f4d8ea65p-9,
      -0x1.d82050aa94a08p-10,  0x1.5cd7dc75d6cbap-11 },
    {  0x1.fff50456dab8cp-1,   0x1.0295ef6591865p-11, -0x1.679880e95a4dap-10,
       0x1.37d38e3a5c8ebp-9,  -0x1.75b3708aebb8fp-9,   0x1.4231c4b4b0296p-9,
      -0x1.8e26476489318p-10,  0x1.45c3b570dd924p-11 },
    {  0x1.fff86cfd3e657p-1,   0x1.6be02102b353dp-12, -0x1.02b157780d6aep-10,
       0x1.cc1d886861133p-10, -0x1.1bff6f12ec9abp-9,   0x1.fc0f77bd9c736p-10,
      -0x1.4a3320bd0959dp-10,  0x1.267f8b4f95d2p-11 },
    {  0x1.fffad0b901755p-1,   0x1.fc0d55470cf5ep-13, -0x1.7121aff5e820ep-11,
       0x1.506d6992f7de5p-10, -0x1.ab595d3ecd0d6p-10,  0x1.8bdd79daaf754p-10,
      -0x1.0d9b090f997c1p-10,  0x1.031ab9fd1c7dap-11 },
    {  0x1.fffc7a37857d2p-1,   0x1.5feada379d8a5p-13, -0x1.05304df58f3aap-11,
       0x1.e79c081b8600fp-11, -0x1.3e5dbe33232e0p-10,  0x1.30eb208200729p-10,
      -0x1.b1d493b147945p-11,  0x1.bd587bbc071bep-12 },
    {  0x1.fffd9fdeabccep-1,   0x1.e3bcf436a1a49p-14, -0x1.6e953111ef0a1p-12,
       0x1.5e3edf6768654p-11, -0x1.d5be67c0547a4p-11,  0x1.d07d9ffa1d435p-11,
      -0x1.58328f5f358cap-11,  0x1.76d42d95c42c4p-12 },
    {  0x1.fffe68f4fa777p-1,   0x1.49e17724f4cddp-14, -0x1.fe48c44e229c1p-13,
       0x1.f2bd95d76f188p-12, -0x1.57388cb12d011p-11,  0x1.5decc25c5c079p-11,
      -0x1.0d7499d1b0d2dp-11,  0x1.359332c94ecdcp-12 },
    {  0x1.fffef1960d85dp-1,   0x1.be6abbb10a4cdp-15, -0x1.6040381a8c313p-13,
       0x1.5fff1dde9ee9dp-12, -0x1.f0c933efa9971p-12,  0x1.04cbf4a5cd760p-11,
      -0x1.a07f150af6dadp-12,  0x1.f68dd183426bap-13 },
    {  0x1.ffff4db27f146p-1,   0x1.2bb5cc22e5cd8p-15, -0x1.e25894899f526p-14,
       0x1.ec8a8e5a72757p-13, -0x1.64256ae0a3cf9p-12,  0x1.80a836c18c46cp-12,
      -0x1.3dea401af6775p-12,  0x1.915ddff3fe0d1p-13 },
    {  0x1.ffff8b500e77cp-1,   0x1.8f4ccca7fc769p-16, -0x1.478cffe305946p-14,
       0x1.559f04adde504p-13, -0x1.f9e1577d6961dp-13,  0x1.18bda53c14716p-12,
      -0x1.df8634c35541cp-13,  0x1.3bb5c6b616337p-13 },
    {  0x1.ffffb43555b5fp-1,   0x1.07ebd2a2d26c8p-16, -0x1.b93e442a37f2bp-15,
       0x1.d5cf15159ce28p-14, -0x1.63f5e1469c006p-13,  0x1.95a03acebac18p-13,
      -0x1.656e5e2a1f8e2p-13,  0x1.e98c437189bdep-14 },
    {  0x1.ffffcf23ff5fcp-1,   0x1.5a2adfa0b492cp-17, -0x1.26c88270759f0p-15,
       0x1.40473572b99a8p-14, -0x1.f057cbde578a5p-14,  0x1.22178d1c3c948p-13,
      -0x1.0765b61a0d859p-13,  0x1.765b3ea03ddbep-14 },
    {  0x1.ffffe0bd3e852p-1,   0x1.c282cd3957a72p-18, -0x1.86ad6dfa44faap-16,
       0x1.b0f313f03a029p-15, -0x1.56e44abecd255p-14,  0x1.9ad1ecfe34a89p-14,
      -0x1.7fe4033478618p-14,  0x1.1a8184e049fbfp-14 },
    {  0x1.ffffec2641a9ep-1,   0x1.22df29821407ep-18, -0x1.00c902a6cfd98p-16,
       0x1.22234eb88671fp-15, -0x1.d57a181c9e6e1p-15,  0x1.200c283b54a90p-14,
      -0x1.14b4c3295a7d0p-14,  0x1.a4f966f713bdep-15 },
    {  0x1.fffff37d63a36p-1,   0x1.74adc8f405eecp-19, -0x1.4ed4228e44858p-17,
       0x1.81918baea92bap-16, -0x1.3e81b17a0009cp-15,  0x1.9004a36116436p-15,
      -0x1.8aa1ba400e076p-15,  0x1.35cd4e2340a9ep-15 },
    {  0x1.fffff82cdcf1bp-1,   0x1.d9c73698fa87dp-20, -0x1.b11017ec67115p-18,
       0x1.fc0dfadf653f8p-17, -0x1.ac4e03cd2dfc2p-16,  0x1.131806b5abbc5p-15,
      -0x1.1672ef66fcaafp-15,  0x1.c2882c7debed7p-16 },
    {  0x1.fffffb248c39dp-1,   0x1.2acee2f5ec66ap-20, -0x1.15cc570408a36p-18,
       0x1.4be757bbb75a3p-17, -0x1.1d6aa5f8d2940p-16,  0x1.76c5937d5105ep-16,
      -0x1.84dffc3ca9302p-16,  0x1.43c8315f2c30ap-16 },
    {  0x1.fffffd01f36afp-1,   0x1.75fa8dbc840bap-21, -0x1.6186da0133f5ap-19,
       0x1.ae023231e1af5p-18, -0x1.790812f7ca394p-17,  0x1.f9c25656d0ef2p-17,
      -0x1.0cc66682e304cp-16,  0x1.cc170a75d6f9cp-17 },
    {  0x1.fffffe2ba0ea5p-1,   0x1.d06ad6ecde88ep-22, -0x1.be46aa8edc9a1p-20,
       0x1.143860c7840b8p-18, -0x1.edaba78fb1260p-18,  0x1.52138a96ecee2p-17,
      -0x1.6fca538c4e2eep-17,  0x1.434040640bcefp-17 },
    {  0x1.fffffee3cc32cp-1,   0x1.1e1e857adb8ddp-22, -0x1.1769ce5f2a6e8p-20,
       0x1.5fe5d479b0543p-19, -0x1.405d865c94c2ap-18,  0x1.bfc94feb96afcp-18,
      -0x1.f245d5f3e8358p-18,  0x1.c142456acf443p-18 },
  };
  float ax = fabsf (x);
  uint32_t ux = asuint (ax);
  double s = x;
  double z = ax;
  /* 0x407ad444 corresponds to x = 0x1.f5a888p+1 = 3.91921..., which is the
     largest float such that erf(x) does not round to 1 (to nearest).  */
  if (__glibc_unlikely (ux > 0x407ad444u))
    {
      float os = copysignf (1.0f, x);
      if (ux > (0xffu << 23))
	return x + x; /* nan */
      if (ux == (0xffu << 23))
	return os; /* +-inf */
      return os - 0x1p-25f * os;
    }
  double v = floor (16.0 * z);
  uint32_t i = 16.0f * ax;
  /* 0x3ee00000 corresponds to x = 0.4375, for smaller x we have i < 7.  */
  if (__glibc_unlikely (ux < 0x3ee00000u))
    {
      static const double c[] =
	{
	  0x1.20dd750429b6dp+0,  -0x1.812746b0375fbp-2,
	  0x1.ce2f219fd6f45p-4,  -0x1.b82ce2cbf0838p-6,
	  0x1.565bb655adb85p-8,  -0x1.c025bfc879c94p-11,
	  0x1.f81718f61309cp-14, -0x1.cc67bd88f5867p-17
	};
      double z2 = s * s, z4 = z2 * z2, z8 = z4 * z4;
      double c0 = c[0] + z2 * c[1];
      double c2 = c[2] + z2 * c[3];
      double c4 = c[4] + z2 * c[5];
      double c6 = c[6] + z2 * c[7];
      c0 += z4 * c2;
      c4 += z4 * c6;
      c0 += z8 * c4;
      return s * c0;
    }
  z = (z - 0.03125) - 0.0625 * v;
  const double *c = C[i - 7];
  double z2 = z * z, z4 = z2 * z2;
  double c0 = c[0] + z * c[1];
  double c2 = c[2] + z * c[3];
  double c4 = c[4] + z * c[5];
  double c6 = c[6] + z * c[7];
  c0 += z2 * c2;
  c4 += z2 * c6;
  c0 += z4 * c4;
  return copysign (c0, s);
}
libm_alias_float (__erf, erf)

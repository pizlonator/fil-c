#include <stdlib.h>
#include <stdio.h>
#include "gmp-impl.h"
#include "longlong.h"
static const int ops[64] = {
1,-1,0,-2,
2,-2,1,-3,
4,-4,3,-5,
8,-8,7,-9,
16,-16,15,-17,
32,-32,31,-33,
64,-64,63,-65,
128,-128,127,-129,
256,-256,255,-257,
512,-512,511,-513,
1024,-1024,1023,-1025,
2048,-2048,2047,-2049,
4096,-4096,4095,-4097,
8192,-8192,8191,-8193,
16384,-16384,16383,-16385,
32768,-32768,32767,-32769,
};
static void f0(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,1);*r1p=r1;*r0p=r0;}
static void f1(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-1);*r1p=r1;*r0p=r0;}
static void f2(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,0);*r1p=r1;*r0p=r0;}
static void f3(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-2);*r1p=r1;*r0p=r0;}
static void f4(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,2);*r1p=r1;*r0p=r0;}
static void f5(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-2);*r1p=r1;*r0p=r0;}
static void f6(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,1);*r1p=r1;*r0p=r0;}
static void f7(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-3);*r1p=r1;*r0p=r0;}
static void f8(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,4);*r1p=r1;*r0p=r0;}
static void f9(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-4);*r1p=r1;*r0p=r0;}
static void f10(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,3);*r1p=r1;*r0p=r0;}
static void f11(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-5);*r1p=r1;*r0p=r0;}
static void f12(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,8);*r1p=r1;*r0p=r0;}
static void f13(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-8);*r1p=r1;*r0p=r0;}
static void f14(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,7);*r1p=r1;*r0p=r0;}
static void f15(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-9);*r1p=r1;*r0p=r0;}
static void f16(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,16);*r1p=r1;*r0p=r0;}
static void f17(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-16);*r1p=r1;*r0p=r0;}
static void f18(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,15);*r1p=r1;*r0p=r0;}
static void f19(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-17);*r1p=r1;*r0p=r0;}
static void f20(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,32);*r1p=r1;*r0p=r0;}
static void f21(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-32);*r1p=r1;*r0p=r0;}
static void f22(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,31);*r1p=r1;*r0p=r0;}
static void f23(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-33);*r1p=r1;*r0p=r0;}
static void f24(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,64);*r1p=r1;*r0p=r0;}
static void f25(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-64);*r1p=r1;*r0p=r0;}
static void f26(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,63);*r1p=r1;*r0p=r0;}
static void f27(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-65);*r1p=r1;*r0p=r0;}
static void f28(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,128);*r1p=r1;*r0p=r0;}
static void f29(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-128);*r1p=r1;*r0p=r0;}
static void f30(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,127);*r1p=r1;*r0p=r0;}
static void f31(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-129);*r1p=r1;*r0p=r0;}
static void f32(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,256);*r1p=r1;*r0p=r0;}
static void f33(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-256);*r1p=r1;*r0p=r0;}
static void f34(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,255);*r1p=r1;*r0p=r0;}
static void f35(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-257);*r1p=r1;*r0p=r0;}
static void f36(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,512);*r1p=r1;*r0p=r0;}
static void f37(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-512);*r1p=r1;*r0p=r0;}
static void f38(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,511);*r1p=r1;*r0p=r0;}
static void f39(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-513);*r1p=r1;*r0p=r0;}
static void f40(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,1024);*r1p=r1;*r0p=r0;}
static void f41(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-1024);*r1p=r1;*r0p=r0;}
static void f42(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,1023);*r1p=r1;*r0p=r0;}
static void f43(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-1025);*r1p=r1;*r0p=r0;}
static void f44(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,2048);*r1p=r1;*r0p=r0;}
static void f45(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-2048);*r1p=r1;*r0p=r0;}
static void f46(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,2047);*r1p=r1;*r0p=r0;}
static void f47(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-2049);*r1p=r1;*r0p=r0;}
static void f48(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,4096);*r1p=r1;*r0p=r0;}
static void f49(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-4096);*r1p=r1;*r0p=r0;}
static void f50(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,4095);*r1p=r1;*r0p=r0;}
static void f51(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-4097);*r1p=r1;*r0p=r0;}
static void f52(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,8192);*r1p=r1;*r0p=r0;}
static void f53(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-8192);*r1p=r1;*r0p=r0;}
static void f54(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,8191);*r1p=r1;*r0p=r0;}
static void f55(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-8193);*r1p=r1;*r0p=r0;}
static void f56(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,16384);*r1p=r1;*r0p=r0;}
static void f57(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-16384);*r1p=r1;*r0p=r0;}
static void f58(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,16383);*r1p=r1;*r0p=r0;}
static void f59(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-16385);*r1p=r1;*r0p=r0;}
static void f60(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,32768);*r1p=r1;*r0p=r0;}
static void f61(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-32768);*r1p=r1;*r0p=r0;}
static void f62(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,32767);*r1p=r1;*r0p=r0;}
static void f63(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-32769);*r1p=r1;*r0p=r0;}
static void f64(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,1);*r1p=r1;*r0p=r0;}
static void f65(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,-1);*r1p=r1;*r0p=r0;}
static void f66(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,0);*r1p=r1;*r0p=r0;}
static void f67(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,-2);*r1p=r1;*r0p=r0;}
static void f68(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,2);*r1p=r1;*r0p=r0;}
static void f69(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,-2);*r1p=r1;*r0p=r0;}
static void f70(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,1);*r1p=r1;*r0p=r0;}
static void f71(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,-3);*r1p=r1;*r0p=r0;}
static void f72(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,4);*r1p=r1;*r0p=r0;}
static void f73(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,-4);*r1p=r1;*r0p=r0;}
static void f74(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,3);*r1p=r1;*r0p=r0;}
static void f75(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,-5);*r1p=r1;*r0p=r0;}
static void f76(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,8);*r1p=r1;*r0p=r0;}
static void f77(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,-8);*r1p=r1;*r0p=r0;}
static void f78(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,7);*r1p=r1;*r0p=r0;}
static void f79(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,-9);*r1p=r1;*r0p=r0;}
static void f80(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,16);*r1p=r1;*r0p=r0;}
static void f81(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,-16);*r1p=r1;*r0p=r0;}
static void f82(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,15);*r1p=r1;*r0p=r0;}
static void f83(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,-17);*r1p=r1;*r0p=r0;}
static void f84(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,32);*r1p=r1;*r0p=r0;}
static void f85(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,-32);*r1p=r1;*r0p=r0;}
static void f86(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,31);*r1p=r1;*r0p=r0;}
static void f87(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,-33);*r1p=r1;*r0p=r0;}
static void f88(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,64);*r1p=r1;*r0p=r0;}
static void f89(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,-64);*r1p=r1;*r0p=r0;}
static void f90(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,63);*r1p=r1;*r0p=r0;}
static void f91(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,-65);*r1p=r1;*r0p=r0;}
static void f92(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,128);*r1p=r1;*r0p=r0;}
static void f93(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,-128);*r1p=r1;*r0p=r0;}
static void f94(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,127);*r1p=r1;*r0p=r0;}
static void f95(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,-129);*r1p=r1;*r0p=r0;}
static void f96(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,256);*r1p=r1;*r0p=r0;}
static void f97(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,-256);*r1p=r1;*r0p=r0;}
static void f98(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,255);*r1p=r1;*r0p=r0;}
static void f99(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,-257);*r1p=r1;*r0p=r0;}
static void f100(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,512);*r1p=r1;*r0p=r0;}
static void f101(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,-512);*r1p=r1;*r0p=r0;}
static void f102(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,511);*r1p=r1;*r0p=r0;}
static void f103(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,-513);*r1p=r1;*r0p=r0;}
static void f104(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,1024);*r1p=r1;*r0p=r0;}
static void f105(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,-1024);*r1p=r1;*r0p=r0;}
static void f106(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,1023);*r1p=r1;*r0p=r0;}
static void f107(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,-1025);*r1p=r1;*r0p=r0;}
static void f108(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,2048);*r1p=r1;*r0p=r0;}
static void f109(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,-2048);*r1p=r1;*r0p=r0;}
static void f110(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,2047);*r1p=r1;*r0p=r0;}
static void f111(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,-2049);*r1p=r1;*r0p=r0;}
static void f112(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,4096);*r1p=r1;*r0p=r0;}
static void f113(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,-4096);*r1p=r1;*r0p=r0;}
static void f114(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,4095);*r1p=r1;*r0p=r0;}
static void f115(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,-4097);*r1p=r1;*r0p=r0;}
static void f116(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,8192);*r1p=r1;*r0p=r0;}
static void f117(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,-8192);*r1p=r1;*r0p=r0;}
static void f118(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,8191);*r1p=r1;*r0p=r0;}
static void f119(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,-8193);*r1p=r1;*r0p=r0;}
static void f120(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,16384);*r1p=r1;*r0p=r0;}
static void f121(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,-16384);*r1p=r1;*r0p=r0;}
static void f122(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,16383);*r1p=r1;*r0p=r0;}
static void f123(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,-16385);*r1p=r1;*r0p=r0;}
static void f124(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,32768);*r1p=r1;*r0p=r0;}
static void f125(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,-32768);*r1p=r1;*r0p=r0;}
static void f126(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,32767);*r1p=r1;*r0p=r0;}
static void f127(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1,0,-32769);*r1p=r1;*r0p=r0;}
static void f128(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,1);*r1p=r1;*r0p=r0;}
static void f129(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,-1);*r1p=r1;*r0p=r0;}
static void f130(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,0);*r1p=r1;*r0p=r0;}
static void f131(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,-2);*r1p=r1;*r0p=r0;}
static void f132(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,2);*r1p=r1;*r0p=r0;}
static void f133(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,-2);*r1p=r1;*r0p=r0;}
static void f134(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,1);*r1p=r1;*r0p=r0;}
static void f135(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,-3);*r1p=r1;*r0p=r0;}
static void f136(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,4);*r1p=r1;*r0p=r0;}
static void f137(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,-4);*r1p=r1;*r0p=r0;}
static void f138(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,3);*r1p=r1;*r0p=r0;}
static void f139(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,-5);*r1p=r1;*r0p=r0;}
static void f140(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,8);*r1p=r1;*r0p=r0;}
static void f141(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,-8);*r1p=r1;*r0p=r0;}
static void f142(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,7);*r1p=r1;*r0p=r0;}
static void f143(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,-9);*r1p=r1;*r0p=r0;}
static void f144(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,16);*r1p=r1;*r0p=r0;}
static void f145(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,-16);*r1p=r1;*r0p=r0;}
static void f146(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,15);*r1p=r1;*r0p=r0;}
static void f147(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,-17);*r1p=r1;*r0p=r0;}
static void f148(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,32);*r1p=r1;*r0p=r0;}
static void f149(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,-32);*r1p=r1;*r0p=r0;}
static void f150(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,31);*r1p=r1;*r0p=r0;}
static void f151(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,-33);*r1p=r1;*r0p=r0;}
static void f152(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,64);*r1p=r1;*r0p=r0;}
static void f153(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,-64);*r1p=r1;*r0p=r0;}
static void f154(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,63);*r1p=r1;*r0p=r0;}
static void f155(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,-65);*r1p=r1;*r0p=r0;}
static void f156(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,128);*r1p=r1;*r0p=r0;}
static void f157(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,-128);*r1p=r1;*r0p=r0;}
static void f158(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,127);*r1p=r1;*r0p=r0;}
static void f159(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,-129);*r1p=r1;*r0p=r0;}
static void f160(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,256);*r1p=r1;*r0p=r0;}
static void f161(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,-256);*r1p=r1;*r0p=r0;}
static void f162(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,255);*r1p=r1;*r0p=r0;}
static void f163(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,-257);*r1p=r1;*r0p=r0;}
static void f164(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,512);*r1p=r1;*r0p=r0;}
static void f165(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,-512);*r1p=r1;*r0p=r0;}
static void f166(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,511);*r1p=r1;*r0p=r0;}
static void f167(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,-513);*r1p=r1;*r0p=r0;}
static void f168(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,1024);*r1p=r1;*r0p=r0;}
static void f169(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,-1024);*r1p=r1;*r0p=r0;}
static void f170(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,1023);*r1p=r1;*r0p=r0;}
static void f171(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,-1025);*r1p=r1;*r0p=r0;}
static void f172(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,2048);*r1p=r1;*r0p=r0;}
static void f173(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,-2048);*r1p=r1;*r0p=r0;}
static void f174(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,2047);*r1p=r1;*r0p=r0;}
static void f175(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,-2049);*r1p=r1;*r0p=r0;}
static void f176(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,4096);*r1p=r1;*r0p=r0;}
static void f177(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,-4096);*r1p=r1;*r0p=r0;}
static void f178(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,4095);*r1p=r1;*r0p=r0;}
static void f179(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,-4097);*r1p=r1;*r0p=r0;}
static void f180(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,8192);*r1p=r1;*r0p=r0;}
static void f181(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,-8192);*r1p=r1;*r0p=r0;}
static void f182(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,8191);*r1p=r1;*r0p=r0;}
static void f183(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,-8193);*r1p=r1;*r0p=r0;}
static void f184(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,16384);*r1p=r1;*r0p=r0;}
static void f185(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,-16384);*r1p=r1;*r0p=r0;}
static void f186(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,16383);*r1p=r1;*r0p=r0;}
static void f187(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,-16385);*r1p=r1;*r0p=r0;}
static void f188(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,32768);*r1p=r1;*r0p=r0;}
static void f189(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,-32768);*r1p=r1;*r0p=r0;}
static void f190(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,32767);*r1p=r1;*r0p=r0;}
static void f191(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,0,0,-32769);*r1p=r1;*r0p=r0;}
static void f192(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,1);*r1p=r1;*r0p=r0;}
static void f193(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-1);*r1p=r1;*r0p=r0;}
static void f194(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,0);*r1p=r1;*r0p=r0;}
static void f195(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-2);*r1p=r1;*r0p=r0;}
static void f196(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,2);*r1p=r1;*r0p=r0;}
static void f197(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-2);*r1p=r1;*r0p=r0;}
static void f198(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,1);*r1p=r1;*r0p=r0;}
static void f199(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-3);*r1p=r1;*r0p=r0;}
static void f200(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,4);*r1p=r1;*r0p=r0;}
static void f201(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-4);*r1p=r1;*r0p=r0;}
static void f202(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,3);*r1p=r1;*r0p=r0;}
static void f203(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-5);*r1p=r1;*r0p=r0;}
static void f204(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,8);*r1p=r1;*r0p=r0;}
static void f205(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-8);*r1p=r1;*r0p=r0;}
static void f206(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,7);*r1p=r1;*r0p=r0;}
static void f207(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-9);*r1p=r1;*r0p=r0;}
static void f208(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,16);*r1p=r1;*r0p=r0;}
static void f209(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-16);*r1p=r1;*r0p=r0;}
static void f210(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,15);*r1p=r1;*r0p=r0;}
static void f211(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-17);*r1p=r1;*r0p=r0;}
static void f212(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,32);*r1p=r1;*r0p=r0;}
static void f213(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-32);*r1p=r1;*r0p=r0;}
static void f214(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,31);*r1p=r1;*r0p=r0;}
static void f215(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-33);*r1p=r1;*r0p=r0;}
static void f216(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,64);*r1p=r1;*r0p=r0;}
static void f217(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-64);*r1p=r1;*r0p=r0;}
static void f218(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,63);*r1p=r1;*r0p=r0;}
static void f219(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-65);*r1p=r1;*r0p=r0;}
static void f220(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,128);*r1p=r1;*r0p=r0;}
static void f221(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-128);*r1p=r1;*r0p=r0;}
static void f222(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,127);*r1p=r1;*r0p=r0;}
static void f223(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-129);*r1p=r1;*r0p=r0;}
static void f224(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,256);*r1p=r1;*r0p=r0;}
static void f225(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-256);*r1p=r1;*r0p=r0;}
static void f226(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,255);*r1p=r1;*r0p=r0;}
static void f227(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-257);*r1p=r1;*r0p=r0;}
static void f228(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,512);*r1p=r1;*r0p=r0;}
static void f229(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-512);*r1p=r1;*r0p=r0;}
static void f230(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,511);*r1p=r1;*r0p=r0;}
static void f231(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-513);*r1p=r1;*r0p=r0;}
static void f232(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,1024);*r1p=r1;*r0p=r0;}
static void f233(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-1024);*r1p=r1;*r0p=r0;}
static void f234(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,1023);*r1p=r1;*r0p=r0;}
static void f235(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-1025);*r1p=r1;*r0p=r0;}
static void f236(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,2048);*r1p=r1;*r0p=r0;}
static void f237(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-2048);*r1p=r1;*r0p=r0;}
static void f238(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,2047);*r1p=r1;*r0p=r0;}
static void f239(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-2049);*r1p=r1;*r0p=r0;}
static void f240(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,4096);*r1p=r1;*r0p=r0;}
static void f241(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-4096);*r1p=r1;*r0p=r0;}
static void f242(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,4095);*r1p=r1;*r0p=r0;}
static void f243(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-4097);*r1p=r1;*r0p=r0;}
static void f244(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,8192);*r1p=r1;*r0p=r0;}
static void f245(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-8192);*r1p=r1;*r0p=r0;}
static void f246(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,8191);*r1p=r1;*r0p=r0;}
static void f247(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-8193);*r1p=r1;*r0p=r0;}
static void f248(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,16384);*r1p=r1;*r0p=r0;}
static void f249(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-16384);*r1p=r1;*r0p=r0;}
static void f250(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,16383);*r1p=r1;*r0p=r0;}
static void f251(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-16385);*r1p=r1;*r0p=r0;}
static void f252(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,32768);*r1p=r1;*r0p=r0;}
static void f253(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-32768);*r1p=r1;*r0p=r0;}
static void f254(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,32767);*r1p=r1;*r0p=r0;}
static void f255(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-32769);*r1p=r1;*r0p=r0;}
static void f256(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,1);*r1p=r1;*r0p=r0;}
static void f257(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,-1);*r1p=r1;*r0p=r0;}
static void f258(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,0);*r1p=r1;*r0p=r0;}
static void f259(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,-2);*r1p=r1;*r0p=r0;}
static void f260(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,2);*r1p=r1;*r0p=r0;}
static void f261(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,-2);*r1p=r1;*r0p=r0;}
static void f262(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,1);*r1p=r1;*r0p=r0;}
static void f263(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,-3);*r1p=r1;*r0p=r0;}
static void f264(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,4);*r1p=r1;*r0p=r0;}
static void f265(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,-4);*r1p=r1;*r0p=r0;}
static void f266(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,3);*r1p=r1;*r0p=r0;}
static void f267(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,-5);*r1p=r1;*r0p=r0;}
static void f268(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,8);*r1p=r1;*r0p=r0;}
static void f269(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,-8);*r1p=r1;*r0p=r0;}
static void f270(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,7);*r1p=r1;*r0p=r0;}
static void f271(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,-9);*r1p=r1;*r0p=r0;}
static void f272(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,16);*r1p=r1;*r0p=r0;}
static void f273(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,-16);*r1p=r1;*r0p=r0;}
static void f274(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,15);*r1p=r1;*r0p=r0;}
static void f275(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,-17);*r1p=r1;*r0p=r0;}
static void f276(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,32);*r1p=r1;*r0p=r0;}
static void f277(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,-32);*r1p=r1;*r0p=r0;}
static void f278(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,31);*r1p=r1;*r0p=r0;}
static void f279(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,-33);*r1p=r1;*r0p=r0;}
static void f280(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,64);*r1p=r1;*r0p=r0;}
static void f281(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,-64);*r1p=r1;*r0p=r0;}
static void f282(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,63);*r1p=r1;*r0p=r0;}
static void f283(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,-65);*r1p=r1;*r0p=r0;}
static void f284(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,128);*r1p=r1;*r0p=r0;}
static void f285(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,-128);*r1p=r1;*r0p=r0;}
static void f286(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,127);*r1p=r1;*r0p=r0;}
static void f287(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,-129);*r1p=r1;*r0p=r0;}
static void f288(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,256);*r1p=r1;*r0p=r0;}
static void f289(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,-256);*r1p=r1;*r0p=r0;}
static void f290(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,255);*r1p=r1;*r0p=r0;}
static void f291(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,-257);*r1p=r1;*r0p=r0;}
static void f292(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,512);*r1p=r1;*r0p=r0;}
static void f293(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,-512);*r1p=r1;*r0p=r0;}
static void f294(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,511);*r1p=r1;*r0p=r0;}
static void f295(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,-513);*r1p=r1;*r0p=r0;}
static void f296(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,1024);*r1p=r1;*r0p=r0;}
static void f297(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,-1024);*r1p=r1;*r0p=r0;}
static void f298(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,1023);*r1p=r1;*r0p=r0;}
static void f299(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,-1025);*r1p=r1;*r0p=r0;}
static void f300(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,2048);*r1p=r1;*r0p=r0;}
static void f301(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,-2048);*r1p=r1;*r0p=r0;}
static void f302(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,2047);*r1p=r1;*r0p=r0;}
static void f303(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,-2049);*r1p=r1;*r0p=r0;}
static void f304(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,4096);*r1p=r1;*r0p=r0;}
static void f305(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,-4096);*r1p=r1;*r0p=r0;}
static void f306(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,4095);*r1p=r1;*r0p=r0;}
static void f307(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,-4097);*r1p=r1;*r0p=r0;}
static void f308(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,8192);*r1p=r1;*r0p=r0;}
static void f309(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,-8192);*r1p=r1;*r0p=r0;}
static void f310(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,8191);*r1p=r1;*r0p=r0;}
static void f311(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,-8193);*r1p=r1;*r0p=r0;}
static void f312(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,16384);*r1p=r1;*r0p=r0;}
static void f313(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,-16384);*r1p=r1;*r0p=r0;}
static void f314(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,16383);*r1p=r1;*r0p=r0;}
static void f315(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,-16385);*r1p=r1;*r0p=r0;}
static void f316(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,32768);*r1p=r1;*r0p=r0;}
static void f317(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,-32768);*r1p=r1;*r0p=r0;}
static void f318(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,32767);*r1p=r1;*r0p=r0;}
static void f319(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2,0,-32769);*r1p=r1;*r0p=r0;}
static void f320(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,1);*r1p=r1;*r0p=r0;}
static void f321(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-1);*r1p=r1;*r0p=r0;}
static void f322(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,0);*r1p=r1;*r0p=r0;}
static void f323(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-2);*r1p=r1;*r0p=r0;}
static void f324(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,2);*r1p=r1;*r0p=r0;}
static void f325(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-2);*r1p=r1;*r0p=r0;}
static void f326(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,1);*r1p=r1;*r0p=r0;}
static void f327(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-3);*r1p=r1;*r0p=r0;}
static void f328(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,4);*r1p=r1;*r0p=r0;}
static void f329(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-4);*r1p=r1;*r0p=r0;}
static void f330(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,3);*r1p=r1;*r0p=r0;}
static void f331(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-5);*r1p=r1;*r0p=r0;}
static void f332(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,8);*r1p=r1;*r0p=r0;}
static void f333(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-8);*r1p=r1;*r0p=r0;}
static void f334(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,7);*r1p=r1;*r0p=r0;}
static void f335(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-9);*r1p=r1;*r0p=r0;}
static void f336(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,16);*r1p=r1;*r0p=r0;}
static void f337(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-16);*r1p=r1;*r0p=r0;}
static void f338(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,15);*r1p=r1;*r0p=r0;}
static void f339(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-17);*r1p=r1;*r0p=r0;}
static void f340(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,32);*r1p=r1;*r0p=r0;}
static void f341(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-32);*r1p=r1;*r0p=r0;}
static void f342(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,31);*r1p=r1;*r0p=r0;}
static void f343(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-33);*r1p=r1;*r0p=r0;}
static void f344(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,64);*r1p=r1;*r0p=r0;}
static void f345(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-64);*r1p=r1;*r0p=r0;}
static void f346(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,63);*r1p=r1;*r0p=r0;}
static void f347(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-65);*r1p=r1;*r0p=r0;}
static void f348(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,128);*r1p=r1;*r0p=r0;}
static void f349(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-128);*r1p=r1;*r0p=r0;}
static void f350(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,127);*r1p=r1;*r0p=r0;}
static void f351(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-129);*r1p=r1;*r0p=r0;}
static void f352(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,256);*r1p=r1;*r0p=r0;}
static void f353(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-256);*r1p=r1;*r0p=r0;}
static void f354(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,255);*r1p=r1;*r0p=r0;}
static void f355(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-257);*r1p=r1;*r0p=r0;}
static void f356(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,512);*r1p=r1;*r0p=r0;}
static void f357(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-512);*r1p=r1;*r0p=r0;}
static void f358(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,511);*r1p=r1;*r0p=r0;}
static void f359(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-513);*r1p=r1;*r0p=r0;}
static void f360(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,1024);*r1p=r1;*r0p=r0;}
static void f361(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-1024);*r1p=r1;*r0p=r0;}
static void f362(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,1023);*r1p=r1;*r0p=r0;}
static void f363(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-1025);*r1p=r1;*r0p=r0;}
static void f364(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,2048);*r1p=r1;*r0p=r0;}
static void f365(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-2048);*r1p=r1;*r0p=r0;}
static void f366(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,2047);*r1p=r1;*r0p=r0;}
static void f367(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-2049);*r1p=r1;*r0p=r0;}
static void f368(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,4096);*r1p=r1;*r0p=r0;}
static void f369(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-4096);*r1p=r1;*r0p=r0;}
static void f370(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,4095);*r1p=r1;*r0p=r0;}
static void f371(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-4097);*r1p=r1;*r0p=r0;}
static void f372(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,8192);*r1p=r1;*r0p=r0;}
static void f373(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-8192);*r1p=r1;*r0p=r0;}
static void f374(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,8191);*r1p=r1;*r0p=r0;}
static void f375(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-8193);*r1p=r1;*r0p=r0;}
static void f376(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,16384);*r1p=r1;*r0p=r0;}
static void f377(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-16384);*r1p=r1;*r0p=r0;}
static void f378(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,16383);*r1p=r1;*r0p=r0;}
static void f379(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-16385);*r1p=r1;*r0p=r0;}
static void f380(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,32768);*r1p=r1;*r0p=r0;}
static void f381(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-32768);*r1p=r1;*r0p=r0;}
static void f382(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,32767);*r1p=r1;*r0p=r0;}
static void f383(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2,0,-32769);*r1p=r1;*r0p=r0;}
static void f384(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,1);*r1p=r1;*r0p=r0;}
static void f385(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-1);*r1p=r1;*r0p=r0;}
static void f386(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,0);*r1p=r1;*r0p=r0;}
static void f387(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-2);*r1p=r1;*r0p=r0;}
static void f388(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,2);*r1p=r1;*r0p=r0;}
static void f389(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-2);*r1p=r1;*r0p=r0;}
static void f390(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,1);*r1p=r1;*r0p=r0;}
static void f391(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-3);*r1p=r1;*r0p=r0;}
static void f392(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,4);*r1p=r1;*r0p=r0;}
static void f393(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-4);*r1p=r1;*r0p=r0;}
static void f394(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,3);*r1p=r1;*r0p=r0;}
static void f395(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-5);*r1p=r1;*r0p=r0;}
static void f396(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,8);*r1p=r1;*r0p=r0;}
static void f397(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-8);*r1p=r1;*r0p=r0;}
static void f398(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,7);*r1p=r1;*r0p=r0;}
static void f399(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-9);*r1p=r1;*r0p=r0;}
static void f400(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,16);*r1p=r1;*r0p=r0;}
static void f401(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-16);*r1p=r1;*r0p=r0;}
static void f402(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,15);*r1p=r1;*r0p=r0;}
static void f403(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-17);*r1p=r1;*r0p=r0;}
static void f404(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,32);*r1p=r1;*r0p=r0;}
static void f405(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-32);*r1p=r1;*r0p=r0;}
static void f406(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,31);*r1p=r1;*r0p=r0;}
static void f407(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-33);*r1p=r1;*r0p=r0;}
static void f408(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,64);*r1p=r1;*r0p=r0;}
static void f409(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-64);*r1p=r1;*r0p=r0;}
static void f410(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,63);*r1p=r1;*r0p=r0;}
static void f411(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-65);*r1p=r1;*r0p=r0;}
static void f412(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,128);*r1p=r1;*r0p=r0;}
static void f413(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-128);*r1p=r1;*r0p=r0;}
static void f414(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,127);*r1p=r1;*r0p=r0;}
static void f415(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-129);*r1p=r1;*r0p=r0;}
static void f416(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,256);*r1p=r1;*r0p=r0;}
static void f417(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-256);*r1p=r1;*r0p=r0;}
static void f418(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,255);*r1p=r1;*r0p=r0;}
static void f419(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-257);*r1p=r1;*r0p=r0;}
static void f420(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,512);*r1p=r1;*r0p=r0;}
static void f421(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-512);*r1p=r1;*r0p=r0;}
static void f422(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,511);*r1p=r1;*r0p=r0;}
static void f423(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-513);*r1p=r1;*r0p=r0;}
static void f424(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,1024);*r1p=r1;*r0p=r0;}
static void f425(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-1024);*r1p=r1;*r0p=r0;}
static void f426(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,1023);*r1p=r1;*r0p=r0;}
static void f427(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-1025);*r1p=r1;*r0p=r0;}
static void f428(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,2048);*r1p=r1;*r0p=r0;}
static void f429(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-2048);*r1p=r1;*r0p=r0;}
static void f430(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,2047);*r1p=r1;*r0p=r0;}
static void f431(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-2049);*r1p=r1;*r0p=r0;}
static void f432(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,4096);*r1p=r1;*r0p=r0;}
static void f433(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-4096);*r1p=r1;*r0p=r0;}
static void f434(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,4095);*r1p=r1;*r0p=r0;}
static void f435(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-4097);*r1p=r1;*r0p=r0;}
static void f436(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,8192);*r1p=r1;*r0p=r0;}
static void f437(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-8192);*r1p=r1;*r0p=r0;}
static void f438(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,8191);*r1p=r1;*r0p=r0;}
static void f439(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-8193);*r1p=r1;*r0p=r0;}
static void f440(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,16384);*r1p=r1;*r0p=r0;}
static void f441(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-16384);*r1p=r1;*r0p=r0;}
static void f442(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,16383);*r1p=r1;*r0p=r0;}
static void f443(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-16385);*r1p=r1;*r0p=r0;}
static void f444(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,32768);*r1p=r1;*r0p=r0;}
static void f445(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-32768);*r1p=r1;*r0p=r0;}
static void f446(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,32767);*r1p=r1;*r0p=r0;}
static void f447(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1,0,-32769);*r1p=r1;*r0p=r0;}
static void f448(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,1);*r1p=r1;*r0p=r0;}
static void f449(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,-1);*r1p=r1;*r0p=r0;}
static void f450(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,0);*r1p=r1;*r0p=r0;}
static void f451(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,-2);*r1p=r1;*r0p=r0;}
static void f452(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,2);*r1p=r1;*r0p=r0;}
static void f453(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,-2);*r1p=r1;*r0p=r0;}
static void f454(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,1);*r1p=r1;*r0p=r0;}
static void f455(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,-3);*r1p=r1;*r0p=r0;}
static void f456(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,4);*r1p=r1;*r0p=r0;}
static void f457(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,-4);*r1p=r1;*r0p=r0;}
static void f458(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,3);*r1p=r1;*r0p=r0;}
static void f459(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,-5);*r1p=r1;*r0p=r0;}
static void f460(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,8);*r1p=r1;*r0p=r0;}
static void f461(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,-8);*r1p=r1;*r0p=r0;}
static void f462(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,7);*r1p=r1;*r0p=r0;}
static void f463(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,-9);*r1p=r1;*r0p=r0;}
static void f464(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,16);*r1p=r1;*r0p=r0;}
static void f465(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,-16);*r1p=r1;*r0p=r0;}
static void f466(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,15);*r1p=r1;*r0p=r0;}
static void f467(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,-17);*r1p=r1;*r0p=r0;}
static void f468(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,32);*r1p=r1;*r0p=r0;}
static void f469(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,-32);*r1p=r1;*r0p=r0;}
static void f470(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,31);*r1p=r1;*r0p=r0;}
static void f471(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,-33);*r1p=r1;*r0p=r0;}
static void f472(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,64);*r1p=r1;*r0p=r0;}
static void f473(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,-64);*r1p=r1;*r0p=r0;}
static void f474(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,63);*r1p=r1;*r0p=r0;}
static void f475(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,-65);*r1p=r1;*r0p=r0;}
static void f476(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,128);*r1p=r1;*r0p=r0;}
static void f477(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,-128);*r1p=r1;*r0p=r0;}
static void f478(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,127);*r1p=r1;*r0p=r0;}
static void f479(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,-129);*r1p=r1;*r0p=r0;}
static void f480(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,256);*r1p=r1;*r0p=r0;}
static void f481(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,-256);*r1p=r1;*r0p=r0;}
static void f482(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,255);*r1p=r1;*r0p=r0;}
static void f483(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,-257);*r1p=r1;*r0p=r0;}
static void f484(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,512);*r1p=r1;*r0p=r0;}
static void f485(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,-512);*r1p=r1;*r0p=r0;}
static void f486(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,511);*r1p=r1;*r0p=r0;}
static void f487(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,-513);*r1p=r1;*r0p=r0;}
static void f488(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,1024);*r1p=r1;*r0p=r0;}
static void f489(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,-1024);*r1p=r1;*r0p=r0;}
static void f490(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,1023);*r1p=r1;*r0p=r0;}
static void f491(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,-1025);*r1p=r1;*r0p=r0;}
static void f492(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,2048);*r1p=r1;*r0p=r0;}
static void f493(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,-2048);*r1p=r1;*r0p=r0;}
static void f494(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,2047);*r1p=r1;*r0p=r0;}
static void f495(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,-2049);*r1p=r1;*r0p=r0;}
static void f496(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,4096);*r1p=r1;*r0p=r0;}
static void f497(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,-4096);*r1p=r1;*r0p=r0;}
static void f498(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,4095);*r1p=r1;*r0p=r0;}
static void f499(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,-4097);*r1p=r1;*r0p=r0;}
static void f500(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,8192);*r1p=r1;*r0p=r0;}
static void f501(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,-8192);*r1p=r1;*r0p=r0;}
static void f502(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,8191);*r1p=r1;*r0p=r0;}
static void f503(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,-8193);*r1p=r1;*r0p=r0;}
static void f504(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,16384);*r1p=r1;*r0p=r0;}
static void f505(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,-16384);*r1p=r1;*r0p=r0;}
static void f506(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,16383);*r1p=r1;*r0p=r0;}
static void f507(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,-16385);*r1p=r1;*r0p=r0;}
static void f508(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,32768);*r1p=r1;*r0p=r0;}
static void f509(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,-32768);*r1p=r1;*r0p=r0;}
static void f510(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,32767);*r1p=r1;*r0p=r0;}
static void f511(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-3,0,-32769);*r1p=r1;*r0p=r0;}
static void f512(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,1);*r1p=r1;*r0p=r0;}
static void f513(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,-1);*r1p=r1;*r0p=r0;}
static void f514(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,0);*r1p=r1;*r0p=r0;}
static void f515(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,-2);*r1p=r1;*r0p=r0;}
static void f516(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,2);*r1p=r1;*r0p=r0;}
static void f517(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,-2);*r1p=r1;*r0p=r0;}
static void f518(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,1);*r1p=r1;*r0p=r0;}
static void f519(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,-3);*r1p=r1;*r0p=r0;}
static void f520(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,4);*r1p=r1;*r0p=r0;}
static void f521(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,-4);*r1p=r1;*r0p=r0;}
static void f522(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,3);*r1p=r1;*r0p=r0;}
static void f523(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,-5);*r1p=r1;*r0p=r0;}
static void f524(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,8);*r1p=r1;*r0p=r0;}
static void f525(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,-8);*r1p=r1;*r0p=r0;}
static void f526(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,7);*r1p=r1;*r0p=r0;}
static void f527(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,-9);*r1p=r1;*r0p=r0;}
static void f528(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,16);*r1p=r1;*r0p=r0;}
static void f529(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,-16);*r1p=r1;*r0p=r0;}
static void f530(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,15);*r1p=r1;*r0p=r0;}
static void f531(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,-17);*r1p=r1;*r0p=r0;}
static void f532(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,32);*r1p=r1;*r0p=r0;}
static void f533(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,-32);*r1p=r1;*r0p=r0;}
static void f534(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,31);*r1p=r1;*r0p=r0;}
static void f535(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,-33);*r1p=r1;*r0p=r0;}
static void f536(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,64);*r1p=r1;*r0p=r0;}
static void f537(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,-64);*r1p=r1;*r0p=r0;}
static void f538(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,63);*r1p=r1;*r0p=r0;}
static void f539(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,-65);*r1p=r1;*r0p=r0;}
static void f540(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,128);*r1p=r1;*r0p=r0;}
static void f541(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,-128);*r1p=r1;*r0p=r0;}
static void f542(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,127);*r1p=r1;*r0p=r0;}
static void f543(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,-129);*r1p=r1;*r0p=r0;}
static void f544(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,256);*r1p=r1;*r0p=r0;}
static void f545(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,-256);*r1p=r1;*r0p=r0;}
static void f546(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,255);*r1p=r1;*r0p=r0;}
static void f547(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,-257);*r1p=r1;*r0p=r0;}
static void f548(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,512);*r1p=r1;*r0p=r0;}
static void f549(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,-512);*r1p=r1;*r0p=r0;}
static void f550(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,511);*r1p=r1;*r0p=r0;}
static void f551(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,-513);*r1p=r1;*r0p=r0;}
static void f552(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,1024);*r1p=r1;*r0p=r0;}
static void f553(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,-1024);*r1p=r1;*r0p=r0;}
static void f554(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,1023);*r1p=r1;*r0p=r0;}
static void f555(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,-1025);*r1p=r1;*r0p=r0;}
static void f556(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,2048);*r1p=r1;*r0p=r0;}
static void f557(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,-2048);*r1p=r1;*r0p=r0;}
static void f558(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,2047);*r1p=r1;*r0p=r0;}
static void f559(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,-2049);*r1p=r1;*r0p=r0;}
static void f560(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,4096);*r1p=r1;*r0p=r0;}
static void f561(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,-4096);*r1p=r1;*r0p=r0;}
static void f562(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,4095);*r1p=r1;*r0p=r0;}
static void f563(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,-4097);*r1p=r1;*r0p=r0;}
static void f564(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,8192);*r1p=r1;*r0p=r0;}
static void f565(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,-8192);*r1p=r1;*r0p=r0;}
static void f566(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,8191);*r1p=r1;*r0p=r0;}
static void f567(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,-8193);*r1p=r1;*r0p=r0;}
static void f568(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,16384);*r1p=r1;*r0p=r0;}
static void f569(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,-16384);*r1p=r1;*r0p=r0;}
static void f570(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,16383);*r1p=r1;*r0p=r0;}
static void f571(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,-16385);*r1p=r1;*r0p=r0;}
static void f572(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,32768);*r1p=r1;*r0p=r0;}
static void f573(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,-32768);*r1p=r1;*r0p=r0;}
static void f574(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,32767);*r1p=r1;*r0p=r0;}
static void f575(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4,0,-32769);*r1p=r1;*r0p=r0;}
static void f576(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,1);*r1p=r1;*r0p=r0;}
static void f577(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,-1);*r1p=r1;*r0p=r0;}
static void f578(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,0);*r1p=r1;*r0p=r0;}
static void f579(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,-2);*r1p=r1;*r0p=r0;}
static void f580(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,2);*r1p=r1;*r0p=r0;}
static void f581(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,-2);*r1p=r1;*r0p=r0;}
static void f582(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,1);*r1p=r1;*r0p=r0;}
static void f583(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,-3);*r1p=r1;*r0p=r0;}
static void f584(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,4);*r1p=r1;*r0p=r0;}
static void f585(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,-4);*r1p=r1;*r0p=r0;}
static void f586(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,3);*r1p=r1;*r0p=r0;}
static void f587(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,-5);*r1p=r1;*r0p=r0;}
static void f588(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,8);*r1p=r1;*r0p=r0;}
static void f589(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,-8);*r1p=r1;*r0p=r0;}
static void f590(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,7);*r1p=r1;*r0p=r0;}
static void f591(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,-9);*r1p=r1;*r0p=r0;}
static void f592(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,16);*r1p=r1;*r0p=r0;}
static void f593(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,-16);*r1p=r1;*r0p=r0;}
static void f594(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,15);*r1p=r1;*r0p=r0;}
static void f595(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,-17);*r1p=r1;*r0p=r0;}
static void f596(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,32);*r1p=r1;*r0p=r0;}
static void f597(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,-32);*r1p=r1;*r0p=r0;}
static void f598(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,31);*r1p=r1;*r0p=r0;}
static void f599(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,-33);*r1p=r1;*r0p=r0;}
static void f600(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,64);*r1p=r1;*r0p=r0;}
static void f601(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,-64);*r1p=r1;*r0p=r0;}
static void f602(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,63);*r1p=r1;*r0p=r0;}
static void f603(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,-65);*r1p=r1;*r0p=r0;}
static void f604(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,128);*r1p=r1;*r0p=r0;}
static void f605(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,-128);*r1p=r1;*r0p=r0;}
static void f606(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,127);*r1p=r1;*r0p=r0;}
static void f607(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,-129);*r1p=r1;*r0p=r0;}
static void f608(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,256);*r1p=r1;*r0p=r0;}
static void f609(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,-256);*r1p=r1;*r0p=r0;}
static void f610(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,255);*r1p=r1;*r0p=r0;}
static void f611(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,-257);*r1p=r1;*r0p=r0;}
static void f612(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,512);*r1p=r1;*r0p=r0;}
static void f613(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,-512);*r1p=r1;*r0p=r0;}
static void f614(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,511);*r1p=r1;*r0p=r0;}
static void f615(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,-513);*r1p=r1;*r0p=r0;}
static void f616(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,1024);*r1p=r1;*r0p=r0;}
static void f617(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,-1024);*r1p=r1;*r0p=r0;}
static void f618(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,1023);*r1p=r1;*r0p=r0;}
static void f619(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,-1025);*r1p=r1;*r0p=r0;}
static void f620(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,2048);*r1p=r1;*r0p=r0;}
static void f621(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,-2048);*r1p=r1;*r0p=r0;}
static void f622(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,2047);*r1p=r1;*r0p=r0;}
static void f623(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,-2049);*r1p=r1;*r0p=r0;}
static void f624(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,4096);*r1p=r1;*r0p=r0;}
static void f625(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,-4096);*r1p=r1;*r0p=r0;}
static void f626(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,4095);*r1p=r1;*r0p=r0;}
static void f627(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,-4097);*r1p=r1;*r0p=r0;}
static void f628(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,8192);*r1p=r1;*r0p=r0;}
static void f629(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,-8192);*r1p=r1;*r0p=r0;}
static void f630(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,8191);*r1p=r1;*r0p=r0;}
static void f631(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,-8193);*r1p=r1;*r0p=r0;}
static void f632(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,16384);*r1p=r1;*r0p=r0;}
static void f633(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,-16384);*r1p=r1;*r0p=r0;}
static void f634(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,16383);*r1p=r1;*r0p=r0;}
static void f635(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,-16385);*r1p=r1;*r0p=r0;}
static void f636(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,32768);*r1p=r1;*r0p=r0;}
static void f637(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,-32768);*r1p=r1;*r0p=r0;}
static void f638(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,32767);*r1p=r1;*r0p=r0;}
static void f639(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4,0,-32769);*r1p=r1;*r0p=r0;}
static void f640(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,1);*r1p=r1;*r0p=r0;}
static void f641(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,-1);*r1p=r1;*r0p=r0;}
static void f642(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,0);*r1p=r1;*r0p=r0;}
static void f643(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,-2);*r1p=r1;*r0p=r0;}
static void f644(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,2);*r1p=r1;*r0p=r0;}
static void f645(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,-2);*r1p=r1;*r0p=r0;}
static void f646(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,1);*r1p=r1;*r0p=r0;}
static void f647(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,-3);*r1p=r1;*r0p=r0;}
static void f648(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,4);*r1p=r1;*r0p=r0;}
static void f649(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,-4);*r1p=r1;*r0p=r0;}
static void f650(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,3);*r1p=r1;*r0p=r0;}
static void f651(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,-5);*r1p=r1;*r0p=r0;}
static void f652(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,8);*r1p=r1;*r0p=r0;}
static void f653(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,-8);*r1p=r1;*r0p=r0;}
static void f654(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,7);*r1p=r1;*r0p=r0;}
static void f655(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,-9);*r1p=r1;*r0p=r0;}
static void f656(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,16);*r1p=r1;*r0p=r0;}
static void f657(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,-16);*r1p=r1;*r0p=r0;}
static void f658(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,15);*r1p=r1;*r0p=r0;}
static void f659(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,-17);*r1p=r1;*r0p=r0;}
static void f660(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,32);*r1p=r1;*r0p=r0;}
static void f661(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,-32);*r1p=r1;*r0p=r0;}
static void f662(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,31);*r1p=r1;*r0p=r0;}
static void f663(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,-33);*r1p=r1;*r0p=r0;}
static void f664(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,64);*r1p=r1;*r0p=r0;}
static void f665(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,-64);*r1p=r1;*r0p=r0;}
static void f666(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,63);*r1p=r1;*r0p=r0;}
static void f667(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,-65);*r1p=r1;*r0p=r0;}
static void f668(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,128);*r1p=r1;*r0p=r0;}
static void f669(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,-128);*r1p=r1;*r0p=r0;}
static void f670(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,127);*r1p=r1;*r0p=r0;}
static void f671(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,-129);*r1p=r1;*r0p=r0;}
static void f672(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,256);*r1p=r1;*r0p=r0;}
static void f673(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,-256);*r1p=r1;*r0p=r0;}
static void f674(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,255);*r1p=r1;*r0p=r0;}
static void f675(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,-257);*r1p=r1;*r0p=r0;}
static void f676(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,512);*r1p=r1;*r0p=r0;}
static void f677(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,-512);*r1p=r1;*r0p=r0;}
static void f678(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,511);*r1p=r1;*r0p=r0;}
static void f679(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,-513);*r1p=r1;*r0p=r0;}
static void f680(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,1024);*r1p=r1;*r0p=r0;}
static void f681(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,-1024);*r1p=r1;*r0p=r0;}
static void f682(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,1023);*r1p=r1;*r0p=r0;}
static void f683(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,-1025);*r1p=r1;*r0p=r0;}
static void f684(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,2048);*r1p=r1;*r0p=r0;}
static void f685(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,-2048);*r1p=r1;*r0p=r0;}
static void f686(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,2047);*r1p=r1;*r0p=r0;}
static void f687(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,-2049);*r1p=r1;*r0p=r0;}
static void f688(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,4096);*r1p=r1;*r0p=r0;}
static void f689(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,-4096);*r1p=r1;*r0p=r0;}
static void f690(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,4095);*r1p=r1;*r0p=r0;}
static void f691(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,-4097);*r1p=r1;*r0p=r0;}
static void f692(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,8192);*r1p=r1;*r0p=r0;}
static void f693(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,-8192);*r1p=r1;*r0p=r0;}
static void f694(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,8191);*r1p=r1;*r0p=r0;}
static void f695(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,-8193);*r1p=r1;*r0p=r0;}
static void f696(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,16384);*r1p=r1;*r0p=r0;}
static void f697(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,-16384);*r1p=r1;*r0p=r0;}
static void f698(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,16383);*r1p=r1;*r0p=r0;}
static void f699(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,-16385);*r1p=r1;*r0p=r0;}
static void f700(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,32768);*r1p=r1;*r0p=r0;}
static void f701(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,-32768);*r1p=r1;*r0p=r0;}
static void f702(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,32767);*r1p=r1;*r0p=r0;}
static void f703(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,3,0,-32769);*r1p=r1;*r0p=r0;}
static void f704(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,1);*r1p=r1;*r0p=r0;}
static void f705(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,-1);*r1p=r1;*r0p=r0;}
static void f706(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,0);*r1p=r1;*r0p=r0;}
static void f707(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,-2);*r1p=r1;*r0p=r0;}
static void f708(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,2);*r1p=r1;*r0p=r0;}
static void f709(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,-2);*r1p=r1;*r0p=r0;}
static void f710(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,1);*r1p=r1;*r0p=r0;}
static void f711(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,-3);*r1p=r1;*r0p=r0;}
static void f712(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,4);*r1p=r1;*r0p=r0;}
static void f713(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,-4);*r1p=r1;*r0p=r0;}
static void f714(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,3);*r1p=r1;*r0p=r0;}
static void f715(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,-5);*r1p=r1;*r0p=r0;}
static void f716(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,8);*r1p=r1;*r0p=r0;}
static void f717(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,-8);*r1p=r1;*r0p=r0;}
static void f718(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,7);*r1p=r1;*r0p=r0;}
static void f719(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,-9);*r1p=r1;*r0p=r0;}
static void f720(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,16);*r1p=r1;*r0p=r0;}
static void f721(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,-16);*r1p=r1;*r0p=r0;}
static void f722(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,15);*r1p=r1;*r0p=r0;}
static void f723(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,-17);*r1p=r1;*r0p=r0;}
static void f724(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,32);*r1p=r1;*r0p=r0;}
static void f725(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,-32);*r1p=r1;*r0p=r0;}
static void f726(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,31);*r1p=r1;*r0p=r0;}
static void f727(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,-33);*r1p=r1;*r0p=r0;}
static void f728(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,64);*r1p=r1;*r0p=r0;}
static void f729(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,-64);*r1p=r1;*r0p=r0;}
static void f730(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,63);*r1p=r1;*r0p=r0;}
static void f731(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,-65);*r1p=r1;*r0p=r0;}
static void f732(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,128);*r1p=r1;*r0p=r0;}
static void f733(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,-128);*r1p=r1;*r0p=r0;}
static void f734(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,127);*r1p=r1;*r0p=r0;}
static void f735(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,-129);*r1p=r1;*r0p=r0;}
static void f736(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,256);*r1p=r1;*r0p=r0;}
static void f737(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,-256);*r1p=r1;*r0p=r0;}
static void f738(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,255);*r1p=r1;*r0p=r0;}
static void f739(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,-257);*r1p=r1;*r0p=r0;}
static void f740(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,512);*r1p=r1;*r0p=r0;}
static void f741(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,-512);*r1p=r1;*r0p=r0;}
static void f742(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,511);*r1p=r1;*r0p=r0;}
static void f743(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,-513);*r1p=r1;*r0p=r0;}
static void f744(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,1024);*r1p=r1;*r0p=r0;}
static void f745(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,-1024);*r1p=r1;*r0p=r0;}
static void f746(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,1023);*r1p=r1;*r0p=r0;}
static void f747(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,-1025);*r1p=r1;*r0p=r0;}
static void f748(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,2048);*r1p=r1;*r0p=r0;}
static void f749(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,-2048);*r1p=r1;*r0p=r0;}
static void f750(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,2047);*r1p=r1;*r0p=r0;}
static void f751(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,-2049);*r1p=r1;*r0p=r0;}
static void f752(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,4096);*r1p=r1;*r0p=r0;}
static void f753(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,-4096);*r1p=r1;*r0p=r0;}
static void f754(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,4095);*r1p=r1;*r0p=r0;}
static void f755(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,-4097);*r1p=r1;*r0p=r0;}
static void f756(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,8192);*r1p=r1;*r0p=r0;}
static void f757(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,-8192);*r1p=r1;*r0p=r0;}
static void f758(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,8191);*r1p=r1;*r0p=r0;}
static void f759(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,-8193);*r1p=r1;*r0p=r0;}
static void f760(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,16384);*r1p=r1;*r0p=r0;}
static void f761(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,-16384);*r1p=r1;*r0p=r0;}
static void f762(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,16383);*r1p=r1;*r0p=r0;}
static void f763(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,-16385);*r1p=r1;*r0p=r0;}
static void f764(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,32768);*r1p=r1;*r0p=r0;}
static void f765(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,-32768);*r1p=r1;*r0p=r0;}
static void f766(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,32767);*r1p=r1;*r0p=r0;}
static void f767(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-5,0,-32769);*r1p=r1;*r0p=r0;}
static void f768(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,1);*r1p=r1;*r0p=r0;}
static void f769(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,-1);*r1p=r1;*r0p=r0;}
static void f770(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,0);*r1p=r1;*r0p=r0;}
static void f771(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,-2);*r1p=r1;*r0p=r0;}
static void f772(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,2);*r1p=r1;*r0p=r0;}
static void f773(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,-2);*r1p=r1;*r0p=r0;}
static void f774(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,1);*r1p=r1;*r0p=r0;}
static void f775(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,-3);*r1p=r1;*r0p=r0;}
static void f776(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,4);*r1p=r1;*r0p=r0;}
static void f777(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,-4);*r1p=r1;*r0p=r0;}
static void f778(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,3);*r1p=r1;*r0p=r0;}
static void f779(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,-5);*r1p=r1;*r0p=r0;}
static void f780(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,8);*r1p=r1;*r0p=r0;}
static void f781(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,-8);*r1p=r1;*r0p=r0;}
static void f782(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,7);*r1p=r1;*r0p=r0;}
static void f783(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,-9);*r1p=r1;*r0p=r0;}
static void f784(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,16);*r1p=r1;*r0p=r0;}
static void f785(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,-16);*r1p=r1;*r0p=r0;}
static void f786(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,15);*r1p=r1;*r0p=r0;}
static void f787(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,-17);*r1p=r1;*r0p=r0;}
static void f788(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,32);*r1p=r1;*r0p=r0;}
static void f789(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,-32);*r1p=r1;*r0p=r0;}
static void f790(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,31);*r1p=r1;*r0p=r0;}
static void f791(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,-33);*r1p=r1;*r0p=r0;}
static void f792(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,64);*r1p=r1;*r0p=r0;}
static void f793(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,-64);*r1p=r1;*r0p=r0;}
static void f794(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,63);*r1p=r1;*r0p=r0;}
static void f795(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,-65);*r1p=r1;*r0p=r0;}
static void f796(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,128);*r1p=r1;*r0p=r0;}
static void f797(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,-128);*r1p=r1;*r0p=r0;}
static void f798(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,127);*r1p=r1;*r0p=r0;}
static void f799(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,-129);*r1p=r1;*r0p=r0;}
static void f800(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,256);*r1p=r1;*r0p=r0;}
static void f801(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,-256);*r1p=r1;*r0p=r0;}
static void f802(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,255);*r1p=r1;*r0p=r0;}
static void f803(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,-257);*r1p=r1;*r0p=r0;}
static void f804(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,512);*r1p=r1;*r0p=r0;}
static void f805(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,-512);*r1p=r1;*r0p=r0;}
static void f806(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,511);*r1p=r1;*r0p=r0;}
static void f807(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,-513);*r1p=r1;*r0p=r0;}
static void f808(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,1024);*r1p=r1;*r0p=r0;}
static void f809(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,-1024);*r1p=r1;*r0p=r0;}
static void f810(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,1023);*r1p=r1;*r0p=r0;}
static void f811(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,-1025);*r1p=r1;*r0p=r0;}
static void f812(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,2048);*r1p=r1;*r0p=r0;}
static void f813(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,-2048);*r1p=r1;*r0p=r0;}
static void f814(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,2047);*r1p=r1;*r0p=r0;}
static void f815(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,-2049);*r1p=r1;*r0p=r0;}
static void f816(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,4096);*r1p=r1;*r0p=r0;}
static void f817(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,-4096);*r1p=r1;*r0p=r0;}
static void f818(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,4095);*r1p=r1;*r0p=r0;}
static void f819(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,-4097);*r1p=r1;*r0p=r0;}
static void f820(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,8192);*r1p=r1;*r0p=r0;}
static void f821(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,-8192);*r1p=r1;*r0p=r0;}
static void f822(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,8191);*r1p=r1;*r0p=r0;}
static void f823(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,-8193);*r1p=r1;*r0p=r0;}
static void f824(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,16384);*r1p=r1;*r0p=r0;}
static void f825(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,-16384);*r1p=r1;*r0p=r0;}
static void f826(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,16383);*r1p=r1;*r0p=r0;}
static void f827(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,-16385);*r1p=r1;*r0p=r0;}
static void f828(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,32768);*r1p=r1;*r0p=r0;}
static void f829(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,-32768);*r1p=r1;*r0p=r0;}
static void f830(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,32767);*r1p=r1;*r0p=r0;}
static void f831(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8,0,-32769);*r1p=r1;*r0p=r0;}
static void f832(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,1);*r1p=r1;*r0p=r0;}
static void f833(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,-1);*r1p=r1;*r0p=r0;}
static void f834(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,0);*r1p=r1;*r0p=r0;}
static void f835(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,-2);*r1p=r1;*r0p=r0;}
static void f836(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,2);*r1p=r1;*r0p=r0;}
static void f837(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,-2);*r1p=r1;*r0p=r0;}
static void f838(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,1);*r1p=r1;*r0p=r0;}
static void f839(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,-3);*r1p=r1;*r0p=r0;}
static void f840(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,4);*r1p=r1;*r0p=r0;}
static void f841(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,-4);*r1p=r1;*r0p=r0;}
static void f842(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,3);*r1p=r1;*r0p=r0;}
static void f843(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,-5);*r1p=r1;*r0p=r0;}
static void f844(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,8);*r1p=r1;*r0p=r0;}
static void f845(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,-8);*r1p=r1;*r0p=r0;}
static void f846(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,7);*r1p=r1;*r0p=r0;}
static void f847(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,-9);*r1p=r1;*r0p=r0;}
static void f848(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,16);*r1p=r1;*r0p=r0;}
static void f849(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,-16);*r1p=r1;*r0p=r0;}
static void f850(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,15);*r1p=r1;*r0p=r0;}
static void f851(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,-17);*r1p=r1;*r0p=r0;}
static void f852(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,32);*r1p=r1;*r0p=r0;}
static void f853(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,-32);*r1p=r1;*r0p=r0;}
static void f854(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,31);*r1p=r1;*r0p=r0;}
static void f855(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,-33);*r1p=r1;*r0p=r0;}
static void f856(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,64);*r1p=r1;*r0p=r0;}
static void f857(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,-64);*r1p=r1;*r0p=r0;}
static void f858(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,63);*r1p=r1;*r0p=r0;}
static void f859(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,-65);*r1p=r1;*r0p=r0;}
static void f860(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,128);*r1p=r1;*r0p=r0;}
static void f861(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,-128);*r1p=r1;*r0p=r0;}
static void f862(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,127);*r1p=r1;*r0p=r0;}
static void f863(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,-129);*r1p=r1;*r0p=r0;}
static void f864(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,256);*r1p=r1;*r0p=r0;}
static void f865(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,-256);*r1p=r1;*r0p=r0;}
static void f866(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,255);*r1p=r1;*r0p=r0;}
static void f867(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,-257);*r1p=r1;*r0p=r0;}
static void f868(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,512);*r1p=r1;*r0p=r0;}
static void f869(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,-512);*r1p=r1;*r0p=r0;}
static void f870(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,511);*r1p=r1;*r0p=r0;}
static void f871(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,-513);*r1p=r1;*r0p=r0;}
static void f872(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,1024);*r1p=r1;*r0p=r0;}
static void f873(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,-1024);*r1p=r1;*r0p=r0;}
static void f874(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,1023);*r1p=r1;*r0p=r0;}
static void f875(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,-1025);*r1p=r1;*r0p=r0;}
static void f876(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,2048);*r1p=r1;*r0p=r0;}
static void f877(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,-2048);*r1p=r1;*r0p=r0;}
static void f878(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,2047);*r1p=r1;*r0p=r0;}
static void f879(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,-2049);*r1p=r1;*r0p=r0;}
static void f880(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,4096);*r1p=r1;*r0p=r0;}
static void f881(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,-4096);*r1p=r1;*r0p=r0;}
static void f882(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,4095);*r1p=r1;*r0p=r0;}
static void f883(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,-4097);*r1p=r1;*r0p=r0;}
static void f884(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,8192);*r1p=r1;*r0p=r0;}
static void f885(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,-8192);*r1p=r1;*r0p=r0;}
static void f886(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,8191);*r1p=r1;*r0p=r0;}
static void f887(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,-8193);*r1p=r1;*r0p=r0;}
static void f888(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,16384);*r1p=r1;*r0p=r0;}
static void f889(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,-16384);*r1p=r1;*r0p=r0;}
static void f890(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,16383);*r1p=r1;*r0p=r0;}
static void f891(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,-16385);*r1p=r1;*r0p=r0;}
static void f892(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,32768);*r1p=r1;*r0p=r0;}
static void f893(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,-32768);*r1p=r1;*r0p=r0;}
static void f894(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,32767);*r1p=r1;*r0p=r0;}
static void f895(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8,0,-32769);*r1p=r1;*r0p=r0;}
static void f896(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,1);*r1p=r1;*r0p=r0;}
static void f897(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,-1);*r1p=r1;*r0p=r0;}
static void f898(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,0);*r1p=r1;*r0p=r0;}
static void f899(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,-2);*r1p=r1;*r0p=r0;}
static void f900(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,2);*r1p=r1;*r0p=r0;}
static void f901(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,-2);*r1p=r1;*r0p=r0;}
static void f902(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,1);*r1p=r1;*r0p=r0;}
static void f903(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,-3);*r1p=r1;*r0p=r0;}
static void f904(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,4);*r1p=r1;*r0p=r0;}
static void f905(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,-4);*r1p=r1;*r0p=r0;}
static void f906(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,3);*r1p=r1;*r0p=r0;}
static void f907(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,-5);*r1p=r1;*r0p=r0;}
static void f908(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,8);*r1p=r1;*r0p=r0;}
static void f909(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,-8);*r1p=r1;*r0p=r0;}
static void f910(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,7);*r1p=r1;*r0p=r0;}
static void f911(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,-9);*r1p=r1;*r0p=r0;}
static void f912(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,16);*r1p=r1;*r0p=r0;}
static void f913(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,-16);*r1p=r1;*r0p=r0;}
static void f914(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,15);*r1p=r1;*r0p=r0;}
static void f915(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,-17);*r1p=r1;*r0p=r0;}
static void f916(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,32);*r1p=r1;*r0p=r0;}
static void f917(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,-32);*r1p=r1;*r0p=r0;}
static void f918(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,31);*r1p=r1;*r0p=r0;}
static void f919(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,-33);*r1p=r1;*r0p=r0;}
static void f920(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,64);*r1p=r1;*r0p=r0;}
static void f921(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,-64);*r1p=r1;*r0p=r0;}
static void f922(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,63);*r1p=r1;*r0p=r0;}
static void f923(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,-65);*r1p=r1;*r0p=r0;}
static void f924(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,128);*r1p=r1;*r0p=r0;}
static void f925(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,-128);*r1p=r1;*r0p=r0;}
static void f926(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,127);*r1p=r1;*r0p=r0;}
static void f927(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,-129);*r1p=r1;*r0p=r0;}
static void f928(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,256);*r1p=r1;*r0p=r0;}
static void f929(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,-256);*r1p=r1;*r0p=r0;}
static void f930(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,255);*r1p=r1;*r0p=r0;}
static void f931(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,-257);*r1p=r1;*r0p=r0;}
static void f932(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,512);*r1p=r1;*r0p=r0;}
static void f933(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,-512);*r1p=r1;*r0p=r0;}
static void f934(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,511);*r1p=r1;*r0p=r0;}
static void f935(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,-513);*r1p=r1;*r0p=r0;}
static void f936(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,1024);*r1p=r1;*r0p=r0;}
static void f937(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,-1024);*r1p=r1;*r0p=r0;}
static void f938(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,1023);*r1p=r1;*r0p=r0;}
static void f939(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,-1025);*r1p=r1;*r0p=r0;}
static void f940(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,2048);*r1p=r1;*r0p=r0;}
static void f941(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,-2048);*r1p=r1;*r0p=r0;}
static void f942(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,2047);*r1p=r1;*r0p=r0;}
static void f943(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,-2049);*r1p=r1;*r0p=r0;}
static void f944(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,4096);*r1p=r1;*r0p=r0;}
static void f945(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,-4096);*r1p=r1;*r0p=r0;}
static void f946(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,4095);*r1p=r1;*r0p=r0;}
static void f947(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,-4097);*r1p=r1;*r0p=r0;}
static void f948(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,8192);*r1p=r1;*r0p=r0;}
static void f949(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,-8192);*r1p=r1;*r0p=r0;}
static void f950(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,8191);*r1p=r1;*r0p=r0;}
static void f951(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,-8193);*r1p=r1;*r0p=r0;}
static void f952(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,16384);*r1p=r1;*r0p=r0;}
static void f953(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,-16384);*r1p=r1;*r0p=r0;}
static void f954(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,16383);*r1p=r1;*r0p=r0;}
static void f955(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,-16385);*r1p=r1;*r0p=r0;}
static void f956(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,32768);*r1p=r1;*r0p=r0;}
static void f957(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,-32768);*r1p=r1;*r0p=r0;}
static void f958(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,32767);*r1p=r1;*r0p=r0;}
static void f959(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,7,0,-32769);*r1p=r1;*r0p=r0;}
static void f960(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,1);*r1p=r1;*r0p=r0;}
static void f961(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,-1);*r1p=r1;*r0p=r0;}
static void f962(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,0);*r1p=r1;*r0p=r0;}
static void f963(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,-2);*r1p=r1;*r0p=r0;}
static void f964(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,2);*r1p=r1;*r0p=r0;}
static void f965(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,-2);*r1p=r1;*r0p=r0;}
static void f966(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,1);*r1p=r1;*r0p=r0;}
static void f967(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,-3);*r1p=r1;*r0p=r0;}
static void f968(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,4);*r1p=r1;*r0p=r0;}
static void f969(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,-4);*r1p=r1;*r0p=r0;}
static void f970(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,3);*r1p=r1;*r0p=r0;}
static void f971(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,-5);*r1p=r1;*r0p=r0;}
static void f972(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,8);*r1p=r1;*r0p=r0;}
static void f973(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,-8);*r1p=r1;*r0p=r0;}
static void f974(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,7);*r1p=r1;*r0p=r0;}
static void f975(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,-9);*r1p=r1;*r0p=r0;}
static void f976(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,16);*r1p=r1;*r0p=r0;}
static void f977(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,-16);*r1p=r1;*r0p=r0;}
static void f978(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,15);*r1p=r1;*r0p=r0;}
static void f979(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,-17);*r1p=r1;*r0p=r0;}
static void f980(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,32);*r1p=r1;*r0p=r0;}
static void f981(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,-32);*r1p=r1;*r0p=r0;}
static void f982(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,31);*r1p=r1;*r0p=r0;}
static void f983(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,-33);*r1p=r1;*r0p=r0;}
static void f984(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,64);*r1p=r1;*r0p=r0;}
static void f985(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,-64);*r1p=r1;*r0p=r0;}
static void f986(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,63);*r1p=r1;*r0p=r0;}
static void f987(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,-65);*r1p=r1;*r0p=r0;}
static void f988(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,128);*r1p=r1;*r0p=r0;}
static void f989(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,-128);*r1p=r1;*r0p=r0;}
static void f990(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,127);*r1p=r1;*r0p=r0;}
static void f991(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,-129);*r1p=r1;*r0p=r0;}
static void f992(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,256);*r1p=r1;*r0p=r0;}
static void f993(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,-256);*r1p=r1;*r0p=r0;}
static void f994(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,255);*r1p=r1;*r0p=r0;}
static void f995(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,-257);*r1p=r1;*r0p=r0;}
static void f996(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,512);*r1p=r1;*r0p=r0;}
static void f997(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,-512);*r1p=r1;*r0p=r0;}
static void f998(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,511);*r1p=r1;*r0p=r0;}
static void f999(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,-513);*r1p=r1;*r0p=r0;}
static void f1000(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,1024);*r1p=r1;*r0p=r0;}
static void f1001(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,-1024);*r1p=r1;*r0p=r0;}
static void f1002(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,1023);*r1p=r1;*r0p=r0;}
static void f1003(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,-1025);*r1p=r1;*r0p=r0;}
static void f1004(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,2048);*r1p=r1;*r0p=r0;}
static void f1005(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,-2048);*r1p=r1;*r0p=r0;}
static void f1006(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,2047);*r1p=r1;*r0p=r0;}
static void f1007(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,-2049);*r1p=r1;*r0p=r0;}
static void f1008(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,4096);*r1p=r1;*r0p=r0;}
static void f1009(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,-4096);*r1p=r1;*r0p=r0;}
static void f1010(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,4095);*r1p=r1;*r0p=r0;}
static void f1011(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,-4097);*r1p=r1;*r0p=r0;}
static void f1012(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,8192);*r1p=r1;*r0p=r0;}
static void f1013(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,-8192);*r1p=r1;*r0p=r0;}
static void f1014(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,8191);*r1p=r1;*r0p=r0;}
static void f1015(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,-8193);*r1p=r1;*r0p=r0;}
static void f1016(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,16384);*r1p=r1;*r0p=r0;}
static void f1017(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,-16384);*r1p=r1;*r0p=r0;}
static void f1018(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,16383);*r1p=r1;*r0p=r0;}
static void f1019(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,-16385);*r1p=r1;*r0p=r0;}
static void f1020(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,32768);*r1p=r1;*r0p=r0;}
static void f1021(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,-32768);*r1p=r1;*r0p=r0;}
static void f1022(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,32767);*r1p=r1;*r0p=r0;}
static void f1023(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-9,0,-32769);*r1p=r1;*r0p=r0;}
static void f1024(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,1);*r1p=r1;*r0p=r0;}
static void f1025(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,-1);*r1p=r1;*r0p=r0;}
static void f1026(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,0);*r1p=r1;*r0p=r0;}
static void f1027(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,-2);*r1p=r1;*r0p=r0;}
static void f1028(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,2);*r1p=r1;*r0p=r0;}
static void f1029(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,-2);*r1p=r1;*r0p=r0;}
static void f1030(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,1);*r1p=r1;*r0p=r0;}
static void f1031(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,-3);*r1p=r1;*r0p=r0;}
static void f1032(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,4);*r1p=r1;*r0p=r0;}
static void f1033(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,-4);*r1p=r1;*r0p=r0;}
static void f1034(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,3);*r1p=r1;*r0p=r0;}
static void f1035(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,-5);*r1p=r1;*r0p=r0;}
static void f1036(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,8);*r1p=r1;*r0p=r0;}
static void f1037(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,-8);*r1p=r1;*r0p=r0;}
static void f1038(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,7);*r1p=r1;*r0p=r0;}
static void f1039(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,-9);*r1p=r1;*r0p=r0;}
static void f1040(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,16);*r1p=r1;*r0p=r0;}
static void f1041(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,-16);*r1p=r1;*r0p=r0;}
static void f1042(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,15);*r1p=r1;*r0p=r0;}
static void f1043(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,-17);*r1p=r1;*r0p=r0;}
static void f1044(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,32);*r1p=r1;*r0p=r0;}
static void f1045(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,-32);*r1p=r1;*r0p=r0;}
static void f1046(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,31);*r1p=r1;*r0p=r0;}
static void f1047(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,-33);*r1p=r1;*r0p=r0;}
static void f1048(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,64);*r1p=r1;*r0p=r0;}
static void f1049(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,-64);*r1p=r1;*r0p=r0;}
static void f1050(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,63);*r1p=r1;*r0p=r0;}
static void f1051(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,-65);*r1p=r1;*r0p=r0;}
static void f1052(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,128);*r1p=r1;*r0p=r0;}
static void f1053(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,-128);*r1p=r1;*r0p=r0;}
static void f1054(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,127);*r1p=r1;*r0p=r0;}
static void f1055(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,-129);*r1p=r1;*r0p=r0;}
static void f1056(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,256);*r1p=r1;*r0p=r0;}
static void f1057(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,-256);*r1p=r1;*r0p=r0;}
static void f1058(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,255);*r1p=r1;*r0p=r0;}
static void f1059(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,-257);*r1p=r1;*r0p=r0;}
static void f1060(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,512);*r1p=r1;*r0p=r0;}
static void f1061(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,-512);*r1p=r1;*r0p=r0;}
static void f1062(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,511);*r1p=r1;*r0p=r0;}
static void f1063(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,-513);*r1p=r1;*r0p=r0;}
static void f1064(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,1024);*r1p=r1;*r0p=r0;}
static void f1065(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,-1024);*r1p=r1;*r0p=r0;}
static void f1066(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,1023);*r1p=r1;*r0p=r0;}
static void f1067(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,-1025);*r1p=r1;*r0p=r0;}
static void f1068(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,2048);*r1p=r1;*r0p=r0;}
static void f1069(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,-2048);*r1p=r1;*r0p=r0;}
static void f1070(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,2047);*r1p=r1;*r0p=r0;}
static void f1071(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,-2049);*r1p=r1;*r0p=r0;}
static void f1072(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,4096);*r1p=r1;*r0p=r0;}
static void f1073(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,-4096);*r1p=r1;*r0p=r0;}
static void f1074(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,4095);*r1p=r1;*r0p=r0;}
static void f1075(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,-4097);*r1p=r1;*r0p=r0;}
static void f1076(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,8192);*r1p=r1;*r0p=r0;}
static void f1077(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,-8192);*r1p=r1;*r0p=r0;}
static void f1078(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,8191);*r1p=r1;*r0p=r0;}
static void f1079(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,-8193);*r1p=r1;*r0p=r0;}
static void f1080(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,16384);*r1p=r1;*r0p=r0;}
static void f1081(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,-16384);*r1p=r1;*r0p=r0;}
static void f1082(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,16383);*r1p=r1;*r0p=r0;}
static void f1083(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,-16385);*r1p=r1;*r0p=r0;}
static void f1084(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,32768);*r1p=r1;*r0p=r0;}
static void f1085(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,-32768);*r1p=r1;*r0p=r0;}
static void f1086(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,32767);*r1p=r1;*r0p=r0;}
static void f1087(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16,0,-32769);*r1p=r1;*r0p=r0;}
static void f1088(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,1);*r1p=r1;*r0p=r0;}
static void f1089(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,-1);*r1p=r1;*r0p=r0;}
static void f1090(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,0);*r1p=r1;*r0p=r0;}
static void f1091(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,-2);*r1p=r1;*r0p=r0;}
static void f1092(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,2);*r1p=r1;*r0p=r0;}
static void f1093(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,-2);*r1p=r1;*r0p=r0;}
static void f1094(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,1);*r1p=r1;*r0p=r0;}
static void f1095(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,-3);*r1p=r1;*r0p=r0;}
static void f1096(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,4);*r1p=r1;*r0p=r0;}
static void f1097(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,-4);*r1p=r1;*r0p=r0;}
static void f1098(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,3);*r1p=r1;*r0p=r0;}
static void f1099(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,-5);*r1p=r1;*r0p=r0;}
static void f1100(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,8);*r1p=r1;*r0p=r0;}
static void f1101(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,-8);*r1p=r1;*r0p=r0;}
static void f1102(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,7);*r1p=r1;*r0p=r0;}
static void f1103(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,-9);*r1p=r1;*r0p=r0;}
static void f1104(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,16);*r1p=r1;*r0p=r0;}
static void f1105(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,-16);*r1p=r1;*r0p=r0;}
static void f1106(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,15);*r1p=r1;*r0p=r0;}
static void f1107(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,-17);*r1p=r1;*r0p=r0;}
static void f1108(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,32);*r1p=r1;*r0p=r0;}
static void f1109(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,-32);*r1p=r1;*r0p=r0;}
static void f1110(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,31);*r1p=r1;*r0p=r0;}
static void f1111(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,-33);*r1p=r1;*r0p=r0;}
static void f1112(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,64);*r1p=r1;*r0p=r0;}
static void f1113(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,-64);*r1p=r1;*r0p=r0;}
static void f1114(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,63);*r1p=r1;*r0p=r0;}
static void f1115(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,-65);*r1p=r1;*r0p=r0;}
static void f1116(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,128);*r1p=r1;*r0p=r0;}
static void f1117(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,-128);*r1p=r1;*r0p=r0;}
static void f1118(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,127);*r1p=r1;*r0p=r0;}
static void f1119(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,-129);*r1p=r1;*r0p=r0;}
static void f1120(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,256);*r1p=r1;*r0p=r0;}
static void f1121(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,-256);*r1p=r1;*r0p=r0;}
static void f1122(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,255);*r1p=r1;*r0p=r0;}
static void f1123(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,-257);*r1p=r1;*r0p=r0;}
static void f1124(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,512);*r1p=r1;*r0p=r0;}
static void f1125(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,-512);*r1p=r1;*r0p=r0;}
static void f1126(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,511);*r1p=r1;*r0p=r0;}
static void f1127(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,-513);*r1p=r1;*r0p=r0;}
static void f1128(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,1024);*r1p=r1;*r0p=r0;}
static void f1129(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,-1024);*r1p=r1;*r0p=r0;}
static void f1130(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,1023);*r1p=r1;*r0p=r0;}
static void f1131(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,-1025);*r1p=r1;*r0p=r0;}
static void f1132(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,2048);*r1p=r1;*r0p=r0;}
static void f1133(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,-2048);*r1p=r1;*r0p=r0;}
static void f1134(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,2047);*r1p=r1;*r0p=r0;}
static void f1135(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,-2049);*r1p=r1;*r0p=r0;}
static void f1136(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,4096);*r1p=r1;*r0p=r0;}
static void f1137(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,-4096);*r1p=r1;*r0p=r0;}
static void f1138(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,4095);*r1p=r1;*r0p=r0;}
static void f1139(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,-4097);*r1p=r1;*r0p=r0;}
static void f1140(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,8192);*r1p=r1;*r0p=r0;}
static void f1141(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,-8192);*r1p=r1;*r0p=r0;}
static void f1142(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,8191);*r1p=r1;*r0p=r0;}
static void f1143(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,-8193);*r1p=r1;*r0p=r0;}
static void f1144(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,16384);*r1p=r1;*r0p=r0;}
static void f1145(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,-16384);*r1p=r1;*r0p=r0;}
static void f1146(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,16383);*r1p=r1;*r0p=r0;}
static void f1147(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,-16385);*r1p=r1;*r0p=r0;}
static void f1148(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,32768);*r1p=r1;*r0p=r0;}
static void f1149(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,-32768);*r1p=r1;*r0p=r0;}
static void f1150(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,32767);*r1p=r1;*r0p=r0;}
static void f1151(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16,0,-32769);*r1p=r1;*r0p=r0;}
static void f1152(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,1);*r1p=r1;*r0p=r0;}
static void f1153(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,-1);*r1p=r1;*r0p=r0;}
static void f1154(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,0);*r1p=r1;*r0p=r0;}
static void f1155(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,-2);*r1p=r1;*r0p=r0;}
static void f1156(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,2);*r1p=r1;*r0p=r0;}
static void f1157(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,-2);*r1p=r1;*r0p=r0;}
static void f1158(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,1);*r1p=r1;*r0p=r0;}
static void f1159(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,-3);*r1p=r1;*r0p=r0;}
static void f1160(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,4);*r1p=r1;*r0p=r0;}
static void f1161(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,-4);*r1p=r1;*r0p=r0;}
static void f1162(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,3);*r1p=r1;*r0p=r0;}
static void f1163(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,-5);*r1p=r1;*r0p=r0;}
static void f1164(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,8);*r1p=r1;*r0p=r0;}
static void f1165(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,-8);*r1p=r1;*r0p=r0;}
static void f1166(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,7);*r1p=r1;*r0p=r0;}
static void f1167(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,-9);*r1p=r1;*r0p=r0;}
static void f1168(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,16);*r1p=r1;*r0p=r0;}
static void f1169(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,-16);*r1p=r1;*r0p=r0;}
static void f1170(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,15);*r1p=r1;*r0p=r0;}
static void f1171(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,-17);*r1p=r1;*r0p=r0;}
static void f1172(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,32);*r1p=r1;*r0p=r0;}
static void f1173(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,-32);*r1p=r1;*r0p=r0;}
static void f1174(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,31);*r1p=r1;*r0p=r0;}
static void f1175(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,-33);*r1p=r1;*r0p=r0;}
static void f1176(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,64);*r1p=r1;*r0p=r0;}
static void f1177(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,-64);*r1p=r1;*r0p=r0;}
static void f1178(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,63);*r1p=r1;*r0p=r0;}
static void f1179(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,-65);*r1p=r1;*r0p=r0;}
static void f1180(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,128);*r1p=r1;*r0p=r0;}
static void f1181(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,-128);*r1p=r1;*r0p=r0;}
static void f1182(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,127);*r1p=r1;*r0p=r0;}
static void f1183(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,-129);*r1p=r1;*r0p=r0;}
static void f1184(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,256);*r1p=r1;*r0p=r0;}
static void f1185(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,-256);*r1p=r1;*r0p=r0;}
static void f1186(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,255);*r1p=r1;*r0p=r0;}
static void f1187(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,-257);*r1p=r1;*r0p=r0;}
static void f1188(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,512);*r1p=r1;*r0p=r0;}
static void f1189(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,-512);*r1p=r1;*r0p=r0;}
static void f1190(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,511);*r1p=r1;*r0p=r0;}
static void f1191(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,-513);*r1p=r1;*r0p=r0;}
static void f1192(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,1024);*r1p=r1;*r0p=r0;}
static void f1193(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,-1024);*r1p=r1;*r0p=r0;}
static void f1194(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,1023);*r1p=r1;*r0p=r0;}
static void f1195(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,-1025);*r1p=r1;*r0p=r0;}
static void f1196(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,2048);*r1p=r1;*r0p=r0;}
static void f1197(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,-2048);*r1p=r1;*r0p=r0;}
static void f1198(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,2047);*r1p=r1;*r0p=r0;}
static void f1199(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,-2049);*r1p=r1;*r0p=r0;}
static void f1200(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,4096);*r1p=r1;*r0p=r0;}
static void f1201(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,-4096);*r1p=r1;*r0p=r0;}
static void f1202(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,4095);*r1p=r1;*r0p=r0;}
static void f1203(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,-4097);*r1p=r1;*r0p=r0;}
static void f1204(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,8192);*r1p=r1;*r0p=r0;}
static void f1205(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,-8192);*r1p=r1;*r0p=r0;}
static void f1206(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,8191);*r1p=r1;*r0p=r0;}
static void f1207(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,-8193);*r1p=r1;*r0p=r0;}
static void f1208(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,16384);*r1p=r1;*r0p=r0;}
static void f1209(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,-16384);*r1p=r1;*r0p=r0;}
static void f1210(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,16383);*r1p=r1;*r0p=r0;}
static void f1211(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,-16385);*r1p=r1;*r0p=r0;}
static void f1212(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,32768);*r1p=r1;*r0p=r0;}
static void f1213(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,-32768);*r1p=r1;*r0p=r0;}
static void f1214(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,32767);*r1p=r1;*r0p=r0;}
static void f1215(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,15,0,-32769);*r1p=r1;*r0p=r0;}
static void f1216(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,1);*r1p=r1;*r0p=r0;}
static void f1217(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,-1);*r1p=r1;*r0p=r0;}
static void f1218(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,0);*r1p=r1;*r0p=r0;}
static void f1219(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,-2);*r1p=r1;*r0p=r0;}
static void f1220(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,2);*r1p=r1;*r0p=r0;}
static void f1221(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,-2);*r1p=r1;*r0p=r0;}
static void f1222(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,1);*r1p=r1;*r0p=r0;}
static void f1223(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,-3);*r1p=r1;*r0p=r0;}
static void f1224(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,4);*r1p=r1;*r0p=r0;}
static void f1225(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,-4);*r1p=r1;*r0p=r0;}
static void f1226(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,3);*r1p=r1;*r0p=r0;}
static void f1227(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,-5);*r1p=r1;*r0p=r0;}
static void f1228(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,8);*r1p=r1;*r0p=r0;}
static void f1229(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,-8);*r1p=r1;*r0p=r0;}
static void f1230(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,7);*r1p=r1;*r0p=r0;}
static void f1231(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,-9);*r1p=r1;*r0p=r0;}
static void f1232(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,16);*r1p=r1;*r0p=r0;}
static void f1233(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,-16);*r1p=r1;*r0p=r0;}
static void f1234(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,15);*r1p=r1;*r0p=r0;}
static void f1235(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,-17);*r1p=r1;*r0p=r0;}
static void f1236(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,32);*r1p=r1;*r0p=r0;}
static void f1237(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,-32);*r1p=r1;*r0p=r0;}
static void f1238(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,31);*r1p=r1;*r0p=r0;}
static void f1239(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,-33);*r1p=r1;*r0p=r0;}
static void f1240(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,64);*r1p=r1;*r0p=r0;}
static void f1241(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,-64);*r1p=r1;*r0p=r0;}
static void f1242(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,63);*r1p=r1;*r0p=r0;}
static void f1243(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,-65);*r1p=r1;*r0p=r0;}
static void f1244(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,128);*r1p=r1;*r0p=r0;}
static void f1245(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,-128);*r1p=r1;*r0p=r0;}
static void f1246(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,127);*r1p=r1;*r0p=r0;}
static void f1247(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,-129);*r1p=r1;*r0p=r0;}
static void f1248(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,256);*r1p=r1;*r0p=r0;}
static void f1249(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,-256);*r1p=r1;*r0p=r0;}
static void f1250(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,255);*r1p=r1;*r0p=r0;}
static void f1251(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,-257);*r1p=r1;*r0p=r0;}
static void f1252(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,512);*r1p=r1;*r0p=r0;}
static void f1253(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,-512);*r1p=r1;*r0p=r0;}
static void f1254(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,511);*r1p=r1;*r0p=r0;}
static void f1255(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,-513);*r1p=r1;*r0p=r0;}
static void f1256(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,1024);*r1p=r1;*r0p=r0;}
static void f1257(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,-1024);*r1p=r1;*r0p=r0;}
static void f1258(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,1023);*r1p=r1;*r0p=r0;}
static void f1259(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,-1025);*r1p=r1;*r0p=r0;}
static void f1260(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,2048);*r1p=r1;*r0p=r0;}
static void f1261(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,-2048);*r1p=r1;*r0p=r0;}
static void f1262(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,2047);*r1p=r1;*r0p=r0;}
static void f1263(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,-2049);*r1p=r1;*r0p=r0;}
static void f1264(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,4096);*r1p=r1;*r0p=r0;}
static void f1265(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,-4096);*r1p=r1;*r0p=r0;}
static void f1266(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,4095);*r1p=r1;*r0p=r0;}
static void f1267(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,-4097);*r1p=r1;*r0p=r0;}
static void f1268(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,8192);*r1p=r1;*r0p=r0;}
static void f1269(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,-8192);*r1p=r1;*r0p=r0;}
static void f1270(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,8191);*r1p=r1;*r0p=r0;}
static void f1271(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,-8193);*r1p=r1;*r0p=r0;}
static void f1272(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,16384);*r1p=r1;*r0p=r0;}
static void f1273(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,-16384);*r1p=r1;*r0p=r0;}
static void f1274(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,16383);*r1p=r1;*r0p=r0;}
static void f1275(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,-16385);*r1p=r1;*r0p=r0;}
static void f1276(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,32768);*r1p=r1;*r0p=r0;}
static void f1277(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,-32768);*r1p=r1;*r0p=r0;}
static void f1278(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,32767);*r1p=r1;*r0p=r0;}
static void f1279(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-17,0,-32769);*r1p=r1;*r0p=r0;}
static void f1280(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,1);*r1p=r1;*r0p=r0;}
static void f1281(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,-1);*r1p=r1;*r0p=r0;}
static void f1282(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,0);*r1p=r1;*r0p=r0;}
static void f1283(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,-2);*r1p=r1;*r0p=r0;}
static void f1284(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,2);*r1p=r1;*r0p=r0;}
static void f1285(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,-2);*r1p=r1;*r0p=r0;}
static void f1286(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,1);*r1p=r1;*r0p=r0;}
static void f1287(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,-3);*r1p=r1;*r0p=r0;}
static void f1288(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,4);*r1p=r1;*r0p=r0;}
static void f1289(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,-4);*r1p=r1;*r0p=r0;}
static void f1290(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,3);*r1p=r1;*r0p=r0;}
static void f1291(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,-5);*r1p=r1;*r0p=r0;}
static void f1292(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,8);*r1p=r1;*r0p=r0;}
static void f1293(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,-8);*r1p=r1;*r0p=r0;}
static void f1294(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,7);*r1p=r1;*r0p=r0;}
static void f1295(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,-9);*r1p=r1;*r0p=r0;}
static void f1296(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,16);*r1p=r1;*r0p=r0;}
static void f1297(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,-16);*r1p=r1;*r0p=r0;}
static void f1298(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,15);*r1p=r1;*r0p=r0;}
static void f1299(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,-17);*r1p=r1;*r0p=r0;}
static void f1300(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,32);*r1p=r1;*r0p=r0;}
static void f1301(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,-32);*r1p=r1;*r0p=r0;}
static void f1302(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,31);*r1p=r1;*r0p=r0;}
static void f1303(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,-33);*r1p=r1;*r0p=r0;}
static void f1304(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,64);*r1p=r1;*r0p=r0;}
static void f1305(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,-64);*r1p=r1;*r0p=r0;}
static void f1306(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,63);*r1p=r1;*r0p=r0;}
static void f1307(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,-65);*r1p=r1;*r0p=r0;}
static void f1308(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,128);*r1p=r1;*r0p=r0;}
static void f1309(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,-128);*r1p=r1;*r0p=r0;}
static void f1310(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,127);*r1p=r1;*r0p=r0;}
static void f1311(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,-129);*r1p=r1;*r0p=r0;}
static void f1312(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,256);*r1p=r1;*r0p=r0;}
static void f1313(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,-256);*r1p=r1;*r0p=r0;}
static void f1314(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,255);*r1p=r1;*r0p=r0;}
static void f1315(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,-257);*r1p=r1;*r0p=r0;}
static void f1316(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,512);*r1p=r1;*r0p=r0;}
static void f1317(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,-512);*r1p=r1;*r0p=r0;}
static void f1318(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,511);*r1p=r1;*r0p=r0;}
static void f1319(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,-513);*r1p=r1;*r0p=r0;}
static void f1320(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,1024);*r1p=r1;*r0p=r0;}
static void f1321(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,-1024);*r1p=r1;*r0p=r0;}
static void f1322(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,1023);*r1p=r1;*r0p=r0;}
static void f1323(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,-1025);*r1p=r1;*r0p=r0;}
static void f1324(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,2048);*r1p=r1;*r0p=r0;}
static void f1325(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,-2048);*r1p=r1;*r0p=r0;}
static void f1326(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,2047);*r1p=r1;*r0p=r0;}
static void f1327(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,-2049);*r1p=r1;*r0p=r0;}
static void f1328(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,4096);*r1p=r1;*r0p=r0;}
static void f1329(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,-4096);*r1p=r1;*r0p=r0;}
static void f1330(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,4095);*r1p=r1;*r0p=r0;}
static void f1331(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,-4097);*r1p=r1;*r0p=r0;}
static void f1332(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,8192);*r1p=r1;*r0p=r0;}
static void f1333(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,-8192);*r1p=r1;*r0p=r0;}
static void f1334(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,8191);*r1p=r1;*r0p=r0;}
static void f1335(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,-8193);*r1p=r1;*r0p=r0;}
static void f1336(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,16384);*r1p=r1;*r0p=r0;}
static void f1337(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,-16384);*r1p=r1;*r0p=r0;}
static void f1338(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,16383);*r1p=r1;*r0p=r0;}
static void f1339(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,-16385);*r1p=r1;*r0p=r0;}
static void f1340(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,32768);*r1p=r1;*r0p=r0;}
static void f1341(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,-32768);*r1p=r1;*r0p=r0;}
static void f1342(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,32767);*r1p=r1;*r0p=r0;}
static void f1343(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32,0,-32769);*r1p=r1;*r0p=r0;}
static void f1344(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,1);*r1p=r1;*r0p=r0;}
static void f1345(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,-1);*r1p=r1;*r0p=r0;}
static void f1346(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,0);*r1p=r1;*r0p=r0;}
static void f1347(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,-2);*r1p=r1;*r0p=r0;}
static void f1348(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,2);*r1p=r1;*r0p=r0;}
static void f1349(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,-2);*r1p=r1;*r0p=r0;}
static void f1350(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,1);*r1p=r1;*r0p=r0;}
static void f1351(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,-3);*r1p=r1;*r0p=r0;}
static void f1352(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,4);*r1p=r1;*r0p=r0;}
static void f1353(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,-4);*r1p=r1;*r0p=r0;}
static void f1354(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,3);*r1p=r1;*r0p=r0;}
static void f1355(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,-5);*r1p=r1;*r0p=r0;}
static void f1356(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,8);*r1p=r1;*r0p=r0;}
static void f1357(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,-8);*r1p=r1;*r0p=r0;}
static void f1358(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,7);*r1p=r1;*r0p=r0;}
static void f1359(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,-9);*r1p=r1;*r0p=r0;}
static void f1360(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,16);*r1p=r1;*r0p=r0;}
static void f1361(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,-16);*r1p=r1;*r0p=r0;}
static void f1362(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,15);*r1p=r1;*r0p=r0;}
static void f1363(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,-17);*r1p=r1;*r0p=r0;}
static void f1364(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,32);*r1p=r1;*r0p=r0;}
static void f1365(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,-32);*r1p=r1;*r0p=r0;}
static void f1366(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,31);*r1p=r1;*r0p=r0;}
static void f1367(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,-33);*r1p=r1;*r0p=r0;}
static void f1368(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,64);*r1p=r1;*r0p=r0;}
static void f1369(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,-64);*r1p=r1;*r0p=r0;}
static void f1370(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,63);*r1p=r1;*r0p=r0;}
static void f1371(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,-65);*r1p=r1;*r0p=r0;}
static void f1372(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,128);*r1p=r1;*r0p=r0;}
static void f1373(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,-128);*r1p=r1;*r0p=r0;}
static void f1374(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,127);*r1p=r1;*r0p=r0;}
static void f1375(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,-129);*r1p=r1;*r0p=r0;}
static void f1376(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,256);*r1p=r1;*r0p=r0;}
static void f1377(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,-256);*r1p=r1;*r0p=r0;}
static void f1378(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,255);*r1p=r1;*r0p=r0;}
static void f1379(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,-257);*r1p=r1;*r0p=r0;}
static void f1380(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,512);*r1p=r1;*r0p=r0;}
static void f1381(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,-512);*r1p=r1;*r0p=r0;}
static void f1382(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,511);*r1p=r1;*r0p=r0;}
static void f1383(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,-513);*r1p=r1;*r0p=r0;}
static void f1384(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,1024);*r1p=r1;*r0p=r0;}
static void f1385(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,-1024);*r1p=r1;*r0p=r0;}
static void f1386(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,1023);*r1p=r1;*r0p=r0;}
static void f1387(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,-1025);*r1p=r1;*r0p=r0;}
static void f1388(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,2048);*r1p=r1;*r0p=r0;}
static void f1389(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,-2048);*r1p=r1;*r0p=r0;}
static void f1390(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,2047);*r1p=r1;*r0p=r0;}
static void f1391(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,-2049);*r1p=r1;*r0p=r0;}
static void f1392(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,4096);*r1p=r1;*r0p=r0;}
static void f1393(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,-4096);*r1p=r1;*r0p=r0;}
static void f1394(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,4095);*r1p=r1;*r0p=r0;}
static void f1395(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,-4097);*r1p=r1;*r0p=r0;}
static void f1396(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,8192);*r1p=r1;*r0p=r0;}
static void f1397(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,-8192);*r1p=r1;*r0p=r0;}
static void f1398(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,8191);*r1p=r1;*r0p=r0;}
static void f1399(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,-8193);*r1p=r1;*r0p=r0;}
static void f1400(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,16384);*r1p=r1;*r0p=r0;}
static void f1401(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,-16384);*r1p=r1;*r0p=r0;}
static void f1402(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,16383);*r1p=r1;*r0p=r0;}
static void f1403(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,-16385);*r1p=r1;*r0p=r0;}
static void f1404(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,32768);*r1p=r1;*r0p=r0;}
static void f1405(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,-32768);*r1p=r1;*r0p=r0;}
static void f1406(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,32767);*r1p=r1;*r0p=r0;}
static void f1407(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32,0,-32769);*r1p=r1;*r0p=r0;}
static void f1408(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,1);*r1p=r1;*r0p=r0;}
static void f1409(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,-1);*r1p=r1;*r0p=r0;}
static void f1410(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,0);*r1p=r1;*r0p=r0;}
static void f1411(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,-2);*r1p=r1;*r0p=r0;}
static void f1412(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,2);*r1p=r1;*r0p=r0;}
static void f1413(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,-2);*r1p=r1;*r0p=r0;}
static void f1414(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,1);*r1p=r1;*r0p=r0;}
static void f1415(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,-3);*r1p=r1;*r0p=r0;}
static void f1416(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,4);*r1p=r1;*r0p=r0;}
static void f1417(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,-4);*r1p=r1;*r0p=r0;}
static void f1418(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,3);*r1p=r1;*r0p=r0;}
static void f1419(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,-5);*r1p=r1;*r0p=r0;}
static void f1420(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,8);*r1p=r1;*r0p=r0;}
static void f1421(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,-8);*r1p=r1;*r0p=r0;}
static void f1422(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,7);*r1p=r1;*r0p=r0;}
static void f1423(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,-9);*r1p=r1;*r0p=r0;}
static void f1424(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,16);*r1p=r1;*r0p=r0;}
static void f1425(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,-16);*r1p=r1;*r0p=r0;}
static void f1426(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,15);*r1p=r1;*r0p=r0;}
static void f1427(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,-17);*r1p=r1;*r0p=r0;}
static void f1428(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,32);*r1p=r1;*r0p=r0;}
static void f1429(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,-32);*r1p=r1;*r0p=r0;}
static void f1430(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,31);*r1p=r1;*r0p=r0;}
static void f1431(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,-33);*r1p=r1;*r0p=r0;}
static void f1432(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,64);*r1p=r1;*r0p=r0;}
static void f1433(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,-64);*r1p=r1;*r0p=r0;}
static void f1434(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,63);*r1p=r1;*r0p=r0;}
static void f1435(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,-65);*r1p=r1;*r0p=r0;}
static void f1436(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,128);*r1p=r1;*r0p=r0;}
static void f1437(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,-128);*r1p=r1;*r0p=r0;}
static void f1438(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,127);*r1p=r1;*r0p=r0;}
static void f1439(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,-129);*r1p=r1;*r0p=r0;}
static void f1440(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,256);*r1p=r1;*r0p=r0;}
static void f1441(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,-256);*r1p=r1;*r0p=r0;}
static void f1442(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,255);*r1p=r1;*r0p=r0;}
static void f1443(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,-257);*r1p=r1;*r0p=r0;}
static void f1444(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,512);*r1p=r1;*r0p=r0;}
static void f1445(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,-512);*r1p=r1;*r0p=r0;}
static void f1446(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,511);*r1p=r1;*r0p=r0;}
static void f1447(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,-513);*r1p=r1;*r0p=r0;}
static void f1448(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,1024);*r1p=r1;*r0p=r0;}
static void f1449(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,-1024);*r1p=r1;*r0p=r0;}
static void f1450(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,1023);*r1p=r1;*r0p=r0;}
static void f1451(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,-1025);*r1p=r1;*r0p=r0;}
static void f1452(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,2048);*r1p=r1;*r0p=r0;}
static void f1453(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,-2048);*r1p=r1;*r0p=r0;}
static void f1454(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,2047);*r1p=r1;*r0p=r0;}
static void f1455(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,-2049);*r1p=r1;*r0p=r0;}
static void f1456(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,4096);*r1p=r1;*r0p=r0;}
static void f1457(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,-4096);*r1p=r1;*r0p=r0;}
static void f1458(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,4095);*r1p=r1;*r0p=r0;}
static void f1459(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,-4097);*r1p=r1;*r0p=r0;}
static void f1460(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,8192);*r1p=r1;*r0p=r0;}
static void f1461(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,-8192);*r1p=r1;*r0p=r0;}
static void f1462(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,8191);*r1p=r1;*r0p=r0;}
static void f1463(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,-8193);*r1p=r1;*r0p=r0;}
static void f1464(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,16384);*r1p=r1;*r0p=r0;}
static void f1465(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,-16384);*r1p=r1;*r0p=r0;}
static void f1466(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,16383);*r1p=r1;*r0p=r0;}
static void f1467(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,-16385);*r1p=r1;*r0p=r0;}
static void f1468(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,32768);*r1p=r1;*r0p=r0;}
static void f1469(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,-32768);*r1p=r1;*r0p=r0;}
static void f1470(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,32767);*r1p=r1;*r0p=r0;}
static void f1471(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,31,0,-32769);*r1p=r1;*r0p=r0;}
static void f1472(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,1);*r1p=r1;*r0p=r0;}
static void f1473(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,-1);*r1p=r1;*r0p=r0;}
static void f1474(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,0);*r1p=r1;*r0p=r0;}
static void f1475(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,-2);*r1p=r1;*r0p=r0;}
static void f1476(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,2);*r1p=r1;*r0p=r0;}
static void f1477(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,-2);*r1p=r1;*r0p=r0;}
static void f1478(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,1);*r1p=r1;*r0p=r0;}
static void f1479(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,-3);*r1p=r1;*r0p=r0;}
static void f1480(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,4);*r1p=r1;*r0p=r0;}
static void f1481(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,-4);*r1p=r1;*r0p=r0;}
static void f1482(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,3);*r1p=r1;*r0p=r0;}
static void f1483(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,-5);*r1p=r1;*r0p=r0;}
static void f1484(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,8);*r1p=r1;*r0p=r0;}
static void f1485(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,-8);*r1p=r1;*r0p=r0;}
static void f1486(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,7);*r1p=r1;*r0p=r0;}
static void f1487(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,-9);*r1p=r1;*r0p=r0;}
static void f1488(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,16);*r1p=r1;*r0p=r0;}
static void f1489(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,-16);*r1p=r1;*r0p=r0;}
static void f1490(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,15);*r1p=r1;*r0p=r0;}
static void f1491(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,-17);*r1p=r1;*r0p=r0;}
static void f1492(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,32);*r1p=r1;*r0p=r0;}
static void f1493(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,-32);*r1p=r1;*r0p=r0;}
static void f1494(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,31);*r1p=r1;*r0p=r0;}
static void f1495(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,-33);*r1p=r1;*r0p=r0;}
static void f1496(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,64);*r1p=r1;*r0p=r0;}
static void f1497(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,-64);*r1p=r1;*r0p=r0;}
static void f1498(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,63);*r1p=r1;*r0p=r0;}
static void f1499(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,-65);*r1p=r1;*r0p=r0;}
static void f1500(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,128);*r1p=r1;*r0p=r0;}
static void f1501(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,-128);*r1p=r1;*r0p=r0;}
static void f1502(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,127);*r1p=r1;*r0p=r0;}
static void f1503(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,-129);*r1p=r1;*r0p=r0;}
static void f1504(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,256);*r1p=r1;*r0p=r0;}
static void f1505(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,-256);*r1p=r1;*r0p=r0;}
static void f1506(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,255);*r1p=r1;*r0p=r0;}
static void f1507(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,-257);*r1p=r1;*r0p=r0;}
static void f1508(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,512);*r1p=r1;*r0p=r0;}
static void f1509(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,-512);*r1p=r1;*r0p=r0;}
static void f1510(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,511);*r1p=r1;*r0p=r0;}
static void f1511(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,-513);*r1p=r1;*r0p=r0;}
static void f1512(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,1024);*r1p=r1;*r0p=r0;}
static void f1513(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,-1024);*r1p=r1;*r0p=r0;}
static void f1514(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,1023);*r1p=r1;*r0p=r0;}
static void f1515(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,-1025);*r1p=r1;*r0p=r0;}
static void f1516(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,2048);*r1p=r1;*r0p=r0;}
static void f1517(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,-2048);*r1p=r1;*r0p=r0;}
static void f1518(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,2047);*r1p=r1;*r0p=r0;}
static void f1519(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,-2049);*r1p=r1;*r0p=r0;}
static void f1520(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,4096);*r1p=r1;*r0p=r0;}
static void f1521(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,-4096);*r1p=r1;*r0p=r0;}
static void f1522(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,4095);*r1p=r1;*r0p=r0;}
static void f1523(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,-4097);*r1p=r1;*r0p=r0;}
static void f1524(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,8192);*r1p=r1;*r0p=r0;}
static void f1525(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,-8192);*r1p=r1;*r0p=r0;}
static void f1526(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,8191);*r1p=r1;*r0p=r0;}
static void f1527(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,-8193);*r1p=r1;*r0p=r0;}
static void f1528(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,16384);*r1p=r1;*r0p=r0;}
static void f1529(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,-16384);*r1p=r1;*r0p=r0;}
static void f1530(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,16383);*r1p=r1;*r0p=r0;}
static void f1531(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,-16385);*r1p=r1;*r0p=r0;}
static void f1532(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,32768);*r1p=r1;*r0p=r0;}
static void f1533(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,-32768);*r1p=r1;*r0p=r0;}
static void f1534(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,32767);*r1p=r1;*r0p=r0;}
static void f1535(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-33,0,-32769);*r1p=r1;*r0p=r0;}
static void f1536(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,1);*r1p=r1;*r0p=r0;}
static void f1537(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,-1);*r1p=r1;*r0p=r0;}
static void f1538(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,0);*r1p=r1;*r0p=r0;}
static void f1539(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,-2);*r1p=r1;*r0p=r0;}
static void f1540(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,2);*r1p=r1;*r0p=r0;}
static void f1541(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,-2);*r1p=r1;*r0p=r0;}
static void f1542(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,1);*r1p=r1;*r0p=r0;}
static void f1543(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,-3);*r1p=r1;*r0p=r0;}
static void f1544(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,4);*r1p=r1;*r0p=r0;}
static void f1545(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,-4);*r1p=r1;*r0p=r0;}
static void f1546(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,3);*r1p=r1;*r0p=r0;}
static void f1547(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,-5);*r1p=r1;*r0p=r0;}
static void f1548(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,8);*r1p=r1;*r0p=r0;}
static void f1549(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,-8);*r1p=r1;*r0p=r0;}
static void f1550(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,7);*r1p=r1;*r0p=r0;}
static void f1551(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,-9);*r1p=r1;*r0p=r0;}
static void f1552(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,16);*r1p=r1;*r0p=r0;}
static void f1553(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,-16);*r1p=r1;*r0p=r0;}
static void f1554(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,15);*r1p=r1;*r0p=r0;}
static void f1555(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,-17);*r1p=r1;*r0p=r0;}
static void f1556(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,32);*r1p=r1;*r0p=r0;}
static void f1557(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,-32);*r1p=r1;*r0p=r0;}
static void f1558(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,31);*r1p=r1;*r0p=r0;}
static void f1559(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,-33);*r1p=r1;*r0p=r0;}
static void f1560(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,64);*r1p=r1;*r0p=r0;}
static void f1561(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,-64);*r1p=r1;*r0p=r0;}
static void f1562(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,63);*r1p=r1;*r0p=r0;}
static void f1563(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,-65);*r1p=r1;*r0p=r0;}
static void f1564(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,128);*r1p=r1;*r0p=r0;}
static void f1565(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,-128);*r1p=r1;*r0p=r0;}
static void f1566(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,127);*r1p=r1;*r0p=r0;}
static void f1567(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,-129);*r1p=r1;*r0p=r0;}
static void f1568(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,256);*r1p=r1;*r0p=r0;}
static void f1569(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,-256);*r1p=r1;*r0p=r0;}
static void f1570(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,255);*r1p=r1;*r0p=r0;}
static void f1571(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,-257);*r1p=r1;*r0p=r0;}
static void f1572(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,512);*r1p=r1;*r0p=r0;}
static void f1573(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,-512);*r1p=r1;*r0p=r0;}
static void f1574(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,511);*r1p=r1;*r0p=r0;}
static void f1575(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,-513);*r1p=r1;*r0p=r0;}
static void f1576(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,1024);*r1p=r1;*r0p=r0;}
static void f1577(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,-1024);*r1p=r1;*r0p=r0;}
static void f1578(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,1023);*r1p=r1;*r0p=r0;}
static void f1579(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,-1025);*r1p=r1;*r0p=r0;}
static void f1580(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,2048);*r1p=r1;*r0p=r0;}
static void f1581(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,-2048);*r1p=r1;*r0p=r0;}
static void f1582(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,2047);*r1p=r1;*r0p=r0;}
static void f1583(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,-2049);*r1p=r1;*r0p=r0;}
static void f1584(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,4096);*r1p=r1;*r0p=r0;}
static void f1585(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,-4096);*r1p=r1;*r0p=r0;}
static void f1586(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,4095);*r1p=r1;*r0p=r0;}
static void f1587(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,-4097);*r1p=r1;*r0p=r0;}
static void f1588(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,8192);*r1p=r1;*r0p=r0;}
static void f1589(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,-8192);*r1p=r1;*r0p=r0;}
static void f1590(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,8191);*r1p=r1;*r0p=r0;}
static void f1591(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,-8193);*r1p=r1;*r0p=r0;}
static void f1592(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,16384);*r1p=r1;*r0p=r0;}
static void f1593(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,-16384);*r1p=r1;*r0p=r0;}
static void f1594(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,16383);*r1p=r1;*r0p=r0;}
static void f1595(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,-16385);*r1p=r1;*r0p=r0;}
static void f1596(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,32768);*r1p=r1;*r0p=r0;}
static void f1597(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,-32768);*r1p=r1;*r0p=r0;}
static void f1598(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,32767);*r1p=r1;*r0p=r0;}
static void f1599(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,64,0,-32769);*r1p=r1;*r0p=r0;}
static void f1600(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,1);*r1p=r1;*r0p=r0;}
static void f1601(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,-1);*r1p=r1;*r0p=r0;}
static void f1602(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,0);*r1p=r1;*r0p=r0;}
static void f1603(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,-2);*r1p=r1;*r0p=r0;}
static void f1604(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,2);*r1p=r1;*r0p=r0;}
static void f1605(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,-2);*r1p=r1;*r0p=r0;}
static void f1606(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,1);*r1p=r1;*r0p=r0;}
static void f1607(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,-3);*r1p=r1;*r0p=r0;}
static void f1608(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,4);*r1p=r1;*r0p=r0;}
static void f1609(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,-4);*r1p=r1;*r0p=r0;}
static void f1610(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,3);*r1p=r1;*r0p=r0;}
static void f1611(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,-5);*r1p=r1;*r0p=r0;}
static void f1612(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,8);*r1p=r1;*r0p=r0;}
static void f1613(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,-8);*r1p=r1;*r0p=r0;}
static void f1614(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,7);*r1p=r1;*r0p=r0;}
static void f1615(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,-9);*r1p=r1;*r0p=r0;}
static void f1616(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,16);*r1p=r1;*r0p=r0;}
static void f1617(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,-16);*r1p=r1;*r0p=r0;}
static void f1618(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,15);*r1p=r1;*r0p=r0;}
static void f1619(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,-17);*r1p=r1;*r0p=r0;}
static void f1620(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,32);*r1p=r1;*r0p=r0;}
static void f1621(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,-32);*r1p=r1;*r0p=r0;}
static void f1622(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,31);*r1p=r1;*r0p=r0;}
static void f1623(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,-33);*r1p=r1;*r0p=r0;}
static void f1624(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,64);*r1p=r1;*r0p=r0;}
static void f1625(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,-64);*r1p=r1;*r0p=r0;}
static void f1626(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,63);*r1p=r1;*r0p=r0;}
static void f1627(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,-65);*r1p=r1;*r0p=r0;}
static void f1628(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,128);*r1p=r1;*r0p=r0;}
static void f1629(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,-128);*r1p=r1;*r0p=r0;}
static void f1630(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,127);*r1p=r1;*r0p=r0;}
static void f1631(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,-129);*r1p=r1;*r0p=r0;}
static void f1632(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,256);*r1p=r1;*r0p=r0;}
static void f1633(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,-256);*r1p=r1;*r0p=r0;}
static void f1634(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,255);*r1p=r1;*r0p=r0;}
static void f1635(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,-257);*r1p=r1;*r0p=r0;}
static void f1636(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,512);*r1p=r1;*r0p=r0;}
static void f1637(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,-512);*r1p=r1;*r0p=r0;}
static void f1638(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,511);*r1p=r1;*r0p=r0;}
static void f1639(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,-513);*r1p=r1;*r0p=r0;}
static void f1640(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,1024);*r1p=r1;*r0p=r0;}
static void f1641(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,-1024);*r1p=r1;*r0p=r0;}
static void f1642(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,1023);*r1p=r1;*r0p=r0;}
static void f1643(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,-1025);*r1p=r1;*r0p=r0;}
static void f1644(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,2048);*r1p=r1;*r0p=r0;}
static void f1645(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,-2048);*r1p=r1;*r0p=r0;}
static void f1646(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,2047);*r1p=r1;*r0p=r0;}
static void f1647(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,-2049);*r1p=r1;*r0p=r0;}
static void f1648(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,4096);*r1p=r1;*r0p=r0;}
static void f1649(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,-4096);*r1p=r1;*r0p=r0;}
static void f1650(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,4095);*r1p=r1;*r0p=r0;}
static void f1651(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,-4097);*r1p=r1;*r0p=r0;}
static void f1652(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,8192);*r1p=r1;*r0p=r0;}
static void f1653(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,-8192);*r1p=r1;*r0p=r0;}
static void f1654(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,8191);*r1p=r1;*r0p=r0;}
static void f1655(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,-8193);*r1p=r1;*r0p=r0;}
static void f1656(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,16384);*r1p=r1;*r0p=r0;}
static void f1657(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,-16384);*r1p=r1;*r0p=r0;}
static void f1658(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,16383);*r1p=r1;*r0p=r0;}
static void f1659(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,-16385);*r1p=r1;*r0p=r0;}
static void f1660(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,32768);*r1p=r1;*r0p=r0;}
static void f1661(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,-32768);*r1p=r1;*r0p=r0;}
static void f1662(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,32767);*r1p=r1;*r0p=r0;}
static void f1663(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-64,0,-32769);*r1p=r1;*r0p=r0;}
static void f1664(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,1);*r1p=r1;*r0p=r0;}
static void f1665(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,-1);*r1p=r1;*r0p=r0;}
static void f1666(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,0);*r1p=r1;*r0p=r0;}
static void f1667(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,-2);*r1p=r1;*r0p=r0;}
static void f1668(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,2);*r1p=r1;*r0p=r0;}
static void f1669(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,-2);*r1p=r1;*r0p=r0;}
static void f1670(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,1);*r1p=r1;*r0p=r0;}
static void f1671(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,-3);*r1p=r1;*r0p=r0;}
static void f1672(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,4);*r1p=r1;*r0p=r0;}
static void f1673(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,-4);*r1p=r1;*r0p=r0;}
static void f1674(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,3);*r1p=r1;*r0p=r0;}
static void f1675(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,-5);*r1p=r1;*r0p=r0;}
static void f1676(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,8);*r1p=r1;*r0p=r0;}
static void f1677(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,-8);*r1p=r1;*r0p=r0;}
static void f1678(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,7);*r1p=r1;*r0p=r0;}
static void f1679(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,-9);*r1p=r1;*r0p=r0;}
static void f1680(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,16);*r1p=r1;*r0p=r0;}
static void f1681(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,-16);*r1p=r1;*r0p=r0;}
static void f1682(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,15);*r1p=r1;*r0p=r0;}
static void f1683(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,-17);*r1p=r1;*r0p=r0;}
static void f1684(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,32);*r1p=r1;*r0p=r0;}
static void f1685(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,-32);*r1p=r1;*r0p=r0;}
static void f1686(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,31);*r1p=r1;*r0p=r0;}
static void f1687(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,-33);*r1p=r1;*r0p=r0;}
static void f1688(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,64);*r1p=r1;*r0p=r0;}
static void f1689(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,-64);*r1p=r1;*r0p=r0;}
static void f1690(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,63);*r1p=r1;*r0p=r0;}
static void f1691(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,-65);*r1p=r1;*r0p=r0;}
static void f1692(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,128);*r1p=r1;*r0p=r0;}
static void f1693(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,-128);*r1p=r1;*r0p=r0;}
static void f1694(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,127);*r1p=r1;*r0p=r0;}
static void f1695(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,-129);*r1p=r1;*r0p=r0;}
static void f1696(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,256);*r1p=r1;*r0p=r0;}
static void f1697(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,-256);*r1p=r1;*r0p=r0;}
static void f1698(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,255);*r1p=r1;*r0p=r0;}
static void f1699(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,-257);*r1p=r1;*r0p=r0;}
static void f1700(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,512);*r1p=r1;*r0p=r0;}
static void f1701(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,-512);*r1p=r1;*r0p=r0;}
static void f1702(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,511);*r1p=r1;*r0p=r0;}
static void f1703(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,-513);*r1p=r1;*r0p=r0;}
static void f1704(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,1024);*r1p=r1;*r0p=r0;}
static void f1705(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,-1024);*r1p=r1;*r0p=r0;}
static void f1706(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,1023);*r1p=r1;*r0p=r0;}
static void f1707(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,-1025);*r1p=r1;*r0p=r0;}
static void f1708(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,2048);*r1p=r1;*r0p=r0;}
static void f1709(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,-2048);*r1p=r1;*r0p=r0;}
static void f1710(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,2047);*r1p=r1;*r0p=r0;}
static void f1711(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,-2049);*r1p=r1;*r0p=r0;}
static void f1712(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,4096);*r1p=r1;*r0p=r0;}
static void f1713(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,-4096);*r1p=r1;*r0p=r0;}
static void f1714(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,4095);*r1p=r1;*r0p=r0;}
static void f1715(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,-4097);*r1p=r1;*r0p=r0;}
static void f1716(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,8192);*r1p=r1;*r0p=r0;}
static void f1717(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,-8192);*r1p=r1;*r0p=r0;}
static void f1718(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,8191);*r1p=r1;*r0p=r0;}
static void f1719(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,-8193);*r1p=r1;*r0p=r0;}
static void f1720(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,16384);*r1p=r1;*r0p=r0;}
static void f1721(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,-16384);*r1p=r1;*r0p=r0;}
static void f1722(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,16383);*r1p=r1;*r0p=r0;}
static void f1723(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,-16385);*r1p=r1;*r0p=r0;}
static void f1724(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,32768);*r1p=r1;*r0p=r0;}
static void f1725(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,-32768);*r1p=r1;*r0p=r0;}
static void f1726(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,32767);*r1p=r1;*r0p=r0;}
static void f1727(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,63,0,-32769);*r1p=r1;*r0p=r0;}
static void f1728(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,1);*r1p=r1;*r0p=r0;}
static void f1729(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,-1);*r1p=r1;*r0p=r0;}
static void f1730(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,0);*r1p=r1;*r0p=r0;}
static void f1731(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,-2);*r1p=r1;*r0p=r0;}
static void f1732(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,2);*r1p=r1;*r0p=r0;}
static void f1733(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,-2);*r1p=r1;*r0p=r0;}
static void f1734(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,1);*r1p=r1;*r0p=r0;}
static void f1735(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,-3);*r1p=r1;*r0p=r0;}
static void f1736(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,4);*r1p=r1;*r0p=r0;}
static void f1737(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,-4);*r1p=r1;*r0p=r0;}
static void f1738(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,3);*r1p=r1;*r0p=r0;}
static void f1739(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,-5);*r1p=r1;*r0p=r0;}
static void f1740(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,8);*r1p=r1;*r0p=r0;}
static void f1741(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,-8);*r1p=r1;*r0p=r0;}
static void f1742(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,7);*r1p=r1;*r0p=r0;}
static void f1743(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,-9);*r1p=r1;*r0p=r0;}
static void f1744(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,16);*r1p=r1;*r0p=r0;}
static void f1745(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,-16);*r1p=r1;*r0p=r0;}
static void f1746(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,15);*r1p=r1;*r0p=r0;}
static void f1747(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,-17);*r1p=r1;*r0p=r0;}
static void f1748(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,32);*r1p=r1;*r0p=r0;}
static void f1749(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,-32);*r1p=r1;*r0p=r0;}
static void f1750(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,31);*r1p=r1;*r0p=r0;}
static void f1751(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,-33);*r1p=r1;*r0p=r0;}
static void f1752(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,64);*r1p=r1;*r0p=r0;}
static void f1753(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,-64);*r1p=r1;*r0p=r0;}
static void f1754(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,63);*r1p=r1;*r0p=r0;}
static void f1755(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,-65);*r1p=r1;*r0p=r0;}
static void f1756(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,128);*r1p=r1;*r0p=r0;}
static void f1757(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,-128);*r1p=r1;*r0p=r0;}
static void f1758(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,127);*r1p=r1;*r0p=r0;}
static void f1759(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,-129);*r1p=r1;*r0p=r0;}
static void f1760(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,256);*r1p=r1;*r0p=r0;}
static void f1761(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,-256);*r1p=r1;*r0p=r0;}
static void f1762(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,255);*r1p=r1;*r0p=r0;}
static void f1763(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,-257);*r1p=r1;*r0p=r0;}
static void f1764(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,512);*r1p=r1;*r0p=r0;}
static void f1765(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,-512);*r1p=r1;*r0p=r0;}
static void f1766(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,511);*r1p=r1;*r0p=r0;}
static void f1767(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,-513);*r1p=r1;*r0p=r0;}
static void f1768(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,1024);*r1p=r1;*r0p=r0;}
static void f1769(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,-1024);*r1p=r1;*r0p=r0;}
static void f1770(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,1023);*r1p=r1;*r0p=r0;}
static void f1771(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,-1025);*r1p=r1;*r0p=r0;}
static void f1772(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,2048);*r1p=r1;*r0p=r0;}
static void f1773(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,-2048);*r1p=r1;*r0p=r0;}
static void f1774(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,2047);*r1p=r1;*r0p=r0;}
static void f1775(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,-2049);*r1p=r1;*r0p=r0;}
static void f1776(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,4096);*r1p=r1;*r0p=r0;}
static void f1777(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,-4096);*r1p=r1;*r0p=r0;}
static void f1778(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,4095);*r1p=r1;*r0p=r0;}
static void f1779(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,-4097);*r1p=r1;*r0p=r0;}
static void f1780(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,8192);*r1p=r1;*r0p=r0;}
static void f1781(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,-8192);*r1p=r1;*r0p=r0;}
static void f1782(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,8191);*r1p=r1;*r0p=r0;}
static void f1783(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,-8193);*r1p=r1;*r0p=r0;}
static void f1784(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,16384);*r1p=r1;*r0p=r0;}
static void f1785(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,-16384);*r1p=r1;*r0p=r0;}
static void f1786(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,16383);*r1p=r1;*r0p=r0;}
static void f1787(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,-16385);*r1p=r1;*r0p=r0;}
static void f1788(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,32768);*r1p=r1;*r0p=r0;}
static void f1789(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,-32768);*r1p=r1;*r0p=r0;}
static void f1790(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,32767);*r1p=r1;*r0p=r0;}
static void f1791(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-65,0,-32769);*r1p=r1;*r0p=r0;}
static void f1792(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,1);*r1p=r1;*r0p=r0;}
static void f1793(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,-1);*r1p=r1;*r0p=r0;}
static void f1794(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,0);*r1p=r1;*r0p=r0;}
static void f1795(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,-2);*r1p=r1;*r0p=r0;}
static void f1796(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,2);*r1p=r1;*r0p=r0;}
static void f1797(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,-2);*r1p=r1;*r0p=r0;}
static void f1798(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,1);*r1p=r1;*r0p=r0;}
static void f1799(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,-3);*r1p=r1;*r0p=r0;}
static void f1800(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,4);*r1p=r1;*r0p=r0;}
static void f1801(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,-4);*r1p=r1;*r0p=r0;}
static void f1802(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,3);*r1p=r1;*r0p=r0;}
static void f1803(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,-5);*r1p=r1;*r0p=r0;}
static void f1804(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,8);*r1p=r1;*r0p=r0;}
static void f1805(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,-8);*r1p=r1;*r0p=r0;}
static void f1806(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,7);*r1p=r1;*r0p=r0;}
static void f1807(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,-9);*r1p=r1;*r0p=r0;}
static void f1808(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,16);*r1p=r1;*r0p=r0;}
static void f1809(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,-16);*r1p=r1;*r0p=r0;}
static void f1810(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,15);*r1p=r1;*r0p=r0;}
static void f1811(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,-17);*r1p=r1;*r0p=r0;}
static void f1812(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,32);*r1p=r1;*r0p=r0;}
static void f1813(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,-32);*r1p=r1;*r0p=r0;}
static void f1814(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,31);*r1p=r1;*r0p=r0;}
static void f1815(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,-33);*r1p=r1;*r0p=r0;}
static void f1816(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,64);*r1p=r1;*r0p=r0;}
static void f1817(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,-64);*r1p=r1;*r0p=r0;}
static void f1818(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,63);*r1p=r1;*r0p=r0;}
static void f1819(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,-65);*r1p=r1;*r0p=r0;}
static void f1820(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,128);*r1p=r1;*r0p=r0;}
static void f1821(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,-128);*r1p=r1;*r0p=r0;}
static void f1822(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,127);*r1p=r1;*r0p=r0;}
static void f1823(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,-129);*r1p=r1;*r0p=r0;}
static void f1824(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,256);*r1p=r1;*r0p=r0;}
static void f1825(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,-256);*r1p=r1;*r0p=r0;}
static void f1826(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,255);*r1p=r1;*r0p=r0;}
static void f1827(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,-257);*r1p=r1;*r0p=r0;}
static void f1828(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,512);*r1p=r1;*r0p=r0;}
static void f1829(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,-512);*r1p=r1;*r0p=r0;}
static void f1830(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,511);*r1p=r1;*r0p=r0;}
static void f1831(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,-513);*r1p=r1;*r0p=r0;}
static void f1832(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,1024);*r1p=r1;*r0p=r0;}
static void f1833(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,-1024);*r1p=r1;*r0p=r0;}
static void f1834(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,1023);*r1p=r1;*r0p=r0;}
static void f1835(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,-1025);*r1p=r1;*r0p=r0;}
static void f1836(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,2048);*r1p=r1;*r0p=r0;}
static void f1837(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,-2048);*r1p=r1;*r0p=r0;}
static void f1838(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,2047);*r1p=r1;*r0p=r0;}
static void f1839(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,-2049);*r1p=r1;*r0p=r0;}
static void f1840(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,4096);*r1p=r1;*r0p=r0;}
static void f1841(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,-4096);*r1p=r1;*r0p=r0;}
static void f1842(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,4095);*r1p=r1;*r0p=r0;}
static void f1843(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,-4097);*r1p=r1;*r0p=r0;}
static void f1844(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,8192);*r1p=r1;*r0p=r0;}
static void f1845(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,-8192);*r1p=r1;*r0p=r0;}
static void f1846(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,8191);*r1p=r1;*r0p=r0;}
static void f1847(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,-8193);*r1p=r1;*r0p=r0;}
static void f1848(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,16384);*r1p=r1;*r0p=r0;}
static void f1849(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,-16384);*r1p=r1;*r0p=r0;}
static void f1850(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,16383);*r1p=r1;*r0p=r0;}
static void f1851(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,-16385);*r1p=r1;*r0p=r0;}
static void f1852(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,32768);*r1p=r1;*r0p=r0;}
static void f1853(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,-32768);*r1p=r1;*r0p=r0;}
static void f1854(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,32767);*r1p=r1;*r0p=r0;}
static void f1855(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,128,0,-32769);*r1p=r1;*r0p=r0;}
static void f1856(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,1);*r1p=r1;*r0p=r0;}
static void f1857(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,-1);*r1p=r1;*r0p=r0;}
static void f1858(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,0);*r1p=r1;*r0p=r0;}
static void f1859(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,-2);*r1p=r1;*r0p=r0;}
static void f1860(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,2);*r1p=r1;*r0p=r0;}
static void f1861(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,-2);*r1p=r1;*r0p=r0;}
static void f1862(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,1);*r1p=r1;*r0p=r0;}
static void f1863(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,-3);*r1p=r1;*r0p=r0;}
static void f1864(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,4);*r1p=r1;*r0p=r0;}
static void f1865(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,-4);*r1p=r1;*r0p=r0;}
static void f1866(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,3);*r1p=r1;*r0p=r0;}
static void f1867(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,-5);*r1p=r1;*r0p=r0;}
static void f1868(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,8);*r1p=r1;*r0p=r0;}
static void f1869(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,-8);*r1p=r1;*r0p=r0;}
static void f1870(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,7);*r1p=r1;*r0p=r0;}
static void f1871(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,-9);*r1p=r1;*r0p=r0;}
static void f1872(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,16);*r1p=r1;*r0p=r0;}
static void f1873(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,-16);*r1p=r1;*r0p=r0;}
static void f1874(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,15);*r1p=r1;*r0p=r0;}
static void f1875(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,-17);*r1p=r1;*r0p=r0;}
static void f1876(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,32);*r1p=r1;*r0p=r0;}
static void f1877(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,-32);*r1p=r1;*r0p=r0;}
static void f1878(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,31);*r1p=r1;*r0p=r0;}
static void f1879(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,-33);*r1p=r1;*r0p=r0;}
static void f1880(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,64);*r1p=r1;*r0p=r0;}
static void f1881(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,-64);*r1p=r1;*r0p=r0;}
static void f1882(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,63);*r1p=r1;*r0p=r0;}
static void f1883(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,-65);*r1p=r1;*r0p=r0;}
static void f1884(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,128);*r1p=r1;*r0p=r0;}
static void f1885(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,-128);*r1p=r1;*r0p=r0;}
static void f1886(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,127);*r1p=r1;*r0p=r0;}
static void f1887(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,-129);*r1p=r1;*r0p=r0;}
static void f1888(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,256);*r1p=r1;*r0p=r0;}
static void f1889(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,-256);*r1p=r1;*r0p=r0;}
static void f1890(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,255);*r1p=r1;*r0p=r0;}
static void f1891(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,-257);*r1p=r1;*r0p=r0;}
static void f1892(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,512);*r1p=r1;*r0p=r0;}
static void f1893(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,-512);*r1p=r1;*r0p=r0;}
static void f1894(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,511);*r1p=r1;*r0p=r0;}
static void f1895(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,-513);*r1p=r1;*r0p=r0;}
static void f1896(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,1024);*r1p=r1;*r0p=r0;}
static void f1897(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,-1024);*r1p=r1;*r0p=r0;}
static void f1898(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,1023);*r1p=r1;*r0p=r0;}
static void f1899(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,-1025);*r1p=r1;*r0p=r0;}
static void f1900(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,2048);*r1p=r1;*r0p=r0;}
static void f1901(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,-2048);*r1p=r1;*r0p=r0;}
static void f1902(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,2047);*r1p=r1;*r0p=r0;}
static void f1903(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,-2049);*r1p=r1;*r0p=r0;}
static void f1904(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,4096);*r1p=r1;*r0p=r0;}
static void f1905(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,-4096);*r1p=r1;*r0p=r0;}
static void f1906(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,4095);*r1p=r1;*r0p=r0;}
static void f1907(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,-4097);*r1p=r1;*r0p=r0;}
static void f1908(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,8192);*r1p=r1;*r0p=r0;}
static void f1909(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,-8192);*r1p=r1;*r0p=r0;}
static void f1910(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,8191);*r1p=r1;*r0p=r0;}
static void f1911(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,-8193);*r1p=r1;*r0p=r0;}
static void f1912(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,16384);*r1p=r1;*r0p=r0;}
static void f1913(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,-16384);*r1p=r1;*r0p=r0;}
static void f1914(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,16383);*r1p=r1;*r0p=r0;}
static void f1915(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,-16385);*r1p=r1;*r0p=r0;}
static void f1916(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,32768);*r1p=r1;*r0p=r0;}
static void f1917(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,-32768);*r1p=r1;*r0p=r0;}
static void f1918(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,32767);*r1p=r1;*r0p=r0;}
static void f1919(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-128,0,-32769);*r1p=r1;*r0p=r0;}
static void f1920(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,1);*r1p=r1;*r0p=r0;}
static void f1921(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,-1);*r1p=r1;*r0p=r0;}
static void f1922(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,0);*r1p=r1;*r0p=r0;}
static void f1923(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,-2);*r1p=r1;*r0p=r0;}
static void f1924(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,2);*r1p=r1;*r0p=r0;}
static void f1925(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,-2);*r1p=r1;*r0p=r0;}
static void f1926(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,1);*r1p=r1;*r0p=r0;}
static void f1927(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,-3);*r1p=r1;*r0p=r0;}
static void f1928(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,4);*r1p=r1;*r0p=r0;}
static void f1929(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,-4);*r1p=r1;*r0p=r0;}
static void f1930(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,3);*r1p=r1;*r0p=r0;}
static void f1931(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,-5);*r1p=r1;*r0p=r0;}
static void f1932(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,8);*r1p=r1;*r0p=r0;}
static void f1933(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,-8);*r1p=r1;*r0p=r0;}
static void f1934(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,7);*r1p=r1;*r0p=r0;}
static void f1935(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,-9);*r1p=r1;*r0p=r0;}
static void f1936(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,16);*r1p=r1;*r0p=r0;}
static void f1937(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,-16);*r1p=r1;*r0p=r0;}
static void f1938(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,15);*r1p=r1;*r0p=r0;}
static void f1939(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,-17);*r1p=r1;*r0p=r0;}
static void f1940(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,32);*r1p=r1;*r0p=r0;}
static void f1941(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,-32);*r1p=r1;*r0p=r0;}
static void f1942(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,31);*r1p=r1;*r0p=r0;}
static void f1943(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,-33);*r1p=r1;*r0p=r0;}
static void f1944(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,64);*r1p=r1;*r0p=r0;}
static void f1945(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,-64);*r1p=r1;*r0p=r0;}
static void f1946(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,63);*r1p=r1;*r0p=r0;}
static void f1947(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,-65);*r1p=r1;*r0p=r0;}
static void f1948(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,128);*r1p=r1;*r0p=r0;}
static void f1949(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,-128);*r1p=r1;*r0p=r0;}
static void f1950(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,127);*r1p=r1;*r0p=r0;}
static void f1951(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,-129);*r1p=r1;*r0p=r0;}
static void f1952(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,256);*r1p=r1;*r0p=r0;}
static void f1953(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,-256);*r1p=r1;*r0p=r0;}
static void f1954(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,255);*r1p=r1;*r0p=r0;}
static void f1955(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,-257);*r1p=r1;*r0p=r0;}
static void f1956(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,512);*r1p=r1;*r0p=r0;}
static void f1957(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,-512);*r1p=r1;*r0p=r0;}
static void f1958(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,511);*r1p=r1;*r0p=r0;}
static void f1959(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,-513);*r1p=r1;*r0p=r0;}
static void f1960(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,1024);*r1p=r1;*r0p=r0;}
static void f1961(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,-1024);*r1p=r1;*r0p=r0;}
static void f1962(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,1023);*r1p=r1;*r0p=r0;}
static void f1963(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,-1025);*r1p=r1;*r0p=r0;}
static void f1964(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,2048);*r1p=r1;*r0p=r0;}
static void f1965(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,-2048);*r1p=r1;*r0p=r0;}
static void f1966(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,2047);*r1p=r1;*r0p=r0;}
static void f1967(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,-2049);*r1p=r1;*r0p=r0;}
static void f1968(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,4096);*r1p=r1;*r0p=r0;}
static void f1969(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,-4096);*r1p=r1;*r0p=r0;}
static void f1970(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,4095);*r1p=r1;*r0p=r0;}
static void f1971(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,-4097);*r1p=r1;*r0p=r0;}
static void f1972(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,8192);*r1p=r1;*r0p=r0;}
static void f1973(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,-8192);*r1p=r1;*r0p=r0;}
static void f1974(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,8191);*r1p=r1;*r0p=r0;}
static void f1975(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,-8193);*r1p=r1;*r0p=r0;}
static void f1976(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,16384);*r1p=r1;*r0p=r0;}
static void f1977(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,-16384);*r1p=r1;*r0p=r0;}
static void f1978(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,16383);*r1p=r1;*r0p=r0;}
static void f1979(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,-16385);*r1p=r1;*r0p=r0;}
static void f1980(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,32768);*r1p=r1;*r0p=r0;}
static void f1981(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,-32768);*r1p=r1;*r0p=r0;}
static void f1982(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,32767);*r1p=r1;*r0p=r0;}
static void f1983(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,127,0,-32769);*r1p=r1;*r0p=r0;}
static void f1984(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,1);*r1p=r1;*r0p=r0;}
static void f1985(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,-1);*r1p=r1;*r0p=r0;}
static void f1986(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,0);*r1p=r1;*r0p=r0;}
static void f1987(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,-2);*r1p=r1;*r0p=r0;}
static void f1988(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,2);*r1p=r1;*r0p=r0;}
static void f1989(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,-2);*r1p=r1;*r0p=r0;}
static void f1990(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,1);*r1p=r1;*r0p=r0;}
static void f1991(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,-3);*r1p=r1;*r0p=r0;}
static void f1992(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,4);*r1p=r1;*r0p=r0;}
static void f1993(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,-4);*r1p=r1;*r0p=r0;}
static void f1994(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,3);*r1p=r1;*r0p=r0;}
static void f1995(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,-5);*r1p=r1;*r0p=r0;}
static void f1996(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,8);*r1p=r1;*r0p=r0;}
static void f1997(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,-8);*r1p=r1;*r0p=r0;}
static void f1998(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,7);*r1p=r1;*r0p=r0;}
static void f1999(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,-9);*r1p=r1;*r0p=r0;}
static void f2000(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,16);*r1p=r1;*r0p=r0;}
static void f2001(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,-16);*r1p=r1;*r0p=r0;}
static void f2002(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,15);*r1p=r1;*r0p=r0;}
static void f2003(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,-17);*r1p=r1;*r0p=r0;}
static void f2004(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,32);*r1p=r1;*r0p=r0;}
static void f2005(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,-32);*r1p=r1;*r0p=r0;}
static void f2006(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,31);*r1p=r1;*r0p=r0;}
static void f2007(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,-33);*r1p=r1;*r0p=r0;}
static void f2008(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,64);*r1p=r1;*r0p=r0;}
static void f2009(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,-64);*r1p=r1;*r0p=r0;}
static void f2010(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,63);*r1p=r1;*r0p=r0;}
static void f2011(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,-65);*r1p=r1;*r0p=r0;}
static void f2012(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,128);*r1p=r1;*r0p=r0;}
static void f2013(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,-128);*r1p=r1;*r0p=r0;}
static void f2014(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,127);*r1p=r1;*r0p=r0;}
static void f2015(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,-129);*r1p=r1;*r0p=r0;}
static void f2016(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,256);*r1p=r1;*r0p=r0;}
static void f2017(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,-256);*r1p=r1;*r0p=r0;}
static void f2018(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,255);*r1p=r1;*r0p=r0;}
static void f2019(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,-257);*r1p=r1;*r0p=r0;}
static void f2020(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,512);*r1p=r1;*r0p=r0;}
static void f2021(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,-512);*r1p=r1;*r0p=r0;}
static void f2022(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,511);*r1p=r1;*r0p=r0;}
static void f2023(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,-513);*r1p=r1;*r0p=r0;}
static void f2024(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,1024);*r1p=r1;*r0p=r0;}
static void f2025(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,-1024);*r1p=r1;*r0p=r0;}
static void f2026(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,1023);*r1p=r1;*r0p=r0;}
static void f2027(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,-1025);*r1p=r1;*r0p=r0;}
static void f2028(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,2048);*r1p=r1;*r0p=r0;}
static void f2029(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,-2048);*r1p=r1;*r0p=r0;}
static void f2030(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,2047);*r1p=r1;*r0p=r0;}
static void f2031(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,-2049);*r1p=r1;*r0p=r0;}
static void f2032(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,4096);*r1p=r1;*r0p=r0;}
static void f2033(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,-4096);*r1p=r1;*r0p=r0;}
static void f2034(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,4095);*r1p=r1;*r0p=r0;}
static void f2035(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,-4097);*r1p=r1;*r0p=r0;}
static void f2036(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,8192);*r1p=r1;*r0p=r0;}
static void f2037(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,-8192);*r1p=r1;*r0p=r0;}
static void f2038(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,8191);*r1p=r1;*r0p=r0;}
static void f2039(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,-8193);*r1p=r1;*r0p=r0;}
static void f2040(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,16384);*r1p=r1;*r0p=r0;}
static void f2041(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,-16384);*r1p=r1;*r0p=r0;}
static void f2042(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,16383);*r1p=r1;*r0p=r0;}
static void f2043(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,-16385);*r1p=r1;*r0p=r0;}
static void f2044(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,32768);*r1p=r1;*r0p=r0;}
static void f2045(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,-32768);*r1p=r1;*r0p=r0;}
static void f2046(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,32767);*r1p=r1;*r0p=r0;}
static void f2047(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-129,0,-32769);*r1p=r1;*r0p=r0;}
static void f2048(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,1);*r1p=r1;*r0p=r0;}
static void f2049(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,-1);*r1p=r1;*r0p=r0;}
static void f2050(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,0);*r1p=r1;*r0p=r0;}
static void f2051(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,-2);*r1p=r1;*r0p=r0;}
static void f2052(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,2);*r1p=r1;*r0p=r0;}
static void f2053(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,-2);*r1p=r1;*r0p=r0;}
static void f2054(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,1);*r1p=r1;*r0p=r0;}
static void f2055(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,-3);*r1p=r1;*r0p=r0;}
static void f2056(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,4);*r1p=r1;*r0p=r0;}
static void f2057(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,-4);*r1p=r1;*r0p=r0;}
static void f2058(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,3);*r1p=r1;*r0p=r0;}
static void f2059(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,-5);*r1p=r1;*r0p=r0;}
static void f2060(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,8);*r1p=r1;*r0p=r0;}
static void f2061(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,-8);*r1p=r1;*r0p=r0;}
static void f2062(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,7);*r1p=r1;*r0p=r0;}
static void f2063(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,-9);*r1p=r1;*r0p=r0;}
static void f2064(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,16);*r1p=r1;*r0p=r0;}
static void f2065(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,-16);*r1p=r1;*r0p=r0;}
static void f2066(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,15);*r1p=r1;*r0p=r0;}
static void f2067(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,-17);*r1p=r1;*r0p=r0;}
static void f2068(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,32);*r1p=r1;*r0p=r0;}
static void f2069(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,-32);*r1p=r1;*r0p=r0;}
static void f2070(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,31);*r1p=r1;*r0p=r0;}
static void f2071(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,-33);*r1p=r1;*r0p=r0;}
static void f2072(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,64);*r1p=r1;*r0p=r0;}
static void f2073(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,-64);*r1p=r1;*r0p=r0;}
static void f2074(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,63);*r1p=r1;*r0p=r0;}
static void f2075(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,-65);*r1p=r1;*r0p=r0;}
static void f2076(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,128);*r1p=r1;*r0p=r0;}
static void f2077(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,-128);*r1p=r1;*r0p=r0;}
static void f2078(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,127);*r1p=r1;*r0p=r0;}
static void f2079(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,-129);*r1p=r1;*r0p=r0;}
static void f2080(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,256);*r1p=r1;*r0p=r0;}
static void f2081(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,-256);*r1p=r1;*r0p=r0;}
static void f2082(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,255);*r1p=r1;*r0p=r0;}
static void f2083(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,-257);*r1p=r1;*r0p=r0;}
static void f2084(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,512);*r1p=r1;*r0p=r0;}
static void f2085(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,-512);*r1p=r1;*r0p=r0;}
static void f2086(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,511);*r1p=r1;*r0p=r0;}
static void f2087(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,-513);*r1p=r1;*r0p=r0;}
static void f2088(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,1024);*r1p=r1;*r0p=r0;}
static void f2089(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,-1024);*r1p=r1;*r0p=r0;}
static void f2090(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,1023);*r1p=r1;*r0p=r0;}
static void f2091(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,-1025);*r1p=r1;*r0p=r0;}
static void f2092(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,2048);*r1p=r1;*r0p=r0;}
static void f2093(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,-2048);*r1p=r1;*r0p=r0;}
static void f2094(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,2047);*r1p=r1;*r0p=r0;}
static void f2095(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,-2049);*r1p=r1;*r0p=r0;}
static void f2096(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,4096);*r1p=r1;*r0p=r0;}
static void f2097(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,-4096);*r1p=r1;*r0p=r0;}
static void f2098(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,4095);*r1p=r1;*r0p=r0;}
static void f2099(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,-4097);*r1p=r1;*r0p=r0;}
static void f2100(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,8192);*r1p=r1;*r0p=r0;}
static void f2101(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,-8192);*r1p=r1;*r0p=r0;}
static void f2102(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,8191);*r1p=r1;*r0p=r0;}
static void f2103(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,-8193);*r1p=r1;*r0p=r0;}
static void f2104(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,16384);*r1p=r1;*r0p=r0;}
static void f2105(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,-16384);*r1p=r1;*r0p=r0;}
static void f2106(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,16383);*r1p=r1;*r0p=r0;}
static void f2107(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,-16385);*r1p=r1;*r0p=r0;}
static void f2108(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,32768);*r1p=r1;*r0p=r0;}
static void f2109(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,-32768);*r1p=r1;*r0p=r0;}
static void f2110(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,32767);*r1p=r1;*r0p=r0;}
static void f2111(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,256,0,-32769);*r1p=r1;*r0p=r0;}
static void f2112(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,1);*r1p=r1;*r0p=r0;}
static void f2113(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,-1);*r1p=r1;*r0p=r0;}
static void f2114(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,0);*r1p=r1;*r0p=r0;}
static void f2115(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,-2);*r1p=r1;*r0p=r0;}
static void f2116(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,2);*r1p=r1;*r0p=r0;}
static void f2117(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,-2);*r1p=r1;*r0p=r0;}
static void f2118(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,1);*r1p=r1;*r0p=r0;}
static void f2119(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,-3);*r1p=r1;*r0p=r0;}
static void f2120(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,4);*r1p=r1;*r0p=r0;}
static void f2121(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,-4);*r1p=r1;*r0p=r0;}
static void f2122(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,3);*r1p=r1;*r0p=r0;}
static void f2123(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,-5);*r1p=r1;*r0p=r0;}
static void f2124(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,8);*r1p=r1;*r0p=r0;}
static void f2125(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,-8);*r1p=r1;*r0p=r0;}
static void f2126(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,7);*r1p=r1;*r0p=r0;}
static void f2127(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,-9);*r1p=r1;*r0p=r0;}
static void f2128(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,16);*r1p=r1;*r0p=r0;}
static void f2129(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,-16);*r1p=r1;*r0p=r0;}
static void f2130(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,15);*r1p=r1;*r0p=r0;}
static void f2131(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,-17);*r1p=r1;*r0p=r0;}
static void f2132(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,32);*r1p=r1;*r0p=r0;}
static void f2133(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,-32);*r1p=r1;*r0p=r0;}
static void f2134(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,31);*r1p=r1;*r0p=r0;}
static void f2135(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,-33);*r1p=r1;*r0p=r0;}
static void f2136(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,64);*r1p=r1;*r0p=r0;}
static void f2137(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,-64);*r1p=r1;*r0p=r0;}
static void f2138(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,63);*r1p=r1;*r0p=r0;}
static void f2139(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,-65);*r1p=r1;*r0p=r0;}
static void f2140(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,128);*r1p=r1;*r0p=r0;}
static void f2141(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,-128);*r1p=r1;*r0p=r0;}
static void f2142(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,127);*r1p=r1;*r0p=r0;}
static void f2143(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,-129);*r1p=r1;*r0p=r0;}
static void f2144(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,256);*r1p=r1;*r0p=r0;}
static void f2145(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,-256);*r1p=r1;*r0p=r0;}
static void f2146(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,255);*r1p=r1;*r0p=r0;}
static void f2147(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,-257);*r1p=r1;*r0p=r0;}
static void f2148(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,512);*r1p=r1;*r0p=r0;}
static void f2149(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,-512);*r1p=r1;*r0p=r0;}
static void f2150(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,511);*r1p=r1;*r0p=r0;}
static void f2151(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,-513);*r1p=r1;*r0p=r0;}
static void f2152(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,1024);*r1p=r1;*r0p=r0;}
static void f2153(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,-1024);*r1p=r1;*r0p=r0;}
static void f2154(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,1023);*r1p=r1;*r0p=r0;}
static void f2155(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,-1025);*r1p=r1;*r0p=r0;}
static void f2156(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,2048);*r1p=r1;*r0p=r0;}
static void f2157(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,-2048);*r1p=r1;*r0p=r0;}
static void f2158(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,2047);*r1p=r1;*r0p=r0;}
static void f2159(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,-2049);*r1p=r1;*r0p=r0;}
static void f2160(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,4096);*r1p=r1;*r0p=r0;}
static void f2161(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,-4096);*r1p=r1;*r0p=r0;}
static void f2162(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,4095);*r1p=r1;*r0p=r0;}
static void f2163(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,-4097);*r1p=r1;*r0p=r0;}
static void f2164(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,8192);*r1p=r1;*r0p=r0;}
static void f2165(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,-8192);*r1p=r1;*r0p=r0;}
static void f2166(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,8191);*r1p=r1;*r0p=r0;}
static void f2167(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,-8193);*r1p=r1;*r0p=r0;}
static void f2168(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,16384);*r1p=r1;*r0p=r0;}
static void f2169(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,-16384);*r1p=r1;*r0p=r0;}
static void f2170(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,16383);*r1p=r1;*r0p=r0;}
static void f2171(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,-16385);*r1p=r1;*r0p=r0;}
static void f2172(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,32768);*r1p=r1;*r0p=r0;}
static void f2173(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,-32768);*r1p=r1;*r0p=r0;}
static void f2174(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,32767);*r1p=r1;*r0p=r0;}
static void f2175(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-256,0,-32769);*r1p=r1;*r0p=r0;}
static void f2176(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,1);*r1p=r1;*r0p=r0;}
static void f2177(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,-1);*r1p=r1;*r0p=r0;}
static void f2178(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,0);*r1p=r1;*r0p=r0;}
static void f2179(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,-2);*r1p=r1;*r0p=r0;}
static void f2180(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,2);*r1p=r1;*r0p=r0;}
static void f2181(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,-2);*r1p=r1;*r0p=r0;}
static void f2182(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,1);*r1p=r1;*r0p=r0;}
static void f2183(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,-3);*r1p=r1;*r0p=r0;}
static void f2184(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,4);*r1p=r1;*r0p=r0;}
static void f2185(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,-4);*r1p=r1;*r0p=r0;}
static void f2186(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,3);*r1p=r1;*r0p=r0;}
static void f2187(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,-5);*r1p=r1;*r0p=r0;}
static void f2188(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,8);*r1p=r1;*r0p=r0;}
static void f2189(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,-8);*r1p=r1;*r0p=r0;}
static void f2190(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,7);*r1p=r1;*r0p=r0;}
static void f2191(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,-9);*r1p=r1;*r0p=r0;}
static void f2192(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,16);*r1p=r1;*r0p=r0;}
static void f2193(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,-16);*r1p=r1;*r0p=r0;}
static void f2194(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,15);*r1p=r1;*r0p=r0;}
static void f2195(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,-17);*r1p=r1;*r0p=r0;}
static void f2196(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,32);*r1p=r1;*r0p=r0;}
static void f2197(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,-32);*r1p=r1;*r0p=r0;}
static void f2198(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,31);*r1p=r1;*r0p=r0;}
static void f2199(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,-33);*r1p=r1;*r0p=r0;}
static void f2200(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,64);*r1p=r1;*r0p=r0;}
static void f2201(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,-64);*r1p=r1;*r0p=r0;}
static void f2202(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,63);*r1p=r1;*r0p=r0;}
static void f2203(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,-65);*r1p=r1;*r0p=r0;}
static void f2204(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,128);*r1p=r1;*r0p=r0;}
static void f2205(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,-128);*r1p=r1;*r0p=r0;}
static void f2206(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,127);*r1p=r1;*r0p=r0;}
static void f2207(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,-129);*r1p=r1;*r0p=r0;}
static void f2208(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,256);*r1p=r1;*r0p=r0;}
static void f2209(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,-256);*r1p=r1;*r0p=r0;}
static void f2210(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,255);*r1p=r1;*r0p=r0;}
static void f2211(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,-257);*r1p=r1;*r0p=r0;}
static void f2212(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,512);*r1p=r1;*r0p=r0;}
static void f2213(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,-512);*r1p=r1;*r0p=r0;}
static void f2214(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,511);*r1p=r1;*r0p=r0;}
static void f2215(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,-513);*r1p=r1;*r0p=r0;}
static void f2216(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,1024);*r1p=r1;*r0p=r0;}
static void f2217(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,-1024);*r1p=r1;*r0p=r0;}
static void f2218(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,1023);*r1p=r1;*r0p=r0;}
static void f2219(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,-1025);*r1p=r1;*r0p=r0;}
static void f2220(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,2048);*r1p=r1;*r0p=r0;}
static void f2221(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,-2048);*r1p=r1;*r0p=r0;}
static void f2222(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,2047);*r1p=r1;*r0p=r0;}
static void f2223(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,-2049);*r1p=r1;*r0p=r0;}
static void f2224(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,4096);*r1p=r1;*r0p=r0;}
static void f2225(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,-4096);*r1p=r1;*r0p=r0;}
static void f2226(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,4095);*r1p=r1;*r0p=r0;}
static void f2227(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,-4097);*r1p=r1;*r0p=r0;}
static void f2228(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,8192);*r1p=r1;*r0p=r0;}
static void f2229(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,-8192);*r1p=r1;*r0p=r0;}
static void f2230(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,8191);*r1p=r1;*r0p=r0;}
static void f2231(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,-8193);*r1p=r1;*r0p=r0;}
static void f2232(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,16384);*r1p=r1;*r0p=r0;}
static void f2233(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,-16384);*r1p=r1;*r0p=r0;}
static void f2234(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,16383);*r1p=r1;*r0p=r0;}
static void f2235(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,-16385);*r1p=r1;*r0p=r0;}
static void f2236(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,32768);*r1p=r1;*r0p=r0;}
static void f2237(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,-32768);*r1p=r1;*r0p=r0;}
static void f2238(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,32767);*r1p=r1;*r0p=r0;}
static void f2239(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,255,0,-32769);*r1p=r1;*r0p=r0;}
static void f2240(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,1);*r1p=r1;*r0p=r0;}
static void f2241(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,-1);*r1p=r1;*r0p=r0;}
static void f2242(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,0);*r1p=r1;*r0p=r0;}
static void f2243(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,-2);*r1p=r1;*r0p=r0;}
static void f2244(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,2);*r1p=r1;*r0p=r0;}
static void f2245(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,-2);*r1p=r1;*r0p=r0;}
static void f2246(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,1);*r1p=r1;*r0p=r0;}
static void f2247(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,-3);*r1p=r1;*r0p=r0;}
static void f2248(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,4);*r1p=r1;*r0p=r0;}
static void f2249(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,-4);*r1p=r1;*r0p=r0;}
static void f2250(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,3);*r1p=r1;*r0p=r0;}
static void f2251(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,-5);*r1p=r1;*r0p=r0;}
static void f2252(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,8);*r1p=r1;*r0p=r0;}
static void f2253(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,-8);*r1p=r1;*r0p=r0;}
static void f2254(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,7);*r1p=r1;*r0p=r0;}
static void f2255(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,-9);*r1p=r1;*r0p=r0;}
static void f2256(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,16);*r1p=r1;*r0p=r0;}
static void f2257(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,-16);*r1p=r1;*r0p=r0;}
static void f2258(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,15);*r1p=r1;*r0p=r0;}
static void f2259(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,-17);*r1p=r1;*r0p=r0;}
static void f2260(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,32);*r1p=r1;*r0p=r0;}
static void f2261(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,-32);*r1p=r1;*r0p=r0;}
static void f2262(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,31);*r1p=r1;*r0p=r0;}
static void f2263(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,-33);*r1p=r1;*r0p=r0;}
static void f2264(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,64);*r1p=r1;*r0p=r0;}
static void f2265(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,-64);*r1p=r1;*r0p=r0;}
static void f2266(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,63);*r1p=r1;*r0p=r0;}
static void f2267(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,-65);*r1p=r1;*r0p=r0;}
static void f2268(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,128);*r1p=r1;*r0p=r0;}
static void f2269(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,-128);*r1p=r1;*r0p=r0;}
static void f2270(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,127);*r1p=r1;*r0p=r0;}
static void f2271(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,-129);*r1p=r1;*r0p=r0;}
static void f2272(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,256);*r1p=r1;*r0p=r0;}
static void f2273(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,-256);*r1p=r1;*r0p=r0;}
static void f2274(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,255);*r1p=r1;*r0p=r0;}
static void f2275(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,-257);*r1p=r1;*r0p=r0;}
static void f2276(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,512);*r1p=r1;*r0p=r0;}
static void f2277(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,-512);*r1p=r1;*r0p=r0;}
static void f2278(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,511);*r1p=r1;*r0p=r0;}
static void f2279(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,-513);*r1p=r1;*r0p=r0;}
static void f2280(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,1024);*r1p=r1;*r0p=r0;}
static void f2281(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,-1024);*r1p=r1;*r0p=r0;}
static void f2282(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,1023);*r1p=r1;*r0p=r0;}
static void f2283(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,-1025);*r1p=r1;*r0p=r0;}
static void f2284(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,2048);*r1p=r1;*r0p=r0;}
static void f2285(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,-2048);*r1p=r1;*r0p=r0;}
static void f2286(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,2047);*r1p=r1;*r0p=r0;}
static void f2287(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,-2049);*r1p=r1;*r0p=r0;}
static void f2288(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,4096);*r1p=r1;*r0p=r0;}
static void f2289(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,-4096);*r1p=r1;*r0p=r0;}
static void f2290(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,4095);*r1p=r1;*r0p=r0;}
static void f2291(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,-4097);*r1p=r1;*r0p=r0;}
static void f2292(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,8192);*r1p=r1;*r0p=r0;}
static void f2293(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,-8192);*r1p=r1;*r0p=r0;}
static void f2294(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,8191);*r1p=r1;*r0p=r0;}
static void f2295(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,-8193);*r1p=r1;*r0p=r0;}
static void f2296(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,16384);*r1p=r1;*r0p=r0;}
static void f2297(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,-16384);*r1p=r1;*r0p=r0;}
static void f2298(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,16383);*r1p=r1;*r0p=r0;}
static void f2299(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,-16385);*r1p=r1;*r0p=r0;}
static void f2300(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,32768);*r1p=r1;*r0p=r0;}
static void f2301(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,-32768);*r1p=r1;*r0p=r0;}
static void f2302(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,32767);*r1p=r1;*r0p=r0;}
static void f2303(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-257,0,-32769);*r1p=r1;*r0p=r0;}
static void f2304(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,1);*r1p=r1;*r0p=r0;}
static void f2305(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,-1);*r1p=r1;*r0p=r0;}
static void f2306(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,0);*r1p=r1;*r0p=r0;}
static void f2307(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,-2);*r1p=r1;*r0p=r0;}
static void f2308(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,2);*r1p=r1;*r0p=r0;}
static void f2309(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,-2);*r1p=r1;*r0p=r0;}
static void f2310(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,1);*r1p=r1;*r0p=r0;}
static void f2311(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,-3);*r1p=r1;*r0p=r0;}
static void f2312(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,4);*r1p=r1;*r0p=r0;}
static void f2313(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,-4);*r1p=r1;*r0p=r0;}
static void f2314(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,3);*r1p=r1;*r0p=r0;}
static void f2315(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,-5);*r1p=r1;*r0p=r0;}
static void f2316(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,8);*r1p=r1;*r0p=r0;}
static void f2317(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,-8);*r1p=r1;*r0p=r0;}
static void f2318(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,7);*r1p=r1;*r0p=r0;}
static void f2319(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,-9);*r1p=r1;*r0p=r0;}
static void f2320(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,16);*r1p=r1;*r0p=r0;}
static void f2321(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,-16);*r1p=r1;*r0p=r0;}
static void f2322(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,15);*r1p=r1;*r0p=r0;}
static void f2323(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,-17);*r1p=r1;*r0p=r0;}
static void f2324(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,32);*r1p=r1;*r0p=r0;}
static void f2325(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,-32);*r1p=r1;*r0p=r0;}
static void f2326(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,31);*r1p=r1;*r0p=r0;}
static void f2327(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,-33);*r1p=r1;*r0p=r0;}
static void f2328(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,64);*r1p=r1;*r0p=r0;}
static void f2329(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,-64);*r1p=r1;*r0p=r0;}
static void f2330(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,63);*r1p=r1;*r0p=r0;}
static void f2331(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,-65);*r1p=r1;*r0p=r0;}
static void f2332(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,128);*r1p=r1;*r0p=r0;}
static void f2333(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,-128);*r1p=r1;*r0p=r0;}
static void f2334(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,127);*r1p=r1;*r0p=r0;}
static void f2335(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,-129);*r1p=r1;*r0p=r0;}
static void f2336(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,256);*r1p=r1;*r0p=r0;}
static void f2337(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,-256);*r1p=r1;*r0p=r0;}
static void f2338(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,255);*r1p=r1;*r0p=r0;}
static void f2339(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,-257);*r1p=r1;*r0p=r0;}
static void f2340(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,512);*r1p=r1;*r0p=r0;}
static void f2341(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,-512);*r1p=r1;*r0p=r0;}
static void f2342(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,511);*r1p=r1;*r0p=r0;}
static void f2343(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,-513);*r1p=r1;*r0p=r0;}
static void f2344(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,1024);*r1p=r1;*r0p=r0;}
static void f2345(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,-1024);*r1p=r1;*r0p=r0;}
static void f2346(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,1023);*r1p=r1;*r0p=r0;}
static void f2347(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,-1025);*r1p=r1;*r0p=r0;}
static void f2348(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,2048);*r1p=r1;*r0p=r0;}
static void f2349(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,-2048);*r1p=r1;*r0p=r0;}
static void f2350(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,2047);*r1p=r1;*r0p=r0;}
static void f2351(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,-2049);*r1p=r1;*r0p=r0;}
static void f2352(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,4096);*r1p=r1;*r0p=r0;}
static void f2353(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,-4096);*r1p=r1;*r0p=r0;}
static void f2354(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,4095);*r1p=r1;*r0p=r0;}
static void f2355(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,-4097);*r1p=r1;*r0p=r0;}
static void f2356(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,8192);*r1p=r1;*r0p=r0;}
static void f2357(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,-8192);*r1p=r1;*r0p=r0;}
static void f2358(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,8191);*r1p=r1;*r0p=r0;}
static void f2359(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,-8193);*r1p=r1;*r0p=r0;}
static void f2360(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,16384);*r1p=r1;*r0p=r0;}
static void f2361(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,-16384);*r1p=r1;*r0p=r0;}
static void f2362(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,16383);*r1p=r1;*r0p=r0;}
static void f2363(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,-16385);*r1p=r1;*r0p=r0;}
static void f2364(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,32768);*r1p=r1;*r0p=r0;}
static void f2365(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,-32768);*r1p=r1;*r0p=r0;}
static void f2366(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,32767);*r1p=r1;*r0p=r0;}
static void f2367(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,512,0,-32769);*r1p=r1;*r0p=r0;}
static void f2368(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,1);*r1p=r1;*r0p=r0;}
static void f2369(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,-1);*r1p=r1;*r0p=r0;}
static void f2370(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,0);*r1p=r1;*r0p=r0;}
static void f2371(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,-2);*r1p=r1;*r0p=r0;}
static void f2372(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,2);*r1p=r1;*r0p=r0;}
static void f2373(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,-2);*r1p=r1;*r0p=r0;}
static void f2374(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,1);*r1p=r1;*r0p=r0;}
static void f2375(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,-3);*r1p=r1;*r0p=r0;}
static void f2376(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,4);*r1p=r1;*r0p=r0;}
static void f2377(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,-4);*r1p=r1;*r0p=r0;}
static void f2378(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,3);*r1p=r1;*r0p=r0;}
static void f2379(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,-5);*r1p=r1;*r0p=r0;}
static void f2380(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,8);*r1p=r1;*r0p=r0;}
static void f2381(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,-8);*r1p=r1;*r0p=r0;}
static void f2382(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,7);*r1p=r1;*r0p=r0;}
static void f2383(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,-9);*r1p=r1;*r0p=r0;}
static void f2384(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,16);*r1p=r1;*r0p=r0;}
static void f2385(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,-16);*r1p=r1;*r0p=r0;}
static void f2386(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,15);*r1p=r1;*r0p=r0;}
static void f2387(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,-17);*r1p=r1;*r0p=r0;}
static void f2388(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,32);*r1p=r1;*r0p=r0;}
static void f2389(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,-32);*r1p=r1;*r0p=r0;}
static void f2390(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,31);*r1p=r1;*r0p=r0;}
static void f2391(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,-33);*r1p=r1;*r0p=r0;}
static void f2392(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,64);*r1p=r1;*r0p=r0;}
static void f2393(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,-64);*r1p=r1;*r0p=r0;}
static void f2394(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,63);*r1p=r1;*r0p=r0;}
static void f2395(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,-65);*r1p=r1;*r0p=r0;}
static void f2396(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,128);*r1p=r1;*r0p=r0;}
static void f2397(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,-128);*r1p=r1;*r0p=r0;}
static void f2398(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,127);*r1p=r1;*r0p=r0;}
static void f2399(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,-129);*r1p=r1;*r0p=r0;}
static void f2400(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,256);*r1p=r1;*r0p=r0;}
static void f2401(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,-256);*r1p=r1;*r0p=r0;}
static void f2402(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,255);*r1p=r1;*r0p=r0;}
static void f2403(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,-257);*r1p=r1;*r0p=r0;}
static void f2404(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,512);*r1p=r1;*r0p=r0;}
static void f2405(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,-512);*r1p=r1;*r0p=r0;}
static void f2406(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,511);*r1p=r1;*r0p=r0;}
static void f2407(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,-513);*r1p=r1;*r0p=r0;}
static void f2408(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,1024);*r1p=r1;*r0p=r0;}
static void f2409(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,-1024);*r1p=r1;*r0p=r0;}
static void f2410(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,1023);*r1p=r1;*r0p=r0;}
static void f2411(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,-1025);*r1p=r1;*r0p=r0;}
static void f2412(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,2048);*r1p=r1;*r0p=r0;}
static void f2413(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,-2048);*r1p=r1;*r0p=r0;}
static void f2414(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,2047);*r1p=r1;*r0p=r0;}
static void f2415(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,-2049);*r1p=r1;*r0p=r0;}
static void f2416(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,4096);*r1p=r1;*r0p=r0;}
static void f2417(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,-4096);*r1p=r1;*r0p=r0;}
static void f2418(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,4095);*r1p=r1;*r0p=r0;}
static void f2419(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,-4097);*r1p=r1;*r0p=r0;}
static void f2420(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,8192);*r1p=r1;*r0p=r0;}
static void f2421(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,-8192);*r1p=r1;*r0p=r0;}
static void f2422(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,8191);*r1p=r1;*r0p=r0;}
static void f2423(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,-8193);*r1p=r1;*r0p=r0;}
static void f2424(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,16384);*r1p=r1;*r0p=r0;}
static void f2425(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,-16384);*r1p=r1;*r0p=r0;}
static void f2426(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,16383);*r1p=r1;*r0p=r0;}
static void f2427(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,-16385);*r1p=r1;*r0p=r0;}
static void f2428(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,32768);*r1p=r1;*r0p=r0;}
static void f2429(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,-32768);*r1p=r1;*r0p=r0;}
static void f2430(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,32767);*r1p=r1;*r0p=r0;}
static void f2431(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-512,0,-32769);*r1p=r1;*r0p=r0;}
static void f2432(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,1);*r1p=r1;*r0p=r0;}
static void f2433(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,-1);*r1p=r1;*r0p=r0;}
static void f2434(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,0);*r1p=r1;*r0p=r0;}
static void f2435(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,-2);*r1p=r1;*r0p=r0;}
static void f2436(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,2);*r1p=r1;*r0p=r0;}
static void f2437(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,-2);*r1p=r1;*r0p=r0;}
static void f2438(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,1);*r1p=r1;*r0p=r0;}
static void f2439(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,-3);*r1p=r1;*r0p=r0;}
static void f2440(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,4);*r1p=r1;*r0p=r0;}
static void f2441(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,-4);*r1p=r1;*r0p=r0;}
static void f2442(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,3);*r1p=r1;*r0p=r0;}
static void f2443(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,-5);*r1p=r1;*r0p=r0;}
static void f2444(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,8);*r1p=r1;*r0p=r0;}
static void f2445(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,-8);*r1p=r1;*r0p=r0;}
static void f2446(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,7);*r1p=r1;*r0p=r0;}
static void f2447(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,-9);*r1p=r1;*r0p=r0;}
static void f2448(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,16);*r1p=r1;*r0p=r0;}
static void f2449(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,-16);*r1p=r1;*r0p=r0;}
static void f2450(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,15);*r1p=r1;*r0p=r0;}
static void f2451(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,-17);*r1p=r1;*r0p=r0;}
static void f2452(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,32);*r1p=r1;*r0p=r0;}
static void f2453(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,-32);*r1p=r1;*r0p=r0;}
static void f2454(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,31);*r1p=r1;*r0p=r0;}
static void f2455(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,-33);*r1p=r1;*r0p=r0;}
static void f2456(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,64);*r1p=r1;*r0p=r0;}
static void f2457(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,-64);*r1p=r1;*r0p=r0;}
static void f2458(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,63);*r1p=r1;*r0p=r0;}
static void f2459(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,-65);*r1p=r1;*r0p=r0;}
static void f2460(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,128);*r1p=r1;*r0p=r0;}
static void f2461(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,-128);*r1p=r1;*r0p=r0;}
static void f2462(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,127);*r1p=r1;*r0p=r0;}
static void f2463(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,-129);*r1p=r1;*r0p=r0;}
static void f2464(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,256);*r1p=r1;*r0p=r0;}
static void f2465(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,-256);*r1p=r1;*r0p=r0;}
static void f2466(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,255);*r1p=r1;*r0p=r0;}
static void f2467(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,-257);*r1p=r1;*r0p=r0;}
static void f2468(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,512);*r1p=r1;*r0p=r0;}
static void f2469(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,-512);*r1p=r1;*r0p=r0;}
static void f2470(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,511);*r1p=r1;*r0p=r0;}
static void f2471(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,-513);*r1p=r1;*r0p=r0;}
static void f2472(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,1024);*r1p=r1;*r0p=r0;}
static void f2473(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,-1024);*r1p=r1;*r0p=r0;}
static void f2474(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,1023);*r1p=r1;*r0p=r0;}
static void f2475(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,-1025);*r1p=r1;*r0p=r0;}
static void f2476(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,2048);*r1p=r1;*r0p=r0;}
static void f2477(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,-2048);*r1p=r1;*r0p=r0;}
static void f2478(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,2047);*r1p=r1;*r0p=r0;}
static void f2479(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,-2049);*r1p=r1;*r0p=r0;}
static void f2480(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,4096);*r1p=r1;*r0p=r0;}
static void f2481(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,-4096);*r1p=r1;*r0p=r0;}
static void f2482(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,4095);*r1p=r1;*r0p=r0;}
static void f2483(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,-4097);*r1p=r1;*r0p=r0;}
static void f2484(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,8192);*r1p=r1;*r0p=r0;}
static void f2485(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,-8192);*r1p=r1;*r0p=r0;}
static void f2486(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,8191);*r1p=r1;*r0p=r0;}
static void f2487(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,-8193);*r1p=r1;*r0p=r0;}
static void f2488(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,16384);*r1p=r1;*r0p=r0;}
static void f2489(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,-16384);*r1p=r1;*r0p=r0;}
static void f2490(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,16383);*r1p=r1;*r0p=r0;}
static void f2491(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,-16385);*r1p=r1;*r0p=r0;}
static void f2492(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,32768);*r1p=r1;*r0p=r0;}
static void f2493(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,-32768);*r1p=r1;*r0p=r0;}
static void f2494(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,32767);*r1p=r1;*r0p=r0;}
static void f2495(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,511,0,-32769);*r1p=r1;*r0p=r0;}
static void f2496(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,1);*r1p=r1;*r0p=r0;}
static void f2497(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,-1);*r1p=r1;*r0p=r0;}
static void f2498(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,0);*r1p=r1;*r0p=r0;}
static void f2499(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,-2);*r1p=r1;*r0p=r0;}
static void f2500(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,2);*r1p=r1;*r0p=r0;}
static void f2501(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,-2);*r1p=r1;*r0p=r0;}
static void f2502(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,1);*r1p=r1;*r0p=r0;}
static void f2503(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,-3);*r1p=r1;*r0p=r0;}
static void f2504(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,4);*r1p=r1;*r0p=r0;}
static void f2505(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,-4);*r1p=r1;*r0p=r0;}
static void f2506(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,3);*r1p=r1;*r0p=r0;}
static void f2507(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,-5);*r1p=r1;*r0p=r0;}
static void f2508(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,8);*r1p=r1;*r0p=r0;}
static void f2509(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,-8);*r1p=r1;*r0p=r0;}
static void f2510(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,7);*r1p=r1;*r0p=r0;}
static void f2511(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,-9);*r1p=r1;*r0p=r0;}
static void f2512(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,16);*r1p=r1;*r0p=r0;}
static void f2513(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,-16);*r1p=r1;*r0p=r0;}
static void f2514(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,15);*r1p=r1;*r0p=r0;}
static void f2515(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,-17);*r1p=r1;*r0p=r0;}
static void f2516(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,32);*r1p=r1;*r0p=r0;}
static void f2517(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,-32);*r1p=r1;*r0p=r0;}
static void f2518(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,31);*r1p=r1;*r0p=r0;}
static void f2519(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,-33);*r1p=r1;*r0p=r0;}
static void f2520(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,64);*r1p=r1;*r0p=r0;}
static void f2521(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,-64);*r1p=r1;*r0p=r0;}
static void f2522(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,63);*r1p=r1;*r0p=r0;}
static void f2523(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,-65);*r1p=r1;*r0p=r0;}
static void f2524(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,128);*r1p=r1;*r0p=r0;}
static void f2525(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,-128);*r1p=r1;*r0p=r0;}
static void f2526(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,127);*r1p=r1;*r0p=r0;}
static void f2527(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,-129);*r1p=r1;*r0p=r0;}
static void f2528(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,256);*r1p=r1;*r0p=r0;}
static void f2529(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,-256);*r1p=r1;*r0p=r0;}
static void f2530(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,255);*r1p=r1;*r0p=r0;}
static void f2531(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,-257);*r1p=r1;*r0p=r0;}
static void f2532(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,512);*r1p=r1;*r0p=r0;}
static void f2533(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,-512);*r1p=r1;*r0p=r0;}
static void f2534(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,511);*r1p=r1;*r0p=r0;}
static void f2535(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,-513);*r1p=r1;*r0p=r0;}
static void f2536(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,1024);*r1p=r1;*r0p=r0;}
static void f2537(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,-1024);*r1p=r1;*r0p=r0;}
static void f2538(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,1023);*r1p=r1;*r0p=r0;}
static void f2539(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,-1025);*r1p=r1;*r0p=r0;}
static void f2540(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,2048);*r1p=r1;*r0p=r0;}
static void f2541(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,-2048);*r1p=r1;*r0p=r0;}
static void f2542(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,2047);*r1p=r1;*r0p=r0;}
static void f2543(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,-2049);*r1p=r1;*r0p=r0;}
static void f2544(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,4096);*r1p=r1;*r0p=r0;}
static void f2545(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,-4096);*r1p=r1;*r0p=r0;}
static void f2546(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,4095);*r1p=r1;*r0p=r0;}
static void f2547(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,-4097);*r1p=r1;*r0p=r0;}
static void f2548(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,8192);*r1p=r1;*r0p=r0;}
static void f2549(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,-8192);*r1p=r1;*r0p=r0;}
static void f2550(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,8191);*r1p=r1;*r0p=r0;}
static void f2551(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,-8193);*r1p=r1;*r0p=r0;}
static void f2552(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,16384);*r1p=r1;*r0p=r0;}
static void f2553(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,-16384);*r1p=r1;*r0p=r0;}
static void f2554(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,16383);*r1p=r1;*r0p=r0;}
static void f2555(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,-16385);*r1p=r1;*r0p=r0;}
static void f2556(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,32768);*r1p=r1;*r0p=r0;}
static void f2557(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,-32768);*r1p=r1;*r0p=r0;}
static void f2558(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,32767);*r1p=r1;*r0p=r0;}
static void f2559(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-513,0,-32769);*r1p=r1;*r0p=r0;}
static void f2560(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,1);*r1p=r1;*r0p=r0;}
static void f2561(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,-1);*r1p=r1;*r0p=r0;}
static void f2562(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,0);*r1p=r1;*r0p=r0;}
static void f2563(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,-2);*r1p=r1;*r0p=r0;}
static void f2564(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,2);*r1p=r1;*r0p=r0;}
static void f2565(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,-2);*r1p=r1;*r0p=r0;}
static void f2566(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,1);*r1p=r1;*r0p=r0;}
static void f2567(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,-3);*r1p=r1;*r0p=r0;}
static void f2568(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,4);*r1p=r1;*r0p=r0;}
static void f2569(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,-4);*r1p=r1;*r0p=r0;}
static void f2570(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,3);*r1p=r1;*r0p=r0;}
static void f2571(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,-5);*r1p=r1;*r0p=r0;}
static void f2572(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,8);*r1p=r1;*r0p=r0;}
static void f2573(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,-8);*r1p=r1;*r0p=r0;}
static void f2574(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,7);*r1p=r1;*r0p=r0;}
static void f2575(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,-9);*r1p=r1;*r0p=r0;}
static void f2576(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,16);*r1p=r1;*r0p=r0;}
static void f2577(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,-16);*r1p=r1;*r0p=r0;}
static void f2578(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,15);*r1p=r1;*r0p=r0;}
static void f2579(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,-17);*r1p=r1;*r0p=r0;}
static void f2580(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,32);*r1p=r1;*r0p=r0;}
static void f2581(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,-32);*r1p=r1;*r0p=r0;}
static void f2582(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,31);*r1p=r1;*r0p=r0;}
static void f2583(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,-33);*r1p=r1;*r0p=r0;}
static void f2584(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,64);*r1p=r1;*r0p=r0;}
static void f2585(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,-64);*r1p=r1;*r0p=r0;}
static void f2586(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,63);*r1p=r1;*r0p=r0;}
static void f2587(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,-65);*r1p=r1;*r0p=r0;}
static void f2588(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,128);*r1p=r1;*r0p=r0;}
static void f2589(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,-128);*r1p=r1;*r0p=r0;}
static void f2590(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,127);*r1p=r1;*r0p=r0;}
static void f2591(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,-129);*r1p=r1;*r0p=r0;}
static void f2592(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,256);*r1p=r1;*r0p=r0;}
static void f2593(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,-256);*r1p=r1;*r0p=r0;}
static void f2594(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,255);*r1p=r1;*r0p=r0;}
static void f2595(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,-257);*r1p=r1;*r0p=r0;}
static void f2596(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,512);*r1p=r1;*r0p=r0;}
static void f2597(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,-512);*r1p=r1;*r0p=r0;}
static void f2598(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,511);*r1p=r1;*r0p=r0;}
static void f2599(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,-513);*r1p=r1;*r0p=r0;}
static void f2600(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,1024);*r1p=r1;*r0p=r0;}
static void f2601(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,-1024);*r1p=r1;*r0p=r0;}
static void f2602(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,1023);*r1p=r1;*r0p=r0;}
static void f2603(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,-1025);*r1p=r1;*r0p=r0;}
static void f2604(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,2048);*r1p=r1;*r0p=r0;}
static void f2605(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,-2048);*r1p=r1;*r0p=r0;}
static void f2606(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,2047);*r1p=r1;*r0p=r0;}
static void f2607(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,-2049);*r1p=r1;*r0p=r0;}
static void f2608(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,4096);*r1p=r1;*r0p=r0;}
static void f2609(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,-4096);*r1p=r1;*r0p=r0;}
static void f2610(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,4095);*r1p=r1;*r0p=r0;}
static void f2611(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,-4097);*r1p=r1;*r0p=r0;}
static void f2612(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,8192);*r1p=r1;*r0p=r0;}
static void f2613(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,-8192);*r1p=r1;*r0p=r0;}
static void f2614(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,8191);*r1p=r1;*r0p=r0;}
static void f2615(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,-8193);*r1p=r1;*r0p=r0;}
static void f2616(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,16384);*r1p=r1;*r0p=r0;}
static void f2617(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,-16384);*r1p=r1;*r0p=r0;}
static void f2618(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,16383);*r1p=r1;*r0p=r0;}
static void f2619(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,-16385);*r1p=r1;*r0p=r0;}
static void f2620(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,32768);*r1p=r1;*r0p=r0;}
static void f2621(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,-32768);*r1p=r1;*r0p=r0;}
static void f2622(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,32767);*r1p=r1;*r0p=r0;}
static void f2623(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1024,0,-32769);*r1p=r1;*r0p=r0;}
static void f2624(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,1);*r1p=r1;*r0p=r0;}
static void f2625(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,-1);*r1p=r1;*r0p=r0;}
static void f2626(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,0);*r1p=r1;*r0p=r0;}
static void f2627(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,-2);*r1p=r1;*r0p=r0;}
static void f2628(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,2);*r1p=r1;*r0p=r0;}
static void f2629(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,-2);*r1p=r1;*r0p=r0;}
static void f2630(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,1);*r1p=r1;*r0p=r0;}
static void f2631(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,-3);*r1p=r1;*r0p=r0;}
static void f2632(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,4);*r1p=r1;*r0p=r0;}
static void f2633(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,-4);*r1p=r1;*r0p=r0;}
static void f2634(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,3);*r1p=r1;*r0p=r0;}
static void f2635(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,-5);*r1p=r1;*r0p=r0;}
static void f2636(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,8);*r1p=r1;*r0p=r0;}
static void f2637(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,-8);*r1p=r1;*r0p=r0;}
static void f2638(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,7);*r1p=r1;*r0p=r0;}
static void f2639(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,-9);*r1p=r1;*r0p=r0;}
static void f2640(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,16);*r1p=r1;*r0p=r0;}
static void f2641(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,-16);*r1p=r1;*r0p=r0;}
static void f2642(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,15);*r1p=r1;*r0p=r0;}
static void f2643(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,-17);*r1p=r1;*r0p=r0;}
static void f2644(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,32);*r1p=r1;*r0p=r0;}
static void f2645(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,-32);*r1p=r1;*r0p=r0;}
static void f2646(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,31);*r1p=r1;*r0p=r0;}
static void f2647(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,-33);*r1p=r1;*r0p=r0;}
static void f2648(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,64);*r1p=r1;*r0p=r0;}
static void f2649(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,-64);*r1p=r1;*r0p=r0;}
static void f2650(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,63);*r1p=r1;*r0p=r0;}
static void f2651(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,-65);*r1p=r1;*r0p=r0;}
static void f2652(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,128);*r1p=r1;*r0p=r0;}
static void f2653(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,-128);*r1p=r1;*r0p=r0;}
static void f2654(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,127);*r1p=r1;*r0p=r0;}
static void f2655(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,-129);*r1p=r1;*r0p=r0;}
static void f2656(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,256);*r1p=r1;*r0p=r0;}
static void f2657(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,-256);*r1p=r1;*r0p=r0;}
static void f2658(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,255);*r1p=r1;*r0p=r0;}
static void f2659(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,-257);*r1p=r1;*r0p=r0;}
static void f2660(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,512);*r1p=r1;*r0p=r0;}
static void f2661(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,-512);*r1p=r1;*r0p=r0;}
static void f2662(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,511);*r1p=r1;*r0p=r0;}
static void f2663(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,-513);*r1p=r1;*r0p=r0;}
static void f2664(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,1024);*r1p=r1;*r0p=r0;}
static void f2665(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,-1024);*r1p=r1;*r0p=r0;}
static void f2666(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,1023);*r1p=r1;*r0p=r0;}
static void f2667(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,-1025);*r1p=r1;*r0p=r0;}
static void f2668(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,2048);*r1p=r1;*r0p=r0;}
static void f2669(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,-2048);*r1p=r1;*r0p=r0;}
static void f2670(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,2047);*r1p=r1;*r0p=r0;}
static void f2671(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,-2049);*r1p=r1;*r0p=r0;}
static void f2672(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,4096);*r1p=r1;*r0p=r0;}
static void f2673(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,-4096);*r1p=r1;*r0p=r0;}
static void f2674(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,4095);*r1p=r1;*r0p=r0;}
static void f2675(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,-4097);*r1p=r1;*r0p=r0;}
static void f2676(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,8192);*r1p=r1;*r0p=r0;}
static void f2677(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,-8192);*r1p=r1;*r0p=r0;}
static void f2678(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,8191);*r1p=r1;*r0p=r0;}
static void f2679(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,-8193);*r1p=r1;*r0p=r0;}
static void f2680(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,16384);*r1p=r1;*r0p=r0;}
static void f2681(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,-16384);*r1p=r1;*r0p=r0;}
static void f2682(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,16383);*r1p=r1;*r0p=r0;}
static void f2683(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,-16385);*r1p=r1;*r0p=r0;}
static void f2684(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,32768);*r1p=r1;*r0p=r0;}
static void f2685(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,-32768);*r1p=r1;*r0p=r0;}
static void f2686(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,32767);*r1p=r1;*r0p=r0;}
static void f2687(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1024,0,-32769);*r1p=r1;*r0p=r0;}
static void f2688(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,1);*r1p=r1;*r0p=r0;}
static void f2689(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,-1);*r1p=r1;*r0p=r0;}
static void f2690(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,0);*r1p=r1;*r0p=r0;}
static void f2691(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,-2);*r1p=r1;*r0p=r0;}
static void f2692(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,2);*r1p=r1;*r0p=r0;}
static void f2693(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,-2);*r1p=r1;*r0p=r0;}
static void f2694(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,1);*r1p=r1;*r0p=r0;}
static void f2695(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,-3);*r1p=r1;*r0p=r0;}
static void f2696(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,4);*r1p=r1;*r0p=r0;}
static void f2697(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,-4);*r1p=r1;*r0p=r0;}
static void f2698(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,3);*r1p=r1;*r0p=r0;}
static void f2699(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,-5);*r1p=r1;*r0p=r0;}
static void f2700(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,8);*r1p=r1;*r0p=r0;}
static void f2701(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,-8);*r1p=r1;*r0p=r0;}
static void f2702(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,7);*r1p=r1;*r0p=r0;}
static void f2703(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,-9);*r1p=r1;*r0p=r0;}
static void f2704(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,16);*r1p=r1;*r0p=r0;}
static void f2705(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,-16);*r1p=r1;*r0p=r0;}
static void f2706(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,15);*r1p=r1;*r0p=r0;}
static void f2707(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,-17);*r1p=r1;*r0p=r0;}
static void f2708(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,32);*r1p=r1;*r0p=r0;}
static void f2709(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,-32);*r1p=r1;*r0p=r0;}
static void f2710(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,31);*r1p=r1;*r0p=r0;}
static void f2711(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,-33);*r1p=r1;*r0p=r0;}
static void f2712(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,64);*r1p=r1;*r0p=r0;}
static void f2713(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,-64);*r1p=r1;*r0p=r0;}
static void f2714(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,63);*r1p=r1;*r0p=r0;}
static void f2715(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,-65);*r1p=r1;*r0p=r0;}
static void f2716(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,128);*r1p=r1;*r0p=r0;}
static void f2717(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,-128);*r1p=r1;*r0p=r0;}
static void f2718(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,127);*r1p=r1;*r0p=r0;}
static void f2719(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,-129);*r1p=r1;*r0p=r0;}
static void f2720(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,256);*r1p=r1;*r0p=r0;}
static void f2721(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,-256);*r1p=r1;*r0p=r0;}
static void f2722(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,255);*r1p=r1;*r0p=r0;}
static void f2723(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,-257);*r1p=r1;*r0p=r0;}
static void f2724(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,512);*r1p=r1;*r0p=r0;}
static void f2725(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,-512);*r1p=r1;*r0p=r0;}
static void f2726(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,511);*r1p=r1;*r0p=r0;}
static void f2727(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,-513);*r1p=r1;*r0p=r0;}
static void f2728(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,1024);*r1p=r1;*r0p=r0;}
static void f2729(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,-1024);*r1p=r1;*r0p=r0;}
static void f2730(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,1023);*r1p=r1;*r0p=r0;}
static void f2731(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,-1025);*r1p=r1;*r0p=r0;}
static void f2732(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,2048);*r1p=r1;*r0p=r0;}
static void f2733(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,-2048);*r1p=r1;*r0p=r0;}
static void f2734(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,2047);*r1p=r1;*r0p=r0;}
static void f2735(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,-2049);*r1p=r1;*r0p=r0;}
static void f2736(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,4096);*r1p=r1;*r0p=r0;}
static void f2737(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,-4096);*r1p=r1;*r0p=r0;}
static void f2738(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,4095);*r1p=r1;*r0p=r0;}
static void f2739(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,-4097);*r1p=r1;*r0p=r0;}
static void f2740(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,8192);*r1p=r1;*r0p=r0;}
static void f2741(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,-8192);*r1p=r1;*r0p=r0;}
static void f2742(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,8191);*r1p=r1;*r0p=r0;}
static void f2743(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,-8193);*r1p=r1;*r0p=r0;}
static void f2744(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,16384);*r1p=r1;*r0p=r0;}
static void f2745(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,-16384);*r1p=r1;*r0p=r0;}
static void f2746(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,16383);*r1p=r1;*r0p=r0;}
static void f2747(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,-16385);*r1p=r1;*r0p=r0;}
static void f2748(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,32768);*r1p=r1;*r0p=r0;}
static void f2749(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,-32768);*r1p=r1;*r0p=r0;}
static void f2750(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,32767);*r1p=r1;*r0p=r0;}
static void f2751(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,1023,0,-32769);*r1p=r1;*r0p=r0;}
static void f2752(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,1);*r1p=r1;*r0p=r0;}
static void f2753(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,-1);*r1p=r1;*r0p=r0;}
static void f2754(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,0);*r1p=r1;*r0p=r0;}
static void f2755(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,-2);*r1p=r1;*r0p=r0;}
static void f2756(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,2);*r1p=r1;*r0p=r0;}
static void f2757(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,-2);*r1p=r1;*r0p=r0;}
static void f2758(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,1);*r1p=r1;*r0p=r0;}
static void f2759(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,-3);*r1p=r1;*r0p=r0;}
static void f2760(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,4);*r1p=r1;*r0p=r0;}
static void f2761(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,-4);*r1p=r1;*r0p=r0;}
static void f2762(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,3);*r1p=r1;*r0p=r0;}
static void f2763(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,-5);*r1p=r1;*r0p=r0;}
static void f2764(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,8);*r1p=r1;*r0p=r0;}
static void f2765(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,-8);*r1p=r1;*r0p=r0;}
static void f2766(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,7);*r1p=r1;*r0p=r0;}
static void f2767(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,-9);*r1p=r1;*r0p=r0;}
static void f2768(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,16);*r1p=r1;*r0p=r0;}
static void f2769(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,-16);*r1p=r1;*r0p=r0;}
static void f2770(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,15);*r1p=r1;*r0p=r0;}
static void f2771(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,-17);*r1p=r1;*r0p=r0;}
static void f2772(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,32);*r1p=r1;*r0p=r0;}
static void f2773(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,-32);*r1p=r1;*r0p=r0;}
static void f2774(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,31);*r1p=r1;*r0p=r0;}
static void f2775(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,-33);*r1p=r1;*r0p=r0;}
static void f2776(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,64);*r1p=r1;*r0p=r0;}
static void f2777(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,-64);*r1p=r1;*r0p=r0;}
static void f2778(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,63);*r1p=r1;*r0p=r0;}
static void f2779(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,-65);*r1p=r1;*r0p=r0;}
static void f2780(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,128);*r1p=r1;*r0p=r0;}
static void f2781(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,-128);*r1p=r1;*r0p=r0;}
static void f2782(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,127);*r1p=r1;*r0p=r0;}
static void f2783(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,-129);*r1p=r1;*r0p=r0;}
static void f2784(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,256);*r1p=r1;*r0p=r0;}
static void f2785(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,-256);*r1p=r1;*r0p=r0;}
static void f2786(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,255);*r1p=r1;*r0p=r0;}
static void f2787(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,-257);*r1p=r1;*r0p=r0;}
static void f2788(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,512);*r1p=r1;*r0p=r0;}
static void f2789(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,-512);*r1p=r1;*r0p=r0;}
static void f2790(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,511);*r1p=r1;*r0p=r0;}
static void f2791(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,-513);*r1p=r1;*r0p=r0;}
static void f2792(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,1024);*r1p=r1;*r0p=r0;}
static void f2793(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,-1024);*r1p=r1;*r0p=r0;}
static void f2794(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,1023);*r1p=r1;*r0p=r0;}
static void f2795(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,-1025);*r1p=r1;*r0p=r0;}
static void f2796(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,2048);*r1p=r1;*r0p=r0;}
static void f2797(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,-2048);*r1p=r1;*r0p=r0;}
static void f2798(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,2047);*r1p=r1;*r0p=r0;}
static void f2799(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,-2049);*r1p=r1;*r0p=r0;}
static void f2800(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,4096);*r1p=r1;*r0p=r0;}
static void f2801(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,-4096);*r1p=r1;*r0p=r0;}
static void f2802(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,4095);*r1p=r1;*r0p=r0;}
static void f2803(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,-4097);*r1p=r1;*r0p=r0;}
static void f2804(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,8192);*r1p=r1;*r0p=r0;}
static void f2805(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,-8192);*r1p=r1;*r0p=r0;}
static void f2806(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,8191);*r1p=r1;*r0p=r0;}
static void f2807(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,-8193);*r1p=r1;*r0p=r0;}
static void f2808(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,16384);*r1p=r1;*r0p=r0;}
static void f2809(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,-16384);*r1p=r1;*r0p=r0;}
static void f2810(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,16383);*r1p=r1;*r0p=r0;}
static void f2811(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,-16385);*r1p=r1;*r0p=r0;}
static void f2812(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,32768);*r1p=r1;*r0p=r0;}
static void f2813(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,-32768);*r1p=r1;*r0p=r0;}
static void f2814(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,32767);*r1p=r1;*r0p=r0;}
static void f2815(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-1025,0,-32769);*r1p=r1;*r0p=r0;}
static void f2816(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,1);*r1p=r1;*r0p=r0;}
static void f2817(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,-1);*r1p=r1;*r0p=r0;}
static void f2818(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,0);*r1p=r1;*r0p=r0;}
static void f2819(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,-2);*r1p=r1;*r0p=r0;}
static void f2820(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,2);*r1p=r1;*r0p=r0;}
static void f2821(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,-2);*r1p=r1;*r0p=r0;}
static void f2822(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,1);*r1p=r1;*r0p=r0;}
static void f2823(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,-3);*r1p=r1;*r0p=r0;}
static void f2824(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,4);*r1p=r1;*r0p=r0;}
static void f2825(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,-4);*r1p=r1;*r0p=r0;}
static void f2826(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,3);*r1p=r1;*r0p=r0;}
static void f2827(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,-5);*r1p=r1;*r0p=r0;}
static void f2828(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,8);*r1p=r1;*r0p=r0;}
static void f2829(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,-8);*r1p=r1;*r0p=r0;}
static void f2830(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,7);*r1p=r1;*r0p=r0;}
static void f2831(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,-9);*r1p=r1;*r0p=r0;}
static void f2832(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,16);*r1p=r1;*r0p=r0;}
static void f2833(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,-16);*r1p=r1;*r0p=r0;}
static void f2834(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,15);*r1p=r1;*r0p=r0;}
static void f2835(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,-17);*r1p=r1;*r0p=r0;}
static void f2836(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,32);*r1p=r1;*r0p=r0;}
static void f2837(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,-32);*r1p=r1;*r0p=r0;}
static void f2838(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,31);*r1p=r1;*r0p=r0;}
static void f2839(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,-33);*r1p=r1;*r0p=r0;}
static void f2840(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,64);*r1p=r1;*r0p=r0;}
static void f2841(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,-64);*r1p=r1;*r0p=r0;}
static void f2842(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,63);*r1p=r1;*r0p=r0;}
static void f2843(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,-65);*r1p=r1;*r0p=r0;}
static void f2844(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,128);*r1p=r1;*r0p=r0;}
static void f2845(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,-128);*r1p=r1;*r0p=r0;}
static void f2846(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,127);*r1p=r1;*r0p=r0;}
static void f2847(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,-129);*r1p=r1;*r0p=r0;}
static void f2848(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,256);*r1p=r1;*r0p=r0;}
static void f2849(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,-256);*r1p=r1;*r0p=r0;}
static void f2850(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,255);*r1p=r1;*r0p=r0;}
static void f2851(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,-257);*r1p=r1;*r0p=r0;}
static void f2852(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,512);*r1p=r1;*r0p=r0;}
static void f2853(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,-512);*r1p=r1;*r0p=r0;}
static void f2854(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,511);*r1p=r1;*r0p=r0;}
static void f2855(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,-513);*r1p=r1;*r0p=r0;}
static void f2856(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,1024);*r1p=r1;*r0p=r0;}
static void f2857(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,-1024);*r1p=r1;*r0p=r0;}
static void f2858(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,1023);*r1p=r1;*r0p=r0;}
static void f2859(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,-1025);*r1p=r1;*r0p=r0;}
static void f2860(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,2048);*r1p=r1;*r0p=r0;}
static void f2861(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,-2048);*r1p=r1;*r0p=r0;}
static void f2862(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,2047);*r1p=r1;*r0p=r0;}
static void f2863(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,-2049);*r1p=r1;*r0p=r0;}
static void f2864(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,4096);*r1p=r1;*r0p=r0;}
static void f2865(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,-4096);*r1p=r1;*r0p=r0;}
static void f2866(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,4095);*r1p=r1;*r0p=r0;}
static void f2867(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,-4097);*r1p=r1;*r0p=r0;}
static void f2868(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,8192);*r1p=r1;*r0p=r0;}
static void f2869(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,-8192);*r1p=r1;*r0p=r0;}
static void f2870(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,8191);*r1p=r1;*r0p=r0;}
static void f2871(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,-8193);*r1p=r1;*r0p=r0;}
static void f2872(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,16384);*r1p=r1;*r0p=r0;}
static void f2873(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,-16384);*r1p=r1;*r0p=r0;}
static void f2874(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,16383);*r1p=r1;*r0p=r0;}
static void f2875(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,-16385);*r1p=r1;*r0p=r0;}
static void f2876(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,32768);*r1p=r1;*r0p=r0;}
static void f2877(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,-32768);*r1p=r1;*r0p=r0;}
static void f2878(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,32767);*r1p=r1;*r0p=r0;}
static void f2879(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2048,0,-32769);*r1p=r1;*r0p=r0;}
static void f2880(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,1);*r1p=r1;*r0p=r0;}
static void f2881(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,-1);*r1p=r1;*r0p=r0;}
static void f2882(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,0);*r1p=r1;*r0p=r0;}
static void f2883(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,-2);*r1p=r1;*r0p=r0;}
static void f2884(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,2);*r1p=r1;*r0p=r0;}
static void f2885(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,-2);*r1p=r1;*r0p=r0;}
static void f2886(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,1);*r1p=r1;*r0p=r0;}
static void f2887(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,-3);*r1p=r1;*r0p=r0;}
static void f2888(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,4);*r1p=r1;*r0p=r0;}
static void f2889(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,-4);*r1p=r1;*r0p=r0;}
static void f2890(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,3);*r1p=r1;*r0p=r0;}
static void f2891(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,-5);*r1p=r1;*r0p=r0;}
static void f2892(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,8);*r1p=r1;*r0p=r0;}
static void f2893(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,-8);*r1p=r1;*r0p=r0;}
static void f2894(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,7);*r1p=r1;*r0p=r0;}
static void f2895(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,-9);*r1p=r1;*r0p=r0;}
static void f2896(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,16);*r1p=r1;*r0p=r0;}
static void f2897(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,-16);*r1p=r1;*r0p=r0;}
static void f2898(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,15);*r1p=r1;*r0p=r0;}
static void f2899(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,-17);*r1p=r1;*r0p=r0;}
static void f2900(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,32);*r1p=r1;*r0p=r0;}
static void f2901(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,-32);*r1p=r1;*r0p=r0;}
static void f2902(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,31);*r1p=r1;*r0p=r0;}
static void f2903(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,-33);*r1p=r1;*r0p=r0;}
static void f2904(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,64);*r1p=r1;*r0p=r0;}
static void f2905(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,-64);*r1p=r1;*r0p=r0;}
static void f2906(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,63);*r1p=r1;*r0p=r0;}
static void f2907(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,-65);*r1p=r1;*r0p=r0;}
static void f2908(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,128);*r1p=r1;*r0p=r0;}
static void f2909(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,-128);*r1p=r1;*r0p=r0;}
static void f2910(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,127);*r1p=r1;*r0p=r0;}
static void f2911(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,-129);*r1p=r1;*r0p=r0;}
static void f2912(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,256);*r1p=r1;*r0p=r0;}
static void f2913(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,-256);*r1p=r1;*r0p=r0;}
static void f2914(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,255);*r1p=r1;*r0p=r0;}
static void f2915(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,-257);*r1p=r1;*r0p=r0;}
static void f2916(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,512);*r1p=r1;*r0p=r0;}
static void f2917(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,-512);*r1p=r1;*r0p=r0;}
static void f2918(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,511);*r1p=r1;*r0p=r0;}
static void f2919(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,-513);*r1p=r1;*r0p=r0;}
static void f2920(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,1024);*r1p=r1;*r0p=r0;}
static void f2921(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,-1024);*r1p=r1;*r0p=r0;}
static void f2922(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,1023);*r1p=r1;*r0p=r0;}
static void f2923(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,-1025);*r1p=r1;*r0p=r0;}
static void f2924(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,2048);*r1p=r1;*r0p=r0;}
static void f2925(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,-2048);*r1p=r1;*r0p=r0;}
static void f2926(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,2047);*r1p=r1;*r0p=r0;}
static void f2927(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,-2049);*r1p=r1;*r0p=r0;}
static void f2928(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,4096);*r1p=r1;*r0p=r0;}
static void f2929(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,-4096);*r1p=r1;*r0p=r0;}
static void f2930(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,4095);*r1p=r1;*r0p=r0;}
static void f2931(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,-4097);*r1p=r1;*r0p=r0;}
static void f2932(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,8192);*r1p=r1;*r0p=r0;}
static void f2933(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,-8192);*r1p=r1;*r0p=r0;}
static void f2934(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,8191);*r1p=r1;*r0p=r0;}
static void f2935(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,-8193);*r1p=r1;*r0p=r0;}
static void f2936(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,16384);*r1p=r1;*r0p=r0;}
static void f2937(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,-16384);*r1p=r1;*r0p=r0;}
static void f2938(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,16383);*r1p=r1;*r0p=r0;}
static void f2939(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,-16385);*r1p=r1;*r0p=r0;}
static void f2940(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,32768);*r1p=r1;*r0p=r0;}
static void f2941(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,-32768);*r1p=r1;*r0p=r0;}
static void f2942(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,32767);*r1p=r1;*r0p=r0;}
static void f2943(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2048,0,-32769);*r1p=r1;*r0p=r0;}
static void f2944(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,1);*r1p=r1;*r0p=r0;}
static void f2945(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,-1);*r1p=r1;*r0p=r0;}
static void f2946(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,0);*r1p=r1;*r0p=r0;}
static void f2947(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,-2);*r1p=r1;*r0p=r0;}
static void f2948(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,2);*r1p=r1;*r0p=r0;}
static void f2949(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,-2);*r1p=r1;*r0p=r0;}
static void f2950(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,1);*r1p=r1;*r0p=r0;}
static void f2951(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,-3);*r1p=r1;*r0p=r0;}
static void f2952(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,4);*r1p=r1;*r0p=r0;}
static void f2953(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,-4);*r1p=r1;*r0p=r0;}
static void f2954(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,3);*r1p=r1;*r0p=r0;}
static void f2955(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,-5);*r1p=r1;*r0p=r0;}
static void f2956(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,8);*r1p=r1;*r0p=r0;}
static void f2957(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,-8);*r1p=r1;*r0p=r0;}
static void f2958(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,7);*r1p=r1;*r0p=r0;}
static void f2959(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,-9);*r1p=r1;*r0p=r0;}
static void f2960(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,16);*r1p=r1;*r0p=r0;}
static void f2961(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,-16);*r1p=r1;*r0p=r0;}
static void f2962(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,15);*r1p=r1;*r0p=r0;}
static void f2963(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,-17);*r1p=r1;*r0p=r0;}
static void f2964(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,32);*r1p=r1;*r0p=r0;}
static void f2965(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,-32);*r1p=r1;*r0p=r0;}
static void f2966(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,31);*r1p=r1;*r0p=r0;}
static void f2967(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,-33);*r1p=r1;*r0p=r0;}
static void f2968(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,64);*r1p=r1;*r0p=r0;}
static void f2969(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,-64);*r1p=r1;*r0p=r0;}
static void f2970(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,63);*r1p=r1;*r0p=r0;}
static void f2971(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,-65);*r1p=r1;*r0p=r0;}
static void f2972(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,128);*r1p=r1;*r0p=r0;}
static void f2973(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,-128);*r1p=r1;*r0p=r0;}
static void f2974(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,127);*r1p=r1;*r0p=r0;}
static void f2975(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,-129);*r1p=r1;*r0p=r0;}
static void f2976(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,256);*r1p=r1;*r0p=r0;}
static void f2977(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,-256);*r1p=r1;*r0p=r0;}
static void f2978(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,255);*r1p=r1;*r0p=r0;}
static void f2979(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,-257);*r1p=r1;*r0p=r0;}
static void f2980(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,512);*r1p=r1;*r0p=r0;}
static void f2981(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,-512);*r1p=r1;*r0p=r0;}
static void f2982(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,511);*r1p=r1;*r0p=r0;}
static void f2983(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,-513);*r1p=r1;*r0p=r0;}
static void f2984(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,1024);*r1p=r1;*r0p=r0;}
static void f2985(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,-1024);*r1p=r1;*r0p=r0;}
static void f2986(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,1023);*r1p=r1;*r0p=r0;}
static void f2987(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,-1025);*r1p=r1;*r0p=r0;}
static void f2988(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,2048);*r1p=r1;*r0p=r0;}
static void f2989(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,-2048);*r1p=r1;*r0p=r0;}
static void f2990(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,2047);*r1p=r1;*r0p=r0;}
static void f2991(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,-2049);*r1p=r1;*r0p=r0;}
static void f2992(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,4096);*r1p=r1;*r0p=r0;}
static void f2993(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,-4096);*r1p=r1;*r0p=r0;}
static void f2994(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,4095);*r1p=r1;*r0p=r0;}
static void f2995(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,-4097);*r1p=r1;*r0p=r0;}
static void f2996(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,8192);*r1p=r1;*r0p=r0;}
static void f2997(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,-8192);*r1p=r1;*r0p=r0;}
static void f2998(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,8191);*r1p=r1;*r0p=r0;}
static void f2999(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,-8193);*r1p=r1;*r0p=r0;}
static void f3000(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,16384);*r1p=r1;*r0p=r0;}
static void f3001(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,-16384);*r1p=r1;*r0p=r0;}
static void f3002(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,16383);*r1p=r1;*r0p=r0;}
static void f3003(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,-16385);*r1p=r1;*r0p=r0;}
static void f3004(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,32768);*r1p=r1;*r0p=r0;}
static void f3005(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,-32768);*r1p=r1;*r0p=r0;}
static void f3006(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,32767);*r1p=r1;*r0p=r0;}
static void f3007(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,2047,0,-32769);*r1p=r1;*r0p=r0;}
static void f3008(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,1);*r1p=r1;*r0p=r0;}
static void f3009(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,-1);*r1p=r1;*r0p=r0;}
static void f3010(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,0);*r1p=r1;*r0p=r0;}
static void f3011(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,-2);*r1p=r1;*r0p=r0;}
static void f3012(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,2);*r1p=r1;*r0p=r0;}
static void f3013(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,-2);*r1p=r1;*r0p=r0;}
static void f3014(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,1);*r1p=r1;*r0p=r0;}
static void f3015(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,-3);*r1p=r1;*r0p=r0;}
static void f3016(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,4);*r1p=r1;*r0p=r0;}
static void f3017(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,-4);*r1p=r1;*r0p=r0;}
static void f3018(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,3);*r1p=r1;*r0p=r0;}
static void f3019(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,-5);*r1p=r1;*r0p=r0;}
static void f3020(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,8);*r1p=r1;*r0p=r0;}
static void f3021(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,-8);*r1p=r1;*r0p=r0;}
static void f3022(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,7);*r1p=r1;*r0p=r0;}
static void f3023(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,-9);*r1p=r1;*r0p=r0;}
static void f3024(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,16);*r1p=r1;*r0p=r0;}
static void f3025(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,-16);*r1p=r1;*r0p=r0;}
static void f3026(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,15);*r1p=r1;*r0p=r0;}
static void f3027(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,-17);*r1p=r1;*r0p=r0;}
static void f3028(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,32);*r1p=r1;*r0p=r0;}
static void f3029(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,-32);*r1p=r1;*r0p=r0;}
static void f3030(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,31);*r1p=r1;*r0p=r0;}
static void f3031(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,-33);*r1p=r1;*r0p=r0;}
static void f3032(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,64);*r1p=r1;*r0p=r0;}
static void f3033(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,-64);*r1p=r1;*r0p=r0;}
static void f3034(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,63);*r1p=r1;*r0p=r0;}
static void f3035(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,-65);*r1p=r1;*r0p=r0;}
static void f3036(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,128);*r1p=r1;*r0p=r0;}
static void f3037(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,-128);*r1p=r1;*r0p=r0;}
static void f3038(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,127);*r1p=r1;*r0p=r0;}
static void f3039(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,-129);*r1p=r1;*r0p=r0;}
static void f3040(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,256);*r1p=r1;*r0p=r0;}
static void f3041(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,-256);*r1p=r1;*r0p=r0;}
static void f3042(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,255);*r1p=r1;*r0p=r0;}
static void f3043(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,-257);*r1p=r1;*r0p=r0;}
static void f3044(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,512);*r1p=r1;*r0p=r0;}
static void f3045(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,-512);*r1p=r1;*r0p=r0;}
static void f3046(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,511);*r1p=r1;*r0p=r0;}
static void f3047(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,-513);*r1p=r1;*r0p=r0;}
static void f3048(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,1024);*r1p=r1;*r0p=r0;}
static void f3049(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,-1024);*r1p=r1;*r0p=r0;}
static void f3050(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,1023);*r1p=r1;*r0p=r0;}
static void f3051(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,-1025);*r1p=r1;*r0p=r0;}
static void f3052(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,2048);*r1p=r1;*r0p=r0;}
static void f3053(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,-2048);*r1p=r1;*r0p=r0;}
static void f3054(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,2047);*r1p=r1;*r0p=r0;}
static void f3055(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,-2049);*r1p=r1;*r0p=r0;}
static void f3056(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,4096);*r1p=r1;*r0p=r0;}
static void f3057(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,-4096);*r1p=r1;*r0p=r0;}
static void f3058(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,4095);*r1p=r1;*r0p=r0;}
static void f3059(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,-4097);*r1p=r1;*r0p=r0;}
static void f3060(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,8192);*r1p=r1;*r0p=r0;}
static void f3061(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,-8192);*r1p=r1;*r0p=r0;}
static void f3062(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,8191);*r1p=r1;*r0p=r0;}
static void f3063(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,-8193);*r1p=r1;*r0p=r0;}
static void f3064(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,16384);*r1p=r1;*r0p=r0;}
static void f3065(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,-16384);*r1p=r1;*r0p=r0;}
static void f3066(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,16383);*r1p=r1;*r0p=r0;}
static void f3067(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,-16385);*r1p=r1;*r0p=r0;}
static void f3068(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,32768);*r1p=r1;*r0p=r0;}
static void f3069(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,-32768);*r1p=r1;*r0p=r0;}
static void f3070(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,32767);*r1p=r1;*r0p=r0;}
static void f3071(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-2049,0,-32769);*r1p=r1;*r0p=r0;}
static void f3072(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,1);*r1p=r1;*r0p=r0;}
static void f3073(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,-1);*r1p=r1;*r0p=r0;}
static void f3074(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,0);*r1p=r1;*r0p=r0;}
static void f3075(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,-2);*r1p=r1;*r0p=r0;}
static void f3076(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,2);*r1p=r1;*r0p=r0;}
static void f3077(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,-2);*r1p=r1;*r0p=r0;}
static void f3078(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,1);*r1p=r1;*r0p=r0;}
static void f3079(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,-3);*r1p=r1;*r0p=r0;}
static void f3080(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,4);*r1p=r1;*r0p=r0;}
static void f3081(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,-4);*r1p=r1;*r0p=r0;}
static void f3082(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,3);*r1p=r1;*r0p=r0;}
static void f3083(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,-5);*r1p=r1;*r0p=r0;}
static void f3084(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,8);*r1p=r1;*r0p=r0;}
static void f3085(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,-8);*r1p=r1;*r0p=r0;}
static void f3086(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,7);*r1p=r1;*r0p=r0;}
static void f3087(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,-9);*r1p=r1;*r0p=r0;}
static void f3088(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,16);*r1p=r1;*r0p=r0;}
static void f3089(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,-16);*r1p=r1;*r0p=r0;}
static void f3090(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,15);*r1p=r1;*r0p=r0;}
static void f3091(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,-17);*r1p=r1;*r0p=r0;}
static void f3092(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,32);*r1p=r1;*r0p=r0;}
static void f3093(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,-32);*r1p=r1;*r0p=r0;}
static void f3094(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,31);*r1p=r1;*r0p=r0;}
static void f3095(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,-33);*r1p=r1;*r0p=r0;}
static void f3096(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,64);*r1p=r1;*r0p=r0;}
static void f3097(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,-64);*r1p=r1;*r0p=r0;}
static void f3098(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,63);*r1p=r1;*r0p=r0;}
static void f3099(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,-65);*r1p=r1;*r0p=r0;}
static void f3100(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,128);*r1p=r1;*r0p=r0;}
static void f3101(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,-128);*r1p=r1;*r0p=r0;}
static void f3102(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,127);*r1p=r1;*r0p=r0;}
static void f3103(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,-129);*r1p=r1;*r0p=r0;}
static void f3104(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,256);*r1p=r1;*r0p=r0;}
static void f3105(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,-256);*r1p=r1;*r0p=r0;}
static void f3106(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,255);*r1p=r1;*r0p=r0;}
static void f3107(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,-257);*r1p=r1;*r0p=r0;}
static void f3108(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,512);*r1p=r1;*r0p=r0;}
static void f3109(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,-512);*r1p=r1;*r0p=r0;}
static void f3110(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,511);*r1p=r1;*r0p=r0;}
static void f3111(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,-513);*r1p=r1;*r0p=r0;}
static void f3112(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,1024);*r1p=r1;*r0p=r0;}
static void f3113(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,-1024);*r1p=r1;*r0p=r0;}
static void f3114(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,1023);*r1p=r1;*r0p=r0;}
static void f3115(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,-1025);*r1p=r1;*r0p=r0;}
static void f3116(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,2048);*r1p=r1;*r0p=r0;}
static void f3117(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,-2048);*r1p=r1;*r0p=r0;}
static void f3118(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,2047);*r1p=r1;*r0p=r0;}
static void f3119(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,-2049);*r1p=r1;*r0p=r0;}
static void f3120(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,4096);*r1p=r1;*r0p=r0;}
static void f3121(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,-4096);*r1p=r1;*r0p=r0;}
static void f3122(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,4095);*r1p=r1;*r0p=r0;}
static void f3123(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,-4097);*r1p=r1;*r0p=r0;}
static void f3124(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,8192);*r1p=r1;*r0p=r0;}
static void f3125(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,-8192);*r1p=r1;*r0p=r0;}
static void f3126(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,8191);*r1p=r1;*r0p=r0;}
static void f3127(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,-8193);*r1p=r1;*r0p=r0;}
static void f3128(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,16384);*r1p=r1;*r0p=r0;}
static void f3129(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,-16384);*r1p=r1;*r0p=r0;}
static void f3130(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,16383);*r1p=r1;*r0p=r0;}
static void f3131(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,-16385);*r1p=r1;*r0p=r0;}
static void f3132(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,32768);*r1p=r1;*r0p=r0;}
static void f3133(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,-32768);*r1p=r1;*r0p=r0;}
static void f3134(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,32767);*r1p=r1;*r0p=r0;}
static void f3135(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4096,0,-32769);*r1p=r1;*r0p=r0;}
static void f3136(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,1);*r1p=r1;*r0p=r0;}
static void f3137(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,-1);*r1p=r1;*r0p=r0;}
static void f3138(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,0);*r1p=r1;*r0p=r0;}
static void f3139(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,-2);*r1p=r1;*r0p=r0;}
static void f3140(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,2);*r1p=r1;*r0p=r0;}
static void f3141(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,-2);*r1p=r1;*r0p=r0;}
static void f3142(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,1);*r1p=r1;*r0p=r0;}
static void f3143(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,-3);*r1p=r1;*r0p=r0;}
static void f3144(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,4);*r1p=r1;*r0p=r0;}
static void f3145(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,-4);*r1p=r1;*r0p=r0;}
static void f3146(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,3);*r1p=r1;*r0p=r0;}
static void f3147(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,-5);*r1p=r1;*r0p=r0;}
static void f3148(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,8);*r1p=r1;*r0p=r0;}
static void f3149(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,-8);*r1p=r1;*r0p=r0;}
static void f3150(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,7);*r1p=r1;*r0p=r0;}
static void f3151(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,-9);*r1p=r1;*r0p=r0;}
static void f3152(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,16);*r1p=r1;*r0p=r0;}
static void f3153(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,-16);*r1p=r1;*r0p=r0;}
static void f3154(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,15);*r1p=r1;*r0p=r0;}
static void f3155(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,-17);*r1p=r1;*r0p=r0;}
static void f3156(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,32);*r1p=r1;*r0p=r0;}
static void f3157(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,-32);*r1p=r1;*r0p=r0;}
static void f3158(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,31);*r1p=r1;*r0p=r0;}
static void f3159(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,-33);*r1p=r1;*r0p=r0;}
static void f3160(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,64);*r1p=r1;*r0p=r0;}
static void f3161(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,-64);*r1p=r1;*r0p=r0;}
static void f3162(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,63);*r1p=r1;*r0p=r0;}
static void f3163(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,-65);*r1p=r1;*r0p=r0;}
static void f3164(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,128);*r1p=r1;*r0p=r0;}
static void f3165(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,-128);*r1p=r1;*r0p=r0;}
static void f3166(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,127);*r1p=r1;*r0p=r0;}
static void f3167(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,-129);*r1p=r1;*r0p=r0;}
static void f3168(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,256);*r1p=r1;*r0p=r0;}
static void f3169(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,-256);*r1p=r1;*r0p=r0;}
static void f3170(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,255);*r1p=r1;*r0p=r0;}
static void f3171(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,-257);*r1p=r1;*r0p=r0;}
static void f3172(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,512);*r1p=r1;*r0p=r0;}
static void f3173(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,-512);*r1p=r1;*r0p=r0;}
static void f3174(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,511);*r1p=r1;*r0p=r0;}
static void f3175(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,-513);*r1p=r1;*r0p=r0;}
static void f3176(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,1024);*r1p=r1;*r0p=r0;}
static void f3177(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,-1024);*r1p=r1;*r0p=r0;}
static void f3178(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,1023);*r1p=r1;*r0p=r0;}
static void f3179(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,-1025);*r1p=r1;*r0p=r0;}
static void f3180(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,2048);*r1p=r1;*r0p=r0;}
static void f3181(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,-2048);*r1p=r1;*r0p=r0;}
static void f3182(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,2047);*r1p=r1;*r0p=r0;}
static void f3183(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,-2049);*r1p=r1;*r0p=r0;}
static void f3184(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,4096);*r1p=r1;*r0p=r0;}
static void f3185(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,-4096);*r1p=r1;*r0p=r0;}
static void f3186(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,4095);*r1p=r1;*r0p=r0;}
static void f3187(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,-4097);*r1p=r1;*r0p=r0;}
static void f3188(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,8192);*r1p=r1;*r0p=r0;}
static void f3189(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,-8192);*r1p=r1;*r0p=r0;}
static void f3190(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,8191);*r1p=r1;*r0p=r0;}
static void f3191(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,-8193);*r1p=r1;*r0p=r0;}
static void f3192(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,16384);*r1p=r1;*r0p=r0;}
static void f3193(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,-16384);*r1p=r1;*r0p=r0;}
static void f3194(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,16383);*r1p=r1;*r0p=r0;}
static void f3195(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,-16385);*r1p=r1;*r0p=r0;}
static void f3196(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,32768);*r1p=r1;*r0p=r0;}
static void f3197(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,-32768);*r1p=r1;*r0p=r0;}
static void f3198(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,32767);*r1p=r1;*r0p=r0;}
static void f3199(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4096,0,-32769);*r1p=r1;*r0p=r0;}
static void f3200(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,1);*r1p=r1;*r0p=r0;}
static void f3201(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,-1);*r1p=r1;*r0p=r0;}
static void f3202(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,0);*r1p=r1;*r0p=r0;}
static void f3203(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,-2);*r1p=r1;*r0p=r0;}
static void f3204(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,2);*r1p=r1;*r0p=r0;}
static void f3205(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,-2);*r1p=r1;*r0p=r0;}
static void f3206(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,1);*r1p=r1;*r0p=r0;}
static void f3207(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,-3);*r1p=r1;*r0p=r0;}
static void f3208(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,4);*r1p=r1;*r0p=r0;}
static void f3209(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,-4);*r1p=r1;*r0p=r0;}
static void f3210(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,3);*r1p=r1;*r0p=r0;}
static void f3211(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,-5);*r1p=r1;*r0p=r0;}
static void f3212(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,8);*r1p=r1;*r0p=r0;}
static void f3213(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,-8);*r1p=r1;*r0p=r0;}
static void f3214(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,7);*r1p=r1;*r0p=r0;}
static void f3215(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,-9);*r1p=r1;*r0p=r0;}
static void f3216(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,16);*r1p=r1;*r0p=r0;}
static void f3217(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,-16);*r1p=r1;*r0p=r0;}
static void f3218(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,15);*r1p=r1;*r0p=r0;}
static void f3219(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,-17);*r1p=r1;*r0p=r0;}
static void f3220(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,32);*r1p=r1;*r0p=r0;}
static void f3221(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,-32);*r1p=r1;*r0p=r0;}
static void f3222(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,31);*r1p=r1;*r0p=r0;}
static void f3223(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,-33);*r1p=r1;*r0p=r0;}
static void f3224(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,64);*r1p=r1;*r0p=r0;}
static void f3225(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,-64);*r1p=r1;*r0p=r0;}
static void f3226(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,63);*r1p=r1;*r0p=r0;}
static void f3227(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,-65);*r1p=r1;*r0p=r0;}
static void f3228(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,128);*r1p=r1;*r0p=r0;}
static void f3229(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,-128);*r1p=r1;*r0p=r0;}
static void f3230(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,127);*r1p=r1;*r0p=r0;}
static void f3231(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,-129);*r1p=r1;*r0p=r0;}
static void f3232(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,256);*r1p=r1;*r0p=r0;}
static void f3233(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,-256);*r1p=r1;*r0p=r0;}
static void f3234(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,255);*r1p=r1;*r0p=r0;}
static void f3235(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,-257);*r1p=r1;*r0p=r0;}
static void f3236(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,512);*r1p=r1;*r0p=r0;}
static void f3237(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,-512);*r1p=r1;*r0p=r0;}
static void f3238(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,511);*r1p=r1;*r0p=r0;}
static void f3239(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,-513);*r1p=r1;*r0p=r0;}
static void f3240(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,1024);*r1p=r1;*r0p=r0;}
static void f3241(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,-1024);*r1p=r1;*r0p=r0;}
static void f3242(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,1023);*r1p=r1;*r0p=r0;}
static void f3243(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,-1025);*r1p=r1;*r0p=r0;}
static void f3244(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,2048);*r1p=r1;*r0p=r0;}
static void f3245(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,-2048);*r1p=r1;*r0p=r0;}
static void f3246(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,2047);*r1p=r1;*r0p=r0;}
static void f3247(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,-2049);*r1p=r1;*r0p=r0;}
static void f3248(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,4096);*r1p=r1;*r0p=r0;}
static void f3249(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,-4096);*r1p=r1;*r0p=r0;}
static void f3250(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,4095);*r1p=r1;*r0p=r0;}
static void f3251(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,-4097);*r1p=r1;*r0p=r0;}
static void f3252(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,8192);*r1p=r1;*r0p=r0;}
static void f3253(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,-8192);*r1p=r1;*r0p=r0;}
static void f3254(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,8191);*r1p=r1;*r0p=r0;}
static void f3255(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,-8193);*r1p=r1;*r0p=r0;}
static void f3256(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,16384);*r1p=r1;*r0p=r0;}
static void f3257(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,-16384);*r1p=r1;*r0p=r0;}
static void f3258(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,16383);*r1p=r1;*r0p=r0;}
static void f3259(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,-16385);*r1p=r1;*r0p=r0;}
static void f3260(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,32768);*r1p=r1;*r0p=r0;}
static void f3261(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,-32768);*r1p=r1;*r0p=r0;}
static void f3262(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,32767);*r1p=r1;*r0p=r0;}
static void f3263(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,4095,0,-32769);*r1p=r1;*r0p=r0;}
static void f3264(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,1);*r1p=r1;*r0p=r0;}
static void f3265(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,-1);*r1p=r1;*r0p=r0;}
static void f3266(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,0);*r1p=r1;*r0p=r0;}
static void f3267(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,-2);*r1p=r1;*r0p=r0;}
static void f3268(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,2);*r1p=r1;*r0p=r0;}
static void f3269(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,-2);*r1p=r1;*r0p=r0;}
static void f3270(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,1);*r1p=r1;*r0p=r0;}
static void f3271(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,-3);*r1p=r1;*r0p=r0;}
static void f3272(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,4);*r1p=r1;*r0p=r0;}
static void f3273(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,-4);*r1p=r1;*r0p=r0;}
static void f3274(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,3);*r1p=r1;*r0p=r0;}
static void f3275(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,-5);*r1p=r1;*r0p=r0;}
static void f3276(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,8);*r1p=r1;*r0p=r0;}
static void f3277(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,-8);*r1p=r1;*r0p=r0;}
static void f3278(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,7);*r1p=r1;*r0p=r0;}
static void f3279(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,-9);*r1p=r1;*r0p=r0;}
static void f3280(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,16);*r1p=r1;*r0p=r0;}
static void f3281(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,-16);*r1p=r1;*r0p=r0;}
static void f3282(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,15);*r1p=r1;*r0p=r0;}
static void f3283(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,-17);*r1p=r1;*r0p=r0;}
static void f3284(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,32);*r1p=r1;*r0p=r0;}
static void f3285(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,-32);*r1p=r1;*r0p=r0;}
static void f3286(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,31);*r1p=r1;*r0p=r0;}
static void f3287(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,-33);*r1p=r1;*r0p=r0;}
static void f3288(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,64);*r1p=r1;*r0p=r0;}
static void f3289(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,-64);*r1p=r1;*r0p=r0;}
static void f3290(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,63);*r1p=r1;*r0p=r0;}
static void f3291(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,-65);*r1p=r1;*r0p=r0;}
static void f3292(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,128);*r1p=r1;*r0p=r0;}
static void f3293(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,-128);*r1p=r1;*r0p=r0;}
static void f3294(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,127);*r1p=r1;*r0p=r0;}
static void f3295(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,-129);*r1p=r1;*r0p=r0;}
static void f3296(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,256);*r1p=r1;*r0p=r0;}
static void f3297(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,-256);*r1p=r1;*r0p=r0;}
static void f3298(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,255);*r1p=r1;*r0p=r0;}
static void f3299(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,-257);*r1p=r1;*r0p=r0;}
static void f3300(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,512);*r1p=r1;*r0p=r0;}
static void f3301(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,-512);*r1p=r1;*r0p=r0;}
static void f3302(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,511);*r1p=r1;*r0p=r0;}
static void f3303(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,-513);*r1p=r1;*r0p=r0;}
static void f3304(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,1024);*r1p=r1;*r0p=r0;}
static void f3305(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,-1024);*r1p=r1;*r0p=r0;}
static void f3306(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,1023);*r1p=r1;*r0p=r0;}
static void f3307(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,-1025);*r1p=r1;*r0p=r0;}
static void f3308(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,2048);*r1p=r1;*r0p=r0;}
static void f3309(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,-2048);*r1p=r1;*r0p=r0;}
static void f3310(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,2047);*r1p=r1;*r0p=r0;}
static void f3311(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,-2049);*r1p=r1;*r0p=r0;}
static void f3312(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,4096);*r1p=r1;*r0p=r0;}
static void f3313(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,-4096);*r1p=r1;*r0p=r0;}
static void f3314(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,4095);*r1p=r1;*r0p=r0;}
static void f3315(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,-4097);*r1p=r1;*r0p=r0;}
static void f3316(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,8192);*r1p=r1;*r0p=r0;}
static void f3317(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,-8192);*r1p=r1;*r0p=r0;}
static void f3318(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,8191);*r1p=r1;*r0p=r0;}
static void f3319(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,-8193);*r1p=r1;*r0p=r0;}
static void f3320(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,16384);*r1p=r1;*r0p=r0;}
static void f3321(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,-16384);*r1p=r1;*r0p=r0;}
static void f3322(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,16383);*r1p=r1;*r0p=r0;}
static void f3323(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,-16385);*r1p=r1;*r0p=r0;}
static void f3324(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,32768);*r1p=r1;*r0p=r0;}
static void f3325(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,-32768);*r1p=r1;*r0p=r0;}
static void f3326(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,32767);*r1p=r1;*r0p=r0;}
static void f3327(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-4097,0,-32769);*r1p=r1;*r0p=r0;}
static void f3328(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,1);*r1p=r1;*r0p=r0;}
static void f3329(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,-1);*r1p=r1;*r0p=r0;}
static void f3330(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,0);*r1p=r1;*r0p=r0;}
static void f3331(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,-2);*r1p=r1;*r0p=r0;}
static void f3332(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,2);*r1p=r1;*r0p=r0;}
static void f3333(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,-2);*r1p=r1;*r0p=r0;}
static void f3334(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,1);*r1p=r1;*r0p=r0;}
static void f3335(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,-3);*r1p=r1;*r0p=r0;}
static void f3336(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,4);*r1p=r1;*r0p=r0;}
static void f3337(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,-4);*r1p=r1;*r0p=r0;}
static void f3338(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,3);*r1p=r1;*r0p=r0;}
static void f3339(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,-5);*r1p=r1;*r0p=r0;}
static void f3340(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,8);*r1p=r1;*r0p=r0;}
static void f3341(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,-8);*r1p=r1;*r0p=r0;}
static void f3342(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,7);*r1p=r1;*r0p=r0;}
static void f3343(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,-9);*r1p=r1;*r0p=r0;}
static void f3344(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,16);*r1p=r1;*r0p=r0;}
static void f3345(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,-16);*r1p=r1;*r0p=r0;}
static void f3346(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,15);*r1p=r1;*r0p=r0;}
static void f3347(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,-17);*r1p=r1;*r0p=r0;}
static void f3348(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,32);*r1p=r1;*r0p=r0;}
static void f3349(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,-32);*r1p=r1;*r0p=r0;}
static void f3350(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,31);*r1p=r1;*r0p=r0;}
static void f3351(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,-33);*r1p=r1;*r0p=r0;}
static void f3352(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,64);*r1p=r1;*r0p=r0;}
static void f3353(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,-64);*r1p=r1;*r0p=r0;}
static void f3354(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,63);*r1p=r1;*r0p=r0;}
static void f3355(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,-65);*r1p=r1;*r0p=r0;}
static void f3356(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,128);*r1p=r1;*r0p=r0;}
static void f3357(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,-128);*r1p=r1;*r0p=r0;}
static void f3358(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,127);*r1p=r1;*r0p=r0;}
static void f3359(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,-129);*r1p=r1;*r0p=r0;}
static void f3360(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,256);*r1p=r1;*r0p=r0;}
static void f3361(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,-256);*r1p=r1;*r0p=r0;}
static void f3362(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,255);*r1p=r1;*r0p=r0;}
static void f3363(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,-257);*r1p=r1;*r0p=r0;}
static void f3364(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,512);*r1p=r1;*r0p=r0;}
static void f3365(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,-512);*r1p=r1;*r0p=r0;}
static void f3366(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,511);*r1p=r1;*r0p=r0;}
static void f3367(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,-513);*r1p=r1;*r0p=r0;}
static void f3368(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,1024);*r1p=r1;*r0p=r0;}
static void f3369(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,-1024);*r1p=r1;*r0p=r0;}
static void f3370(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,1023);*r1p=r1;*r0p=r0;}
static void f3371(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,-1025);*r1p=r1;*r0p=r0;}
static void f3372(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,2048);*r1p=r1;*r0p=r0;}
static void f3373(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,-2048);*r1p=r1;*r0p=r0;}
static void f3374(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,2047);*r1p=r1;*r0p=r0;}
static void f3375(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,-2049);*r1p=r1;*r0p=r0;}
static void f3376(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,4096);*r1p=r1;*r0p=r0;}
static void f3377(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,-4096);*r1p=r1;*r0p=r0;}
static void f3378(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,4095);*r1p=r1;*r0p=r0;}
static void f3379(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,-4097);*r1p=r1;*r0p=r0;}
static void f3380(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,8192);*r1p=r1;*r0p=r0;}
static void f3381(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,-8192);*r1p=r1;*r0p=r0;}
static void f3382(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,8191);*r1p=r1;*r0p=r0;}
static void f3383(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,-8193);*r1p=r1;*r0p=r0;}
static void f3384(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,16384);*r1p=r1;*r0p=r0;}
static void f3385(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,-16384);*r1p=r1;*r0p=r0;}
static void f3386(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,16383);*r1p=r1;*r0p=r0;}
static void f3387(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,-16385);*r1p=r1;*r0p=r0;}
static void f3388(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,32768);*r1p=r1;*r0p=r0;}
static void f3389(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,-32768);*r1p=r1;*r0p=r0;}
static void f3390(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,32767);*r1p=r1;*r0p=r0;}
static void f3391(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8192,0,-32769);*r1p=r1;*r0p=r0;}
static void f3392(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,1);*r1p=r1;*r0p=r0;}
static void f3393(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,-1);*r1p=r1;*r0p=r0;}
static void f3394(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,0);*r1p=r1;*r0p=r0;}
static void f3395(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,-2);*r1p=r1;*r0p=r0;}
static void f3396(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,2);*r1p=r1;*r0p=r0;}
static void f3397(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,-2);*r1p=r1;*r0p=r0;}
static void f3398(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,1);*r1p=r1;*r0p=r0;}
static void f3399(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,-3);*r1p=r1;*r0p=r0;}
static void f3400(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,4);*r1p=r1;*r0p=r0;}
static void f3401(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,-4);*r1p=r1;*r0p=r0;}
static void f3402(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,3);*r1p=r1;*r0p=r0;}
static void f3403(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,-5);*r1p=r1;*r0p=r0;}
static void f3404(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,8);*r1p=r1;*r0p=r0;}
static void f3405(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,-8);*r1p=r1;*r0p=r0;}
static void f3406(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,7);*r1p=r1;*r0p=r0;}
static void f3407(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,-9);*r1p=r1;*r0p=r0;}
static void f3408(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,16);*r1p=r1;*r0p=r0;}
static void f3409(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,-16);*r1p=r1;*r0p=r0;}
static void f3410(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,15);*r1p=r1;*r0p=r0;}
static void f3411(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,-17);*r1p=r1;*r0p=r0;}
static void f3412(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,32);*r1p=r1;*r0p=r0;}
static void f3413(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,-32);*r1p=r1;*r0p=r0;}
static void f3414(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,31);*r1p=r1;*r0p=r0;}
static void f3415(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,-33);*r1p=r1;*r0p=r0;}
static void f3416(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,64);*r1p=r1;*r0p=r0;}
static void f3417(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,-64);*r1p=r1;*r0p=r0;}
static void f3418(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,63);*r1p=r1;*r0p=r0;}
static void f3419(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,-65);*r1p=r1;*r0p=r0;}
static void f3420(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,128);*r1p=r1;*r0p=r0;}
static void f3421(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,-128);*r1p=r1;*r0p=r0;}
static void f3422(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,127);*r1p=r1;*r0p=r0;}
static void f3423(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,-129);*r1p=r1;*r0p=r0;}
static void f3424(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,256);*r1p=r1;*r0p=r0;}
static void f3425(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,-256);*r1p=r1;*r0p=r0;}
static void f3426(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,255);*r1p=r1;*r0p=r0;}
static void f3427(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,-257);*r1p=r1;*r0p=r0;}
static void f3428(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,512);*r1p=r1;*r0p=r0;}
static void f3429(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,-512);*r1p=r1;*r0p=r0;}
static void f3430(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,511);*r1p=r1;*r0p=r0;}
static void f3431(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,-513);*r1p=r1;*r0p=r0;}
static void f3432(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,1024);*r1p=r1;*r0p=r0;}
static void f3433(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,-1024);*r1p=r1;*r0p=r0;}
static void f3434(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,1023);*r1p=r1;*r0p=r0;}
static void f3435(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,-1025);*r1p=r1;*r0p=r0;}
static void f3436(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,2048);*r1p=r1;*r0p=r0;}
static void f3437(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,-2048);*r1p=r1;*r0p=r0;}
static void f3438(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,2047);*r1p=r1;*r0p=r0;}
static void f3439(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,-2049);*r1p=r1;*r0p=r0;}
static void f3440(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,4096);*r1p=r1;*r0p=r0;}
static void f3441(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,-4096);*r1p=r1;*r0p=r0;}
static void f3442(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,4095);*r1p=r1;*r0p=r0;}
static void f3443(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,-4097);*r1p=r1;*r0p=r0;}
static void f3444(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,8192);*r1p=r1;*r0p=r0;}
static void f3445(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,-8192);*r1p=r1;*r0p=r0;}
static void f3446(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,8191);*r1p=r1;*r0p=r0;}
static void f3447(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,-8193);*r1p=r1;*r0p=r0;}
static void f3448(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,16384);*r1p=r1;*r0p=r0;}
static void f3449(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,-16384);*r1p=r1;*r0p=r0;}
static void f3450(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,16383);*r1p=r1;*r0p=r0;}
static void f3451(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,-16385);*r1p=r1;*r0p=r0;}
static void f3452(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,32768);*r1p=r1;*r0p=r0;}
static void f3453(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,-32768);*r1p=r1;*r0p=r0;}
static void f3454(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,32767);*r1p=r1;*r0p=r0;}
static void f3455(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8192,0,-32769);*r1p=r1;*r0p=r0;}
static void f3456(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,1);*r1p=r1;*r0p=r0;}
static void f3457(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,-1);*r1p=r1;*r0p=r0;}
static void f3458(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,0);*r1p=r1;*r0p=r0;}
static void f3459(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,-2);*r1p=r1;*r0p=r0;}
static void f3460(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,2);*r1p=r1;*r0p=r0;}
static void f3461(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,-2);*r1p=r1;*r0p=r0;}
static void f3462(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,1);*r1p=r1;*r0p=r0;}
static void f3463(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,-3);*r1p=r1;*r0p=r0;}
static void f3464(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,4);*r1p=r1;*r0p=r0;}
static void f3465(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,-4);*r1p=r1;*r0p=r0;}
static void f3466(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,3);*r1p=r1;*r0p=r0;}
static void f3467(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,-5);*r1p=r1;*r0p=r0;}
static void f3468(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,8);*r1p=r1;*r0p=r0;}
static void f3469(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,-8);*r1p=r1;*r0p=r0;}
static void f3470(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,7);*r1p=r1;*r0p=r0;}
static void f3471(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,-9);*r1p=r1;*r0p=r0;}
static void f3472(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,16);*r1p=r1;*r0p=r0;}
static void f3473(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,-16);*r1p=r1;*r0p=r0;}
static void f3474(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,15);*r1p=r1;*r0p=r0;}
static void f3475(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,-17);*r1p=r1;*r0p=r0;}
static void f3476(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,32);*r1p=r1;*r0p=r0;}
static void f3477(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,-32);*r1p=r1;*r0p=r0;}
static void f3478(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,31);*r1p=r1;*r0p=r0;}
static void f3479(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,-33);*r1p=r1;*r0p=r0;}
static void f3480(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,64);*r1p=r1;*r0p=r0;}
static void f3481(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,-64);*r1p=r1;*r0p=r0;}
static void f3482(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,63);*r1p=r1;*r0p=r0;}
static void f3483(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,-65);*r1p=r1;*r0p=r0;}
static void f3484(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,128);*r1p=r1;*r0p=r0;}
static void f3485(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,-128);*r1p=r1;*r0p=r0;}
static void f3486(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,127);*r1p=r1;*r0p=r0;}
static void f3487(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,-129);*r1p=r1;*r0p=r0;}
static void f3488(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,256);*r1p=r1;*r0p=r0;}
static void f3489(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,-256);*r1p=r1;*r0p=r0;}
static void f3490(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,255);*r1p=r1;*r0p=r0;}
static void f3491(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,-257);*r1p=r1;*r0p=r0;}
static void f3492(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,512);*r1p=r1;*r0p=r0;}
static void f3493(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,-512);*r1p=r1;*r0p=r0;}
static void f3494(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,511);*r1p=r1;*r0p=r0;}
static void f3495(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,-513);*r1p=r1;*r0p=r0;}
static void f3496(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,1024);*r1p=r1;*r0p=r0;}
static void f3497(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,-1024);*r1p=r1;*r0p=r0;}
static void f3498(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,1023);*r1p=r1;*r0p=r0;}
static void f3499(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,-1025);*r1p=r1;*r0p=r0;}
static void f3500(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,2048);*r1p=r1;*r0p=r0;}
static void f3501(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,-2048);*r1p=r1;*r0p=r0;}
static void f3502(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,2047);*r1p=r1;*r0p=r0;}
static void f3503(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,-2049);*r1p=r1;*r0p=r0;}
static void f3504(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,4096);*r1p=r1;*r0p=r0;}
static void f3505(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,-4096);*r1p=r1;*r0p=r0;}
static void f3506(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,4095);*r1p=r1;*r0p=r0;}
static void f3507(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,-4097);*r1p=r1;*r0p=r0;}
static void f3508(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,8192);*r1p=r1;*r0p=r0;}
static void f3509(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,-8192);*r1p=r1;*r0p=r0;}
static void f3510(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,8191);*r1p=r1;*r0p=r0;}
static void f3511(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,-8193);*r1p=r1;*r0p=r0;}
static void f3512(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,16384);*r1p=r1;*r0p=r0;}
static void f3513(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,-16384);*r1p=r1;*r0p=r0;}
static void f3514(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,16383);*r1p=r1;*r0p=r0;}
static void f3515(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,-16385);*r1p=r1;*r0p=r0;}
static void f3516(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,32768);*r1p=r1;*r0p=r0;}
static void f3517(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,-32768);*r1p=r1;*r0p=r0;}
static void f3518(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,32767);*r1p=r1;*r0p=r0;}
static void f3519(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,8191,0,-32769);*r1p=r1;*r0p=r0;}
static void f3520(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,1);*r1p=r1;*r0p=r0;}
static void f3521(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,-1);*r1p=r1;*r0p=r0;}
static void f3522(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,0);*r1p=r1;*r0p=r0;}
static void f3523(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,-2);*r1p=r1;*r0p=r0;}
static void f3524(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,2);*r1p=r1;*r0p=r0;}
static void f3525(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,-2);*r1p=r1;*r0p=r0;}
static void f3526(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,1);*r1p=r1;*r0p=r0;}
static void f3527(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,-3);*r1p=r1;*r0p=r0;}
static void f3528(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,4);*r1p=r1;*r0p=r0;}
static void f3529(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,-4);*r1p=r1;*r0p=r0;}
static void f3530(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,3);*r1p=r1;*r0p=r0;}
static void f3531(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,-5);*r1p=r1;*r0p=r0;}
static void f3532(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,8);*r1p=r1;*r0p=r0;}
static void f3533(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,-8);*r1p=r1;*r0p=r0;}
static void f3534(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,7);*r1p=r1;*r0p=r0;}
static void f3535(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,-9);*r1p=r1;*r0p=r0;}
static void f3536(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,16);*r1p=r1;*r0p=r0;}
static void f3537(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,-16);*r1p=r1;*r0p=r0;}
static void f3538(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,15);*r1p=r1;*r0p=r0;}
static void f3539(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,-17);*r1p=r1;*r0p=r0;}
static void f3540(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,32);*r1p=r1;*r0p=r0;}
static void f3541(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,-32);*r1p=r1;*r0p=r0;}
static void f3542(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,31);*r1p=r1;*r0p=r0;}
static void f3543(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,-33);*r1p=r1;*r0p=r0;}
static void f3544(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,64);*r1p=r1;*r0p=r0;}
static void f3545(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,-64);*r1p=r1;*r0p=r0;}
static void f3546(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,63);*r1p=r1;*r0p=r0;}
static void f3547(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,-65);*r1p=r1;*r0p=r0;}
static void f3548(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,128);*r1p=r1;*r0p=r0;}
static void f3549(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,-128);*r1p=r1;*r0p=r0;}
static void f3550(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,127);*r1p=r1;*r0p=r0;}
static void f3551(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,-129);*r1p=r1;*r0p=r0;}
static void f3552(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,256);*r1p=r1;*r0p=r0;}
static void f3553(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,-256);*r1p=r1;*r0p=r0;}
static void f3554(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,255);*r1p=r1;*r0p=r0;}
static void f3555(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,-257);*r1p=r1;*r0p=r0;}
static void f3556(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,512);*r1p=r1;*r0p=r0;}
static void f3557(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,-512);*r1p=r1;*r0p=r0;}
static void f3558(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,511);*r1p=r1;*r0p=r0;}
static void f3559(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,-513);*r1p=r1;*r0p=r0;}
static void f3560(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,1024);*r1p=r1;*r0p=r0;}
static void f3561(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,-1024);*r1p=r1;*r0p=r0;}
static void f3562(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,1023);*r1p=r1;*r0p=r0;}
static void f3563(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,-1025);*r1p=r1;*r0p=r0;}
static void f3564(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,2048);*r1p=r1;*r0p=r0;}
static void f3565(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,-2048);*r1p=r1;*r0p=r0;}
static void f3566(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,2047);*r1p=r1;*r0p=r0;}
static void f3567(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,-2049);*r1p=r1;*r0p=r0;}
static void f3568(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,4096);*r1p=r1;*r0p=r0;}
static void f3569(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,-4096);*r1p=r1;*r0p=r0;}
static void f3570(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,4095);*r1p=r1;*r0p=r0;}
static void f3571(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,-4097);*r1p=r1;*r0p=r0;}
static void f3572(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,8192);*r1p=r1;*r0p=r0;}
static void f3573(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,-8192);*r1p=r1;*r0p=r0;}
static void f3574(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,8191);*r1p=r1;*r0p=r0;}
static void f3575(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,-8193);*r1p=r1;*r0p=r0;}
static void f3576(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,16384);*r1p=r1;*r0p=r0;}
static void f3577(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,-16384);*r1p=r1;*r0p=r0;}
static void f3578(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,16383);*r1p=r1;*r0p=r0;}
static void f3579(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,-16385);*r1p=r1;*r0p=r0;}
static void f3580(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,32768);*r1p=r1;*r0p=r0;}
static void f3581(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,-32768);*r1p=r1;*r0p=r0;}
static void f3582(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,32767);*r1p=r1;*r0p=r0;}
static void f3583(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-8193,0,-32769);*r1p=r1;*r0p=r0;}
static void f3584(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,1);*r1p=r1;*r0p=r0;}
static void f3585(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,-1);*r1p=r1;*r0p=r0;}
static void f3586(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,0);*r1p=r1;*r0p=r0;}
static void f3587(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,-2);*r1p=r1;*r0p=r0;}
static void f3588(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,2);*r1p=r1;*r0p=r0;}
static void f3589(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,-2);*r1p=r1;*r0p=r0;}
static void f3590(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,1);*r1p=r1;*r0p=r0;}
static void f3591(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,-3);*r1p=r1;*r0p=r0;}
static void f3592(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,4);*r1p=r1;*r0p=r0;}
static void f3593(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,-4);*r1p=r1;*r0p=r0;}
static void f3594(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,3);*r1p=r1;*r0p=r0;}
static void f3595(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,-5);*r1p=r1;*r0p=r0;}
static void f3596(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,8);*r1p=r1;*r0p=r0;}
static void f3597(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,-8);*r1p=r1;*r0p=r0;}
static void f3598(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,7);*r1p=r1;*r0p=r0;}
static void f3599(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,-9);*r1p=r1;*r0p=r0;}
static void f3600(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,16);*r1p=r1;*r0p=r0;}
static void f3601(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,-16);*r1p=r1;*r0p=r0;}
static void f3602(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,15);*r1p=r1;*r0p=r0;}
static void f3603(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,-17);*r1p=r1;*r0p=r0;}
static void f3604(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,32);*r1p=r1;*r0p=r0;}
static void f3605(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,-32);*r1p=r1;*r0p=r0;}
static void f3606(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,31);*r1p=r1;*r0p=r0;}
static void f3607(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,-33);*r1p=r1;*r0p=r0;}
static void f3608(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,64);*r1p=r1;*r0p=r0;}
static void f3609(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,-64);*r1p=r1;*r0p=r0;}
static void f3610(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,63);*r1p=r1;*r0p=r0;}
static void f3611(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,-65);*r1p=r1;*r0p=r0;}
static void f3612(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,128);*r1p=r1;*r0p=r0;}
static void f3613(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,-128);*r1p=r1;*r0p=r0;}
static void f3614(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,127);*r1p=r1;*r0p=r0;}
static void f3615(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,-129);*r1p=r1;*r0p=r0;}
static void f3616(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,256);*r1p=r1;*r0p=r0;}
static void f3617(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,-256);*r1p=r1;*r0p=r0;}
static void f3618(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,255);*r1p=r1;*r0p=r0;}
static void f3619(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,-257);*r1p=r1;*r0p=r0;}
static void f3620(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,512);*r1p=r1;*r0p=r0;}
static void f3621(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,-512);*r1p=r1;*r0p=r0;}
static void f3622(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,511);*r1p=r1;*r0p=r0;}
static void f3623(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,-513);*r1p=r1;*r0p=r0;}
static void f3624(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,1024);*r1p=r1;*r0p=r0;}
static void f3625(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,-1024);*r1p=r1;*r0p=r0;}
static void f3626(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,1023);*r1p=r1;*r0p=r0;}
static void f3627(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,-1025);*r1p=r1;*r0p=r0;}
static void f3628(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,2048);*r1p=r1;*r0p=r0;}
static void f3629(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,-2048);*r1p=r1;*r0p=r0;}
static void f3630(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,2047);*r1p=r1;*r0p=r0;}
static void f3631(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,-2049);*r1p=r1;*r0p=r0;}
static void f3632(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,4096);*r1p=r1;*r0p=r0;}
static void f3633(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,-4096);*r1p=r1;*r0p=r0;}
static void f3634(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,4095);*r1p=r1;*r0p=r0;}
static void f3635(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,-4097);*r1p=r1;*r0p=r0;}
static void f3636(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,8192);*r1p=r1;*r0p=r0;}
static void f3637(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,-8192);*r1p=r1;*r0p=r0;}
static void f3638(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,8191);*r1p=r1;*r0p=r0;}
static void f3639(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,-8193);*r1p=r1;*r0p=r0;}
static void f3640(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,16384);*r1p=r1;*r0p=r0;}
static void f3641(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,-16384);*r1p=r1;*r0p=r0;}
static void f3642(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,16383);*r1p=r1;*r0p=r0;}
static void f3643(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,-16385);*r1p=r1;*r0p=r0;}
static void f3644(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,32768);*r1p=r1;*r0p=r0;}
static void f3645(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,-32768);*r1p=r1;*r0p=r0;}
static void f3646(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,32767);*r1p=r1;*r0p=r0;}
static void f3647(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16384,0,-32769);*r1p=r1;*r0p=r0;}
static void f3648(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,1);*r1p=r1;*r0p=r0;}
static void f3649(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,-1);*r1p=r1;*r0p=r0;}
static void f3650(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,0);*r1p=r1;*r0p=r0;}
static void f3651(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,-2);*r1p=r1;*r0p=r0;}
static void f3652(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,2);*r1p=r1;*r0p=r0;}
static void f3653(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,-2);*r1p=r1;*r0p=r0;}
static void f3654(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,1);*r1p=r1;*r0p=r0;}
static void f3655(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,-3);*r1p=r1;*r0p=r0;}
static void f3656(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,4);*r1p=r1;*r0p=r0;}
static void f3657(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,-4);*r1p=r1;*r0p=r0;}
static void f3658(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,3);*r1p=r1;*r0p=r0;}
static void f3659(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,-5);*r1p=r1;*r0p=r0;}
static void f3660(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,8);*r1p=r1;*r0p=r0;}
static void f3661(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,-8);*r1p=r1;*r0p=r0;}
static void f3662(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,7);*r1p=r1;*r0p=r0;}
static void f3663(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,-9);*r1p=r1;*r0p=r0;}
static void f3664(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,16);*r1p=r1;*r0p=r0;}
static void f3665(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,-16);*r1p=r1;*r0p=r0;}
static void f3666(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,15);*r1p=r1;*r0p=r0;}
static void f3667(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,-17);*r1p=r1;*r0p=r0;}
static void f3668(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,32);*r1p=r1;*r0p=r0;}
static void f3669(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,-32);*r1p=r1;*r0p=r0;}
static void f3670(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,31);*r1p=r1;*r0p=r0;}
static void f3671(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,-33);*r1p=r1;*r0p=r0;}
static void f3672(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,64);*r1p=r1;*r0p=r0;}
static void f3673(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,-64);*r1p=r1;*r0p=r0;}
static void f3674(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,63);*r1p=r1;*r0p=r0;}
static void f3675(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,-65);*r1p=r1;*r0p=r0;}
static void f3676(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,128);*r1p=r1;*r0p=r0;}
static void f3677(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,-128);*r1p=r1;*r0p=r0;}
static void f3678(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,127);*r1p=r1;*r0p=r0;}
static void f3679(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,-129);*r1p=r1;*r0p=r0;}
static void f3680(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,256);*r1p=r1;*r0p=r0;}
static void f3681(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,-256);*r1p=r1;*r0p=r0;}
static void f3682(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,255);*r1p=r1;*r0p=r0;}
static void f3683(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,-257);*r1p=r1;*r0p=r0;}
static void f3684(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,512);*r1p=r1;*r0p=r0;}
static void f3685(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,-512);*r1p=r1;*r0p=r0;}
static void f3686(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,511);*r1p=r1;*r0p=r0;}
static void f3687(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,-513);*r1p=r1;*r0p=r0;}
static void f3688(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,1024);*r1p=r1;*r0p=r0;}
static void f3689(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,-1024);*r1p=r1;*r0p=r0;}
static void f3690(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,1023);*r1p=r1;*r0p=r0;}
static void f3691(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,-1025);*r1p=r1;*r0p=r0;}
static void f3692(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,2048);*r1p=r1;*r0p=r0;}
static void f3693(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,-2048);*r1p=r1;*r0p=r0;}
static void f3694(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,2047);*r1p=r1;*r0p=r0;}
static void f3695(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,-2049);*r1p=r1;*r0p=r0;}
static void f3696(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,4096);*r1p=r1;*r0p=r0;}
static void f3697(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,-4096);*r1p=r1;*r0p=r0;}
static void f3698(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,4095);*r1p=r1;*r0p=r0;}
static void f3699(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,-4097);*r1p=r1;*r0p=r0;}
static void f3700(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,8192);*r1p=r1;*r0p=r0;}
static void f3701(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,-8192);*r1p=r1;*r0p=r0;}
static void f3702(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,8191);*r1p=r1;*r0p=r0;}
static void f3703(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,-8193);*r1p=r1;*r0p=r0;}
static void f3704(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,16384);*r1p=r1;*r0p=r0;}
static void f3705(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,-16384);*r1p=r1;*r0p=r0;}
static void f3706(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,16383);*r1p=r1;*r0p=r0;}
static void f3707(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,-16385);*r1p=r1;*r0p=r0;}
static void f3708(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,32768);*r1p=r1;*r0p=r0;}
static void f3709(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,-32768);*r1p=r1;*r0p=r0;}
static void f3710(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,32767);*r1p=r1;*r0p=r0;}
static void f3711(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16384,0,-32769);*r1p=r1;*r0p=r0;}
static void f3712(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,1);*r1p=r1;*r0p=r0;}
static void f3713(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,-1);*r1p=r1;*r0p=r0;}
static void f3714(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,0);*r1p=r1;*r0p=r0;}
static void f3715(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,-2);*r1p=r1;*r0p=r0;}
static void f3716(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,2);*r1p=r1;*r0p=r0;}
static void f3717(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,-2);*r1p=r1;*r0p=r0;}
static void f3718(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,1);*r1p=r1;*r0p=r0;}
static void f3719(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,-3);*r1p=r1;*r0p=r0;}
static void f3720(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,4);*r1p=r1;*r0p=r0;}
static void f3721(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,-4);*r1p=r1;*r0p=r0;}
static void f3722(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,3);*r1p=r1;*r0p=r0;}
static void f3723(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,-5);*r1p=r1;*r0p=r0;}
static void f3724(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,8);*r1p=r1;*r0p=r0;}
static void f3725(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,-8);*r1p=r1;*r0p=r0;}
static void f3726(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,7);*r1p=r1;*r0p=r0;}
static void f3727(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,-9);*r1p=r1;*r0p=r0;}
static void f3728(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,16);*r1p=r1;*r0p=r0;}
static void f3729(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,-16);*r1p=r1;*r0p=r0;}
static void f3730(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,15);*r1p=r1;*r0p=r0;}
static void f3731(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,-17);*r1p=r1;*r0p=r0;}
static void f3732(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,32);*r1p=r1;*r0p=r0;}
static void f3733(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,-32);*r1p=r1;*r0p=r0;}
static void f3734(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,31);*r1p=r1;*r0p=r0;}
static void f3735(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,-33);*r1p=r1;*r0p=r0;}
static void f3736(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,64);*r1p=r1;*r0p=r0;}
static void f3737(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,-64);*r1p=r1;*r0p=r0;}
static void f3738(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,63);*r1p=r1;*r0p=r0;}
static void f3739(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,-65);*r1p=r1;*r0p=r0;}
static void f3740(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,128);*r1p=r1;*r0p=r0;}
static void f3741(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,-128);*r1p=r1;*r0p=r0;}
static void f3742(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,127);*r1p=r1;*r0p=r0;}
static void f3743(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,-129);*r1p=r1;*r0p=r0;}
static void f3744(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,256);*r1p=r1;*r0p=r0;}
static void f3745(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,-256);*r1p=r1;*r0p=r0;}
static void f3746(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,255);*r1p=r1;*r0p=r0;}
static void f3747(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,-257);*r1p=r1;*r0p=r0;}
static void f3748(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,512);*r1p=r1;*r0p=r0;}
static void f3749(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,-512);*r1p=r1;*r0p=r0;}
static void f3750(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,511);*r1p=r1;*r0p=r0;}
static void f3751(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,-513);*r1p=r1;*r0p=r0;}
static void f3752(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,1024);*r1p=r1;*r0p=r0;}
static void f3753(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,-1024);*r1p=r1;*r0p=r0;}
static void f3754(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,1023);*r1p=r1;*r0p=r0;}
static void f3755(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,-1025);*r1p=r1;*r0p=r0;}
static void f3756(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,2048);*r1p=r1;*r0p=r0;}
static void f3757(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,-2048);*r1p=r1;*r0p=r0;}
static void f3758(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,2047);*r1p=r1;*r0p=r0;}
static void f3759(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,-2049);*r1p=r1;*r0p=r0;}
static void f3760(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,4096);*r1p=r1;*r0p=r0;}
static void f3761(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,-4096);*r1p=r1;*r0p=r0;}
static void f3762(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,4095);*r1p=r1;*r0p=r0;}
static void f3763(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,-4097);*r1p=r1;*r0p=r0;}
static void f3764(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,8192);*r1p=r1;*r0p=r0;}
static void f3765(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,-8192);*r1p=r1;*r0p=r0;}
static void f3766(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,8191);*r1p=r1;*r0p=r0;}
static void f3767(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,-8193);*r1p=r1;*r0p=r0;}
static void f3768(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,16384);*r1p=r1;*r0p=r0;}
static void f3769(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,-16384);*r1p=r1;*r0p=r0;}
static void f3770(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,16383);*r1p=r1;*r0p=r0;}
static void f3771(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,-16385);*r1p=r1;*r0p=r0;}
static void f3772(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,32768);*r1p=r1;*r0p=r0;}
static void f3773(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,-32768);*r1p=r1;*r0p=r0;}
static void f3774(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,32767);*r1p=r1;*r0p=r0;}
static void f3775(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,16383,0,-32769);*r1p=r1;*r0p=r0;}
static void f3776(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,1);*r1p=r1;*r0p=r0;}
static void f3777(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,-1);*r1p=r1;*r0p=r0;}
static void f3778(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,0);*r1p=r1;*r0p=r0;}
static void f3779(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,-2);*r1p=r1;*r0p=r0;}
static void f3780(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,2);*r1p=r1;*r0p=r0;}
static void f3781(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,-2);*r1p=r1;*r0p=r0;}
static void f3782(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,1);*r1p=r1;*r0p=r0;}
static void f3783(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,-3);*r1p=r1;*r0p=r0;}
static void f3784(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,4);*r1p=r1;*r0p=r0;}
static void f3785(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,-4);*r1p=r1;*r0p=r0;}
static void f3786(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,3);*r1p=r1;*r0p=r0;}
static void f3787(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,-5);*r1p=r1;*r0p=r0;}
static void f3788(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,8);*r1p=r1;*r0p=r0;}
static void f3789(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,-8);*r1p=r1;*r0p=r0;}
static void f3790(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,7);*r1p=r1;*r0p=r0;}
static void f3791(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,-9);*r1p=r1;*r0p=r0;}
static void f3792(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,16);*r1p=r1;*r0p=r0;}
static void f3793(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,-16);*r1p=r1;*r0p=r0;}
static void f3794(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,15);*r1p=r1;*r0p=r0;}
static void f3795(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,-17);*r1p=r1;*r0p=r0;}
static void f3796(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,32);*r1p=r1;*r0p=r0;}
static void f3797(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,-32);*r1p=r1;*r0p=r0;}
static void f3798(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,31);*r1p=r1;*r0p=r0;}
static void f3799(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,-33);*r1p=r1;*r0p=r0;}
static void f3800(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,64);*r1p=r1;*r0p=r0;}
static void f3801(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,-64);*r1p=r1;*r0p=r0;}
static void f3802(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,63);*r1p=r1;*r0p=r0;}
static void f3803(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,-65);*r1p=r1;*r0p=r0;}
static void f3804(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,128);*r1p=r1;*r0p=r0;}
static void f3805(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,-128);*r1p=r1;*r0p=r0;}
static void f3806(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,127);*r1p=r1;*r0p=r0;}
static void f3807(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,-129);*r1p=r1;*r0p=r0;}
static void f3808(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,256);*r1p=r1;*r0p=r0;}
static void f3809(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,-256);*r1p=r1;*r0p=r0;}
static void f3810(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,255);*r1p=r1;*r0p=r0;}
static void f3811(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,-257);*r1p=r1;*r0p=r0;}
static void f3812(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,512);*r1p=r1;*r0p=r0;}
static void f3813(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,-512);*r1p=r1;*r0p=r0;}
static void f3814(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,511);*r1p=r1;*r0p=r0;}
static void f3815(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,-513);*r1p=r1;*r0p=r0;}
static void f3816(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,1024);*r1p=r1;*r0p=r0;}
static void f3817(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,-1024);*r1p=r1;*r0p=r0;}
static void f3818(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,1023);*r1p=r1;*r0p=r0;}
static void f3819(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,-1025);*r1p=r1;*r0p=r0;}
static void f3820(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,2048);*r1p=r1;*r0p=r0;}
static void f3821(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,-2048);*r1p=r1;*r0p=r0;}
static void f3822(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,2047);*r1p=r1;*r0p=r0;}
static void f3823(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,-2049);*r1p=r1;*r0p=r0;}
static void f3824(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,4096);*r1p=r1;*r0p=r0;}
static void f3825(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,-4096);*r1p=r1;*r0p=r0;}
static void f3826(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,4095);*r1p=r1;*r0p=r0;}
static void f3827(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,-4097);*r1p=r1;*r0p=r0;}
static void f3828(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,8192);*r1p=r1;*r0p=r0;}
static void f3829(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,-8192);*r1p=r1;*r0p=r0;}
static void f3830(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,8191);*r1p=r1;*r0p=r0;}
static void f3831(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,-8193);*r1p=r1;*r0p=r0;}
static void f3832(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,16384);*r1p=r1;*r0p=r0;}
static void f3833(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,-16384);*r1p=r1;*r0p=r0;}
static void f3834(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,16383);*r1p=r1;*r0p=r0;}
static void f3835(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,-16385);*r1p=r1;*r0p=r0;}
static void f3836(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,32768);*r1p=r1;*r0p=r0;}
static void f3837(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,-32768);*r1p=r1;*r0p=r0;}
static void f3838(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,32767);*r1p=r1;*r0p=r0;}
static void f3839(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-16385,0,-32769);*r1p=r1;*r0p=r0;}
static void f3840(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,1);*r1p=r1;*r0p=r0;}
static void f3841(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,-1);*r1p=r1;*r0p=r0;}
static void f3842(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,0);*r1p=r1;*r0p=r0;}
static void f3843(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,-2);*r1p=r1;*r0p=r0;}
static void f3844(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,2);*r1p=r1;*r0p=r0;}
static void f3845(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,-2);*r1p=r1;*r0p=r0;}
static void f3846(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,1);*r1p=r1;*r0p=r0;}
static void f3847(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,-3);*r1p=r1;*r0p=r0;}
static void f3848(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,4);*r1p=r1;*r0p=r0;}
static void f3849(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,-4);*r1p=r1;*r0p=r0;}
static void f3850(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,3);*r1p=r1;*r0p=r0;}
static void f3851(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,-5);*r1p=r1;*r0p=r0;}
static void f3852(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,8);*r1p=r1;*r0p=r0;}
static void f3853(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,-8);*r1p=r1;*r0p=r0;}
static void f3854(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,7);*r1p=r1;*r0p=r0;}
static void f3855(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,-9);*r1p=r1;*r0p=r0;}
static void f3856(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,16);*r1p=r1;*r0p=r0;}
static void f3857(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,-16);*r1p=r1;*r0p=r0;}
static void f3858(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,15);*r1p=r1;*r0p=r0;}
static void f3859(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,-17);*r1p=r1;*r0p=r0;}
static void f3860(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,32);*r1p=r1;*r0p=r0;}
static void f3861(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,-32);*r1p=r1;*r0p=r0;}
static void f3862(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,31);*r1p=r1;*r0p=r0;}
static void f3863(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,-33);*r1p=r1;*r0p=r0;}
static void f3864(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,64);*r1p=r1;*r0p=r0;}
static void f3865(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,-64);*r1p=r1;*r0p=r0;}
static void f3866(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,63);*r1p=r1;*r0p=r0;}
static void f3867(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,-65);*r1p=r1;*r0p=r0;}
static void f3868(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,128);*r1p=r1;*r0p=r0;}
static void f3869(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,-128);*r1p=r1;*r0p=r0;}
static void f3870(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,127);*r1p=r1;*r0p=r0;}
static void f3871(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,-129);*r1p=r1;*r0p=r0;}
static void f3872(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,256);*r1p=r1;*r0p=r0;}
static void f3873(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,-256);*r1p=r1;*r0p=r0;}
static void f3874(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,255);*r1p=r1;*r0p=r0;}
static void f3875(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,-257);*r1p=r1;*r0p=r0;}
static void f3876(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,512);*r1p=r1;*r0p=r0;}
static void f3877(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,-512);*r1p=r1;*r0p=r0;}
static void f3878(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,511);*r1p=r1;*r0p=r0;}
static void f3879(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,-513);*r1p=r1;*r0p=r0;}
static void f3880(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,1024);*r1p=r1;*r0p=r0;}
static void f3881(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,-1024);*r1p=r1;*r0p=r0;}
static void f3882(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,1023);*r1p=r1;*r0p=r0;}
static void f3883(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,-1025);*r1p=r1;*r0p=r0;}
static void f3884(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,2048);*r1p=r1;*r0p=r0;}
static void f3885(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,-2048);*r1p=r1;*r0p=r0;}
static void f3886(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,2047);*r1p=r1;*r0p=r0;}
static void f3887(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,-2049);*r1p=r1;*r0p=r0;}
static void f3888(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,4096);*r1p=r1;*r0p=r0;}
static void f3889(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,-4096);*r1p=r1;*r0p=r0;}
static void f3890(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,4095);*r1p=r1;*r0p=r0;}
static void f3891(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,-4097);*r1p=r1;*r0p=r0;}
static void f3892(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,8192);*r1p=r1;*r0p=r0;}
static void f3893(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,-8192);*r1p=r1;*r0p=r0;}
static void f3894(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,8191);*r1p=r1;*r0p=r0;}
static void f3895(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,-8193);*r1p=r1;*r0p=r0;}
static void f3896(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,16384);*r1p=r1;*r0p=r0;}
static void f3897(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,-16384);*r1p=r1;*r0p=r0;}
static void f3898(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,16383);*r1p=r1;*r0p=r0;}
static void f3899(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,-16385);*r1p=r1;*r0p=r0;}
static void f3900(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,32768);*r1p=r1;*r0p=r0;}
static void f3901(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,-32768);*r1p=r1;*r0p=r0;}
static void f3902(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,32767);*r1p=r1;*r0p=r0;}
static void f3903(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32768,0,-32769);*r1p=r1;*r0p=r0;}
static void f3904(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,1);*r1p=r1;*r0p=r0;}
static void f3905(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,-1);*r1p=r1;*r0p=r0;}
static void f3906(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,0);*r1p=r1;*r0p=r0;}
static void f3907(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,-2);*r1p=r1;*r0p=r0;}
static void f3908(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,2);*r1p=r1;*r0p=r0;}
static void f3909(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,-2);*r1p=r1;*r0p=r0;}
static void f3910(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,1);*r1p=r1;*r0p=r0;}
static void f3911(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,-3);*r1p=r1;*r0p=r0;}
static void f3912(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,4);*r1p=r1;*r0p=r0;}
static void f3913(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,-4);*r1p=r1;*r0p=r0;}
static void f3914(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,3);*r1p=r1;*r0p=r0;}
static void f3915(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,-5);*r1p=r1;*r0p=r0;}
static void f3916(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,8);*r1p=r1;*r0p=r0;}
static void f3917(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,-8);*r1p=r1;*r0p=r0;}
static void f3918(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,7);*r1p=r1;*r0p=r0;}
static void f3919(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,-9);*r1p=r1;*r0p=r0;}
static void f3920(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,16);*r1p=r1;*r0p=r0;}
static void f3921(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,-16);*r1p=r1;*r0p=r0;}
static void f3922(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,15);*r1p=r1;*r0p=r0;}
static void f3923(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,-17);*r1p=r1;*r0p=r0;}
static void f3924(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,32);*r1p=r1;*r0p=r0;}
static void f3925(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,-32);*r1p=r1;*r0p=r0;}
static void f3926(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,31);*r1p=r1;*r0p=r0;}
static void f3927(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,-33);*r1p=r1;*r0p=r0;}
static void f3928(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,64);*r1p=r1;*r0p=r0;}
static void f3929(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,-64);*r1p=r1;*r0p=r0;}
static void f3930(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,63);*r1p=r1;*r0p=r0;}
static void f3931(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,-65);*r1p=r1;*r0p=r0;}
static void f3932(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,128);*r1p=r1;*r0p=r0;}
static void f3933(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,-128);*r1p=r1;*r0p=r0;}
static void f3934(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,127);*r1p=r1;*r0p=r0;}
static void f3935(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,-129);*r1p=r1;*r0p=r0;}
static void f3936(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,256);*r1p=r1;*r0p=r0;}
static void f3937(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,-256);*r1p=r1;*r0p=r0;}
static void f3938(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,255);*r1p=r1;*r0p=r0;}
static void f3939(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,-257);*r1p=r1;*r0p=r0;}
static void f3940(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,512);*r1p=r1;*r0p=r0;}
static void f3941(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,-512);*r1p=r1;*r0p=r0;}
static void f3942(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,511);*r1p=r1;*r0p=r0;}
static void f3943(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,-513);*r1p=r1;*r0p=r0;}
static void f3944(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,1024);*r1p=r1;*r0p=r0;}
static void f3945(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,-1024);*r1p=r1;*r0p=r0;}
static void f3946(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,1023);*r1p=r1;*r0p=r0;}
static void f3947(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,-1025);*r1p=r1;*r0p=r0;}
static void f3948(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,2048);*r1p=r1;*r0p=r0;}
static void f3949(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,-2048);*r1p=r1;*r0p=r0;}
static void f3950(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,2047);*r1p=r1;*r0p=r0;}
static void f3951(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,-2049);*r1p=r1;*r0p=r0;}
static void f3952(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,4096);*r1p=r1;*r0p=r0;}
static void f3953(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,-4096);*r1p=r1;*r0p=r0;}
static void f3954(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,4095);*r1p=r1;*r0p=r0;}
static void f3955(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,-4097);*r1p=r1;*r0p=r0;}
static void f3956(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,8192);*r1p=r1;*r0p=r0;}
static void f3957(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,-8192);*r1p=r1;*r0p=r0;}
static void f3958(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,8191);*r1p=r1;*r0p=r0;}
static void f3959(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,-8193);*r1p=r1;*r0p=r0;}
static void f3960(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,16384);*r1p=r1;*r0p=r0;}
static void f3961(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,-16384);*r1p=r1;*r0p=r0;}
static void f3962(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,16383);*r1p=r1;*r0p=r0;}
static void f3963(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,-16385);*r1p=r1;*r0p=r0;}
static void f3964(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,32768);*r1p=r1;*r0p=r0;}
static void f3965(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,-32768);*r1p=r1;*r0p=r0;}
static void f3966(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,32767);*r1p=r1;*r0p=r0;}
static void f3967(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32768,0,-32769);*r1p=r1;*r0p=r0;}
static void f3968(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,1);*r1p=r1;*r0p=r0;}
static void f3969(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,-1);*r1p=r1;*r0p=r0;}
static void f3970(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,0);*r1p=r1;*r0p=r0;}
static void f3971(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,-2);*r1p=r1;*r0p=r0;}
static void f3972(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,2);*r1p=r1;*r0p=r0;}
static void f3973(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,-2);*r1p=r1;*r0p=r0;}
static void f3974(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,1);*r1p=r1;*r0p=r0;}
static void f3975(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,-3);*r1p=r1;*r0p=r0;}
static void f3976(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,4);*r1p=r1;*r0p=r0;}
static void f3977(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,-4);*r1p=r1;*r0p=r0;}
static void f3978(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,3);*r1p=r1;*r0p=r0;}
static void f3979(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,-5);*r1p=r1;*r0p=r0;}
static void f3980(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,8);*r1p=r1;*r0p=r0;}
static void f3981(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,-8);*r1p=r1;*r0p=r0;}
static void f3982(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,7);*r1p=r1;*r0p=r0;}
static void f3983(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,-9);*r1p=r1;*r0p=r0;}
static void f3984(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,16);*r1p=r1;*r0p=r0;}
static void f3985(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,-16);*r1p=r1;*r0p=r0;}
static void f3986(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,15);*r1p=r1;*r0p=r0;}
static void f3987(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,-17);*r1p=r1;*r0p=r0;}
static void f3988(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,32);*r1p=r1;*r0p=r0;}
static void f3989(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,-32);*r1p=r1;*r0p=r0;}
static void f3990(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,31);*r1p=r1;*r0p=r0;}
static void f3991(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,-33);*r1p=r1;*r0p=r0;}
static void f3992(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,64);*r1p=r1;*r0p=r0;}
static void f3993(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,-64);*r1p=r1;*r0p=r0;}
static void f3994(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,63);*r1p=r1;*r0p=r0;}
static void f3995(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,-65);*r1p=r1;*r0p=r0;}
static void f3996(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,128);*r1p=r1;*r0p=r0;}
static void f3997(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,-128);*r1p=r1;*r0p=r0;}
static void f3998(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,127);*r1p=r1;*r0p=r0;}
static void f3999(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,-129);*r1p=r1;*r0p=r0;}
static void f4000(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,256);*r1p=r1;*r0p=r0;}
static void f4001(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,-256);*r1p=r1;*r0p=r0;}
static void f4002(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,255);*r1p=r1;*r0p=r0;}
static void f4003(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,-257);*r1p=r1;*r0p=r0;}
static void f4004(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,512);*r1p=r1;*r0p=r0;}
static void f4005(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,-512);*r1p=r1;*r0p=r0;}
static void f4006(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,511);*r1p=r1;*r0p=r0;}
static void f4007(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,-513);*r1p=r1;*r0p=r0;}
static void f4008(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,1024);*r1p=r1;*r0p=r0;}
static void f4009(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,-1024);*r1p=r1;*r0p=r0;}
static void f4010(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,1023);*r1p=r1;*r0p=r0;}
static void f4011(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,-1025);*r1p=r1;*r0p=r0;}
static void f4012(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,2048);*r1p=r1;*r0p=r0;}
static void f4013(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,-2048);*r1p=r1;*r0p=r0;}
static void f4014(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,2047);*r1p=r1;*r0p=r0;}
static void f4015(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,-2049);*r1p=r1;*r0p=r0;}
static void f4016(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,4096);*r1p=r1;*r0p=r0;}
static void f4017(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,-4096);*r1p=r1;*r0p=r0;}
static void f4018(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,4095);*r1p=r1;*r0p=r0;}
static void f4019(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,-4097);*r1p=r1;*r0p=r0;}
static void f4020(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,8192);*r1p=r1;*r0p=r0;}
static void f4021(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,-8192);*r1p=r1;*r0p=r0;}
static void f4022(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,8191);*r1p=r1;*r0p=r0;}
static void f4023(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,-8193);*r1p=r1;*r0p=r0;}
static void f4024(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,16384);*r1p=r1;*r0p=r0;}
static void f4025(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,-16384);*r1p=r1;*r0p=r0;}
static void f4026(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,16383);*r1p=r1;*r0p=r0;}
static void f4027(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,-16385);*r1p=r1;*r0p=r0;}
static void f4028(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,32768);*r1p=r1;*r0p=r0;}
static void f4029(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,-32768);*r1p=r1;*r0p=r0;}
static void f4030(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,32767);*r1p=r1;*r0p=r0;}
static void f4031(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,32767,0,-32769);*r1p=r1;*r0p=r0;}
static void f4032(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,1);*r1p=r1;*r0p=r0;}
static void f4033(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,-1);*r1p=r1;*r0p=r0;}
static void f4034(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,0);*r1p=r1;*r0p=r0;}
static void f4035(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,-2);*r1p=r1;*r0p=r0;}
static void f4036(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,2);*r1p=r1;*r0p=r0;}
static void f4037(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,-2);*r1p=r1;*r0p=r0;}
static void f4038(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,1);*r1p=r1;*r0p=r0;}
static void f4039(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,-3);*r1p=r1;*r0p=r0;}
static void f4040(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,4);*r1p=r1;*r0p=r0;}
static void f4041(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,-4);*r1p=r1;*r0p=r0;}
static void f4042(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,3);*r1p=r1;*r0p=r0;}
static void f4043(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,-5);*r1p=r1;*r0p=r0;}
static void f4044(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,8);*r1p=r1;*r0p=r0;}
static void f4045(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,-8);*r1p=r1;*r0p=r0;}
static void f4046(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,7);*r1p=r1;*r0p=r0;}
static void f4047(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,-9);*r1p=r1;*r0p=r0;}
static void f4048(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,16);*r1p=r1;*r0p=r0;}
static void f4049(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,-16);*r1p=r1;*r0p=r0;}
static void f4050(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,15);*r1p=r1;*r0p=r0;}
static void f4051(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,-17);*r1p=r1;*r0p=r0;}
static void f4052(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,32);*r1p=r1;*r0p=r0;}
static void f4053(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,-32);*r1p=r1;*r0p=r0;}
static void f4054(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,31);*r1p=r1;*r0p=r0;}
static void f4055(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,-33);*r1p=r1;*r0p=r0;}
static void f4056(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,64);*r1p=r1;*r0p=r0;}
static void f4057(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,-64);*r1p=r1;*r0p=r0;}
static void f4058(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,63);*r1p=r1;*r0p=r0;}
static void f4059(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,-65);*r1p=r1;*r0p=r0;}
static void f4060(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,128);*r1p=r1;*r0p=r0;}
static void f4061(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,-128);*r1p=r1;*r0p=r0;}
static void f4062(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,127);*r1p=r1;*r0p=r0;}
static void f4063(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,-129);*r1p=r1;*r0p=r0;}
static void f4064(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,256);*r1p=r1;*r0p=r0;}
static void f4065(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,-256);*r1p=r1;*r0p=r0;}
static void f4066(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,255);*r1p=r1;*r0p=r0;}
static void f4067(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,-257);*r1p=r1;*r0p=r0;}
static void f4068(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,512);*r1p=r1;*r0p=r0;}
static void f4069(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,-512);*r1p=r1;*r0p=r0;}
static void f4070(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,511);*r1p=r1;*r0p=r0;}
static void f4071(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,-513);*r1p=r1;*r0p=r0;}
static void f4072(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,1024);*r1p=r1;*r0p=r0;}
static void f4073(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,-1024);*r1p=r1;*r0p=r0;}
static void f4074(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,1023);*r1p=r1;*r0p=r0;}
static void f4075(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,-1025);*r1p=r1;*r0p=r0;}
static void f4076(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,2048);*r1p=r1;*r0p=r0;}
static void f4077(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,-2048);*r1p=r1;*r0p=r0;}
static void f4078(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,2047);*r1p=r1;*r0p=r0;}
static void f4079(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,-2049);*r1p=r1;*r0p=r0;}
static void f4080(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,4096);*r1p=r1;*r0p=r0;}
static void f4081(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,-4096);*r1p=r1;*r0p=r0;}
static void f4082(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,4095);*r1p=r1;*r0p=r0;}
static void f4083(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,-4097);*r1p=r1;*r0p=r0;}
static void f4084(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,8192);*r1p=r1;*r0p=r0;}
static void f4085(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,-8192);*r1p=r1;*r0p=r0;}
static void f4086(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,8191);*r1p=r1;*r0p=r0;}
static void f4087(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,-8193);*r1p=r1;*r0p=r0;}
static void f4088(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,16384);*r1p=r1;*r0p=r0;}
static void f4089(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,-16384);*r1p=r1;*r0p=r0;}
static void f4090(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,16383);*r1p=r1;*r0p=r0;}
static void f4091(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,-16385);*r1p=r1;*r0p=r0;}
static void f4092(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,32768);*r1p=r1;*r0p=r0;}
static void f4093(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,-32768);*r1p=r1;*r0p=r0;}
static void f4094(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,32767);*r1p=r1;*r0p=r0;}
static void f4095(mp_limb_t*r1p,mp_limb_t*r0p){mp_limb_t r1,r0;add_ssaaaa(r1,r0,0,-32769,0,-32769);*r1p=r1;*r0p=r0;}
typedef void (*func_t) (mp_limb_t*, mp_limb_t*);
static const func_t funcs[4096] = {
f0,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,
f16,f17,f18,f19,f20,f21,f22,f23,f24,f25,f26,f27,f28,f29,f30,f31,
f32,f33,f34,f35,f36,f37,f38,f39,f40,f41,f42,f43,f44,f45,f46,f47,
f48,f49,f50,f51,f52,f53,f54,f55,f56,f57,f58,f59,f60,f61,f62,f63,
f64,f65,f66,f67,f68,f69,f70,f71,f72,f73,f74,f75,f76,f77,f78,f79,
f80,f81,f82,f83,f84,f85,f86,f87,f88,f89,f90,f91,f92,f93,f94,f95,
f96,f97,f98,f99,f100,f101,f102,f103,f104,f105,f106,f107,f108,f109,f110,f111,
f112,f113,f114,f115,f116,f117,f118,f119,f120,f121,f122,f123,f124,f125,f126,f127,
f128,f129,f130,f131,f132,f133,f134,f135,f136,f137,f138,f139,f140,f141,f142,f143,
f144,f145,f146,f147,f148,f149,f150,f151,f152,f153,f154,f155,f156,f157,f158,f159,
f160,f161,f162,f163,f164,f165,f166,f167,f168,f169,f170,f171,f172,f173,f174,f175,
f176,f177,f178,f179,f180,f181,f182,f183,f184,f185,f186,f187,f188,f189,f190,f191,
f192,f193,f194,f195,f196,f197,f198,f199,f200,f201,f202,f203,f204,f205,f206,f207,
f208,f209,f210,f211,f212,f213,f214,f215,f216,f217,f218,f219,f220,f221,f222,f223,
f224,f225,f226,f227,f228,f229,f230,f231,f232,f233,f234,f235,f236,f237,f238,f239,
f240,f241,f242,f243,f244,f245,f246,f247,f248,f249,f250,f251,f252,f253,f254,f255,
f256,f257,f258,f259,f260,f261,f262,f263,f264,f265,f266,f267,f268,f269,f270,f271,
f272,f273,f274,f275,f276,f277,f278,f279,f280,f281,f282,f283,f284,f285,f286,f287,
f288,f289,f290,f291,f292,f293,f294,f295,f296,f297,f298,f299,f300,f301,f302,f303,
f304,f305,f306,f307,f308,f309,f310,f311,f312,f313,f314,f315,f316,f317,f318,f319,
f320,f321,f322,f323,f324,f325,f326,f327,f328,f329,f330,f331,f332,f333,f334,f335,
f336,f337,f338,f339,f340,f341,f342,f343,f344,f345,f346,f347,f348,f349,f350,f351,
f352,f353,f354,f355,f356,f357,f358,f359,f360,f361,f362,f363,f364,f365,f366,f367,
f368,f369,f370,f371,f372,f373,f374,f375,f376,f377,f378,f379,f380,f381,f382,f383,
f384,f385,f386,f387,f388,f389,f390,f391,f392,f393,f394,f395,f396,f397,f398,f399,
f400,f401,f402,f403,f404,f405,f406,f407,f408,f409,f410,f411,f412,f413,f414,f415,
f416,f417,f418,f419,f420,f421,f422,f423,f424,f425,f426,f427,f428,f429,f430,f431,
f432,f433,f434,f435,f436,f437,f438,f439,f440,f441,f442,f443,f444,f445,f446,f447,
f448,f449,f450,f451,f452,f453,f454,f455,f456,f457,f458,f459,f460,f461,f462,f463,
f464,f465,f466,f467,f468,f469,f470,f471,f472,f473,f474,f475,f476,f477,f478,f479,
f480,f481,f482,f483,f484,f485,f486,f487,f488,f489,f490,f491,f492,f493,f494,f495,
f496,f497,f498,f499,f500,f501,f502,f503,f504,f505,f506,f507,f508,f509,f510,f511,
f512,f513,f514,f515,f516,f517,f518,f519,f520,f521,f522,f523,f524,f525,f526,f527,
f528,f529,f530,f531,f532,f533,f534,f535,f536,f537,f538,f539,f540,f541,f542,f543,
f544,f545,f546,f547,f548,f549,f550,f551,f552,f553,f554,f555,f556,f557,f558,f559,
f560,f561,f562,f563,f564,f565,f566,f567,f568,f569,f570,f571,f572,f573,f574,f575,
f576,f577,f578,f579,f580,f581,f582,f583,f584,f585,f586,f587,f588,f589,f590,f591,
f592,f593,f594,f595,f596,f597,f598,f599,f600,f601,f602,f603,f604,f605,f606,f607,
f608,f609,f610,f611,f612,f613,f614,f615,f616,f617,f618,f619,f620,f621,f622,f623,
f624,f625,f626,f627,f628,f629,f630,f631,f632,f633,f634,f635,f636,f637,f638,f639,
f640,f641,f642,f643,f644,f645,f646,f647,f648,f649,f650,f651,f652,f653,f654,f655,
f656,f657,f658,f659,f660,f661,f662,f663,f664,f665,f666,f667,f668,f669,f670,f671,
f672,f673,f674,f675,f676,f677,f678,f679,f680,f681,f682,f683,f684,f685,f686,f687,
f688,f689,f690,f691,f692,f693,f694,f695,f696,f697,f698,f699,f700,f701,f702,f703,
f704,f705,f706,f707,f708,f709,f710,f711,f712,f713,f714,f715,f716,f717,f718,f719,
f720,f721,f722,f723,f724,f725,f726,f727,f728,f729,f730,f731,f732,f733,f734,f735,
f736,f737,f738,f739,f740,f741,f742,f743,f744,f745,f746,f747,f748,f749,f750,f751,
f752,f753,f754,f755,f756,f757,f758,f759,f760,f761,f762,f763,f764,f765,f766,f767,
f768,f769,f770,f771,f772,f773,f774,f775,f776,f777,f778,f779,f780,f781,f782,f783,
f784,f785,f786,f787,f788,f789,f790,f791,f792,f793,f794,f795,f796,f797,f798,f799,
f800,f801,f802,f803,f804,f805,f806,f807,f808,f809,f810,f811,f812,f813,f814,f815,
f816,f817,f818,f819,f820,f821,f822,f823,f824,f825,f826,f827,f828,f829,f830,f831,
f832,f833,f834,f835,f836,f837,f838,f839,f840,f841,f842,f843,f844,f845,f846,f847,
f848,f849,f850,f851,f852,f853,f854,f855,f856,f857,f858,f859,f860,f861,f862,f863,
f864,f865,f866,f867,f868,f869,f870,f871,f872,f873,f874,f875,f876,f877,f878,f879,
f880,f881,f882,f883,f884,f885,f886,f887,f888,f889,f890,f891,f892,f893,f894,f895,
f896,f897,f898,f899,f900,f901,f902,f903,f904,f905,f906,f907,f908,f909,f910,f911,
f912,f913,f914,f915,f916,f917,f918,f919,f920,f921,f922,f923,f924,f925,f926,f927,
f928,f929,f930,f931,f932,f933,f934,f935,f936,f937,f938,f939,f940,f941,f942,f943,
f944,f945,f946,f947,f948,f949,f950,f951,f952,f953,f954,f955,f956,f957,f958,f959,
f960,f961,f962,f963,f964,f965,f966,f967,f968,f969,f970,f971,f972,f973,f974,f975,
f976,f977,f978,f979,f980,f981,f982,f983,f984,f985,f986,f987,f988,f989,f990,f991,
f992,f993,f994,f995,f996,f997,f998,f999,f1000,f1001,f1002,f1003,f1004,f1005,f1006,f1007,
f1008,f1009,f1010,f1011,f1012,f1013,f1014,f1015,f1016,f1017,f1018,f1019,f1020,f1021,f1022,f1023,
f1024,f1025,f1026,f1027,f1028,f1029,f1030,f1031,f1032,f1033,f1034,f1035,f1036,f1037,f1038,f1039,
f1040,f1041,f1042,f1043,f1044,f1045,f1046,f1047,f1048,f1049,f1050,f1051,f1052,f1053,f1054,f1055,
f1056,f1057,f1058,f1059,f1060,f1061,f1062,f1063,f1064,f1065,f1066,f1067,f1068,f1069,f1070,f1071,
f1072,f1073,f1074,f1075,f1076,f1077,f1078,f1079,f1080,f1081,f1082,f1083,f1084,f1085,f1086,f1087,
f1088,f1089,f1090,f1091,f1092,f1093,f1094,f1095,f1096,f1097,f1098,f1099,f1100,f1101,f1102,f1103,
f1104,f1105,f1106,f1107,f1108,f1109,f1110,f1111,f1112,f1113,f1114,f1115,f1116,f1117,f1118,f1119,
f1120,f1121,f1122,f1123,f1124,f1125,f1126,f1127,f1128,f1129,f1130,f1131,f1132,f1133,f1134,f1135,
f1136,f1137,f1138,f1139,f1140,f1141,f1142,f1143,f1144,f1145,f1146,f1147,f1148,f1149,f1150,f1151,
f1152,f1153,f1154,f1155,f1156,f1157,f1158,f1159,f1160,f1161,f1162,f1163,f1164,f1165,f1166,f1167,
f1168,f1169,f1170,f1171,f1172,f1173,f1174,f1175,f1176,f1177,f1178,f1179,f1180,f1181,f1182,f1183,
f1184,f1185,f1186,f1187,f1188,f1189,f1190,f1191,f1192,f1193,f1194,f1195,f1196,f1197,f1198,f1199,
f1200,f1201,f1202,f1203,f1204,f1205,f1206,f1207,f1208,f1209,f1210,f1211,f1212,f1213,f1214,f1215,
f1216,f1217,f1218,f1219,f1220,f1221,f1222,f1223,f1224,f1225,f1226,f1227,f1228,f1229,f1230,f1231,
f1232,f1233,f1234,f1235,f1236,f1237,f1238,f1239,f1240,f1241,f1242,f1243,f1244,f1245,f1246,f1247,
f1248,f1249,f1250,f1251,f1252,f1253,f1254,f1255,f1256,f1257,f1258,f1259,f1260,f1261,f1262,f1263,
f1264,f1265,f1266,f1267,f1268,f1269,f1270,f1271,f1272,f1273,f1274,f1275,f1276,f1277,f1278,f1279,
f1280,f1281,f1282,f1283,f1284,f1285,f1286,f1287,f1288,f1289,f1290,f1291,f1292,f1293,f1294,f1295,
f1296,f1297,f1298,f1299,f1300,f1301,f1302,f1303,f1304,f1305,f1306,f1307,f1308,f1309,f1310,f1311,
f1312,f1313,f1314,f1315,f1316,f1317,f1318,f1319,f1320,f1321,f1322,f1323,f1324,f1325,f1326,f1327,
f1328,f1329,f1330,f1331,f1332,f1333,f1334,f1335,f1336,f1337,f1338,f1339,f1340,f1341,f1342,f1343,
f1344,f1345,f1346,f1347,f1348,f1349,f1350,f1351,f1352,f1353,f1354,f1355,f1356,f1357,f1358,f1359,
f1360,f1361,f1362,f1363,f1364,f1365,f1366,f1367,f1368,f1369,f1370,f1371,f1372,f1373,f1374,f1375,
f1376,f1377,f1378,f1379,f1380,f1381,f1382,f1383,f1384,f1385,f1386,f1387,f1388,f1389,f1390,f1391,
f1392,f1393,f1394,f1395,f1396,f1397,f1398,f1399,f1400,f1401,f1402,f1403,f1404,f1405,f1406,f1407,
f1408,f1409,f1410,f1411,f1412,f1413,f1414,f1415,f1416,f1417,f1418,f1419,f1420,f1421,f1422,f1423,
f1424,f1425,f1426,f1427,f1428,f1429,f1430,f1431,f1432,f1433,f1434,f1435,f1436,f1437,f1438,f1439,
f1440,f1441,f1442,f1443,f1444,f1445,f1446,f1447,f1448,f1449,f1450,f1451,f1452,f1453,f1454,f1455,
f1456,f1457,f1458,f1459,f1460,f1461,f1462,f1463,f1464,f1465,f1466,f1467,f1468,f1469,f1470,f1471,
f1472,f1473,f1474,f1475,f1476,f1477,f1478,f1479,f1480,f1481,f1482,f1483,f1484,f1485,f1486,f1487,
f1488,f1489,f1490,f1491,f1492,f1493,f1494,f1495,f1496,f1497,f1498,f1499,f1500,f1501,f1502,f1503,
f1504,f1505,f1506,f1507,f1508,f1509,f1510,f1511,f1512,f1513,f1514,f1515,f1516,f1517,f1518,f1519,
f1520,f1521,f1522,f1523,f1524,f1525,f1526,f1527,f1528,f1529,f1530,f1531,f1532,f1533,f1534,f1535,
f1536,f1537,f1538,f1539,f1540,f1541,f1542,f1543,f1544,f1545,f1546,f1547,f1548,f1549,f1550,f1551,
f1552,f1553,f1554,f1555,f1556,f1557,f1558,f1559,f1560,f1561,f1562,f1563,f1564,f1565,f1566,f1567,
f1568,f1569,f1570,f1571,f1572,f1573,f1574,f1575,f1576,f1577,f1578,f1579,f1580,f1581,f1582,f1583,
f1584,f1585,f1586,f1587,f1588,f1589,f1590,f1591,f1592,f1593,f1594,f1595,f1596,f1597,f1598,f1599,
f1600,f1601,f1602,f1603,f1604,f1605,f1606,f1607,f1608,f1609,f1610,f1611,f1612,f1613,f1614,f1615,
f1616,f1617,f1618,f1619,f1620,f1621,f1622,f1623,f1624,f1625,f1626,f1627,f1628,f1629,f1630,f1631,
f1632,f1633,f1634,f1635,f1636,f1637,f1638,f1639,f1640,f1641,f1642,f1643,f1644,f1645,f1646,f1647,
f1648,f1649,f1650,f1651,f1652,f1653,f1654,f1655,f1656,f1657,f1658,f1659,f1660,f1661,f1662,f1663,
f1664,f1665,f1666,f1667,f1668,f1669,f1670,f1671,f1672,f1673,f1674,f1675,f1676,f1677,f1678,f1679,
f1680,f1681,f1682,f1683,f1684,f1685,f1686,f1687,f1688,f1689,f1690,f1691,f1692,f1693,f1694,f1695,
f1696,f1697,f1698,f1699,f1700,f1701,f1702,f1703,f1704,f1705,f1706,f1707,f1708,f1709,f1710,f1711,
f1712,f1713,f1714,f1715,f1716,f1717,f1718,f1719,f1720,f1721,f1722,f1723,f1724,f1725,f1726,f1727,
f1728,f1729,f1730,f1731,f1732,f1733,f1734,f1735,f1736,f1737,f1738,f1739,f1740,f1741,f1742,f1743,
f1744,f1745,f1746,f1747,f1748,f1749,f1750,f1751,f1752,f1753,f1754,f1755,f1756,f1757,f1758,f1759,
f1760,f1761,f1762,f1763,f1764,f1765,f1766,f1767,f1768,f1769,f1770,f1771,f1772,f1773,f1774,f1775,
f1776,f1777,f1778,f1779,f1780,f1781,f1782,f1783,f1784,f1785,f1786,f1787,f1788,f1789,f1790,f1791,
f1792,f1793,f1794,f1795,f1796,f1797,f1798,f1799,f1800,f1801,f1802,f1803,f1804,f1805,f1806,f1807,
f1808,f1809,f1810,f1811,f1812,f1813,f1814,f1815,f1816,f1817,f1818,f1819,f1820,f1821,f1822,f1823,
f1824,f1825,f1826,f1827,f1828,f1829,f1830,f1831,f1832,f1833,f1834,f1835,f1836,f1837,f1838,f1839,
f1840,f1841,f1842,f1843,f1844,f1845,f1846,f1847,f1848,f1849,f1850,f1851,f1852,f1853,f1854,f1855,
f1856,f1857,f1858,f1859,f1860,f1861,f1862,f1863,f1864,f1865,f1866,f1867,f1868,f1869,f1870,f1871,
f1872,f1873,f1874,f1875,f1876,f1877,f1878,f1879,f1880,f1881,f1882,f1883,f1884,f1885,f1886,f1887,
f1888,f1889,f1890,f1891,f1892,f1893,f1894,f1895,f1896,f1897,f1898,f1899,f1900,f1901,f1902,f1903,
f1904,f1905,f1906,f1907,f1908,f1909,f1910,f1911,f1912,f1913,f1914,f1915,f1916,f1917,f1918,f1919,
f1920,f1921,f1922,f1923,f1924,f1925,f1926,f1927,f1928,f1929,f1930,f1931,f1932,f1933,f1934,f1935,
f1936,f1937,f1938,f1939,f1940,f1941,f1942,f1943,f1944,f1945,f1946,f1947,f1948,f1949,f1950,f1951,
f1952,f1953,f1954,f1955,f1956,f1957,f1958,f1959,f1960,f1961,f1962,f1963,f1964,f1965,f1966,f1967,
f1968,f1969,f1970,f1971,f1972,f1973,f1974,f1975,f1976,f1977,f1978,f1979,f1980,f1981,f1982,f1983,
f1984,f1985,f1986,f1987,f1988,f1989,f1990,f1991,f1992,f1993,f1994,f1995,f1996,f1997,f1998,f1999,
f2000,f2001,f2002,f2003,f2004,f2005,f2006,f2007,f2008,f2009,f2010,f2011,f2012,f2013,f2014,f2015,
f2016,f2017,f2018,f2019,f2020,f2021,f2022,f2023,f2024,f2025,f2026,f2027,f2028,f2029,f2030,f2031,
f2032,f2033,f2034,f2035,f2036,f2037,f2038,f2039,f2040,f2041,f2042,f2043,f2044,f2045,f2046,f2047,
f2048,f2049,f2050,f2051,f2052,f2053,f2054,f2055,f2056,f2057,f2058,f2059,f2060,f2061,f2062,f2063,
f2064,f2065,f2066,f2067,f2068,f2069,f2070,f2071,f2072,f2073,f2074,f2075,f2076,f2077,f2078,f2079,
f2080,f2081,f2082,f2083,f2084,f2085,f2086,f2087,f2088,f2089,f2090,f2091,f2092,f2093,f2094,f2095,
f2096,f2097,f2098,f2099,f2100,f2101,f2102,f2103,f2104,f2105,f2106,f2107,f2108,f2109,f2110,f2111,
f2112,f2113,f2114,f2115,f2116,f2117,f2118,f2119,f2120,f2121,f2122,f2123,f2124,f2125,f2126,f2127,
f2128,f2129,f2130,f2131,f2132,f2133,f2134,f2135,f2136,f2137,f2138,f2139,f2140,f2141,f2142,f2143,
f2144,f2145,f2146,f2147,f2148,f2149,f2150,f2151,f2152,f2153,f2154,f2155,f2156,f2157,f2158,f2159,
f2160,f2161,f2162,f2163,f2164,f2165,f2166,f2167,f2168,f2169,f2170,f2171,f2172,f2173,f2174,f2175,
f2176,f2177,f2178,f2179,f2180,f2181,f2182,f2183,f2184,f2185,f2186,f2187,f2188,f2189,f2190,f2191,
f2192,f2193,f2194,f2195,f2196,f2197,f2198,f2199,f2200,f2201,f2202,f2203,f2204,f2205,f2206,f2207,
f2208,f2209,f2210,f2211,f2212,f2213,f2214,f2215,f2216,f2217,f2218,f2219,f2220,f2221,f2222,f2223,
f2224,f2225,f2226,f2227,f2228,f2229,f2230,f2231,f2232,f2233,f2234,f2235,f2236,f2237,f2238,f2239,
f2240,f2241,f2242,f2243,f2244,f2245,f2246,f2247,f2248,f2249,f2250,f2251,f2252,f2253,f2254,f2255,
f2256,f2257,f2258,f2259,f2260,f2261,f2262,f2263,f2264,f2265,f2266,f2267,f2268,f2269,f2270,f2271,
f2272,f2273,f2274,f2275,f2276,f2277,f2278,f2279,f2280,f2281,f2282,f2283,f2284,f2285,f2286,f2287,
f2288,f2289,f2290,f2291,f2292,f2293,f2294,f2295,f2296,f2297,f2298,f2299,f2300,f2301,f2302,f2303,
f2304,f2305,f2306,f2307,f2308,f2309,f2310,f2311,f2312,f2313,f2314,f2315,f2316,f2317,f2318,f2319,
f2320,f2321,f2322,f2323,f2324,f2325,f2326,f2327,f2328,f2329,f2330,f2331,f2332,f2333,f2334,f2335,
f2336,f2337,f2338,f2339,f2340,f2341,f2342,f2343,f2344,f2345,f2346,f2347,f2348,f2349,f2350,f2351,
f2352,f2353,f2354,f2355,f2356,f2357,f2358,f2359,f2360,f2361,f2362,f2363,f2364,f2365,f2366,f2367,
f2368,f2369,f2370,f2371,f2372,f2373,f2374,f2375,f2376,f2377,f2378,f2379,f2380,f2381,f2382,f2383,
f2384,f2385,f2386,f2387,f2388,f2389,f2390,f2391,f2392,f2393,f2394,f2395,f2396,f2397,f2398,f2399,
f2400,f2401,f2402,f2403,f2404,f2405,f2406,f2407,f2408,f2409,f2410,f2411,f2412,f2413,f2414,f2415,
f2416,f2417,f2418,f2419,f2420,f2421,f2422,f2423,f2424,f2425,f2426,f2427,f2428,f2429,f2430,f2431,
f2432,f2433,f2434,f2435,f2436,f2437,f2438,f2439,f2440,f2441,f2442,f2443,f2444,f2445,f2446,f2447,
f2448,f2449,f2450,f2451,f2452,f2453,f2454,f2455,f2456,f2457,f2458,f2459,f2460,f2461,f2462,f2463,
f2464,f2465,f2466,f2467,f2468,f2469,f2470,f2471,f2472,f2473,f2474,f2475,f2476,f2477,f2478,f2479,
f2480,f2481,f2482,f2483,f2484,f2485,f2486,f2487,f2488,f2489,f2490,f2491,f2492,f2493,f2494,f2495,
f2496,f2497,f2498,f2499,f2500,f2501,f2502,f2503,f2504,f2505,f2506,f2507,f2508,f2509,f2510,f2511,
f2512,f2513,f2514,f2515,f2516,f2517,f2518,f2519,f2520,f2521,f2522,f2523,f2524,f2525,f2526,f2527,
f2528,f2529,f2530,f2531,f2532,f2533,f2534,f2535,f2536,f2537,f2538,f2539,f2540,f2541,f2542,f2543,
f2544,f2545,f2546,f2547,f2548,f2549,f2550,f2551,f2552,f2553,f2554,f2555,f2556,f2557,f2558,f2559,
f2560,f2561,f2562,f2563,f2564,f2565,f2566,f2567,f2568,f2569,f2570,f2571,f2572,f2573,f2574,f2575,
f2576,f2577,f2578,f2579,f2580,f2581,f2582,f2583,f2584,f2585,f2586,f2587,f2588,f2589,f2590,f2591,
f2592,f2593,f2594,f2595,f2596,f2597,f2598,f2599,f2600,f2601,f2602,f2603,f2604,f2605,f2606,f2607,
f2608,f2609,f2610,f2611,f2612,f2613,f2614,f2615,f2616,f2617,f2618,f2619,f2620,f2621,f2622,f2623,
f2624,f2625,f2626,f2627,f2628,f2629,f2630,f2631,f2632,f2633,f2634,f2635,f2636,f2637,f2638,f2639,
f2640,f2641,f2642,f2643,f2644,f2645,f2646,f2647,f2648,f2649,f2650,f2651,f2652,f2653,f2654,f2655,
f2656,f2657,f2658,f2659,f2660,f2661,f2662,f2663,f2664,f2665,f2666,f2667,f2668,f2669,f2670,f2671,
f2672,f2673,f2674,f2675,f2676,f2677,f2678,f2679,f2680,f2681,f2682,f2683,f2684,f2685,f2686,f2687,
f2688,f2689,f2690,f2691,f2692,f2693,f2694,f2695,f2696,f2697,f2698,f2699,f2700,f2701,f2702,f2703,
f2704,f2705,f2706,f2707,f2708,f2709,f2710,f2711,f2712,f2713,f2714,f2715,f2716,f2717,f2718,f2719,
f2720,f2721,f2722,f2723,f2724,f2725,f2726,f2727,f2728,f2729,f2730,f2731,f2732,f2733,f2734,f2735,
f2736,f2737,f2738,f2739,f2740,f2741,f2742,f2743,f2744,f2745,f2746,f2747,f2748,f2749,f2750,f2751,
f2752,f2753,f2754,f2755,f2756,f2757,f2758,f2759,f2760,f2761,f2762,f2763,f2764,f2765,f2766,f2767,
f2768,f2769,f2770,f2771,f2772,f2773,f2774,f2775,f2776,f2777,f2778,f2779,f2780,f2781,f2782,f2783,
f2784,f2785,f2786,f2787,f2788,f2789,f2790,f2791,f2792,f2793,f2794,f2795,f2796,f2797,f2798,f2799,
f2800,f2801,f2802,f2803,f2804,f2805,f2806,f2807,f2808,f2809,f2810,f2811,f2812,f2813,f2814,f2815,
f2816,f2817,f2818,f2819,f2820,f2821,f2822,f2823,f2824,f2825,f2826,f2827,f2828,f2829,f2830,f2831,
f2832,f2833,f2834,f2835,f2836,f2837,f2838,f2839,f2840,f2841,f2842,f2843,f2844,f2845,f2846,f2847,
f2848,f2849,f2850,f2851,f2852,f2853,f2854,f2855,f2856,f2857,f2858,f2859,f2860,f2861,f2862,f2863,
f2864,f2865,f2866,f2867,f2868,f2869,f2870,f2871,f2872,f2873,f2874,f2875,f2876,f2877,f2878,f2879,
f2880,f2881,f2882,f2883,f2884,f2885,f2886,f2887,f2888,f2889,f2890,f2891,f2892,f2893,f2894,f2895,
f2896,f2897,f2898,f2899,f2900,f2901,f2902,f2903,f2904,f2905,f2906,f2907,f2908,f2909,f2910,f2911,
f2912,f2913,f2914,f2915,f2916,f2917,f2918,f2919,f2920,f2921,f2922,f2923,f2924,f2925,f2926,f2927,
f2928,f2929,f2930,f2931,f2932,f2933,f2934,f2935,f2936,f2937,f2938,f2939,f2940,f2941,f2942,f2943,
f2944,f2945,f2946,f2947,f2948,f2949,f2950,f2951,f2952,f2953,f2954,f2955,f2956,f2957,f2958,f2959,
f2960,f2961,f2962,f2963,f2964,f2965,f2966,f2967,f2968,f2969,f2970,f2971,f2972,f2973,f2974,f2975,
f2976,f2977,f2978,f2979,f2980,f2981,f2982,f2983,f2984,f2985,f2986,f2987,f2988,f2989,f2990,f2991,
f2992,f2993,f2994,f2995,f2996,f2997,f2998,f2999,f3000,f3001,f3002,f3003,f3004,f3005,f3006,f3007,
f3008,f3009,f3010,f3011,f3012,f3013,f3014,f3015,f3016,f3017,f3018,f3019,f3020,f3021,f3022,f3023,
f3024,f3025,f3026,f3027,f3028,f3029,f3030,f3031,f3032,f3033,f3034,f3035,f3036,f3037,f3038,f3039,
f3040,f3041,f3042,f3043,f3044,f3045,f3046,f3047,f3048,f3049,f3050,f3051,f3052,f3053,f3054,f3055,
f3056,f3057,f3058,f3059,f3060,f3061,f3062,f3063,f3064,f3065,f3066,f3067,f3068,f3069,f3070,f3071,
f3072,f3073,f3074,f3075,f3076,f3077,f3078,f3079,f3080,f3081,f3082,f3083,f3084,f3085,f3086,f3087,
f3088,f3089,f3090,f3091,f3092,f3093,f3094,f3095,f3096,f3097,f3098,f3099,f3100,f3101,f3102,f3103,
f3104,f3105,f3106,f3107,f3108,f3109,f3110,f3111,f3112,f3113,f3114,f3115,f3116,f3117,f3118,f3119,
f3120,f3121,f3122,f3123,f3124,f3125,f3126,f3127,f3128,f3129,f3130,f3131,f3132,f3133,f3134,f3135,
f3136,f3137,f3138,f3139,f3140,f3141,f3142,f3143,f3144,f3145,f3146,f3147,f3148,f3149,f3150,f3151,
f3152,f3153,f3154,f3155,f3156,f3157,f3158,f3159,f3160,f3161,f3162,f3163,f3164,f3165,f3166,f3167,
f3168,f3169,f3170,f3171,f3172,f3173,f3174,f3175,f3176,f3177,f3178,f3179,f3180,f3181,f3182,f3183,
f3184,f3185,f3186,f3187,f3188,f3189,f3190,f3191,f3192,f3193,f3194,f3195,f3196,f3197,f3198,f3199,
f3200,f3201,f3202,f3203,f3204,f3205,f3206,f3207,f3208,f3209,f3210,f3211,f3212,f3213,f3214,f3215,
f3216,f3217,f3218,f3219,f3220,f3221,f3222,f3223,f3224,f3225,f3226,f3227,f3228,f3229,f3230,f3231,
f3232,f3233,f3234,f3235,f3236,f3237,f3238,f3239,f3240,f3241,f3242,f3243,f3244,f3245,f3246,f3247,
f3248,f3249,f3250,f3251,f3252,f3253,f3254,f3255,f3256,f3257,f3258,f3259,f3260,f3261,f3262,f3263,
f3264,f3265,f3266,f3267,f3268,f3269,f3270,f3271,f3272,f3273,f3274,f3275,f3276,f3277,f3278,f3279,
f3280,f3281,f3282,f3283,f3284,f3285,f3286,f3287,f3288,f3289,f3290,f3291,f3292,f3293,f3294,f3295,
f3296,f3297,f3298,f3299,f3300,f3301,f3302,f3303,f3304,f3305,f3306,f3307,f3308,f3309,f3310,f3311,
f3312,f3313,f3314,f3315,f3316,f3317,f3318,f3319,f3320,f3321,f3322,f3323,f3324,f3325,f3326,f3327,
f3328,f3329,f3330,f3331,f3332,f3333,f3334,f3335,f3336,f3337,f3338,f3339,f3340,f3341,f3342,f3343,
f3344,f3345,f3346,f3347,f3348,f3349,f3350,f3351,f3352,f3353,f3354,f3355,f3356,f3357,f3358,f3359,
f3360,f3361,f3362,f3363,f3364,f3365,f3366,f3367,f3368,f3369,f3370,f3371,f3372,f3373,f3374,f3375,
f3376,f3377,f3378,f3379,f3380,f3381,f3382,f3383,f3384,f3385,f3386,f3387,f3388,f3389,f3390,f3391,
f3392,f3393,f3394,f3395,f3396,f3397,f3398,f3399,f3400,f3401,f3402,f3403,f3404,f3405,f3406,f3407,
f3408,f3409,f3410,f3411,f3412,f3413,f3414,f3415,f3416,f3417,f3418,f3419,f3420,f3421,f3422,f3423,
f3424,f3425,f3426,f3427,f3428,f3429,f3430,f3431,f3432,f3433,f3434,f3435,f3436,f3437,f3438,f3439,
f3440,f3441,f3442,f3443,f3444,f3445,f3446,f3447,f3448,f3449,f3450,f3451,f3452,f3453,f3454,f3455,
f3456,f3457,f3458,f3459,f3460,f3461,f3462,f3463,f3464,f3465,f3466,f3467,f3468,f3469,f3470,f3471,
f3472,f3473,f3474,f3475,f3476,f3477,f3478,f3479,f3480,f3481,f3482,f3483,f3484,f3485,f3486,f3487,
f3488,f3489,f3490,f3491,f3492,f3493,f3494,f3495,f3496,f3497,f3498,f3499,f3500,f3501,f3502,f3503,
f3504,f3505,f3506,f3507,f3508,f3509,f3510,f3511,f3512,f3513,f3514,f3515,f3516,f3517,f3518,f3519,
f3520,f3521,f3522,f3523,f3524,f3525,f3526,f3527,f3528,f3529,f3530,f3531,f3532,f3533,f3534,f3535,
f3536,f3537,f3538,f3539,f3540,f3541,f3542,f3543,f3544,f3545,f3546,f3547,f3548,f3549,f3550,f3551,
f3552,f3553,f3554,f3555,f3556,f3557,f3558,f3559,f3560,f3561,f3562,f3563,f3564,f3565,f3566,f3567,
f3568,f3569,f3570,f3571,f3572,f3573,f3574,f3575,f3576,f3577,f3578,f3579,f3580,f3581,f3582,f3583,
f3584,f3585,f3586,f3587,f3588,f3589,f3590,f3591,f3592,f3593,f3594,f3595,f3596,f3597,f3598,f3599,
f3600,f3601,f3602,f3603,f3604,f3605,f3606,f3607,f3608,f3609,f3610,f3611,f3612,f3613,f3614,f3615,
f3616,f3617,f3618,f3619,f3620,f3621,f3622,f3623,f3624,f3625,f3626,f3627,f3628,f3629,f3630,f3631,
f3632,f3633,f3634,f3635,f3636,f3637,f3638,f3639,f3640,f3641,f3642,f3643,f3644,f3645,f3646,f3647,
f3648,f3649,f3650,f3651,f3652,f3653,f3654,f3655,f3656,f3657,f3658,f3659,f3660,f3661,f3662,f3663,
f3664,f3665,f3666,f3667,f3668,f3669,f3670,f3671,f3672,f3673,f3674,f3675,f3676,f3677,f3678,f3679,
f3680,f3681,f3682,f3683,f3684,f3685,f3686,f3687,f3688,f3689,f3690,f3691,f3692,f3693,f3694,f3695,
f3696,f3697,f3698,f3699,f3700,f3701,f3702,f3703,f3704,f3705,f3706,f3707,f3708,f3709,f3710,f3711,
f3712,f3713,f3714,f3715,f3716,f3717,f3718,f3719,f3720,f3721,f3722,f3723,f3724,f3725,f3726,f3727,
f3728,f3729,f3730,f3731,f3732,f3733,f3734,f3735,f3736,f3737,f3738,f3739,f3740,f3741,f3742,f3743,
f3744,f3745,f3746,f3747,f3748,f3749,f3750,f3751,f3752,f3753,f3754,f3755,f3756,f3757,f3758,f3759,
f3760,f3761,f3762,f3763,f3764,f3765,f3766,f3767,f3768,f3769,f3770,f3771,f3772,f3773,f3774,f3775,
f3776,f3777,f3778,f3779,f3780,f3781,f3782,f3783,f3784,f3785,f3786,f3787,f3788,f3789,f3790,f3791,
f3792,f3793,f3794,f3795,f3796,f3797,f3798,f3799,f3800,f3801,f3802,f3803,f3804,f3805,f3806,f3807,
f3808,f3809,f3810,f3811,f3812,f3813,f3814,f3815,f3816,f3817,f3818,f3819,f3820,f3821,f3822,f3823,
f3824,f3825,f3826,f3827,f3828,f3829,f3830,f3831,f3832,f3833,f3834,f3835,f3836,f3837,f3838,f3839,
f3840,f3841,f3842,f3843,f3844,f3845,f3846,f3847,f3848,f3849,f3850,f3851,f3852,f3853,f3854,f3855,
f3856,f3857,f3858,f3859,f3860,f3861,f3862,f3863,f3864,f3865,f3866,f3867,f3868,f3869,f3870,f3871,
f3872,f3873,f3874,f3875,f3876,f3877,f3878,f3879,f3880,f3881,f3882,f3883,f3884,f3885,f3886,f3887,
f3888,f3889,f3890,f3891,f3892,f3893,f3894,f3895,f3896,f3897,f3898,f3899,f3900,f3901,f3902,f3903,
f3904,f3905,f3906,f3907,f3908,f3909,f3910,f3911,f3912,f3913,f3914,f3915,f3916,f3917,f3918,f3919,
f3920,f3921,f3922,f3923,f3924,f3925,f3926,f3927,f3928,f3929,f3930,f3931,f3932,f3933,f3934,f3935,
f3936,f3937,f3938,f3939,f3940,f3941,f3942,f3943,f3944,f3945,f3946,f3947,f3948,f3949,f3950,f3951,
f3952,f3953,f3954,f3955,f3956,f3957,f3958,f3959,f3960,f3961,f3962,f3963,f3964,f3965,f3966,f3967,
f3968,f3969,f3970,f3971,f3972,f3973,f3974,f3975,f3976,f3977,f3978,f3979,f3980,f3981,f3982,f3983,
f3984,f3985,f3986,f3987,f3988,f3989,f3990,f3991,f3992,f3993,f3994,f3995,f3996,f3997,f3998,f3999,
f4000,f4001,f4002,f4003,f4004,f4005,f4006,f4007,f4008,f4009,f4010,f4011,f4012,f4013,f4014,f4015,
f4016,f4017,f4018,f4019,f4020,f4021,f4022,f4023,f4024,f4025,f4026,f4027,f4028,f4029,f4030,f4031,
f4032,f4033,f4034,f4035,f4036,f4037,f4038,f4039,f4040,f4041,f4042,f4043,f4044,f4045,f4046,f4047,
f4048,f4049,f4050,f4051,f4052,f4053,f4054,f4055,f4056,f4057,f4058,f4059,f4060,f4061,f4062,f4063,
f4064,f4065,f4066,f4067,f4068,f4069,f4070,f4071,f4072,f4073,f4074,f4075,f4076,f4077,f4078,f4079,
f4080,f4081,f4082,f4083,f4084,f4085,f4086,f4087,f4088,f4089,f4090,f4091,f4092,f4093,f4094,f4095,
};
static const int ref[4096][2] = {
{     2, 0},
{     0, 1},{     1, 0},{    -1, 0},{     3, 0},{    -1, 0},{     2, 0},{    -2, 0},{     5, 0},
{    -3, 0},{     4, 0},{    -4, 0},{     9, 0},{    -7, 0},{     8, 0},{    -8, 0},{    17, 0},
{   -15, 0},{    16, 0},{   -16, 0},{    33, 0},{   -31, 0},{    32, 0},{   -32, 0},{    65, 0},
{   -63, 0},{    64, 0},{   -64, 0},{   129, 0},{  -127, 0},{   128, 0},{  -128, 0},{   257, 0},
{  -255, 0},{   256, 0},{  -256, 0},{   513, 0},{  -511, 0},{   512, 0},{  -512, 0},{  1025, 0},
{ -1023, 0},{  1024, 0},{ -1024, 0},{  2049, 0},{ -2047, 0},{  2048, 0},{ -2048, 0},{  4097, 0},
{ -4095, 0},{  4096, 0},{ -4096, 0},{  8193, 0},{ -8191, 0},{  8192, 0},{ -8192, 0},{ 16385, 0},
{-16383, 0},{ 16384, 0},{-16384, 0},{ 32769, 0},{-32767, 0},{ 32768, 0},{-32768, 0},{     0, 1},
{    -2, 1},{    -1, 0},{    -3, 1},{     1, 1},{    -3, 1},{     0, 1},{    -4, 1},{     3, 1},
{    -5, 1},{     2, 1},{    -6, 1},{     7, 1},{    -9, 1},{     6, 1},{   -10, 1},{    15, 1},
{   -17, 1},{    14, 1},{   -18, 1},{    31, 1},{   -33, 1},{    30, 1},{   -34, 1},{    63, 1},
{   -65, 1},{    62, 1},{   -66, 1},{   127, 1},{  -129, 1},{   126, 1},{  -130, 1},{   255, 1},
{  -257, 1},{   254, 1},{  -258, 1},{   511, 1},{  -513, 1},{   510, 1},{  -514, 1},{  1023, 1},
{ -1025, 1},{  1022, 1},{ -1026, 1},{  2047, 1},{ -2049, 1},{  2046, 1},{ -2050, 1},{  4095, 1},
{ -4097, 1},{  4094, 1},{ -4098, 1},{  8191, 1},{ -8193, 1},{  8190, 1},{ -8194, 1},{ 16383, 1},
{-16385, 1},{ 16382, 1},{-16386, 1},{ 32767, 1},{-32769, 1},{ 32766, 1},{-32770, 1},{     1, 0},
{    -1, 0},{     0, 0},{    -2, 0},{     2, 0},{    -2, 0},{     1, 0},{    -3, 0},{     4, 0},
{    -4, 0},{     3, 0},{    -5, 0},{     8, 0},{    -8, 0},{     7, 0},{    -9, 0},{    16, 0},
{   -16, 0},{    15, 0},{   -17, 0},{    32, 0},{   -32, 0},{    31, 0},{   -33, 0},{    64, 0},
{   -64, 0},{    63, 0},{   -65, 0},{   128, 0},{  -128, 0},{   127, 0},{  -129, 0},{   256, 0},
{  -256, 0},{   255, 0},{  -257, 0},{   512, 0},{  -512, 0},{   511, 0},{  -513, 0},{  1024, 0},
{ -1024, 0},{  1023, 0},{ -1025, 0},{  2048, 0},{ -2048, 0},{  2047, 0},{ -2049, 0},{  4096, 0},
{ -4096, 0},{  4095, 0},{ -4097, 0},{  8192, 0},{ -8192, 0},{  8191, 0},{ -8193, 0},{ 16384, 0},
{-16384, 0},{ 16383, 0},{-16385, 0},{ 32768, 0},{-32768, 0},{ 32767, 0},{-32769, 0},{    -1, 0},
{    -3, 1},{    -2, 0},{    -4, 1},{     0, 1},{    -4, 1},{    -1, 0},{    -5, 1},{     2, 1},
{    -6, 1},{     1, 1},{    -7, 1},{     6, 1},{   -10, 1},{     5, 1},{   -11, 1},{    14, 1},
{   -18, 1},{    13, 1},{   -19, 1},{    30, 1},{   -34, 1},{    29, 1},{   -35, 1},{    62, 1},
{   -66, 1},{    61, 1},{   -67, 1},{   126, 1},{  -130, 1},{   125, 1},{  -131, 1},{   254, 1},
{  -258, 1},{   253, 1},{  -259, 1},{   510, 1},{  -514, 1},{   509, 1},{  -515, 1},{  1022, 1},
{ -1026, 1},{  1021, 1},{ -1027, 1},{  2046, 1},{ -2050, 1},{  2045, 1},{ -2051, 1},{  4094, 1},
{ -4098, 1},{  4093, 1},{ -4099, 1},{  8190, 1},{ -8194, 1},{  8189, 1},{ -8195, 1},{ 16382, 1},
{-16386, 1},{ 16381, 1},{-16387, 1},{ 32766, 1},{-32770, 1},{ 32765, 1},{-32771, 1},{     3, 0},
{     1, 1},{     2, 0},{     0, 1},{     4, 0},{     0, 1},{     3, 0},{    -1, 0},{     6, 0},
{    -2, 0},{     5, 0},{    -3, 0},{    10, 0},{    -6, 0},{     9, 0},{    -7, 0},{    18, 0},
{   -14, 0},{    17, 0},{   -15, 0},{    34, 0},{   -30, 0},{    33, 0},{   -31, 0},{    66, 0},
{   -62, 0},{    65, 0},{   -63, 0},{   130, 0},{  -126, 0},{   129, 0},{  -127, 0},{   258, 0},
{  -254, 0},{   257, 0},{  -255, 0},{   514, 0},{  -510, 0},{   513, 0},{  -511, 0},{  1026, 0},
{ -1022, 0},{  1025, 0},{ -1023, 0},{  2050, 0},{ -2046, 0},{  2049, 0},{ -2047, 0},{  4098, 0},
{ -4094, 0},{  4097, 0},{ -4095, 0},{  8194, 0},{ -8190, 0},{  8193, 0},{ -8191, 0},{ 16386, 0},
{-16382, 0},{ 16385, 0},{-16383, 0},{ 32770, 0},{-32766, 0},{ 32769, 0},{-32767, 0},{    -1, 0},
{    -3, 1},{    -2, 0},{    -4, 1},{     0, 1},{    -4, 1},{    -1, 0},{    -5, 1},{     2, 1},
{    -6, 1},{     1, 1},{    -7, 1},{     6, 1},{   -10, 1},{     5, 1},{   -11, 1},{    14, 1},
{   -18, 1},{    13, 1},{   -19, 1},{    30, 1},{   -34, 1},{    29, 1},{   -35, 1},{    62, 1},
{   -66, 1},{    61, 1},{   -67, 1},{   126, 1},{  -130, 1},{   125, 1},{  -131, 1},{   254, 1},
{  -258, 1},{   253, 1},{  -259, 1},{   510, 1},{  -514, 1},{   509, 1},{  -515, 1},{  1022, 1},
{ -1026, 1},{  1021, 1},{ -1027, 1},{  2046, 1},{ -2050, 1},{  2045, 1},{ -2051, 1},{  4094, 1},
{ -4098, 1},{  4093, 1},{ -4099, 1},{  8190, 1},{ -8194, 1},{  8189, 1},{ -8195, 1},{ 16382, 1},
{-16386, 1},{ 16381, 1},{-16387, 1},{ 32766, 1},{-32770, 1},{ 32765, 1},{-32771, 1},{     2, 0},
{     0, 1},{     1, 0},{    -1, 0},{     3, 0},{    -1, 0},{     2, 0},{    -2, 0},{     5, 0},
{    -3, 0},{     4, 0},{    -4, 0},{     9, 0},{    -7, 0},{     8, 0},{    -8, 0},{    17, 0},
{   -15, 0},{    16, 0},{   -16, 0},{    33, 0},{   -31, 0},{    32, 0},{   -32, 0},{    65, 0},
{   -63, 0},{    64, 0},{   -64, 0},{   129, 0},{  -127, 0},{   128, 0},{  -128, 0},{   257, 0},
{  -255, 0},{   256, 0},{  -256, 0},{   513, 0},{  -511, 0},{   512, 0},{  -512, 0},{  1025, 0},
{ -1023, 0},{  1024, 0},{ -1024, 0},{  2049, 0},{ -2047, 0},{  2048, 0},{ -2048, 0},{  4097, 0},
{ -4095, 0},{  4096, 0},{ -4096, 0},{  8193, 0},{ -8191, 0},{  8192, 0},{ -8192, 0},{ 16385, 0},
{-16383, 0},{ 16384, 0},{-16384, 0},{ 32769, 0},{-32767, 0},{ 32768, 0},{-32768, 0},{    -2, 0},
{    -4, 1},{    -3, 0},{    -5, 1},{    -1, 0},{    -5, 1},{    -2, 0},{    -6, 1},{     1, 1},
{    -7, 1},{     0, 1},{    -8, 1},{     5, 1},{   -11, 1},{     4, 1},{   -12, 1},{    13, 1},
{   -19, 1},{    12, 1},{   -20, 1},{    29, 1},{   -35, 1},{    28, 1},{   -36, 1},{    61, 1},
{   -67, 1},{    60, 1},{   -68, 1},{   125, 1},{  -131, 1},{   124, 1},{  -132, 1},{   253, 1},
{  -259, 1},{   252, 1},{  -260, 1},{   509, 1},{  -515, 1},{   508, 1},{  -516, 1},{  1021, 1},
{ -1027, 1},{  1020, 1},{ -1028, 1},{  2045, 1},{ -2051, 1},{  2044, 1},{ -2052, 1},{  4093, 1},
{ -4099, 1},{  4092, 1},{ -4100, 1},{  8189, 1},{ -8195, 1},{  8188, 1},{ -8196, 1},{ 16381, 1},
{-16387, 1},{ 16380, 1},{-16388, 1},{ 32765, 1},{-32771, 1},{ 32764, 1},{-32772, 1},{     5, 0},
{     3, 1},{     4, 0},{     2, 1},{     6, 0},{     2, 1},{     5, 0},{     1, 1},{     8, 0},
{     0, 1},{     7, 0},{    -1, 0},{    12, 0},{    -4, 0},{    11, 0},{    -5, 0},{    20, 0},
{   -12, 0},{    19, 0},{   -13, 0},{    36, 0},{   -28, 0},{    35, 0},{   -29, 0},{    68, 0},
{   -60, 0},{    67, 0},{   -61, 0},{   132, 0},{  -124, 0},{   131, 0},{  -125, 0},{   260, 0},
{  -252, 0},{   259, 0},{  -253, 0},{   516, 0},{  -508, 0},{   515, 0},{  -509, 0},{  1028, 0},
{ -1020, 0},{  1027, 0},{ -1021, 0},{  2052, 0},{ -2044, 0},{  2051, 0},{ -2045, 0},{  4100, 0},
{ -4092, 0},{  4099, 0},{ -4093, 0},{  8196, 0},{ -8188, 0},{  8195, 0},{ -8189, 0},{ 16388, 0},
{-16380, 0},{ 16387, 0},{-16381, 0},{ 32772, 0},{-32764, 0},{ 32771, 0},{-32765, 0},{    -3, 0},
{    -5, 1},{    -4, 0},{    -6, 1},{    -2, 0},{    -6, 1},{    -3, 0},{    -7, 1},{     0, 1},
{    -8, 1},{    -1, 0},{    -9, 1},{     4, 1},{   -12, 1},{     3, 1},{   -13, 1},{    12, 1},
{   -20, 1},{    11, 1},{   -21, 1},{    28, 1},{   -36, 1},{    27, 1},{   -37, 1},{    60, 1},
{   -68, 1},{    59, 1},{   -69, 1},{   124, 1},{  -132, 1},{   123, 1},{  -133, 1},{   252, 1},
{  -260, 1},{   251, 1},{  -261, 1},{   508, 1},{  -516, 1},{   507, 1},{  -517, 1},{  1020, 1},
{ -1028, 1},{  1019, 1},{ -1029, 1},{  2044, 1},{ -2052, 1},{  2043, 1},{ -2053, 1},{  4092, 1},
{ -4100, 1},{  4091, 1},{ -4101, 1},{  8188, 1},{ -8196, 1},{  8187, 1},{ -8197, 1},{ 16380, 1},
{-16388, 1},{ 16379, 1},{-16389, 1},{ 32764, 1},{-32772, 1},{ 32763, 1},{-32773, 1},{     4, 0},
{     2, 1},{     3, 0},{     1, 1},{     5, 0},{     1, 1},{     4, 0},{     0, 1},{     7, 0},
{    -1, 0},{     6, 0},{    -2, 0},{    11, 0},{    -5, 0},{    10, 0},{    -6, 0},{    19, 0},
{   -13, 0},{    18, 0},{   -14, 0},{    35, 0},{   -29, 0},{    34, 0},{   -30, 0},{    67, 0},
{   -61, 0},{    66, 0},{   -62, 0},{   131, 0},{  -125, 0},{   130, 0},{  -126, 0},{   259, 0},
{  -253, 0},{   258, 0},{  -254, 0},{   515, 0},{  -509, 0},{   514, 0},{  -510, 0},{  1027, 0},
{ -1021, 0},{  1026, 0},{ -1022, 0},{  2051, 0},{ -2045, 0},{  2050, 0},{ -2046, 0},{  4099, 0},
{ -4093, 0},{  4098, 0},{ -4094, 0},{  8195, 0},{ -8189, 0},{  8194, 0},{ -8190, 0},{ 16387, 0},
{-16381, 0},{ 16386, 0},{-16382, 0},{ 32771, 0},{-32765, 0},{ 32770, 0},{-32766, 0},{    -4, 0},
{    -6, 1},{    -5, 0},{    -7, 1},{    -3, 0},{    -7, 1},{    -4, 0},{    -8, 1},{    -1, 0},
{    -9, 1},{    -2, 0},{   -10, 1},{     3, 1},{   -13, 1},{     2, 1},{   -14, 1},{    11, 1},
{   -21, 1},{    10, 1},{   -22, 1},{    27, 1},{   -37, 1},{    26, 1},{   -38, 1},{    59, 1},
{   -69, 1},{    58, 1},{   -70, 1},{   123, 1},{  -133, 1},{   122, 1},{  -134, 1},{   251, 1},
{  -261, 1},{   250, 1},{  -262, 1},{   507, 1},{  -517, 1},{   506, 1},{  -518, 1},{  1019, 1},
{ -1029, 1},{  1018, 1},{ -1030, 1},{  2043, 1},{ -2053, 1},{  2042, 1},{ -2054, 1},{  4091, 1},
{ -4101, 1},{  4090, 1},{ -4102, 1},{  8187, 1},{ -8197, 1},{  8186, 1},{ -8198, 1},{ 16379, 1},
{-16389, 1},{ 16378, 1},{-16390, 1},{ 32763, 1},{-32773, 1},{ 32762, 1},{-32774, 1},{     9, 0},
{     7, 1},{     8, 0},{     6, 1},{    10, 0},{     6, 1},{     9, 0},{     5, 1},{    12, 0},
{     4, 1},{    11, 0},{     3, 1},{    16, 0},{     0, 1},{    15, 0},{    -1, 0},{    24, 0},
{    -8, 0},{    23, 0},{    -9, 0},{    40, 0},{   -24, 0},{    39, 0},{   -25, 0},{    72, 0},
{   -56, 0},{    71, 0},{   -57, 0},{   136, 0},{  -120, 0},{   135, 0},{  -121, 0},{   264, 0},
{  -248, 0},{   263, 0},{  -249, 0},{   520, 0},{  -504, 0},{   519, 0},{  -505, 0},{  1032, 0},
{ -1016, 0},{  1031, 0},{ -1017, 0},{  2056, 0},{ -2040, 0},{  2055, 0},{ -2041, 0},{  4104, 0},
{ -4088, 0},{  4103, 0},{ -4089, 0},{  8200, 0},{ -8184, 0},{  8199, 0},{ -8185, 0},{ 16392, 0},
{-16376, 0},{ 16391, 0},{-16377, 0},{ 32776, 0},{-32760, 0},{ 32775, 0},{-32761, 0},{    -7, 0},
{    -9, 1},{    -8, 0},{   -10, 1},{    -6, 0},{   -10, 1},{    -7, 0},{   -11, 1},{    -4, 0},
{   -12, 1},{    -5, 0},{   -13, 1},{     0, 1},{   -16, 1},{    -1, 0},{   -17, 1},{     8, 1},
{   -24, 1},{     7, 1},{   -25, 1},{    24, 1},{   -40, 1},{    23, 1},{   -41, 1},{    56, 1},
{   -72, 1},{    55, 1},{   -73, 1},{   120, 1},{  -136, 1},{   119, 1},{  -137, 1},{   248, 1},
{  -264, 1},{   247, 1},{  -265, 1},{   504, 1},{  -520, 1},{   503, 1},{  -521, 1},{  1016, 1},
{ -1032, 1},{  1015, 1},{ -1033, 1},{  2040, 1},{ -2056, 1},{  2039, 1},{ -2057, 1},{  4088, 1},
{ -4104, 1},{  4087, 1},{ -4105, 1},{  8184, 1},{ -8200, 1},{  8183, 1},{ -8201, 1},{ 16376, 1},
{-16392, 1},{ 16375, 1},{-16393, 1},{ 32760, 1},{-32776, 1},{ 32759, 1},{-32777, 1},{     8, 0},
{     6, 1},{     7, 0},{     5, 1},{     9, 0},{     5, 1},{     8, 0},{     4, 1},{    11, 0},
{     3, 1},{    10, 0},{     2, 1},{    15, 0},{    -1, 0},{    14, 0},{    -2, 0},{    23, 0},
{    -9, 0},{    22, 0},{   -10, 0},{    39, 0},{   -25, 0},{    38, 0},{   -26, 0},{    71, 0},
{   -57, 0},{    70, 0},{   -58, 0},{   135, 0},{  -121, 0},{   134, 0},{  -122, 0},{   263, 0},
{  -249, 0},{   262, 0},{  -250, 0},{   519, 0},{  -505, 0},{   518, 0},{  -506, 0},{  1031, 0},
{ -1017, 0},{  1030, 0},{ -1018, 0},{  2055, 0},{ -2041, 0},{  2054, 0},{ -2042, 0},{  4103, 0},
{ -4089, 0},{  4102, 0},{ -4090, 0},{  8199, 0},{ -8185, 0},{  8198, 0},{ -8186, 0},{ 16391, 0},
{-16377, 0},{ 16390, 0},{-16378, 0},{ 32775, 0},{-32761, 0},{ 32774, 0},{-32762, 0},{    -8, 0},
{   -10, 1},{    -9, 0},{   -11, 1},{    -7, 0},{   -11, 1},{    -8, 0},{   -12, 1},{    -5, 0},
{   -13, 1},{    -6, 0},{   -14, 1},{    -1, 0},{   -17, 1},{    -2, 0},{   -18, 1},{     7, 1},
{   -25, 1},{     6, 1},{   -26, 1},{    23, 1},{   -41, 1},{    22, 1},{   -42, 1},{    55, 1},
{   -73, 1},{    54, 1},{   -74, 1},{   119, 1},{  -137, 1},{   118, 1},{  -138, 1},{   247, 1},
{  -265, 1},{   246, 1},{  -266, 1},{   503, 1},{  -521, 1},{   502, 1},{  -522, 1},{  1015, 1},
{ -1033, 1},{  1014, 1},{ -1034, 1},{  2039, 1},{ -2057, 1},{  2038, 1},{ -2058, 1},{  4087, 1},
{ -4105, 1},{  4086, 1},{ -4106, 1},{  8183, 1},{ -8201, 1},{  8182, 1},{ -8202, 1},{ 16375, 1},
{-16393, 1},{ 16374, 1},{-16394, 1},{ 32759, 1},{-32777, 1},{ 32758, 1},{-32778, 1},{    17, 0},
{    15, 1},{    16, 0},{    14, 1},{    18, 0},{    14, 1},{    17, 0},{    13, 1},{    20, 0},
{    12, 1},{    19, 0},{    11, 1},{    24, 0},{     8, 1},{    23, 0},{     7, 1},{    32, 0},
{     0, 1},{    31, 0},{    -1, 0},{    48, 0},{   -16, 0},{    47, 0},{   -17, 0},{    80, 0},
{   -48, 0},{    79, 0},{   -49, 0},{   144, 0},{  -112, 0},{   143, 0},{  -113, 0},{   272, 0},
{  -240, 0},{   271, 0},{  -241, 0},{   528, 0},{  -496, 0},{   527, 0},{  -497, 0},{  1040, 0},
{ -1008, 0},{  1039, 0},{ -1009, 0},{  2064, 0},{ -2032, 0},{  2063, 0},{ -2033, 0},{  4112, 0},
{ -4080, 0},{  4111, 0},{ -4081, 0},{  8208, 0},{ -8176, 0},{  8207, 0},{ -8177, 0},{ 16400, 0},
{-16368, 0},{ 16399, 0},{-16369, 0},{ 32784, 0},{-32752, 0},{ 32783, 0},{-32753, 0},{   -15, 0},
{   -17, 1},{   -16, 0},{   -18, 1},{   -14, 0},{   -18, 1},{   -15, 0},{   -19, 1},{   -12, 0},
{   -20, 1},{   -13, 0},{   -21, 1},{    -8, 0},{   -24, 1},{    -9, 0},{   -25, 1},{     0, 1},
{   -32, 1},{    -1, 0},{   -33, 1},{    16, 1},{   -48, 1},{    15, 1},{   -49, 1},{    48, 1},
{   -80, 1},{    47, 1},{   -81, 1},{   112, 1},{  -144, 1},{   111, 1},{  -145, 1},{   240, 1},
{  -272, 1},{   239, 1},{  -273, 1},{   496, 1},{  -528, 1},{   495, 1},{  -529, 1},{  1008, 1},
{ -1040, 1},{  1007, 1},{ -1041, 1},{  2032, 1},{ -2064, 1},{  2031, 1},{ -2065, 1},{  4080, 1},
{ -4112, 1},{  4079, 1},{ -4113, 1},{  8176, 1},{ -8208, 1},{  8175, 1},{ -8209, 1},{ 16368, 1},
{-16400, 1},{ 16367, 1},{-16401, 1},{ 32752, 1},{-32784, 1},{ 32751, 1},{-32785, 1},{    16, 0},
{    14, 1},{    15, 0},{    13, 1},{    17, 0},{    13, 1},{    16, 0},{    12, 1},{    19, 0},
{    11, 1},{    18, 0},{    10, 1},{    23, 0},{     7, 1},{    22, 0},{     6, 1},{    31, 0},
{    -1, 0},{    30, 0},{    -2, 0},{    47, 0},{   -17, 0},{    46, 0},{   -18, 0},{    79, 0},
{   -49, 0},{    78, 0},{   -50, 0},{   143, 0},{  -113, 0},{   142, 0},{  -114, 0},{   271, 0},
{  -241, 0},{   270, 0},{  -242, 0},{   527, 0},{  -497, 0},{   526, 0},{  -498, 0},{  1039, 0},
{ -1009, 0},{  1038, 0},{ -1010, 0},{  2063, 0},{ -2033, 0},{  2062, 0},{ -2034, 0},{  4111, 0},
{ -4081, 0},{  4110, 0},{ -4082, 0},{  8207, 0},{ -8177, 0},{  8206, 0},{ -8178, 0},{ 16399, 0},
{-16369, 0},{ 16398, 0},{-16370, 0},{ 32783, 0},{-32753, 0},{ 32782, 0},{-32754, 0},{   -16, 0},
{   -18, 1},{   -17, 0},{   -19, 1},{   -15, 0},{   -19, 1},{   -16, 0},{   -20, 1},{   -13, 0},
{   -21, 1},{   -14, 0},{   -22, 1},{    -9, 0},{   -25, 1},{   -10, 0},{   -26, 1},{    -1, 0},
{   -33, 1},{    -2, 0},{   -34, 1},{    15, 1},{   -49, 1},{    14, 1},{   -50, 1},{    47, 1},
{   -81, 1},{    46, 1},{   -82, 1},{   111, 1},{  -145, 1},{   110, 1},{  -146, 1},{   239, 1},
{  -273, 1},{   238, 1},{  -274, 1},{   495, 1},{  -529, 1},{   494, 1},{  -530, 1},{  1007, 1},
{ -1041, 1},{  1006, 1},{ -1042, 1},{  2031, 1},{ -2065, 1},{  2030, 1},{ -2066, 1},{  4079, 1},
{ -4113, 1},{  4078, 1},{ -4114, 1},{  8175, 1},{ -8209, 1},{  8174, 1},{ -8210, 1},{ 16367, 1},
{-16401, 1},{ 16366, 1},{-16402, 1},{ 32751, 1},{-32785, 1},{ 32750, 1},{-32786, 1},{    33, 0},
{    31, 1},{    32, 0},{    30, 1},{    34, 0},{    30, 1},{    33, 0},{    29, 1},{    36, 0},
{    28, 1},{    35, 0},{    27, 1},{    40, 0},{    24, 1},{    39, 0},{    23, 1},{    48, 0},
{    16, 1},{    47, 0},{    15, 1},{    64, 0},{     0, 1},{    63, 0},{    -1, 0},{    96, 0},
{   -32, 0},{    95, 0},{   -33, 0},{   160, 0},{   -96, 0},{   159, 0},{   -97, 0},{   288, 0},
{  -224, 0},{   287, 0},{  -225, 0},{   544, 0},{  -480, 0},{   543, 0},{  -481, 0},{  1056, 0},
{  -992, 0},{  1055, 0},{  -993, 0},{  2080, 0},{ -2016, 0},{  2079, 0},{ -2017, 0},{  4128, 0},
{ -4064, 0},{  4127, 0},{ -4065, 0},{  8224, 0},{ -8160, 0},{  8223, 0},{ -8161, 0},{ 16416, 0},
{-16352, 0},{ 16415, 0},{-16353, 0},{ 32800, 0},{-32736, 0},{ 32799, 0},{-32737, 0},{   -31, 0},
{   -33, 1},{   -32, 0},{   -34, 1},{   -30, 0},{   -34, 1},{   -31, 0},{   -35, 1},{   -28, 0},
{   -36, 1},{   -29, 0},{   -37, 1},{   -24, 0},{   -40, 1},{   -25, 0},{   -41, 1},{   -16, 0},
{   -48, 1},{   -17, 0},{   -49, 1},{     0, 1},{   -64, 1},{    -1, 0},{   -65, 1},{    32, 1},
{   -96, 1},{    31, 1},{   -97, 1},{    96, 1},{  -160, 1},{    95, 1},{  -161, 1},{   224, 1},
{  -288, 1},{   223, 1},{  -289, 1},{   480, 1},{  -544, 1},{   479, 1},{  -545, 1},{   992, 1},
{ -1056, 1},{   991, 1},{ -1057, 1},{  2016, 1},{ -2080, 1},{  2015, 1},{ -2081, 1},{  4064, 1},
{ -4128, 1},{  4063, 1},{ -4129, 1},{  8160, 1},{ -8224, 1},{  8159, 1},{ -8225, 1},{ 16352, 1},
{-16416, 1},{ 16351, 1},{-16417, 1},{ 32736, 1},{-32800, 1},{ 32735, 1},{-32801, 1},{    32, 0},
{    30, 1},{    31, 0},{    29, 1},{    33, 0},{    29, 1},{    32, 0},{    28, 1},{    35, 0},
{    27, 1},{    34, 0},{    26, 1},{    39, 0},{    23, 1},{    38, 0},{    22, 1},{    47, 0},
{    15, 1},{    46, 0},{    14, 1},{    63, 0},{    -1, 0},{    62, 0},{    -2, 0},{    95, 0},
{   -33, 0},{    94, 0},{   -34, 0},{   159, 0},{   -97, 0},{   158, 0},{   -98, 0},{   287, 0},
{  -225, 0},{   286, 0},{  -226, 0},{   543, 0},{  -481, 0},{   542, 0},{  -482, 0},{  1055, 0},
{  -993, 0},{  1054, 0},{  -994, 0},{  2079, 0},{ -2017, 0},{  2078, 0},{ -2018, 0},{  4127, 0},
{ -4065, 0},{  4126, 0},{ -4066, 0},{  8223, 0},{ -8161, 0},{  8222, 0},{ -8162, 0},{ 16415, 0},
{-16353, 0},{ 16414, 0},{-16354, 0},{ 32799, 0},{-32737, 0},{ 32798, 0},{-32738, 0},{   -32, 0},
{   -34, 1},{   -33, 0},{   -35, 1},{   -31, 0},{   -35, 1},{   -32, 0},{   -36, 1},{   -29, 0},
{   -37, 1},{   -30, 0},{   -38, 1},{   -25, 0},{   -41, 1},{   -26, 0},{   -42, 1},{   -17, 0},
{   -49, 1},{   -18, 0},{   -50, 1},{    -1, 0},{   -65, 1},{    -2, 0},{   -66, 1},{    31, 1},
{   -97, 1},{    30, 1},{   -98, 1},{    95, 1},{  -161, 1},{    94, 1},{  -162, 1},{   223, 1},
{  -289, 1},{   222, 1},{  -290, 1},{   479, 1},{  -545, 1},{   478, 1},{  -546, 1},{   991, 1},
{ -1057, 1},{   990, 1},{ -1058, 1},{  2015, 1},{ -2081, 1},{  2014, 1},{ -2082, 1},{  4063, 1},
{ -4129, 1},{  4062, 1},{ -4130, 1},{  8159, 1},{ -8225, 1},{  8158, 1},{ -8226, 1},{ 16351, 1},
{-16417, 1},{ 16350, 1},{-16418, 1},{ 32735, 1},{-32801, 1},{ 32734, 1},{-32802, 1},{    65, 0},
{    63, 1},{    64, 0},{    62, 1},{    66, 0},{    62, 1},{    65, 0},{    61, 1},{    68, 0},
{    60, 1},{    67, 0},{    59, 1},{    72, 0},{    56, 1},{    71, 0},{    55, 1},{    80, 0},
{    48, 1},{    79, 0},{    47, 1},{    96, 0},{    32, 1},{    95, 0},{    31, 1},{   128, 0},
{     0, 1},{   127, 0},{    -1, 0},{   192, 0},{   -64, 0},{   191, 0},{   -65, 0},{   320, 0},
{  -192, 0},{   319, 0},{  -193, 0},{   576, 0},{  -448, 0},{   575, 0},{  -449, 0},{  1088, 0},
{  -960, 0},{  1087, 0},{  -961, 0},{  2112, 0},{ -1984, 0},{  2111, 0},{ -1985, 0},{  4160, 0},
{ -4032, 0},{  4159, 0},{ -4033, 0},{  8256, 0},{ -8128, 0},{  8255, 0},{ -8129, 0},{ 16448, 0},
{-16320, 0},{ 16447, 0},{-16321, 0},{ 32832, 0},{-32704, 0},{ 32831, 0},{-32705, 0},{   -63, 0},
{   -65, 1},{   -64, 0},{   -66, 1},{   -62, 0},{   -66, 1},{   -63, 0},{   -67, 1},{   -60, 0},
{   -68, 1},{   -61, 0},{   -69, 1},{   -56, 0},{   -72, 1},{   -57, 0},{   -73, 1},{   -48, 0},
{   -80, 1},{   -49, 0},{   -81, 1},{   -32, 0},{   -96, 1},{   -33, 0},{   -97, 1},{     0, 1},
{  -128, 1},{    -1, 0},{  -129, 1},{    64, 1},{  -192, 1},{    63, 1},{  -193, 1},{   192, 1},
{  -320, 1},{   191, 1},{  -321, 1},{   448, 1},{  -576, 1},{   447, 1},{  -577, 1},{   960, 1},
{ -1088, 1},{   959, 1},{ -1089, 1},{  1984, 1},{ -2112, 1},{  1983, 1},{ -2113, 1},{  4032, 1},
{ -4160, 1},{  4031, 1},{ -4161, 1},{  8128, 1},{ -8256, 1},{  8127, 1},{ -8257, 1},{ 16320, 1},
{-16448, 1},{ 16319, 1},{-16449, 1},{ 32704, 1},{-32832, 1},{ 32703, 1},{-32833, 1},{    64, 0},
{    62, 1},{    63, 0},{    61, 1},{    65, 0},{    61, 1},{    64, 0},{    60, 1},{    67, 0},
{    59, 1},{    66, 0},{    58, 1},{    71, 0},{    55, 1},{    70, 0},{    54, 1},{    79, 0},
{    47, 1},{    78, 0},{    46, 1},{    95, 0},{    31, 1},{    94, 0},{    30, 1},{   127, 0},
{    -1, 0},{   126, 0},{    -2, 0},{   191, 0},{   -65, 0},{   190, 0},{   -66, 0},{   319, 0},
{  -193, 0},{   318, 0},{  -194, 0},{   575, 0},{  -449, 0},{   574, 0},{  -450, 0},{  1087, 0},
{  -961, 0},{  1086, 0},{  -962, 0},{  2111, 0},{ -1985, 0},{  2110, 0},{ -1986, 0},{  4159, 0},
{ -4033, 0},{  4158, 0},{ -4034, 0},{  8255, 0},{ -8129, 0},{  8254, 0},{ -8130, 0},{ 16447, 0},
{-16321, 0},{ 16446, 0},{-16322, 0},{ 32831, 0},{-32705, 0},{ 32830, 0},{-32706, 0},{   -64, 0},
{   -66, 1},{   -65, 0},{   -67, 1},{   -63, 0},{   -67, 1},{   -64, 0},{   -68, 1},{   -61, 0},
{   -69, 1},{   -62, 0},{   -70, 1},{   -57, 0},{   -73, 1},{   -58, 0},{   -74, 1},{   -49, 0},
{   -81, 1},{   -50, 0},{   -82, 1},{   -33, 0},{   -97, 1},{   -34, 0},{   -98, 1},{    -1, 0},
{  -129, 1},{    -2, 0},{  -130, 1},{    63, 1},{  -193, 1},{    62, 1},{  -194, 1},{   191, 1},
{  -321, 1},{   190, 1},{  -322, 1},{   447, 1},{  -577, 1},{   446, 1},{  -578, 1},{   959, 1},
{ -1089, 1},{   958, 1},{ -1090, 1},{  1983, 1},{ -2113, 1},{  1982, 1},{ -2114, 1},{  4031, 1},
{ -4161, 1},{  4030, 1},{ -4162, 1},{  8127, 1},{ -8257, 1},{  8126, 1},{ -8258, 1},{ 16319, 1},
{-16449, 1},{ 16318, 1},{-16450, 1},{ 32703, 1},{-32833, 1},{ 32702, 1},{-32834, 1},{   129, 0},
{   127, 1},{   128, 0},{   126, 1},{   130, 0},{   126, 1},{   129, 0},{   125, 1},{   132, 0},
{   124, 1},{   131, 0},{   123, 1},{   136, 0},{   120, 1},{   135, 0},{   119, 1},{   144, 0},
{   112, 1},{   143, 0},{   111, 1},{   160, 0},{    96, 1},{   159, 0},{    95, 1},{   192, 0},
{    64, 1},{   191, 0},{    63, 1},{   256, 0},{     0, 1},{   255, 0},{    -1, 0},{   384, 0},
{  -128, 0},{   383, 0},{  -129, 0},{   640, 0},{  -384, 0},{   639, 0},{  -385, 0},{  1152, 0},
{  -896, 0},{  1151, 0},{  -897, 0},{  2176, 0},{ -1920, 0},{  2175, 0},{ -1921, 0},{  4224, 0},
{ -3968, 0},{  4223, 0},{ -3969, 0},{  8320, 0},{ -8064, 0},{  8319, 0},{ -8065, 0},{ 16512, 0},
{-16256, 0},{ 16511, 0},{-16257, 0},{ 32896, 0},{-32640, 0},{ 32895, 0},{-32641, 0},{  -127, 0},
{  -129, 1},{  -128, 0},{  -130, 1},{  -126, 0},{  -130, 1},{  -127, 0},{  -131, 1},{  -124, 0},
{  -132, 1},{  -125, 0},{  -133, 1},{  -120, 0},{  -136, 1},{  -121, 0},{  -137, 1},{  -112, 0},
{  -144, 1},{  -113, 0},{  -145, 1},{   -96, 0},{  -160, 1},{   -97, 0},{  -161, 1},{   -64, 0},
{  -192, 1},{   -65, 0},{  -193, 1},{     0, 1},{  -256, 1},{    -1, 0},{  -257, 1},{   128, 1},
{  -384, 1},{   127, 1},{  -385, 1},{   384, 1},{  -640, 1},{   383, 1},{  -641, 1},{   896, 1},
{ -1152, 1},{   895, 1},{ -1153, 1},{  1920, 1},{ -2176, 1},{  1919, 1},{ -2177, 1},{  3968, 1},
{ -4224, 1},{  3967, 1},{ -4225, 1},{  8064, 1},{ -8320, 1},{  8063, 1},{ -8321, 1},{ 16256, 1},
{-16512, 1},{ 16255, 1},{-16513, 1},{ 32640, 1},{-32896, 1},{ 32639, 1},{-32897, 1},{   128, 0},
{   126, 1},{   127, 0},{   125, 1},{   129, 0},{   125, 1},{   128, 0},{   124, 1},{   131, 0},
{   123, 1},{   130, 0},{   122, 1},{   135, 0},{   119, 1},{   134, 0},{   118, 1},{   143, 0},
{   111, 1},{   142, 0},{   110, 1},{   159, 0},{    95, 1},{   158, 0},{    94, 1},{   191, 0},
{    63, 1},{   190, 0},{    62, 1},{   255, 0},{    -1, 0},{   254, 0},{    -2, 0},{   383, 0},
{  -129, 0},{   382, 0},{  -130, 0},{   639, 0},{  -385, 0},{   638, 0},{  -386, 0},{  1151, 0},
{  -897, 0},{  1150, 0},{  -898, 0},{  2175, 0},{ -1921, 0},{  2174, 0},{ -1922, 0},{  4223, 0},
{ -3969, 0},{  4222, 0},{ -3970, 0},{  8319, 0},{ -8065, 0},{  8318, 0},{ -8066, 0},{ 16511, 0},
{-16257, 0},{ 16510, 0},{-16258, 0},{ 32895, 0},{-32641, 0},{ 32894, 0},{-32642, 0},{  -128, 0},
{  -130, 1},{  -129, 0},{  -131, 1},{  -127, 0},{  -131, 1},{  -128, 0},{  -132, 1},{  -125, 0},
{  -133, 1},{  -126, 0},{  -134, 1},{  -121, 0},{  -137, 1},{  -122, 0},{  -138, 1},{  -113, 0},
{  -145, 1},{  -114, 0},{  -146, 1},{   -97, 0},{  -161, 1},{   -98, 0},{  -162, 1},{   -65, 0},
{  -193, 1},{   -66, 0},{  -194, 1},{    -1, 0},{  -257, 1},{    -2, 0},{  -258, 1},{   127, 1},
{  -385, 1},{   126, 1},{  -386, 1},{   383, 1},{  -641, 1},{   382, 1},{  -642, 1},{   895, 1},
{ -1153, 1},{   894, 1},{ -1154, 1},{  1919, 1},{ -2177, 1},{  1918, 1},{ -2178, 1},{  3967, 1},
{ -4225, 1},{  3966, 1},{ -4226, 1},{  8063, 1},{ -8321, 1},{  8062, 1},{ -8322, 1},{ 16255, 1},
{-16513, 1},{ 16254, 1},{-16514, 1},{ 32639, 1},{-32897, 1},{ 32638, 1},{-32898, 1},{   257, 0},
{   255, 1},{   256, 0},{   254, 1},{   258, 0},{   254, 1},{   257, 0},{   253, 1},{   260, 0},
{   252, 1},{   259, 0},{   251, 1},{   264, 0},{   248, 1},{   263, 0},{   247, 1},{   272, 0},
{   240, 1},{   271, 0},{   239, 1},{   288, 0},{   224, 1},{   287, 0},{   223, 1},{   320, 0},
{   192, 1},{   319, 0},{   191, 1},{   384, 0},{   128, 1},{   383, 0},{   127, 1},{   512, 0},
{     0, 1},{   511, 0},{    -1, 0},{   768, 0},{  -256, 0},{   767, 0},{  -257, 0},{  1280, 0},
{  -768, 0},{  1279, 0},{  -769, 0},{  2304, 0},{ -1792, 0},{  2303, 0},{ -1793, 0},{  4352, 0},
{ -3840, 0},{  4351, 0},{ -3841, 0},{  8448, 0},{ -7936, 0},{  8447, 0},{ -7937, 0},{ 16640, 0},
{-16128, 0},{ 16639, 0},{-16129, 0},{ 33024, 0},{-32512, 0},{ 33023, 0},{-32513, 0},{  -255, 0},
{  -257, 1},{  -256, 0},{  -258, 1},{  -254, 0},{  -258, 1},{  -255, 0},{  -259, 1},{  -252, 0},
{  -260, 1},{  -253, 0},{  -261, 1},{  -248, 0},{  -264, 1},{  -249, 0},{  -265, 1},{  -240, 0},
{  -272, 1},{  -241, 0},{  -273, 1},{  -224, 0},{  -288, 1},{  -225, 0},{  -289, 1},{  -192, 0},
{  -320, 1},{  -193, 0},{  -321, 1},{  -128, 0},{  -384, 1},{  -129, 0},{  -385, 1},{     0, 1},
{  -512, 1},{    -1, 0},{  -513, 1},{   256, 1},{  -768, 1},{   255, 1},{  -769, 1},{   768, 1},
{ -1280, 1},{   767, 1},{ -1281, 1},{  1792, 1},{ -2304, 1},{  1791, 1},{ -2305, 1},{  3840, 1},
{ -4352, 1},{  3839, 1},{ -4353, 1},{  7936, 1},{ -8448, 1},{  7935, 1},{ -8449, 1},{ 16128, 1},
{-16640, 1},{ 16127, 1},{-16641, 1},{ 32512, 1},{-33024, 1},{ 32511, 1},{-33025, 1},{   256, 0},
{   254, 1},{   255, 0},{   253, 1},{   257, 0},{   253, 1},{   256, 0},{   252, 1},{   259, 0},
{   251, 1},{   258, 0},{   250, 1},{   263, 0},{   247, 1},{   262, 0},{   246, 1},{   271, 0},
{   239, 1},{   270, 0},{   238, 1},{   287, 0},{   223, 1},{   286, 0},{   222, 1},{   319, 0},
{   191, 1},{   318, 0},{   190, 1},{   383, 0},{   127, 1},{   382, 0},{   126, 1},{   511, 0},
{    -1, 0},{   510, 0},{    -2, 0},{   767, 0},{  -257, 0},{   766, 0},{  -258, 0},{  1279, 0},
{  -769, 0},{  1278, 0},{  -770, 0},{  2303, 0},{ -1793, 0},{  2302, 0},{ -1794, 0},{  4351, 0},
{ -3841, 0},{  4350, 0},{ -3842, 0},{  8447, 0},{ -7937, 0},{  8446, 0},{ -7938, 0},{ 16639, 0},
{-16129, 0},{ 16638, 0},{-16130, 0},{ 33023, 0},{-32513, 0},{ 33022, 0},{-32514, 0},{  -256, 0},
{  -258, 1},{  -257, 0},{  -259, 1},{  -255, 0},{  -259, 1},{  -256, 0},{  -260, 1},{  -253, 0},
{  -261, 1},{  -254, 0},{  -262, 1},{  -249, 0},{  -265, 1},{  -250, 0},{  -266, 1},{  -241, 0},
{  -273, 1},{  -242, 0},{  -274, 1},{  -225, 0},{  -289, 1},{  -226, 0},{  -290, 1},{  -193, 0},
{  -321, 1},{  -194, 0},{  -322, 1},{  -129, 0},{  -385, 1},{  -130, 0},{  -386, 1},{    -1, 0},
{  -513, 1},{    -2, 0},{  -514, 1},{   255, 1},{  -769, 1},{   254, 1},{  -770, 1},{   767, 1},
{ -1281, 1},{   766, 1},{ -1282, 1},{  1791, 1},{ -2305, 1},{  1790, 1},{ -2306, 1},{  3839, 1},
{ -4353, 1},{  3838, 1},{ -4354, 1},{  7935, 1},{ -8449, 1},{  7934, 1},{ -8450, 1},{ 16127, 1},
{-16641, 1},{ 16126, 1},{-16642, 1},{ 32511, 1},{-33025, 1},{ 32510, 1},{-33026, 1},{   513, 0},
{   511, 1},{   512, 0},{   510, 1},{   514, 0},{   510, 1},{   513, 0},{   509, 1},{   516, 0},
{   508, 1},{   515, 0},{   507, 1},{   520, 0},{   504, 1},{   519, 0},{   503, 1},{   528, 0},
{   496, 1},{   527, 0},{   495, 1},{   544, 0},{   480, 1},{   543, 0},{   479, 1},{   576, 0},
{   448, 1},{   575, 0},{   447, 1},{   640, 0},{   384, 1},{   639, 0},{   383, 1},{   768, 0},
{   256, 1},{   767, 0},{   255, 1},{  1024, 0},{     0, 1},{  1023, 0},{    -1, 0},{  1536, 0},
{  -512, 0},{  1535, 0},{  -513, 0},{  2560, 0},{ -1536, 0},{  2559, 0},{ -1537, 0},{  4608, 0},
{ -3584, 0},{  4607, 0},{ -3585, 0},{  8704, 0},{ -7680, 0},{  8703, 0},{ -7681, 0},{ 16896, 0},
{-15872, 0},{ 16895, 0},{-15873, 0},{ 33280, 0},{-32256, 0},{ 33279, 0},{-32257, 0},{  -511, 0},
{  -513, 1},{  -512, 0},{  -514, 1},{  -510, 0},{  -514, 1},{  -511, 0},{  -515, 1},{  -508, 0},
{  -516, 1},{  -509, 0},{  -517, 1},{  -504, 0},{  -520, 1},{  -505, 0},{  -521, 1},{  -496, 0},
{  -528, 1},{  -497, 0},{  -529, 1},{  -480, 0},{  -544, 1},{  -481, 0},{  -545, 1},{  -448, 0},
{  -576, 1},{  -449, 0},{  -577, 1},{  -384, 0},{  -640, 1},{  -385, 0},{  -641, 1},{  -256, 0},
{  -768, 1},{  -257, 0},{  -769, 1},{     0, 1},{ -1024, 1},{    -1, 0},{ -1025, 1},{   512, 1},
{ -1536, 1},{   511, 1},{ -1537, 1},{  1536, 1},{ -2560, 1},{  1535, 1},{ -2561, 1},{  3584, 1},
{ -4608, 1},{  3583, 1},{ -4609, 1},{  7680, 1},{ -8704, 1},{  7679, 1},{ -8705, 1},{ 15872, 1},
{-16896, 1},{ 15871, 1},{-16897, 1},{ 32256, 1},{-33280, 1},{ 32255, 1},{-33281, 1},{   512, 0},
{   510, 1},{   511, 0},{   509, 1},{   513, 0},{   509, 1},{   512, 0},{   508, 1},{   515, 0},
{   507, 1},{   514, 0},{   506, 1},{   519, 0},{   503, 1},{   518, 0},{   502, 1},{   527, 0},
{   495, 1},{   526, 0},{   494, 1},{   543, 0},{   479, 1},{   542, 0},{   478, 1},{   575, 0},
{   447, 1},{   574, 0},{   446, 1},{   639, 0},{   383, 1},{   638, 0},{   382, 1},{   767, 0},
{   255, 1},{   766, 0},{   254, 1},{  1023, 0},{    -1, 0},{  1022, 0},{    -2, 0},{  1535, 0},
{  -513, 0},{  1534, 0},{  -514, 0},{  2559, 0},{ -1537, 0},{  2558, 0},{ -1538, 0},{  4607, 0},
{ -3585, 0},{  4606, 0},{ -3586, 0},{  8703, 0},{ -7681, 0},{  8702, 0},{ -7682, 0},{ 16895, 0},
{-15873, 0},{ 16894, 0},{-15874, 0},{ 33279, 0},{-32257, 0},{ 33278, 0},{-32258, 0},{  -512, 0},
{  -514, 1},{  -513, 0},{  -515, 1},{  -511, 0},{  -515, 1},{  -512, 0},{  -516, 1},{  -509, 0},
{  -517, 1},{  -510, 0},{  -518, 1},{  -505, 0},{  -521, 1},{  -506, 0},{  -522, 1},{  -497, 0},
{  -529, 1},{  -498, 0},{  -530, 1},{  -481, 0},{  -545, 1},{  -482, 0},{  -546, 1},{  -449, 0},
{  -577, 1},{  -450, 0},{  -578, 1},{  -385, 0},{  -641, 1},{  -386, 0},{  -642, 1},{  -257, 0},
{  -769, 1},{  -258, 0},{  -770, 1},{    -1, 0},{ -1025, 1},{    -2, 0},{ -1026, 1},{   511, 1},
{ -1537, 1},{   510, 1},{ -1538, 1},{  1535, 1},{ -2561, 1},{  1534, 1},{ -2562, 1},{  3583, 1},
{ -4609, 1},{  3582, 1},{ -4610, 1},{  7679, 1},{ -8705, 1},{  7678, 1},{ -8706, 1},{ 15871, 1},
{-16897, 1},{ 15870, 1},{-16898, 1},{ 32255, 1},{-33281, 1},{ 32254, 1},{-33282, 1},{  1025, 0},
{  1023, 1},{  1024, 0},{  1022, 1},{  1026, 0},{  1022, 1},{  1025, 0},{  1021, 1},{  1028, 0},
{  1020, 1},{  1027, 0},{  1019, 1},{  1032, 0},{  1016, 1},{  1031, 0},{  1015, 1},{  1040, 0},
{  1008, 1},{  1039, 0},{  1007, 1},{  1056, 0},{   992, 1},{  1055, 0},{   991, 1},{  1088, 0},
{   960, 1},{  1087, 0},{   959, 1},{  1152, 0},{   896, 1},{  1151, 0},{   895, 1},{  1280, 0},
{   768, 1},{  1279, 0},{   767, 1},{  1536, 0},{   512, 1},{  1535, 0},{   511, 1},{  2048, 0},
{     0, 1},{  2047, 0},{    -1, 0},{  3072, 0},{ -1024, 0},{  3071, 0},{ -1025, 0},{  5120, 0},
{ -3072, 0},{  5119, 0},{ -3073, 0},{  9216, 0},{ -7168, 0},{  9215, 0},{ -7169, 0},{ 17408, 0},
{-15360, 0},{ 17407, 0},{-15361, 0},{ 33792, 0},{-31744, 0},{ 33791, 0},{-31745, 0},{ -1023, 0},
{ -1025, 1},{ -1024, 0},{ -1026, 1},{ -1022, 0},{ -1026, 1},{ -1023, 0},{ -1027, 1},{ -1020, 0},
{ -1028, 1},{ -1021, 0},{ -1029, 1},{ -1016, 0},{ -1032, 1},{ -1017, 0},{ -1033, 1},{ -1008, 0},
{ -1040, 1},{ -1009, 0},{ -1041, 1},{  -992, 0},{ -1056, 1},{  -993, 0},{ -1057, 1},{  -960, 0},
{ -1088, 1},{  -961, 0},{ -1089, 1},{  -896, 0},{ -1152, 1},{  -897, 0},{ -1153, 1},{  -768, 0},
{ -1280, 1},{  -769, 0},{ -1281, 1},{  -512, 0},{ -1536, 1},{  -513, 0},{ -1537, 1},{     0, 1},
{ -2048, 1},{    -1, 0},{ -2049, 1},{  1024, 1},{ -3072, 1},{  1023, 1},{ -3073, 1},{  3072, 1},
{ -5120, 1},{  3071, 1},{ -5121, 1},{  7168, 1},{ -9216, 1},{  7167, 1},{ -9217, 1},{ 15360, 1},
{-17408, 1},{ 15359, 1},{-17409, 1},{ 31744, 1},{-33792, 1},{ 31743, 1},{-33793, 1},{  1024, 0},
{  1022, 1},{  1023, 0},{  1021, 1},{  1025, 0},{  1021, 1},{  1024, 0},{  1020, 1},{  1027, 0},
{  1019, 1},{  1026, 0},{  1018, 1},{  1031, 0},{  1015, 1},{  1030, 0},{  1014, 1},{  1039, 0},
{  1007, 1},{  1038, 0},{  1006, 1},{  1055, 0},{   991, 1},{  1054, 0},{   990, 1},{  1087, 0},
{   959, 1},{  1086, 0},{   958, 1},{  1151, 0},{   895, 1},{  1150, 0},{   894, 1},{  1279, 0},
{   767, 1},{  1278, 0},{   766, 1},{  1535, 0},{   511, 1},{  1534, 0},{   510, 1},{  2047, 0},
{    -1, 0},{  2046, 0},{    -2, 0},{  3071, 0},{ -1025, 0},{  3070, 0},{ -1026, 0},{  5119, 0},
{ -3073, 0},{  5118, 0},{ -3074, 0},{  9215, 0},{ -7169, 0},{  9214, 0},{ -7170, 0},{ 17407, 0},
{-15361, 0},{ 17406, 0},{-15362, 0},{ 33791, 0},{-31745, 0},{ 33790, 0},{-31746, 0},{ -1024, 0},
{ -1026, 1},{ -1025, 0},{ -1027, 1},{ -1023, 0},{ -1027, 1},{ -1024, 0},{ -1028, 1},{ -1021, 0},
{ -1029, 1},{ -1022, 0},{ -1030, 1},{ -1017, 0},{ -1033, 1},{ -1018, 0},{ -1034, 1},{ -1009, 0},
{ -1041, 1},{ -1010, 0},{ -1042, 1},{  -993, 0},{ -1057, 1},{  -994, 0},{ -1058, 1},{  -961, 0},
{ -1089, 1},{  -962, 0},{ -1090, 1},{  -897, 0},{ -1153, 1},{  -898, 0},{ -1154, 1},{  -769, 0},
{ -1281, 1},{  -770, 0},{ -1282, 1},{  -513, 0},{ -1537, 1},{  -514, 0},{ -1538, 1},{    -1, 0},
{ -2049, 1},{    -2, 0},{ -2050, 1},{  1023, 1},{ -3073, 1},{  1022, 1},{ -3074, 1},{  3071, 1},
{ -5121, 1},{  3070, 1},{ -5122, 1},{  7167, 1},{ -9217, 1},{  7166, 1},{ -9218, 1},{ 15359, 1},
{-17409, 1},{ 15358, 1},{-17410, 1},{ 31743, 1},{-33793, 1},{ 31742, 1},{-33794, 1},{  2049, 0},
{  2047, 1},{  2048, 0},{  2046, 1},{  2050, 0},{  2046, 1},{  2049, 0},{  2045, 1},{  2052, 0},
{  2044, 1},{  2051, 0},{  2043, 1},{  2056, 0},{  2040, 1},{  2055, 0},{  2039, 1},{  2064, 0},
{  2032, 1},{  2063, 0},{  2031, 1},{  2080, 0},{  2016, 1},{  2079, 0},{  2015, 1},{  2112, 0},
{  1984, 1},{  2111, 0},{  1983, 1},{  2176, 0},{  1920, 1},{  2175, 0},{  1919, 1},{  2304, 0},
{  1792, 1},{  2303, 0},{  1791, 1},{  2560, 0},{  1536, 1},{  2559, 0},{  1535, 1},{  3072, 0},
{  1024, 1},{  3071, 0},{  1023, 1},{  4096, 0},{     0, 1},{  4095, 0},{    -1, 0},{  6144, 0},
{ -2048, 0},{  6143, 0},{ -2049, 0},{ 10240, 0},{ -6144, 0},{ 10239, 0},{ -6145, 0},{ 18432, 0},
{-14336, 0},{ 18431, 0},{-14337, 0},{ 34816, 0},{-30720, 0},{ 34815, 0},{-30721, 0},{ -2047, 0},
{ -2049, 1},{ -2048, 0},{ -2050, 1},{ -2046, 0},{ -2050, 1},{ -2047, 0},{ -2051, 1},{ -2044, 0},
{ -2052, 1},{ -2045, 0},{ -2053, 1},{ -2040, 0},{ -2056, 1},{ -2041, 0},{ -2057, 1},{ -2032, 0},
{ -2064, 1},{ -2033, 0},{ -2065, 1},{ -2016, 0},{ -2080, 1},{ -2017, 0},{ -2081, 1},{ -1984, 0},
{ -2112, 1},{ -1985, 0},{ -2113, 1},{ -1920, 0},{ -2176, 1},{ -1921, 0},{ -2177, 1},{ -1792, 0},
{ -2304, 1},{ -1793, 0},{ -2305, 1},{ -1536, 0},{ -2560, 1},{ -1537, 0},{ -2561, 1},{ -1024, 0},
{ -3072, 1},{ -1025, 0},{ -3073, 1},{     0, 1},{ -4096, 1},{    -1, 0},{ -4097, 1},{  2048, 1},
{ -6144, 1},{  2047, 1},{ -6145, 1},{  6144, 1},{-10240, 1},{  6143, 1},{-10241, 1},{ 14336, 1},
{-18432, 1},{ 14335, 1},{-18433, 1},{ 30720, 1},{-34816, 1},{ 30719, 1},{-34817, 1},{  2048, 0},
{  2046, 1},{  2047, 0},{  2045, 1},{  2049, 0},{  2045, 1},{  2048, 0},{  2044, 1},{  2051, 0},
{  2043, 1},{  2050, 0},{  2042, 1},{  2055, 0},{  2039, 1},{  2054, 0},{  2038, 1},{  2063, 0},
{  2031, 1},{  2062, 0},{  2030, 1},{  2079, 0},{  2015, 1},{  2078, 0},{  2014, 1},{  2111, 0},
{  1983, 1},{  2110, 0},{  1982, 1},{  2175, 0},{  1919, 1},{  2174, 0},{  1918, 1},{  2303, 0},
{  1791, 1},{  2302, 0},{  1790, 1},{  2559, 0},{  1535, 1},{  2558, 0},{  1534, 1},{  3071, 0},
{  1023, 1},{  3070, 0},{  1022, 1},{  4095, 0},{    -1, 0},{  4094, 0},{    -2, 0},{  6143, 0},
{ -2049, 0},{  6142, 0},{ -2050, 0},{ 10239, 0},{ -6145, 0},{ 10238, 0},{ -6146, 0},{ 18431, 0},
{-14337, 0},{ 18430, 0},{-14338, 0},{ 34815, 0},{-30721, 0},{ 34814, 0},{-30722, 0},{ -2048, 0},
{ -2050, 1},{ -2049, 0},{ -2051, 1},{ -2047, 0},{ -2051, 1},{ -2048, 0},{ -2052, 1},{ -2045, 0},
{ -2053, 1},{ -2046, 0},{ -2054, 1},{ -2041, 0},{ -2057, 1},{ -2042, 0},{ -2058, 1},{ -2033, 0},
{ -2065, 1},{ -2034, 0},{ -2066, 1},{ -2017, 0},{ -2081, 1},{ -2018, 0},{ -2082, 1},{ -1985, 0},
{ -2113, 1},{ -1986, 0},{ -2114, 1},{ -1921, 0},{ -2177, 1},{ -1922, 0},{ -2178, 1},{ -1793, 0},
{ -2305, 1},{ -1794, 0},{ -2306, 1},{ -1537, 0},{ -2561, 1},{ -1538, 0},{ -2562, 1},{ -1025, 0},
{ -3073, 1},{ -1026, 0},{ -3074, 1},{    -1, 0},{ -4097, 1},{    -2, 0},{ -4098, 1},{  2047, 1},
{ -6145, 1},{  2046, 1},{ -6146, 1},{  6143, 1},{-10241, 1},{  6142, 1},{-10242, 1},{ 14335, 1},
{-18433, 1},{ 14334, 1},{-18434, 1},{ 30719, 1},{-34817, 1},{ 30718, 1},{-34818, 1},{  4097, 0},
{  4095, 1},{  4096, 0},{  4094, 1},{  4098, 0},{  4094, 1},{  4097, 0},{  4093, 1},{  4100, 0},
{  4092, 1},{  4099, 0},{  4091, 1},{  4104, 0},{  4088, 1},{  4103, 0},{  4087, 1},{  4112, 0},
{  4080, 1},{  4111, 0},{  4079, 1},{  4128, 0},{  4064, 1},{  4127, 0},{  4063, 1},{  4160, 0},
{  4032, 1},{  4159, 0},{  4031, 1},{  4224, 0},{  3968, 1},{  4223, 0},{  3967, 1},{  4352, 0},
{  3840, 1},{  4351, 0},{  3839, 1},{  4608, 0},{  3584, 1},{  4607, 0},{  3583, 1},{  5120, 0},
{  3072, 1},{  5119, 0},{  3071, 1},{  6144, 0},{  2048, 1},{  6143, 0},{  2047, 1},{  8192, 0},
{     0, 1},{  8191, 0},{    -1, 0},{ 12288, 0},{ -4096, 0},{ 12287, 0},{ -4097, 0},{ 20480, 0},
{-12288, 0},{ 20479, 0},{-12289, 0},{ 36864, 0},{-28672, 0},{ 36863, 0},{-28673, 0},{ -4095, 0},
{ -4097, 1},{ -4096, 0},{ -4098, 1},{ -4094, 0},{ -4098, 1},{ -4095, 0},{ -4099, 1},{ -4092, 0},
{ -4100, 1},{ -4093, 0},{ -4101, 1},{ -4088, 0},{ -4104, 1},{ -4089, 0},{ -4105, 1},{ -4080, 0},
{ -4112, 1},{ -4081, 0},{ -4113, 1},{ -4064, 0},{ -4128, 1},{ -4065, 0},{ -4129, 1},{ -4032, 0},
{ -4160, 1},{ -4033, 0},{ -4161, 1},{ -3968, 0},{ -4224, 1},{ -3969, 0},{ -4225, 1},{ -3840, 0},
{ -4352, 1},{ -3841, 0},{ -4353, 1},{ -3584, 0},{ -4608, 1},{ -3585, 0},{ -4609, 1},{ -3072, 0},
{ -5120, 1},{ -3073, 0},{ -5121, 1},{ -2048, 0},{ -6144, 1},{ -2049, 0},{ -6145, 1},{     0, 1},
{ -8192, 1},{    -1, 0},{ -8193, 1},{  4096, 1},{-12288, 1},{  4095, 1},{-12289, 1},{ 12288, 1},
{-20480, 1},{ 12287, 1},{-20481, 1},{ 28672, 1},{-36864, 1},{ 28671, 1},{-36865, 1},{  4096, 0},
{  4094, 1},{  4095, 0},{  4093, 1},{  4097, 0},{  4093, 1},{  4096, 0},{  4092, 1},{  4099, 0},
{  4091, 1},{  4098, 0},{  4090, 1},{  4103, 0},{  4087, 1},{  4102, 0},{  4086, 1},{  4111, 0},
{  4079, 1},{  4110, 0},{  4078, 1},{  4127, 0},{  4063, 1},{  4126, 0},{  4062, 1},{  4159, 0},
{  4031, 1},{  4158, 0},{  4030, 1},{  4223, 0},{  3967, 1},{  4222, 0},{  3966, 1},{  4351, 0},
{  3839, 1},{  4350, 0},{  3838, 1},{  4607, 0},{  3583, 1},{  4606, 0},{  3582, 1},{  5119, 0},
{  3071, 1},{  5118, 0},{  3070, 1},{  6143, 0},{  2047, 1},{  6142, 0},{  2046, 1},{  8191, 0},
{    -1, 0},{  8190, 0},{    -2, 0},{ 12287, 0},{ -4097, 0},{ 12286, 0},{ -4098, 0},{ 20479, 0},
{-12289, 0},{ 20478, 0},{-12290, 0},{ 36863, 0},{-28673, 0},{ 36862, 0},{-28674, 0},{ -4096, 0},
{ -4098, 1},{ -4097, 0},{ -4099, 1},{ -4095, 0},{ -4099, 1},{ -4096, 0},{ -4100, 1},{ -4093, 0},
{ -4101, 1},{ -4094, 0},{ -4102, 1},{ -4089, 0},{ -4105, 1},{ -4090, 0},{ -4106, 1},{ -4081, 0},
{ -4113, 1},{ -4082, 0},{ -4114, 1},{ -4065, 0},{ -4129, 1},{ -4066, 0},{ -4130, 1},{ -4033, 0},
{ -4161, 1},{ -4034, 0},{ -4162, 1},{ -3969, 0},{ -4225, 1},{ -3970, 0},{ -4226, 1},{ -3841, 0},
{ -4353, 1},{ -3842, 0},{ -4354, 1},{ -3585, 0},{ -4609, 1},{ -3586, 0},{ -4610, 1},{ -3073, 0},
{ -5121, 1},{ -3074, 0},{ -5122, 1},{ -2049, 0},{ -6145, 1},{ -2050, 0},{ -6146, 1},{    -1, 0},
{ -8193, 1},{    -2, 0},{ -8194, 1},{  4095, 1},{-12289, 1},{  4094, 1},{-12290, 1},{ 12287, 1},
{-20481, 1},{ 12286, 1},{-20482, 1},{ 28671, 1},{-36865, 1},{ 28670, 1},{-36866, 1},{  8193, 0},
{  8191, 1},{  8192, 0},{  8190, 1},{  8194, 0},{  8190, 1},{  8193, 0},{  8189, 1},{  8196, 0},
{  8188, 1},{  8195, 0},{  8187, 1},{  8200, 0},{  8184, 1},{  8199, 0},{  8183, 1},{  8208, 0},
{  8176, 1},{  8207, 0},{  8175, 1},{  8224, 0},{  8160, 1},{  8223, 0},{  8159, 1},{  8256, 0},
{  8128, 1},{  8255, 0},{  8127, 1},{  8320, 0},{  8064, 1},{  8319, 0},{  8063, 1},{  8448, 0},
{  7936, 1},{  8447, 0},{  7935, 1},{  8704, 0},{  7680, 1},{  8703, 0},{  7679, 1},{  9216, 0},
{  7168, 1},{  9215, 0},{  7167, 1},{ 10240, 0},{  6144, 1},{ 10239, 0},{  6143, 1},{ 12288, 0},
{  4096, 1},{ 12287, 0},{  4095, 1},{ 16384, 0},{     0, 1},{ 16383, 0},{    -1, 0},{ 24576, 0},
{ -8192, 0},{ 24575, 0},{ -8193, 0},{ 40960, 0},{-24576, 0},{ 40959, 0},{-24577, 0},{ -8191, 0},
{ -8193, 1},{ -8192, 0},{ -8194, 1},{ -8190, 0},{ -8194, 1},{ -8191, 0},{ -8195, 1},{ -8188, 0},
{ -8196, 1},{ -8189, 0},{ -8197, 1},{ -8184, 0},{ -8200, 1},{ -8185, 0},{ -8201, 1},{ -8176, 0},
{ -8208, 1},{ -8177, 0},{ -8209, 1},{ -8160, 0},{ -8224, 1},{ -8161, 0},{ -8225, 1},{ -8128, 0},
{ -8256, 1},{ -8129, 0},{ -8257, 1},{ -8064, 0},{ -8320, 1},{ -8065, 0},{ -8321, 1},{ -7936, 0},
{ -8448, 1},{ -7937, 0},{ -8449, 1},{ -7680, 0},{ -8704, 1},{ -7681, 0},{ -8705, 1},{ -7168, 0},
{ -9216, 1},{ -7169, 0},{ -9217, 1},{ -6144, 0},{-10240, 1},{ -6145, 0},{-10241, 1},{ -4096, 0},
{-12288, 1},{ -4097, 0},{-12289, 1},{     0, 1},{-16384, 1},{    -1, 0},{-16385, 1},{  8192, 1},
{-24576, 1},{  8191, 1},{-24577, 1},{ 24576, 1},{-40960, 1},{ 24575, 1},{-40961, 1},{  8192, 0},
{  8190, 1},{  8191, 0},{  8189, 1},{  8193, 0},{  8189, 1},{  8192, 0},{  8188, 1},{  8195, 0},
{  8187, 1},{  8194, 0},{  8186, 1},{  8199, 0},{  8183, 1},{  8198, 0},{  8182, 1},{  8207, 0},
{  8175, 1},{  8206, 0},{  8174, 1},{  8223, 0},{  8159, 1},{  8222, 0},{  8158, 1},{  8255, 0},
{  8127, 1},{  8254, 0},{  8126, 1},{  8319, 0},{  8063, 1},{  8318, 0},{  8062, 1},{  8447, 0},
{  7935, 1},{  8446, 0},{  7934, 1},{  8703, 0},{  7679, 1},{  8702, 0},{  7678, 1},{  9215, 0},
{  7167, 1},{  9214, 0},{  7166, 1},{ 10239, 0},{  6143, 1},{ 10238, 0},{  6142, 1},{ 12287, 0},
{  4095, 1},{ 12286, 0},{  4094, 1},{ 16383, 0},{    -1, 0},{ 16382, 0},{    -2, 0},{ 24575, 0},
{ -8193, 0},{ 24574, 0},{ -8194, 0},{ 40959, 0},{-24577, 0},{ 40958, 0},{-24578, 0},{ -8192, 0},
{ -8194, 1},{ -8193, 0},{ -8195, 1},{ -8191, 0},{ -8195, 1},{ -8192, 0},{ -8196, 1},{ -8189, 0},
{ -8197, 1},{ -8190, 0},{ -8198, 1},{ -8185, 0},{ -8201, 1},{ -8186, 0},{ -8202, 1},{ -8177, 0},
{ -8209, 1},{ -8178, 0},{ -8210, 1},{ -8161, 0},{ -8225, 1},{ -8162, 0},{ -8226, 1},{ -8129, 0},
{ -8257, 1},{ -8130, 0},{ -8258, 1},{ -8065, 0},{ -8321, 1},{ -8066, 0},{ -8322, 1},{ -7937, 0},
{ -8449, 1},{ -7938, 0},{ -8450, 1},{ -7681, 0},{ -8705, 1},{ -7682, 0},{ -8706, 1},{ -7169, 0},
{ -9217, 1},{ -7170, 0},{ -9218, 1},{ -6145, 0},{-10241, 1},{ -6146, 0},{-10242, 1},{ -4097, 0},
{-12289, 1},{ -4098, 0},{-12290, 1},{    -1, 0},{-16385, 1},{    -2, 0},{-16386, 1},{  8191, 1},
{-24577, 1},{  8190, 1},{-24578, 1},{ 24575, 1},{-40961, 1},{ 24574, 1},{-40962, 1},{ 16385, 0},
{ 16383, 1},{ 16384, 0},{ 16382, 1},{ 16386, 0},{ 16382, 1},{ 16385, 0},{ 16381, 1},{ 16388, 0},
{ 16380, 1},{ 16387, 0},{ 16379, 1},{ 16392, 0},{ 16376, 1},{ 16391, 0},{ 16375, 1},{ 16400, 0},
{ 16368, 1},{ 16399, 0},{ 16367, 1},{ 16416, 0},{ 16352, 1},{ 16415, 0},{ 16351, 1},{ 16448, 0},
{ 16320, 1},{ 16447, 0},{ 16319, 1},{ 16512, 0},{ 16256, 1},{ 16511, 0},{ 16255, 1},{ 16640, 0},
{ 16128, 1},{ 16639, 0},{ 16127, 1},{ 16896, 0},{ 15872, 1},{ 16895, 0},{ 15871, 1},{ 17408, 0},
{ 15360, 1},{ 17407, 0},{ 15359, 1},{ 18432, 0},{ 14336, 1},{ 18431, 0},{ 14335, 1},{ 20480, 0},
{ 12288, 1},{ 20479, 0},{ 12287, 1},{ 24576, 0},{  8192, 1},{ 24575, 0},{  8191, 1},{ 32768, 0},
{     0, 1},{ 32767, 0},{    -1, 0},{ 49152, 0},{-16384, 0},{ 49151, 0},{-16385, 0},{-16383, 0},
{-16385, 1},{-16384, 0},{-16386, 1},{-16382, 0},{-16386, 1},{-16383, 0},{-16387, 1},{-16380, 0},
{-16388, 1},{-16381, 0},{-16389, 1},{-16376, 0},{-16392, 1},{-16377, 0},{-16393, 1},{-16368, 0},
{-16400, 1},{-16369, 0},{-16401, 1},{-16352, 0},{-16416, 1},{-16353, 0},{-16417, 1},{-16320, 0},
{-16448, 1},{-16321, 0},{-16449, 1},{-16256, 0},{-16512, 1},{-16257, 0},{-16513, 1},{-16128, 0},
{-16640, 1},{-16129, 0},{-16641, 1},{-15872, 0},{-16896, 1},{-15873, 0},{-16897, 1},{-15360, 0},
{-17408, 1},{-15361, 0},{-17409, 1},{-14336, 0},{-18432, 1},{-14337, 0},{-18433, 1},{-12288, 0},
{-20480, 1},{-12289, 0},{-20481, 1},{ -8192, 0},{-24576, 1},{ -8193, 0},{-24577, 1},{     0, 1},
{-32768, 1},{    -1, 0},{-32769, 1},{ 16384, 1},{-49152, 1},{ 16383, 1},{-49153, 1},{ 16384, 0},
{ 16382, 1},{ 16383, 0},{ 16381, 1},{ 16385, 0},{ 16381, 1},{ 16384, 0},{ 16380, 1},{ 16387, 0},
{ 16379, 1},{ 16386, 0},{ 16378, 1},{ 16391, 0},{ 16375, 1},{ 16390, 0},{ 16374, 1},{ 16399, 0},
{ 16367, 1},{ 16398, 0},{ 16366, 1},{ 16415, 0},{ 16351, 1},{ 16414, 0},{ 16350, 1},{ 16447, 0},
{ 16319, 1},{ 16446, 0},{ 16318, 1},{ 16511, 0},{ 16255, 1},{ 16510, 0},{ 16254, 1},{ 16639, 0},
{ 16127, 1},{ 16638, 0},{ 16126, 1},{ 16895, 0},{ 15871, 1},{ 16894, 0},{ 15870, 1},{ 17407, 0},
{ 15359, 1},{ 17406, 0},{ 15358, 1},{ 18431, 0},{ 14335, 1},{ 18430, 0},{ 14334, 1},{ 20479, 0},
{ 12287, 1},{ 20478, 0},{ 12286, 1},{ 24575, 0},{  8191, 1},{ 24574, 0},{  8190, 1},{ 32767, 0},
{    -1, 0},{ 32766, 0},{    -2, 0},{ 49151, 0},{-16385, 0},{ 49150, 0},{-16386, 0},{-16384, 0},
{-16386, 1},{-16385, 0},{-16387, 1},{-16383, 0},{-16387, 1},{-16384, 0},{-16388, 1},{-16381, 0},
{-16389, 1},{-16382, 0},{-16390, 1},{-16377, 0},{-16393, 1},{-16378, 0},{-16394, 1},{-16369, 0},
{-16401, 1},{-16370, 0},{-16402, 1},{-16353, 0},{-16417, 1},{-16354, 0},{-16418, 1},{-16321, 0},
{-16449, 1},{-16322, 0},{-16450, 1},{-16257, 0},{-16513, 1},{-16258, 0},{-16514, 1},{-16129, 0},
{-16641, 1},{-16130, 0},{-16642, 1},{-15873, 0},{-16897, 1},{-15874, 0},{-16898, 1},{-15361, 0},
{-17409, 1},{-15362, 0},{-17410, 1},{-14337, 0},{-18433, 1},{-14338, 0},{-18434, 1},{-12289, 0},
{-20481, 1},{-12290, 0},{-20482, 1},{ -8193, 0},{-24577, 1},{ -8194, 0},{-24578, 1},{    -1, 0},
{-32769, 1},{    -2, 0},{-32770, 1},{ 16383, 1},{-49153, 1},{ 16382, 1},{-49154, 1},{ 32769, 0},
{ 32767, 1},{ 32768, 0},{ 32766, 1},{ 32770, 0},{ 32766, 1},{ 32769, 0},{ 32765, 1},{ 32772, 0},
{ 32764, 1},{ 32771, 0},{ 32763, 1},{ 32776, 0},{ 32760, 1},{ 32775, 0},{ 32759, 1},{ 32784, 0},
{ 32752, 1},{ 32783, 0},{ 32751, 1},{ 32800, 0},{ 32736, 1},{ 32799, 0},{ 32735, 1},{ 32832, 0},
{ 32704, 1},{ 32831, 0},{ 32703, 1},{ 32896, 0},{ 32640, 1},{ 32895, 0},{ 32639, 1},{ 33024, 0},
{ 32512, 1},{ 33023, 0},{ 32511, 1},{ 33280, 0},{ 32256, 1},{ 33279, 0},{ 32255, 1},{ 33792, 0},
{ 31744, 1},{ 33791, 0},{ 31743, 1},{ 34816, 0},{ 30720, 1},{ 34815, 0},{ 30719, 1},{ 36864, 0},
{ 28672, 1},{ 36863, 0},{ 28671, 1},{ 40960, 0},{ 24576, 1},{ 40959, 0},{ 24575, 1},{ 49152, 0},
{ 16384, 1},{ 49151, 0},{ 16383, 1},{ 65536, 0},{     0, 1},{ 65535, 0},{    -1, 0},{-32767, 0},
{-32769, 1},{-32768, 0},{-32770, 1},{-32766, 0},{-32770, 1},{-32767, 0},{-32771, 1},{-32764, 0},
{-32772, 1},{-32765, 0},{-32773, 1},{-32760, 0},{-32776, 1},{-32761, 0},{-32777, 1},{-32752, 0},
{-32784, 1},{-32753, 0},{-32785, 1},{-32736, 0},{-32800, 1},{-32737, 0},{-32801, 1},{-32704, 0},
{-32832, 1},{-32705, 0},{-32833, 1},{-32640, 0},{-32896, 1},{-32641, 0},{-32897, 1},{-32512, 0},
{-33024, 1},{-32513, 0},{-33025, 1},{-32256, 0},{-33280, 1},{-32257, 0},{-33281, 1},{-31744, 0},
{-33792, 1},{-31745, 0},{-33793, 1},{-30720, 0},{-34816, 1},{-30721, 0},{-34817, 1},{-28672, 0},
{-36864, 1},{-28673, 0},{-36865, 1},{-24576, 0},{-40960, 1},{-24577, 0},{-40961, 1},{-16384, 0},
{-49152, 1},{-16385, 0},{-49153, 1},{     0, 1},{-65536, 1},{    -1, 0},{-65537, 1},{ 32768, 0},
{ 32766, 1},{ 32767, 0},{ 32765, 1},{ 32769, 0},{ 32765, 1},{ 32768, 0},{ 32764, 1},{ 32771, 0},
{ 32763, 1},{ 32770, 0},{ 32762, 1},{ 32775, 0},{ 32759, 1},{ 32774, 0},{ 32758, 1},{ 32783, 0},
{ 32751, 1},{ 32782, 0},{ 32750, 1},{ 32799, 0},{ 32735, 1},{ 32798, 0},{ 32734, 1},{ 32831, 0},
{ 32703, 1},{ 32830, 0},{ 32702, 1},{ 32895, 0},{ 32639, 1},{ 32894, 0},{ 32638, 1},{ 33023, 0},
{ 32511, 1},{ 33022, 0},{ 32510, 1},{ 33279, 0},{ 32255, 1},{ 33278, 0},{ 32254, 1},{ 33791, 0},
{ 31743, 1},{ 33790, 0},{ 31742, 1},{ 34815, 0},{ 30719, 1},{ 34814, 0},{ 30718, 1},{ 36863, 0},
{ 28671, 1},{ 36862, 0},{ 28670, 1},{ 40959, 0},{ 24575, 1},{ 40958, 0},{ 24574, 1},{ 49151, 0},
{ 16383, 1},{ 49150, 0},{ 16382, 1},{ 65535, 0},{    -1, 0},{ 65534, 0},{    -2, 0},{-32768, 0},
{-32770, 1},{-32769, 0},{-32771, 1},{-32767, 0},{-32771, 1},{-32768, 0},{-32772, 1},{-32765, 0},
{-32773, 1},{-32766, 0},{-32774, 1},{-32761, 0},{-32777, 1},{-32762, 0},{-32778, 1},{-32753, 0},
{-32785, 1},{-32754, 0},{-32786, 1},{-32737, 0},{-32801, 1},{-32738, 0},{-32802, 1},{-32705, 0},
{-32833, 1},{-32706, 0},{-32834, 1},{-32641, 0},{-32897, 1},{-32642, 0},{-32898, 1},{-32513, 0},
{-33025, 1},{-32514, 0},{-33026, 1},{-32257, 0},{-33281, 1},{-32258, 0},{-33282, 1},{-31745, 0},
{-33793, 1},{-31746, 0},{-33794, 1},{-30721, 0},{-34817, 1},{-30722, 0},{-34818, 1},{-28673, 0},
{-36865, 1},{-28674, 0},{-36866, 1},{-24577, 0},{-40961, 1},{-24578, 0},{-40962, 1},{-16385, 0},
{-49153, 1},{-16386, 0},{-49154, 1},{    -1, 0},{-65537, 1},{    -2, 0},{-65538, 1},};
int main ()
{
  mp_limb_t r1, r0;
  int err = 0;
  size_t ind = 0;
  for (size_t i = 0; i < 4096; i++)
    {
      int ii = i / 64, jj = i % 64;
      funcs[i](&r1, &r0);
      if (r0 != (mp_limb_signed_t) ref[ind][0] || r1 != (mp_limb_signed_t) ref[ind][1]) {
         printf ("error for f%zu(%d,%d): want (%d,%d) got (%d,%d)\n", i, (int) ops[ii], (int) ops[jj], ref[ind][1], ref[ind][0], (int) r1, (int) r0);
         err++;
       }
      ind++;
    }
  return err != 0;
}

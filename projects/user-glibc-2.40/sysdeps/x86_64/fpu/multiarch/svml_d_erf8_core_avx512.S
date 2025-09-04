/* Function erf vectorized with AVX-512.
   Copyright (C) 2021-2024 Free Software Foundation, Inc.
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
   https://www.gnu.org/licenses/.  */

/*
 * ALGORITHM DESCRIPTION:
 *
 *   Basic formula is
 *    erf(x) ~ erf(x0) +
 *              + exp(-x0*x0)*D*(1+c0+T*P1(T)+D^2*P3(T)+D^4*P5(T)+D^6*p7+D^8*p9)
 *   where D=x-x0, T=x0*D
 *   x0 is x rounded to a specified number of fractional bits (in this case 7),
 *    except that x0=0 for |x|<3.5/128.0 (using x0=0 for first 4 table entries)
 *
 *   Data table packs both erf(x0)_high and a few bits of erf(x0)_low in one
 *   entry (in place of redundant exponent bits)
 *
 */

/* Offsets for data table __svml_derf_data_internal
 */
#define _erf_tbl			0
#define _AbsMask			12288
#define _MaxThreshold			12352
#define _SRound				12416
#define _U2Threshold			12480
#define _poly1_0			12544
#define _poly1_1			12608
#define _poly3_0			12672
#define _poly3_1			12736
#define _poly5_0			12800
#define _poly5_1			12864
#define _poly1_2			12928
#define _poly3_2			12992
#define _poly1_3			13056
#define _poly3_3			13120
#define _Mask32				13184

#include <sysdep.h>

	.section .text.evex512, "ax", @progbits
ENTRY(_ZGVeN8v_erf_skx)
	/*
	 * vector gather: erf(x0),
	 * second value is exp(-x0*x0)
	 */
	lea	__svml_derf_data_internal(%rip), %rax

	/*
	 * erf(x) rounds to 1.0 for x>_MaxThreshold (5.9921875)
	 * can compute all results in the main path
	 */
	vmovups	_MaxThreshold+__svml_derf_data_internal(%rip), %zmm9
	vmovups	_SRound+__svml_derf_data_internal(%rip), %zmm11
	vmovups	_U2Threshold+__svml_derf_data_internal(%rip), %zmm10
	vandpd	_AbsMask+__svml_derf_data_internal(%rip), %zmm0, %zmm7
	vpternlogd $0xff, %zmm1, %zmm1, %zmm14
	kxnorw	%k0, %k0, %k3
	kxnorw	%k0, %k0, %k2
	vminpd	{sae}, %zmm9, %zmm7, %zmm12

	/* save sign */
	vxorpd	%zmm0, %zmm7, %zmm8
	vaddpd	{rn-sae}, %zmm11, %zmm12, %zmm15
	vcmppd	$26, {sae}, %zmm10, %zmm12, %k1

	/*
	 * _LA_ polynomial computation
	 * Start polynomial evaluation
	 */
	vmovups	_poly1_0+__svml_derf_data_internal(%rip), %zmm10
	vpsllq	$4, %zmm15, %zmm3
	vsubpd	{rn-sae}, %zmm11, %zmm15, %zmm13
	vmovups	_poly3_0+__svml_derf_data_internal(%rip), %zmm11
	vmovups	_poly3_3+__svml_derf_data_internal(%rip), %zmm15
	vsubpd	{rn-sae}, %zmm13, %zmm12, %zmm1
	vmulpd	{rn-sae}, %zmm1, %zmm13, %zmm6

	/* NaN fixup */
	vminpd	{sae}, %zmm7, %zmm1, %zmm7
	vmovups	_poly1_2+__svml_derf_data_internal(%rip), %zmm13
	vpandq	_Mask32+__svml_derf_data_internal(%rip), %zmm3, %zmm2
	vpmovqd	%zmm2, %ymm0
	vmovups	_poly1_1+__svml_derf_data_internal(%rip), %zmm2
	vfmadd231pd {rn-sae}, %zmm6, %zmm10, %zmm2
	vfmadd213pd {rn-sae}, %zmm13, %zmm6, %zmm2
	vpxord	%zmm4, %zmm4, %zmm4
	vgatherdpd 8(%rax, %ymm0), %zmm4{%k3}
	vpxord	%zmm5, %zmm5, %zmm5
	vgatherdpd (%rax, %ymm0), %zmm5{%k2}
	vmovups	_poly3_1+__svml_derf_data_internal(%rip), %zmm0

	/* Sign | _Erf_H */
	vxorpd	%zmm8, %zmm5, %zmm5
	vfmadd231pd {rn-sae}, %zmm6, %zmm11, %zmm0
	vpandnq	%zmm12, %zmm12, %zmm14{%k1}
	vandpd	%zmm14, %zmm1, %zmm9

	/* Sign | Diff */
	vxorpd	%zmm8, %zmm7, %zmm1
	vmovups	_poly5_0+__svml_derf_data_internal(%rip), %zmm12
	vmovups	_poly5_1+__svml_derf_data_internal(%rip), %zmm7
	vmovups	_poly3_2+__svml_derf_data_internal(%rip), %zmm14

	/* D2 = Diff^2 */
	vmulpd	{rn-sae}, %zmm9, %zmm9, %zmm3

	/* T^2 */
	vmulpd	{rn-sae}, %zmm6, %zmm6, %zmm9

	/* exp_h(x0) * Diff */
	vmulpd	{rn-sae}, %zmm1, %zmm4, %zmm4
	vfmadd231pd {rn-sae}, %zmm6, %zmm12, %zmm7
	vmovups	_poly1_3+__svml_derf_data_internal(%rip), %zmm12
	vfmadd213pd {rn-sae}, %zmm14, %zmm6, %zmm0
	vfmadd213pd {rn-sae}, %zmm15, %zmm3, %zmm7
	vfmadd213pd {rn-sae}, %zmm12, %zmm6, %zmm2
	vfmadd213pd {rn-sae}, %zmm7, %zmm6, %zmm0

	/* P1 = T^2*P1 - T */
	vfmsub213pd {rn-sae}, %zmm6, %zmm9, %zmm2

	/* P1 + P3*D2 */
	vfmadd213pd {rn-sae}, %zmm2, %zmm3, %zmm0

	/*
	 * branch-free
	 * low part of result: exp_h(x0) * Diff*(1+P1)
	 */
	vfmadd213pd {rn-sae}, %zmm4, %zmm4, %zmm0

	/* Final result */
	vaddpd	{rn-sae}, %zmm5, %zmm0, %zmm6

	/* Fix erf(-0) = -0 */
	vorpd	%zmm8, %zmm6, %zmm0
	ret

END(_ZGVeN8v_erf_skx)

	.section .rodata, "a"
	.align	64

#ifdef __svml_derf_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(64)) VUINT32 _erf_tbl[6*128*2][2];
	__declspec(align(64)) VUINT32 _AbsMask[8][2];
	__declspec(align(64)) VUINT32 _MaxThreshold[8][2];
	__declspec(align(64)) VUINT32 _SRound[8][2];
	__declspec(align(64)) VUINT32 _U2Threshold[8][2];
	__declspec(align(64)) VUINT32 _poly1_0[8][2];
	__declspec(align(64)) VUINT32 _poly1_1[8][2];
	__declspec(align(64)) VUINT32 _poly3_0[8][2];
	__declspec(align(64)) VUINT32 _poly3_1[8][2];
	__declspec(align(64)) VUINT32 _poly5_0[8][2];
	__declspec(align(64)) VUINT32 _poly5_1[8][2];
	__declspec(align(64)) VUINT32 _poly1_2[8][2];
	__declspec(align(64)) VUINT32 _poly3_2[8][2];
	__declspec(align(64)) VUINT32 _poly1_3[8][2];
	__declspec(align(64)) VUINT32 _poly3_3[8][2];
	__declspec(align(64)) VUINT32 _Mask32[8][2];
} __svml_derf_data_internal;
#endif
__svml_derf_data_internal:
	/* _erf_tbl */
	.quad	0x0000000000000000, 0x3ff20dd750429b6d
	.quad	0x3f820dbf3deb1340, 0x3ff20d8f1975c85d
	.quad	0x3f920d77083f17a0, 0x3ff20cb67bd452c7
	.quad	0x3f9b137e0cf584dc, 0x3ff20b4d8bac36c1
	.quad	0x3fa20c5645dd2538, 0x3ff209546ad13ccf
	.quad	0x3fa68e5d3bbc9526, 0x3ff206cb4897b148
	.quad	0x3fab0fafef135745, 0x3ff203b261cd0053
	.quad	0x3faf902a77bd3821, 0x3ff2000a00ae3804
	.quad	0x3fb207d480e90658, 0x3ff1fbd27cdc72d3
	.quad	0x3fb44703e87e8593, 0x3ff1f70c3b4f2cc8
	.quad	0x3fb68591a1e83b5d, 0x3ff1f1b7ae44867f
	.quad	0x3fb8c36beb8a8d23, 0x3ff1ebd5552f795b
	.quad	0x3fbb0081148a873a, 0x3ff1e565bca400d4
	.quad	0x3fbd3cbf7e70a4b3, 0x3ff1de697e413d29
	.quad	0x3fbf78159ec8bb50, 0x3ff1d6e14099944a
	.quad	0x3fc0d939005f65e5, 0x3ff1cecdb718d61c
	.quad	0x3fc1f5e1a35c3b89, 0x3ff1c62fa1e869b6
	.quad	0x3fc311fc15f56d14, 0x3ff1bd07cdd189ac
	.quad	0x3fc42d7fc2f64959, 0x3ff1b357141d95d5
	.quad	0x3fc548642321d7c6, 0x3ff1a91e5a748165
	.quad	0x3fc662a0bdf7a89f, 0x3ff19e5e92b964ab
	.quad	0x3fc77c2d2a765f9e, 0x3ff19318bae53a04
	.quad	0x3fc895010fdbdbfd, 0x3ff1874ddcdfce24
	.quad	0x3fc9ad142662e14d, 0x3ff17aff0e56ec10
	.quad	0x3fcac45e37fe2526, 0x3ff16e2d7093cd8c
	.quad	0x3fcbdad72110a648, 0x3ff160da304ed92f
	.quad	0x3fccf076d1233237, 0x3ff153068581b781
	.quad	0x3fce05354b96ff36, 0x3ff144b3b337c90c
	.quad	0x3fcf190aa85540e2, 0x3ff135e3075d076b
	.quad	0x3fd015f78a3dcf3d, 0x3ff12695da8b5bde
	.quad	0x3fd09eed6982b948, 0x3ff116cd8fd67618
	.quad	0x3fd127631eb8de32, 0x3ff1068b94962e5e
	.quad	0x3fd1af54e232d609, 0x3ff0f5d1602f7e41
	.quad	0x3fd236bef825d9a2, 0x3ff0e4a073dc1b91
	.quad	0x3fd2bd9db0f7827f, 0x3ff0d2fa5a70c168
	.quad	0x3fd343ed6989b7d9, 0x3ff0c0e0a8223359
	.quad	0x3fd3c9aa8b84beda, 0x3ff0ae54fa490723
	.quad	0x3fd44ed18d9f6462, 0x3ff09b58f724416b
	.quad	0x3fd4d35ef3e5372e, 0x3ff087ee4d9ad247
	.quad	0x3fd5574f4ffac98e, 0x3ff07416b4fbfe7c
	.quad	0x3fd5da9f415ff23f, 0x3ff05fd3ecbec298
	.quad	0x3fd65d4b75b00471, 0x3ff04b27bc403d30
	.quad	0x3fd6df50a8dff772, 0x3ff03613f2812daf
	.quad	0x3fd760aba57a76bf, 0x3ff0209a65e29545
	.quad	0x3fd7e15944d9d3e4, 0x3ff00abcf3e187a9
	.quad	0x3fd861566f5fd3c0, 0x3fefe8fb01a47307
	.quad	0x3fd8e0a01cab516b, 0x3fefbbbbef34b4b2
	.quad	0x3fd95f3353cbb146, 0x3fef8dc092d58ff8
	.quad	0x3fd9dd0d2b721f39, 0x3fef5f0cdaf15313
	.quad	0x3fda5a2aca209394, 0x3fef2fa4c16c0019
	.quad	0x3fdad68966569a87, 0x3feeff8c4b1375db
	.quad	0x3fdb522646bbda68, 0x3feecec7870ebca8
	.quad	0x3fdbccfec24855b8, 0x3fee9d5a8e4c934e
	.quad	0x3fdc4710406a65fc, 0x3fee6b4982f158b9
	.quad	0x3fdcc058392a6d2d, 0x3fee38988fc46e72
	.quad	0x3fdd38d4354c3bd0, 0x3fee054be79d3042
	.quad	0x3fddb081ce6e2a48, 0x3fedd167c4cf9d2a
	.quad	0x3fde275eaf25e458, 0x3fed9cf06898cdaf
	.quad	0x3fde9d68931ae650, 0x3fed67ea1a8b5368
	.quad	0x3fdf129d471eabb1, 0x3fed325927fb9d89
	.quad	0x3fdf86faa9428f9d, 0x3fecfc41e36c7df9
	.quad	0x3fdffa7ea8eb5fd0, 0x3fecc5a8a3fbea40
	.quad	0x3fe03693a371519c, 0x3fec8e91c4d01368
	.quad	0x3fe06f794ab2cae7, 0x3fec5701a484ef9d
	.quad	0x3fe0a7ef5c18edd2, 0x3fec1efca49a5011
	.quad	0x3fe0dff4f247f6c6, 0x3febe68728e29d5e
	.quad	0x3fe1178930ada115, 0x3febada596f25436
	.quad	0x3fe14eab43841b55, 0x3feb745c55905bf8
	.quad	0x3fe1855a5fd3dd50, 0x3feb3aafcc27502e
	.quad	0x3fe1bb95c3746199, 0x3feb00a46237d5be
	.quad	0x3fe1f15cb50bc4de, 0x3feac63e7ecc1411
	.quad	0x3fe226ae840d4d70, 0x3fea8b8287ec6a09
	.quad	0x3fe25b8a88b6dd7f, 0x3fea5074e2157620
	.quad	0x3fe28ff0240d52cd, 0x3fea1519efaf889e
	.quad	0x3fe2c3debfd7d6c1, 0x3fe9d97610879642
	.quad	0x3fe2f755ce9a21f4, 0x3fe99d8da149c13f
	.quad	0x3fe32a54cb8db67b, 0x3fe96164fafd8de3
	.quad	0x3fe35cdb3a9a144d, 0x3fe925007283d7aa
	.quad	0x3fe38ee8a84beb71, 0x3fe8e86458169af8
	.quad	0x3fe3c07ca9cb4f9e, 0x3fe8ab94f6caa71d
	.quad	0x3fe3f196dcd0f135, 0x3fe86e9694134b9e
	.quad	0x3fe42236e79a5fa6, 0x3fe8316d6f48133d
	.quad	0x3fe4525c78dd5966, 0x3fe7f41dc12c9e89
	.quad	0x3fe4820747ba2dc2, 0x3fe7b6abbb7aaf19
	.quad	0x3fe4b13713ad3513, 0x3fe7791b886e7403
	.quad	0x3fe4dfeba47f63cc, 0x3fe73b714a552763
	.quad	0x3fe50e24ca35fd2c, 0x3fe6fdb11b1e0c34
	.quad	0x3fe53be25d016a4f, 0x3fe6bfdf0beddaf5
	.quad	0x3fe569243d2b3a9b, 0x3fe681ff24b4ab04
	.quad	0x3fe595ea53035283, 0x3fe6441563c665d4
	.quad	0x3fe5c2348ecc4dc3, 0x3fe60625bd75d07b
	.quad	0x3fe5ee02e8a71a53, 0x3fe5c8341bb23767
	.quad	0x3fe61955607dd15d, 0x3fe58a445da7c74c
	.quad	0x3fe6442bfdedd397, 0x3fe54c5a57629db0
	.quad	0x3fe66e86d0312e82, 0x3fe50e79d1749ac9
	.quad	0x3fe69865ee075011, 0x3fe4d0a6889dfd9f
	.quad	0x3fe6c1c9759d0e5f, 0x3fe492e42d78d2c5
	.quad	0x3fe6eab18c74091b, 0x3fe4553664273d24
	.quad	0x3fe7131e5f496a5a, 0x3fe417a0c4049fd0
	.quad	0x3fe73b1021fc0cb8, 0x3fe3da26d759aef5
	.quad	0x3fe762870f720c6f, 0x3fe39ccc1b136d5a
	.quad	0x3fe78983697dc96f, 0x3fe35f93fe7d1b3d
	.quad	0x3fe7b00578c26037, 0x3fe32281e2fd1a92
	.quad	0x3fe7d60d8c979f7b, 0x3fe2e5991bd4cbfc
	.quad	0x3fe7fb9bfaed8078, 0x3fe2a8dcede3673b
	.quad	0x3fe820b1202f27fb, 0x3fe26c508f6bd0ff
	.quad	0x3fe8454d5f25760d, 0x3fe22ff727dd6f7b
	.quad	0x3fe8697120d92a4a, 0x3fe1f3d3cf9ffe5a
	.quad	0x3fe88d1cd474a2e0, 0x3fe1b7e98fe26217
	.quad	0x3fe8b050ef253c37, 0x3fe17c3b626c7a12
	.quad	0x3fe8d30debfc572e, 0x3fe140cc3173f007
	.quad	0x3fe8f5544bd00c04, 0x3fe1059ed7740313
	.quad	0x3fe91724951b8fc6, 0x3fe0cab61f084b93
	.quad	0x3fe9387f53df5238, 0x3fe09014c2ca74da
	.quad	0x3fe959651980da31, 0x3fe055bd6d32e8d7
	.quad	0x3fe979d67caa6631, 0x3fe01bb2b87c6968
	.quad	0x3fe999d4192a5715, 0x3fdfc3ee5d1524b0
	.quad	0x3fe9b95e8fd26aba, 0x3fdf511a91a67d2a
	.quad	0x3fe9d8768656cc42, 0x3fdedeeee0959518
	.quad	0x3fe9f71ca72cffb6, 0x3fde6d6ffaa65a25
	.quad	0x3fea1551a16aaeaf, 0x3fddfca26f5bbf88
	.quad	0x3fea331628a45b92, 0x3fdd8c8aace11e63
	.quad	0x3fea506af4cc00f4, 0x3fdd1d2cfff91594
	.quad	0x3fea6d50c20fa293, 0x3fdcae8d93f1d7b7
	.quad	0x3fea89c850b7d54d, 0x3fdc40b0729ed548
	.quad	0x3feaa5d265064366, 0x3fdbd3998457afdb
	.quad	0x3feac16fc7143263, 0x3fdb674c8ffc6283
	.quad	0x3feadca142b10f98, 0x3fdafbcd3afe8ab6
	.quad	0x3feaf767a741088b, 0x3fda911f096fbc26
	.quad	0x3feb11c3c79bb424, 0x3fda27455e14c93c
	.quad	0x3feb2bb679ead19c, 0x3fd9be437a7de946
	.quad	0x3feb4540978921ee, 0x3fd9561c7f23a47b
	.quad	0x3feb5e62fce16095, 0x3fd8eed36b886d93
	.quad	0x3feb771e894d602e, 0x3fd8886b1e5ecfd1
	.quad	0x3feb8f741ef54f83, 0x3fd822e655b417e7
	.quad	0x3feba764a2af2b78, 0x3fd7be47af1f5d89
	.quad	0x3febbef0fbde6221, 0x3fd75a91a7f4d2ed
	.quad	0x3febd61a1453ab44, 0x3fd6f7c69d7d3ef8
	.quad	0x3febece0d82d1a5c, 0x3fd695e8cd31867e
	.quad	0x3fec034635b66e23, 0x3fd634fa54fa285f
	.quad	0x3fec194b1d49a184, 0x3fd5d4fd33729015
	.quad	0x3fec2ef0812fc1bd, 0x3fd575f3483021c3
	.quad	0x3fec443755820d64, 0x3fd517de540ce2a3
	.quad	0x3fec5920900b5fd1, 0x3fd4babff975a04c
	.quad	0x3fec6dad2829ec62, 0x3fd45e99bcbb7915
	.quad	0x3fec81de16b14cef, 0x3fd4036d0468a7a2
	.quad	0x3fec95b455cce69d, 0x3fd3a93b1998736c
	.quad	0x3feca930e0e2a825, 0x3fd35005285227f1
	.quad	0x3fecbc54b476248d, 0x3fd2f7cc3fe6f423
	.quad	0x3feccf20ce0c0d27, 0x3fd2a09153529381
	.quad	0x3fece1962c0e0d8b, 0x3fd24a55399ea239
	.quad	0x3fecf3b5cdaf0c39, 0x3fd1f518ae487dc8
	.quad	0x3fed0580b2cfd249, 0x3fd1a0dc51a9934d
	.quad	0x3fed16f7dbe41ca0, 0x3fd14da0a961fd14
	.quad	0x3fed281c49d818d0, 0x3fd0fb6620c550af
	.quad	0x3fed38eefdf64fdd, 0x3fd0aa2d09497f2b
	.quad	0x3fed4970f9ce00d9, 0x3fd059f59af7a906
	.quad	0x3fed59a33f19ed42, 0x3fd00abff4dec7a3
	.quad	0x3fed6986cfa798e7, 0x3fcf79183b101c5b
	.quad	0x3fed791cad3eff01, 0x3fcedeb406d9c825
	.quad	0x3fed8865d98abe01, 0x3fce4652fadcb6b2
	.quad	0x3fed97635600bb89, 0x3fcdaff4969c0b04
	.quad	0x3feda61623cb41e0, 0x3fcd1b982c501370
	.quad	0x3fedb47f43b2980d, 0x3fcc893ce1dcbef7
	.quad	0x3fedc29fb60715af, 0x3fcbf8e1b1ca2279
	.quad	0x3fedd0787a8bb39d, 0x3fcb6a856c3ed54f
	.quad	0x3fedde0a90611a0d, 0x3fcade26b7fbed95
	.quad	0x3fedeb56f5f12d28, 0x3fca53c4135a6526
	.quad	0x3fedf85ea8db188e, 0x3fc9cb5bd549b111
	.quad	0x3fee0522a5dfda73, 0x3fc944ec2e4f5630
	.quad	0x3fee11a3e8cf4eb8, 0x3fc8c07329874652
	.quad	0x3fee1de36c75ba58, 0x3fc83deeada4d25a
	.quad	0x3fee29e22a89d766, 0x3fc7bd5c7df3fe9c
	.quad	0x3fee35a11b9b61ce, 0x3fc73eba3b5b07b7
	.quad	0x3fee4121370224cc, 0x3fc6c205655be720
	.quad	0x3fee4c6372cd8927, 0x3fc6473b5b15a7a1
	.quad	0x3fee5768c3b4a3fc, 0x3fc5ce595c455b0a
	.quad	0x3fee62321d06c5e0, 0x3fc5575c8a468362
	.quad	0x3fee6cc0709c8a0d, 0x3fc4e241e912c305
	.quad	0x3fee7714aec96534, 0x3fc46f066040a832
	.quad	0x3fee812fc64db369, 0x3fc3fda6bc016994
	.quad	0x3fee8b12a44944a8, 0x3fc38e1fae1d6a9d
	.quad	0x3fee94be342e6743, 0x3fc3206dceef5f87
	.quad	0x3fee9e335fb56f87, 0x3fc2b48d9e5dea1c
	.quad	0x3feea7730ed0bbb9, 0x3fc24a7b84d38971
	.quad	0x3feeb07e27a133aa, 0x3fc1e233d434b813
	.quad	0x3feeb9558e6b42ce, 0x3fc17bb2c8d41535
	.quad	0x3feec1fa258c4bea, 0x3fc116f48a6476cc
	.quad	0x3feeca6ccd709544, 0x3fc0b3f52ce8c383
	.quad	0x3feed2ae6489ac1e, 0x3fc052b0b1a174ea
	.quad	0x3feedabfc7453e63, 0x3fbfe6460fef4680
	.quad	0x3feee2a1d004692c, 0x3fbf2a901ccafb37
	.quad	0x3feeea5557137ae0, 0x3fbe723726b824a9
	.quad	0x3feef1db32a2277c, 0x3fbdbd32ac4c99b0
	.quad	0x3feef93436bc2daa, 0x3fbd0b7a0f921e7c
	.quad	0x3fef006135426b26, 0x3fbc5d0497c09e74
	.quad	0x3fef0762fde45ee6, 0x3fbbb1c972f23e50
	.quad	0x3fef0e3a5e1a1788, 0x3fbb09bfb7d11a84
	.quad	0x3fef14e8211e8c55, 0x3fba64de673e8837
	.quad	0x3fef1b6d0fea5f4d, 0x3fb9c31c6df3b1b8
	.quad	0x3fef21c9f12f0677, 0x3fb92470a61b6965
	.quad	0x3fef27ff89525acf, 0x3fb888d1d8e510a3
	.quad	0x3fef2e0e9a6a8b09, 0x3fb7f036c0107294
	.quad	0x3fef33f7e43a706b, 0x3fb75a96077274ba
	.quad	0x3fef39bc242e43e6, 0x3fb6c7e64e7281cb
	.quad	0x3fef3f5c1558b19e, 0x3fb6381e2980956b
	.quad	0x3fef44d870704911, 0x3fb5ab342383d178
	.quad	0x3fef4a31ebcd47df, 0x3fb5211ebf41880b
	.quad	0x3fef4f693b67bd77, 0x3fb499d478bca735
	.quad	0x3fef547f10d60597, 0x3fb4154bc68d75c3
	.quad	0x3fef59741b4b97cf, 0x3fb3937b1b31925a
	.quad	0x3fef5e4907982a07, 0x3fb31458e6542847
	.quad	0x3fef62fe80272419, 0x3fb297db960e4f63
	.quad	0x3fef67952cff6282, 0x3fb21df9981f8e53
	.quad	0x3fef6c0db3c34641, 0x3fb1a6a95b1e786f
	.quad	0x3fef7068b7b10fd9, 0x3fb131e14fa1625d
	.quad	0x3fef74a6d9a38383, 0x3fb0bf97e95f2a64
	.quad	0x3fef78c8b812d498, 0x3fb04fc3a0481321
	.quad	0x3fef7cceef15d631, 0x3fafc4b5e32d6259
	.quad	0x3fef80ba18636f07, 0x3faeeea8c1b1db94
	.quad	0x3fef848acb544e95, 0x3fae1d4cf1e2450a
	.quad	0x3fef88419ce4e184, 0x3fad508f9a1ea64f
	.quad	0x3fef8bdf1fb78370, 0x3fac885df3451a07
	.quad	0x3fef8f63e416ebff, 0x3fabc4a54a84e834
	.quad	0x3fef92d077f8d56d, 0x3fab055303221015
	.quad	0x3fef96256700da8e, 0x3faa4a549829587e
	.quad	0x3fef99633a838a57, 0x3fa993979e14fffe
	.quad	0x3fef9c8a7989af0d, 0x3fa8e109c4622913
	.quad	0x3fef9f9ba8d3c733, 0x3fa83298d717210e
	.quad	0x3fefa2974addae45, 0x3fa78832c03aa2b1
	.quad	0x3fefa57ddfe27376, 0x3fa6e1c5893c380b
	.quad	0x3fefa84fe5e05c8d, 0x3fa63f3f5c4de13b
	.quad	0x3fefab0dd89d1309, 0x3fa5a08e85af27e0
	.quad	0x3fefadb831a9f9c3, 0x3fa505a174e9c929
	.quad	0x3fefb04f6868a944, 0x3fa46e66be002240
	.quad	0x3fefb2d3f20f9101, 0x3fa3dacd1a8d8cce
	.quad	0x3fefb54641aebbc9, 0x3fa34ac36ad8dafe
	.quad	0x3fefb7a6c834b5a2, 0x3fa2be38b6d92415
	.quad	0x3fefb9f5f4739170, 0x3fa2351c2f2d1449
	.quad	0x3fefbc3433260ca5, 0x3fa1af5d2e04f3f6
	.quad	0x3fefbe61eef4cf6a, 0x3fa12ceb37ff9bc3
	.quad	0x3fefc07f907bc794, 0x3fa0adb5fcfa8c75
	.quad	0x3fefc28d7e4f9cd0, 0x3fa031ad58d56279
	.quad	0x3fefc48c1d033c7a, 0x3f9f7182a851bca2
	.quad	0x3fefc67bcf2d7b8f, 0x3f9e85c449e377f3
	.quad	0x3fefc85cf56ecd38, 0x3f9da0005e5f28df
	.quad	0x3fefca2fee770c79, 0x3f9cc0180af00a8b
	.quad	0x3fefcbf5170b578b, 0x3f9be5ecd2fcb5f9
	.quad	0x3fefcdacca0bfb73, 0x3f9b1160991ff737
	.quad	0x3fefcf57607a6e7c, 0x3f9a4255a00b9f03
	.quad	0x3fefd0f5317f582f, 0x3f9978ae8b55ce1b
	.quad	0x3fefd2869270a56f, 0x3f98b44e6031383e
	.quad	0x3fefd40bd6d7a785, 0x3f97f5188610ddc8
	.quad	0x3fefd58550773cb5, 0x3f973af0c737bb45
	.quad	0x3fefd6f34f52013a, 0x3f9685bb5134ef13
	.quad	0x3fefd85621b0876d, 0x3f95d55cb54cd53a
	.quad	0x3fefd9ae142795e3, 0x3f9529b9e8cf9a1e
	.quad	0x3fefdafb719e6a69, 0x3f9482b8455dc491
	.quad	0x3fefdc3e835500b3, 0x3f93e03d891b37de
	.quad	0x3fefdd7790ea5bc0, 0x3f93422fd6d12e2b
	.quad	0x3fefdea6e062d0c9, 0x3f92a875b5ffab56
	.quad	0x3fefdfccb62e52d3, 0x3f9212f612dee7fb
	.quad	0x3fefe0e9552ebdd6, 0x3f9181983e5133dd
	.quad	0x3fefe1fcfebe2083, 0x3f90f443edc5ce49
	.quad	0x3fefe307f2b503d0, 0x3f906ae13b0d3255
	.quad	0x3fefe40a6f70af4b, 0x3f8fcab1483ea7fc
	.quad	0x3fefe504b1d9696c, 0x3f8ec72615a894c4
	.quad	0x3fefe5f6f568b301, 0x3f8dcaf3691fc448
	.quad	0x3fefe6e1742f7cf6, 0x3f8cd5ec93c12432
	.quad	0x3fefe7c466dc57a1, 0x3f8be7e5ac24963b
	.quad	0x3fefe8a004c19ae6, 0x3f8b00b38d6b3575
	.quad	0x3fefe97483db8670, 0x3f8a202bd6372dce
	.quad	0x3fefea4218d6594a, 0x3f894624e78e0faf
	.quad	0x3fefeb08f7146046, 0x3f887275e3a6869e
	.quad	0x3fefebc950b3fa75, 0x3f87a4f6aca256cb
	.quad	0x3fefec835695932e, 0x3f86dd7fe3358230
	.quad	0x3fefed37386190fb, 0x3f861beae53b72b7
	.quad	0x3fefede5248e38f4, 0x3f856011cc3b036d
	.quad	0x3fefee8d486585ee, 0x3f84a9cf6bda3f4c
	.quad	0x3fefef2fd00af31a, 0x3f83f8ff5042a88e
	.quad	0x3fefefcce6813974, 0x3f834d7dbc76d7e5
	.quad	0x3feff064b5afffbe, 0x3f82a727a89a3f14
	.quad	0x3feff0f766697c76, 0x3f8205dac02bd6b9
	.quad	0x3feff18520700971, 0x3f81697560347b26
	.quad	0x3feff20e0a7ba8c2, 0x3f80d1d69569b82d
	.quad	0x3feff2924a3f7a83, 0x3f803ede1a45bfee
	.quad	0x3feff312046f2339, 0x3f7f60d8aa2a88f2
	.quad	0x3feff38d5cc4227f, 0x3f7e4cc4abf7d065
	.quad	0x3feff404760319b4, 0x3f7d4143a9dfe965
	.quad	0x3feff47772010262, 0x3f7c3e1a5f5c077c
	.quad	0x3feff4e671a85425, 0x3f7b430ecf4a83a8
	.quad	0x3feff55194fe19df, 0x3f7a4fe83fb9db25
	.quad	0x3feff5b8fb26f5f6, 0x3f79646f35a76624
	.quad	0x3feff61cc26c1578, 0x3f78806d70b2fc36
	.quad	0x3feff67d08401202, 0x3f77a3ade6c8b3e5
	.quad	0x3feff6d9e943c231, 0x3f76cdfcbfc1e263
	.quad	0x3feff733814af88c, 0x3f75ff2750fe7820
	.quad	0x3feff789eb6130c9, 0x3f7536fc18f7ce5c
	.quad	0x3feff7dd41ce2b4d, 0x3f74754abacdf1dc
	.quad	0x3feff82d9e1a76d8, 0x3f73b9e3f9d06e3f
	.quad	0x3feff87b1913e853, 0x3f730499b503957f
	.quad	0x3feff8c5cad200a5, 0x3f72553ee2a336bf
	.quad	0x3feff90dcaba4096, 0x3f71aba78ba3af89
	.quad	0x3feff9532f846ab0, 0x3f7107a8c7323a6e
	.quad	0x3feff9960f3eb327, 0x3f706918b6355624
	.quad	0x3feff9d67f51ddba, 0x3f6f9f9cfd9c3035
	.quad	0x3feffa14948549a7, 0x3f6e77448fb66bb9
	.quad	0x3feffa506302ebae, 0x3f6d58da68fd1170
	.quad	0x3feffa89fe5b3625, 0x3f6c4412bf4b8f0b
	.quad	0x3feffac17988ef4b, 0x3f6b38a3af2e55b4
	.quad	0x3feffaf6e6f4f5c0, 0x3f6a3645330550ff
	.quad	0x3feffb2a5879f35e, 0x3f693cb11a30d765
	.quad	0x3feffb5bdf67fe6f, 0x3f684ba3004a50d0
	.quad	0x3feffb8b8c88295f, 0x3f6762d84469c18f
	.quad	0x3feffbb970200110, 0x3f66821000795a03
	.quad	0x3feffbe599f4f9d9, 0x3f65a90b00981d93
	.quad	0x3feffc10194fcb64, 0x3f64d78bba8ca5fd
	.quad	0x3feffc38fcffbb7c, 0x3f640d564548fad7
	.quad	0x3feffc60535dd7f5, 0x3f634a305080681f
	.quad	0x3feffc862a501fd7, 0x3f628de11c5031eb
	.quad	0x3feffcaa8f4c9bea, 0x3f61d83170fbf6fb
	.quad	0x3feffccd8f5c66d1, 0x3f6128eb96be8798
	.quad	0x3feffcef371ea4d7, 0x3f607fdb4dafea5f
	.quad	0x3feffd0f92cb6ba7, 0x3f5fb99b8b8279e1
	.quad	0x3feffd2eae369a07, 0x3f5e7f232d9e2630
	.quad	0x3feffd4c94d29fdb, 0x3f5d4fed7195d7e8
	.quad	0x3feffd6951b33686, 0x3f5c2b9cf7f893bf
	.quad	0x3feffd84ef9009ee, 0x3f5b11d702b3deb2
	.quad	0x3feffd9f78c7524a, 0x3f5a024365f771bd
	.quad	0x3feffdb8f7605ee7, 0x3f58fc8c794b03b5
	.quad	0x3feffdd1750e1220, 0x3f58005f08d6f1ef
	.quad	0x3feffde8fb314ebf, 0x3f570d6a46e07dda
	.quad	0x3feffdff92db56e5, 0x3f56235fbd7a4345
	.quad	0x3feffe1544d01ccb, 0x3f5541f340697987
	.quad	0x3feffe2a1988857c, 0x3f5468dadf4080ab
	.quad	0x3feffe3e19349dc7, 0x3f5397ced7af2b15
	.quad	0x3feffe514bbdc197, 0x3f52ce898809244e
	.quad	0x3feffe63b8c8b5f7, 0x3f520cc76202c5fb
	.quad	0x3feffe7567b7b5e1, 0x3f515246dda49d47
	.quad	0x3feffe865fac722b, 0x3f509ec86c75d497
	.quad	0x3feffe96a78a04a9, 0x3f4fe41cd9bb4eee
	.quad	0x3feffea645f6d6da, 0x3f4e97ba3b77f306
	.quad	0x3feffeb5415e7c44, 0x3f4d57f524723822
	.quad	0x3feffec39ff380b9, 0x3f4c245d4b99847a
	.quad	0x3feffed167b12ac2, 0x3f4afc85e0f82e12
	.quad	0x3feffede9e5d3262, 0x3f49e005769dbc1d
	.quad	0x3feffeeb49896c6d, 0x3f48ce75e9f6f8a0
	.quad	0x3feffef76e956a9f, 0x3f47c7744d9378f7
	.quad	0x3fefff0312b010b5, 0x3f46caa0d3582fe9
	.quad	0x3fefff0e3ad91ec2, 0x3f45d79eb71e893b
	.quad	0x3fefff18ebe2b0e1, 0x3f44ee1429bf7cc0
	.quad	0x3fefff232a72b48e, 0x3f440daa3c89f5b6
	.quad	0x3fefff2cfb0453d9, 0x3f43360ccd23db3a
	.quad	0x3fefff3661e9569d, 0x3f4266ea71d4f71a
	.quad	0x3fefff3f634b79f9, 0x3f419ff4663ae9df
	.quad	0x3fefff48032dbe40, 0x3f40e0de78654d1e
	.quad	0x3fefff50456dab8c, 0x3f40295ef6591848
	.quad	0x3fefff582dc48d30, 0x3f3ef25d37f49fe1
	.quad	0x3fefff5fbfc8a439, 0x3f3da01102b5f851
	.quad	0x3fefff66feee5129, 0x3f3c5b5412dcafad
	.quad	0x3fefff6dee89352e, 0x3f3b23a5a23e4210
	.quad	0x3fefff7491cd4af6, 0x3f39f8893d8fd1c1
	.quad	0x3fefff7aebcff755, 0x3f38d986a4187285
	.quad	0x3fefff80ff8911fd, 0x3f37c629a822bc9e
	.quad	0x3fefff86cfd3e657, 0x3f36be02102b3520
	.quad	0x3fefff8c5f702ccf, 0x3f35c0a378c90bca
	.quad	0x3fefff91b102fca8, 0x3f34cda5374ea275
	.quad	0x3fefff96c717b695, 0x3f33e4a23d1f4703
	.quad	0x3fefff9ba420e834, 0x3f330538fbb77ecd
	.quad	0x3fefffa04a7928b1, 0x3f322f0b496539be
	.quad	0x3fefffa4bc63ee9a, 0x3f3161be46ad3b50
	.quad	0x3fefffa8fc0e5f33, 0x3f309cfa445b00ff
	.quad	0x3fefffad0b901755, 0x3f2fc0d55470cf51
	.quad	0x3fefffb0ecebee1b, 0x3f2e577bbcd49935
	.quad	0x3fefffb4a210b172, 0x3f2cfd4a5adec5c0
	.quad	0x3fefffb82cd9dcbf, 0x3f2bb1a9657ce465
	.quad	0x3fefffbb8f1049c6, 0x3f2a740684026555
	.quad	0x3fefffbeca6adbe9, 0x3f2943d4a1d1ed39
	.quad	0x3fefffc1e08f25f5, 0x3f28208bc334a6a5
	.quad	0x3fefffc4d3120aa1, 0x3f2709a8db59f25c
	.quad	0x3fefffc7a37857d2, 0x3f25feada379d8b7
	.quad	0x3fefffca53375ce3, 0x3f24ff207314a102
	.quad	0x3fefffcce3b57bff, 0x3f240a8c1949f75e
	.quad	0x3fefffcf564ab6b7, 0x3f23207fb7420eb9
	.quad	0x3fefffd1ac4135f9, 0x3f22408e9ba3327f
	.quad	0x3fefffd3e6d5cd87, 0x3f216a501f0e42ca
	.quad	0x3fefffd607387b07, 0x3f209d5f819c9e29
	.quad	0x3fefffd80e8ce0da, 0x3f1fb2b792b40a22
	.quad	0x3fefffd9fdeabcce, 0x3f1e3bcf436a1a95
	.quad	0x3fefffdbd65e5ad0, 0x3f1cd55277c18d05
	.quad	0x3fefffdd98e903b2, 0x3f1b7e94604479dc
	.quad	0x3fefffdf46816833, 0x3f1a36eec00926dd
	.quad	0x3fefffe0e0140857, 0x3f18fdc1b2dcf7b9
	.quad	0x3fefffe26683972a, 0x3f17d2737527c3f9
	.quad	0x3fefffe3daa95b18, 0x3f16b4702d7d5849
	.quad	0x3fefffe53d558ae9, 0x3f15a329b7d30748
	.quad	0x3fefffe68f4fa777, 0x3f149e17724f4d41
	.quad	0x3fefffe7d156d244, 0x3f13a4b60ba9aa4e
	.quad	0x3fefffe904222101, 0x3f12b6875310f785
	.quad	0x3fefffea2860ee1e, 0x3f11d312098e9dba
	.quad	0x3fefffeb3ebb267b, 0x3f10f9e1b4dd36df
	.quad	0x3fefffec47d19457, 0x3f102a8673a94692
	.quad	0x3fefffed443e2787, 0x3f0ec929a665b449
	.quad	0x3fefffee34943b15, 0x3f0d4f4b4c8e09ed
	.quad	0x3fefffef1960d85d, 0x3f0be6abbb10a5aa
	.quad	0x3fefffeff32af7af, 0x3f0a8e8cc1fadef6
	.quad	0x3feffff0c273bea2, 0x3f094637d5bacfdb
	.quad	0x3feffff187b6bc0e, 0x3f080cfdc72220cf
	.quad	0x3feffff2436a21dc, 0x3f06e2367dc27f95
	.quad	0x3feffff2f5fefcaa, 0x3f05c540b4936fd2
	.quad	0x3feffff39fe16963, 0x3f04b581b8d170fc
	.quad	0x3feffff44178c8d2, 0x3f03b2652b06c2b2
	.quad	0x3feffff4db27f146, 0x3f02bb5cc22e5db6
	.quad	0x3feffff56d4d5e5e, 0x3f01cfe010e2052d
	.quad	0x3feffff5f8435efc, 0x3f00ef6c4c84a0fe
	.quad	0x3feffff67c604180, 0x3f001984165a5f36
	.quad	0x3feffff6f9f67e55, 0x3efe9b5e8d00ce77
	.quad	0x3feffff77154e0d6, 0x3efd16f5716c6c1a
	.quad	0x3feffff7e2c6aea2, 0x3efba4f035d60e03
	.quad	0x3feffff84e93cd75, 0x3efa447b7b03f045
	.quad	0x3feffff8b500e77c, 0x3ef8f4ccca7fc90d
	.quad	0x3feffff9164f8e46, 0x3ef7b5223dac7336
	.quad	0x3feffff972be5c59, 0x3ef684c227fcacef
	.quad	0x3feffff9ca891572, 0x3ef562fac4329b48
	.quad	0x3feffffa1de8c582, 0x3ef44f21e49054f2
	.quad	0x3feffffa6d13de73, 0x3ef34894a5e24657
	.quad	0x3feffffab83e54b8, 0x3ef24eb7254ccf83
	.quad	0x3feffffaff99bac4, 0x3ef160f438c70913
	.quad	0x3feffffb43555b5f, 0x3ef07ebd2a2d2844
	.quad	0x3feffffb839e52f3, 0x3eef4f12e9ab070a
	.quad	0x3feffffbc09fa7cd, 0x3eedb5ad0b27805c
	.quad	0x3feffffbfa82616b, 0x3eec304efa2c6f4e
	.quad	0x3feffffc316d9ed0, 0x3eeabe09e9144b5e
	.quad	0x3feffffc6586abf6, 0x3ee95df988e76644
	.quad	0x3feffffc96f1165e, 0x3ee80f439b4ee04b
	.quad	0x3feffffcc5cec0c1, 0x3ee6d11788a69c64
	.quad	0x3feffffcf23ff5fc, 0x3ee5a2adfa0b4bc4
	.quad	0x3feffffd1c637b2b, 0x3ee4834877429b8f
	.quad	0x3feffffd4456a10d, 0x3ee37231085c7d9a
	.quad	0x3feffffd6a3554a1, 0x3ee26eb9daed6f7e
	.quad	0x3feffffd8e1a2f22, 0x3ee1783ceac28910
	.quad	0x3feffffdb01e8546, 0x3ee08e1badf0fced
	.quad	0x3feffffdd05a75ea, 0x3edf5f7d88472604
	.quad	0x3feffffdeee4f810, 0x3eddb92b5212fb8d
	.quad	0x3feffffe0bd3e852, 0x3edc282cd3957eda
	.quad	0x3feffffe273c15b7, 0x3edaab7abace48dc
	.quad	0x3feffffe41314e06, 0x3ed94219bfcb4928
	.quad	0x3feffffe59c6698b, 0x3ed7eb1a2075864e
	.quad	0x3feffffe710d565e, 0x3ed6a597219a93da
	.quad	0x3feffffe8717232d, 0x3ed570b69502f313
	.quad	0x3feffffe9bf4098c, 0x3ed44ba864670882
	.quad	0x3feffffeafb377d5, 0x3ed335a62115bce2
	.quad	0x3feffffec2641a9e, 0x3ed22df298214423
	.quad	0x3feffffed413e5b7, 0x3ed133d96ae7e0dd
	.quad	0x3feffffee4d01cd6, 0x3ed046aeabcfcdec
	.quad	0x3feffffef4a55bd4, 0x3ececb9cfe1d8642
	.quad	0x3fefffff039f9e8f, 0x3ecd21397ead99cb
	.quad	0x3fefffff11ca4876, 0x3ecb8d094c86d374
	.quad	0x3fefffff1f302bc1, 0x3eca0df0f0c626dc
	.quad	0x3fefffff2bdb904d, 0x3ec8a2e269750a39
	.quad	0x3fefffff37d63a36, 0x3ec74adc8f4064d3
	.quad	0x3fefffff43297019, 0x3ec604ea819f007c
	.quad	0x3fefffff4dde0118, 0x3ec4d0231928c6f9
	.quad	0x3fefffff57fc4a95, 0x3ec3aba85fe22e20
	.quad	0x3fefffff618c3da6, 0x3ec296a70f414053
	.quad	0x3fefffff6a956450, 0x3ec1905613b3abf2
	.quad	0x3fefffff731ee681, 0x3ec097f6156f32c5
	.quad	0x3fefffff7b2f8ed6, 0x3ebf59a20caf6695
	.quad	0x3fefffff82cdcf1b, 0x3ebd9c73698fb1dc
	.quad	0x3fefffff89ffc4aa, 0x3ebbf716c6168bae
	.quad	0x3fefffff90cb3c81, 0x3eba6852c6b58392
	.quad	0x3fefffff9735b73b, 0x3eb8eefd70594a89
	.quad	0x3fefffff9d446ccc, 0x3eb789fb715aae95
	.quad	0x3fefffffa2fc5015, 0x3eb6383f726a8e04
	.quad	0x3fefffffa8621251, 0x3eb4f8c96f26a26a
	.quad	0x3fefffffad7a2652, 0x3eb3caa61607f920
	.quad	0x3fefffffb248c39d, 0x3eb2acee2f5ecdb8
	.quad	0x3fefffffb6d1e95d, 0x3eb19ec60b1242ed
	.quad	0x3fefffffbb196132, 0x3eb09f5cf4dd2877
	.quad	0x3fefffffbf22c1e2, 0x3eaf5bd95d8730d8
	.quad	0x3fefffffc2f171e3, 0x3ead9371e2ff7c35
	.quad	0x3fefffffc688a9cf, 0x3eabe41de54d155a
	.quad	0x3fefffffc9eb76ac, 0x3eaa4c89e08ef4f3
	.quad	0x3fefffffcd1cbc28, 0x3ea8cb738399b12c
	.quad	0x3fefffffd01f36af, 0x3ea75fa8dbc84bec
	.quad	0x3fefffffd2f57d68, 0x3ea608078a70dcbc
	.quad	0x3fefffffd5a2041f, 0x3ea4c37c0394d094
	.quad	0x3fefffffd8271d12, 0x3ea39100d5687bfe
	.quad	0x3fefffffda86faa9, 0x3ea26f9df8519bd7
	.quad	0x3fefffffdcc3b117, 0x3ea15e6827001f18
	.quad	0x3fefffffdedf37ed, 0x3ea05c803e4831c1
	.quad	0x3fefffffe0db6b91, 0x3e9ed22548cffd35
	.quad	0x3fefffffe2ba0ea5, 0x3e9d06ad6ecdf971
	.quad	0x3fefffffe47ccb60, 0x3e9b551c847fbc96
	.quad	0x3fefffffe62534d4, 0x3e99bc09f112b494
	.quad	0x3fefffffe7b4c81e, 0x3e983a1ff0aa239d
	.quad	0x3fefffffe92ced93, 0x3e96ce1aa3fd7bdd
	.quad	0x3fefffffea8ef9cf, 0x3e9576c72b514859
	.quad	0x3fefffffebdc2ec6, 0x3e943302cc4a0da8
	.quad	0x3fefffffed15bcba, 0x3e9301ba221dc9bb
	.quad	0x3fefffffee3cc32c, 0x3e91e1e857adc568
	.quad	0x3fefffffef5251c2, 0x3e90d2966b1746f7
	.quad	0x3feffffff0576917, 0x3e8fa5b4f49cc6b2
	.quad	0x3feffffff14cfb92, 0x3e8dc3ae30b55c16
	.quad	0x3feffffff233ee1d, 0x3e8bfd7555a3bd68
	.quad	0x3feffffff30d18e8, 0x3e8a517d9e61628a
	.quad	0x3feffffff3d9480f, 0x3e88be4f8f6c951f
	.quad	0x3feffffff4993c46, 0x3e874287ded49339
	.quad	0x3feffffff54dab72, 0x3e85dcd669f2cd34
	.quad	0x3feffffff5f74141, 0x3e848bfd38302871
	.quad	0x3feffffff6969fb8, 0x3e834ecf8a3c124a
	.quad	0x3feffffff72c5fb6, 0x3e822430f521cbcf
	.quad	0x3feffffff7b91176, 0x3e810b1488aeb235
	.quad	0x3feffffff83d3d07, 0x3e80027c00a263a6
	.quad	0x3feffffff8b962be, 0x3e7e12ee004efc37
	.quad	0x3feffffff92dfba2, 0x3e7c3e44ae32b16b
	.quad	0x3feffffff99b79d2, 0x3e7a854ea14102a8
	.quad	0x3feffffffa0248e8, 0x3e78e6761569f45d
	.quad	0x3feffffffa62ce54, 0x3e77603bac345f65
	.quad	0x3feffffffabd69b4, 0x3e75f1353cdad001
	.quad	0x3feffffffb127525, 0x3e74980cb3c80949
	.quad	0x3feffffffb624592, 0x3e73537f00b6ad4d
	.quad	0x3feffffffbad2aff, 0x3e72225b12bffc68
	.quad	0x3feffffffbf370cd, 0x3e710380e1adb7e9
	.quad	0x3feffffffc355dfd, 0x3e6febc107d5efaa
	.quad	0x3feffffffc733572, 0x3e6df0f2a0ee6947
	.quad	0x3feffffffcad3626, 0x3e6c14b2188bcee4
	.quad	0x3feffffffce39b67, 0x3e6a553644f7f07d
	.quad	0x3feffffffd169d0c, 0x3e68b0cfce0579e0
	.quad	0x3feffffffd466fa5, 0x3e6725e7c5dd20f7
	.quad	0x3feffffffd7344aa, 0x3e65b2fe547a1340
	.quad	0x3feffffffd9d4aab, 0x3e6456a974e92e93
	.quad	0x3feffffffdc4ad7a, 0x3e630f93c3699078
	.quad	0x3feffffffde9964e, 0x3e61dc7b5b978cf8
	.quad	0x3feffffffe0c2bf0, 0x3e60bc30c5d52f15
	.quad	0x3feffffffe2c92db, 0x3e5f5b2be65a0c7f
	.quad	0x3feffffffe4aed5e, 0x3e5d5f3a8dea7357
	.quad	0x3feffffffe675bbd, 0x3e5b82915b03515b
	.quad	0x3feffffffe81fc4e, 0x3e59c3517e789488
	.quad	0x3feffffffe9aeb97, 0x3e581fb7df06136e
	.quad	0x3feffffffeb24467, 0x3e56961b8d641d06
	.quad	0x3feffffffec81ff2, 0x3e5524ec4d916cae
	.quad	0x3feffffffedc95e7, 0x3e53cab1343d18d1
	.quad	0x3feffffffeefbc85, 0x3e52860757487a01
	.quad	0x3fefffffff01a8b6, 0x3e5155a09065d4f7
	.quad	0x3fefffffff126e1e, 0x3e50384250e4c9fc
	.quad	0x3fefffffff221f30, 0x3e4e59890b926c78
	.quad	0x3fefffffff30cd3f, 0x3e4c642116a8a9e3
	.quad	0x3fefffffff3e8892, 0x3e4a8e405e651ab6
	.quad	0x3fefffffff4b606f, 0x3e48d5f98114f872
	.quad	0x3fefffffff57632d, 0x3e47397c5a66e307
	.quad	0x3fefffffff629e44, 0x3e45b71456c5a4c4
	.quad	0x3fefffffff6d1e56, 0x3e444d26de513197
	.quad	0x3fefffffff76ef3f, 0x3e42fa31d6371537
	.quad	0x3fefffffff801c1f, 0x3e41bcca373b7b43
	.quad	0x3fefffffff88af67, 0x3e40939ab853339f
	.quad	0x3fefffffff90b2e3, 0x3e3efac5187b2863
	.quad	0x3fefffffff982fc1, 0x3e3cf1e86235d0e7
	.quad	0x3fefffffff9f2e9f, 0x3e3b0a68a2128bab
	.quad	0x3fefffffffa5b790, 0x3e39423165bc4444
	.quad	0x3fefffffffabd229, 0x3e37974e743dea3d
	.quad	0x3fefffffffb18582, 0x3e3607e9eacd1050
	.quad	0x3fefffffffb6d844, 0x3e34924a74dec729
	.quad	0x3fefffffffbbd0aa, 0x3e3334d19e0c2160
	.quad	0x3fefffffffc0748f, 0x3e31edfa3c5f5cca
	.quad	0x3fefffffffc4c96c, 0x3e30bc56f1b54701
	.quad	0x3fefffffffc8d462, 0x3e2f3d2185e047d9
	.quad	0x3fefffffffcc9a41, 0x3e2d26cb87945e87
	.quad	0x3fefffffffd01f89, 0x3e2b334fac4b9f99
	.quad	0x3fefffffffd36871, 0x3e296076f7918d1c
	.quad	0x3fefffffffd678ed, 0x3e27ac2d72fc2c63
	.quad	0x3fefffffffd954ae, 0x3e2614801550319e
	.quad	0x3fefffffffdbff2a, 0x3e24979ac8b28927
	.quad	0x3fefffffffde7ba0, 0x3e2333c68e2d0548
	.quad	0x3fefffffffe0cd16, 0x3e21e767bce37dd7
	.quad	0x3fefffffffe2f664, 0x3e20b0fc5b6d05a0
	.quad	0x3fefffffffe4fa30, 0x3e1f1e3523b41d7d
	.quad	0x3fefffffffe6daf7, 0x3e1d00de6608effe
	.quad	0x3fefffffffe89b0c, 0x3e1b0778b7b3301b
	.quad	0x3fefffffffea3c9a, 0x3e192fb04ec0f6cf
	.quad	0x3fefffffffebc1a9, 0x3e177756ec9f78fa
	.quad	0x3fefffffffed2c21, 0x3e15dc61922d5a06
	.quad	0x3fefffffffee7dc8, 0x3e145ce65699ff6d
	.quad	0x3fefffffffefb847, 0x3e12f71a5f159970
	.quad	0x3feffffffff0dd2b, 0x3e11a94ff571654f
	.quad	0x3feffffffff1ede9, 0x3e1071f4bbea09ec
	.quad	0x3feffffffff2ebda, 0x3e0e9f1ff8ddd774
	.quad	0x3feffffffff3d843, 0x3e0c818223a202c7
	.quad	0x3feffffffff4b453, 0x3e0a887bd2b4404d
	.quad	0x3feffffffff58126, 0x3e08b1a336c5eb6b
	.quad	0x3feffffffff63fc3, 0x3e06fab63324088a
	.quad	0x3feffffffff6f121, 0x3e056197e30205ba
	.quad	0x3feffffffff79626, 0x3e03e44e45301b92
	.quad	0x3feffffffff82fab, 0x3e0281000bfe4c3f
	.quad	0x3feffffffff8be77, 0x3e0135f28f2d50b4
	.quad	0x3feffffffff94346, 0x3e000187dded5975
	.quad	0x3feffffffff9bec8, 0x3dfdc479de0ef001
	.quad	0x3feffffffffa319f, 0x3dfbad4fdad3caa1
	.quad	0x3feffffffffa9c63, 0x3df9baed3ed27ab8
	.quad	0x3feffffffffaffa4, 0x3df7ead9ce4285bb
	.quad	0x3feffffffffb5be5, 0x3df63ac6b4edc88e
	.quad	0x3feffffffffbb1a2, 0x3df4a88be2a6390c
	.quad	0x3feffffffffc014e, 0x3df332259185f1a0
	.quad	0x3feffffffffc4b56, 0x3df1d5b1f3793044
	.quad	0x3feffffffffc901c, 0x3df0916f04b6e18b
	.quad	0x3feffffffffccfff, 0x3deec77101de6926
	.quad	0x3feffffffffd0b56, 0x3dec960bf23153e0
	.quad	0x3feffffffffd4271, 0x3dea8bd20fc65ef7
	.quad	0x3feffffffffd759d, 0x3de8a61745ec7d1d
	.quad	0x3feffffffffda520, 0x3de6e25d0e756261
	.quad	0x3feffffffffdd13c, 0x3de53e4f7d1666cb
	.quad	0x3feffffffffdfa2d, 0x3de3b7c27a7ddb0e
	.quad	0x3feffffffffe202d, 0x3de24caf2c32af14
	.quad	0x3feffffffffe4371, 0x3de0fb3186804d0f
	.quad	0x3feffffffffe642a, 0x3ddf830c0bb41fd7
	.quad	0x3feffffffffe8286, 0x3ddd3c0f1a91c846
	.quad	0x3feffffffffe9eb0, 0x3ddb1e5acf351d87
	.quad	0x3feffffffffeb8d0, 0x3dd92712d259ce66
	.quad	0x3feffffffffed10a, 0x3dd7538c60a04476
	.quad	0x3feffffffffee782, 0x3dd5a14b04b47879
	.quad	0x3feffffffffefc57, 0x3dd40dfd87456f4c
	.quad	0x3fefffffffff0fa7, 0x3dd2977b1172b9d5
	.quad	0x3fefffffffff218f, 0x3dd13bc07e891491
	.quad	0x3fefffffffff3227, 0x3dcff1dbb4300811
	.quad	0x3fefffffffff4188, 0x3dcd9a880f306bd8
	.quad	0x3fefffffffff4fc9, 0x3dcb6e45220b55e0
	.quad	0x3fefffffffff5cfd, 0x3dc96a0b33f2c4da
	.quad	0x3fefffffffff6939, 0x3dc78b07e9e924ac
	.quad	0x3fefffffffff748e, 0x3dc5ce9ab1670dd2
	.quad	0x3fefffffffff7f0d, 0x3dc4325167006bb0
	.quad	0x3fefffffffff88c5, 0x3dc2b3e53538ff3f
	.quad	0x3fefffffffff91c6, 0x3dc15137a7f44864
	.quad	0x3fefffffffff9a1b, 0x3dc0084ff125639d
	.quad	0x3fefffffffffa1d2, 0x3dbdaeb0b7311ec7
	.quad	0x3fefffffffffa8f6, 0x3dbb7937d1c40c53
	.quad	0x3fefffffffffaf92, 0x3db96d082f59ab06
	.quad	0x3fefffffffffb5b0, 0x3db7872d9fa10aad
	.quad	0x3fefffffffffbb58, 0x3db5c4e8e37bc7d0
	.quad	0x3fefffffffffc095, 0x3db423ac0df49a40
	.quad	0x3fefffffffffc56d, 0x3db2a117230ad284
	.quad	0x3fefffffffffc9e8, 0x3db13af4f04f9998
	.quad	0x3fefffffffffce0d, 0x3dafde703724e560
	.quad	0x3fefffffffffd1e1, 0x3dad77f0c82e7641
	.quad	0x3fefffffffffd56c, 0x3dab3ee02611d7dd
	.quad	0x3fefffffffffd8b3, 0x3da92ff33023d5bd
	.quad	0x3fefffffffffdbba, 0x3da7481a9e69f53f
	.quad	0x3fefffffffffde86, 0x3da5847eda620959
	.quad	0x3fefffffffffe11d, 0x3da3e27c1fcc74bd
	.quad	0x3fefffffffffe380, 0x3da25f9ee0b923dc
	.quad	0x3fefffffffffe5b6, 0x3da0f9a068653200
	.quad	0x3fefffffffffe7c0, 0x3d9f5cc7718082b0
	.quad	0x3fefffffffffe9a2, 0x3d9cf7e53d6a2ca5
	.quad	0x3fefffffffffeb60, 0x3d9ac0f5f3229372
	.quad	0x3fefffffffffecfb, 0x3d98b498644847ea
	.quad	0x3fefffffffffee77, 0x3d96cfa9bcca59dc
	.quad	0x3fefffffffffefd6, 0x3d950f411d4fd2cd
	.quad	0x3feffffffffff11a, 0x3d9370ab8327af5e
	.quad	0x3feffffffffff245, 0x3d91f167f88c6b6e
	.quad	0x3feffffffffff359, 0x3d908f24085d4597
	.quad	0x3feffffffffff457, 0x3d8e8f70e181d61a
	.quad	0x3feffffffffff542, 0x3d8c324c20e337dc
	.quad	0x3feffffffffff61b, 0x3d8a03261574b54e
	.quad	0x3feffffffffff6e3, 0x3d87fe903cdf5855
	.quad	0x3feffffffffff79b, 0x3d86215c58da3450
	.quad	0x3feffffffffff845, 0x3d846897d4b69fc6
	.quad	0x3feffffffffff8e2, 0x3d82d1877d731b7b
	.quad	0x3feffffffffff973, 0x3d8159a386b11517
	.quad	0x3feffffffffff9f8, 0x3d7ffd27ae9393ce
	.quad	0x3feffffffffffa73, 0x3d7d7c593130dd0b
	.quad	0x3feffffffffffae4, 0x3d7b2cd607c79bcf
	.quad	0x3feffffffffffb4c, 0x3d790ae4d3405651
	.quad	0x3feffffffffffbad, 0x3d771312dd1759e2
	.quad	0x3feffffffffffc05, 0x3d75422ef5d8949d
	.quad	0x3feffffffffffc57, 0x3d739544b0ecc957
	.quad	0x3feffffffffffca2, 0x3d720997f73e73dd
	.quad	0x3feffffffffffce7, 0x3d709ca0eaacd277
	.quad	0x3feffffffffffd27, 0x3d6e9810295890ec
	.quad	0x3feffffffffffd62, 0x3d6c2b45b5aa4a1d
	.quad	0x3feffffffffffd98, 0x3d69eee068fa7596
	.quad	0x3feffffffffffdca, 0x3d67df2b399c10a8
	.quad	0x3feffffffffffdf8, 0x3d65f8b87a31bd85
	.quad	0x3feffffffffffe22, 0x3d64385c96e9a2d9
	.quad	0x3feffffffffffe49, 0x3d629b2933ef4cbc
	.quad	0x3feffffffffffe6c, 0x3d611e68a6378f8a
	.quad	0x3feffffffffffe8d, 0x3d5f7f338086a86b
	.quad	0x3feffffffffffeab, 0x3d5cf8d7d9ce040a
	.quad	0x3feffffffffffec7, 0x3d5aa577251ae485
	.quad	0x3feffffffffffee1, 0x3d58811d739efb5f
	.quad	0x3feffffffffffef8, 0x3d568823e52970be
	.quad	0x3fefffffffffff0e, 0x3d54b72ae68e8b4c
	.quad	0x3fefffffffffff22, 0x3d530b14dbe876bc
	.quad	0x3fefffffffffff34, 0x3d5181012ef86610
	.quad	0x3fefffffffffff45, 0x3d501647ba798745
	.quad	0x3fefffffffffff54, 0x3d4d90e917701675
	.quad	0x3fefffffffffff62, 0x3d4b2a87e86d0c8a
	.quad	0x3fefffffffffff6f, 0x3d48f53dcb377293
	.quad	0x3fefffffffffff7b, 0x3d46ed2f2515e933
	.quad	0x3fefffffffffff86, 0x3d450ecc9ed47f19
	.quad	0x3fefffffffffff90, 0x3d4356cd5ce7799e
	.quad	0x3fefffffffffff9a, 0x3d41c229a587ab78
	.quad	0x3fefffffffffffa2, 0x3d404e15ecc7f3f6
	.quad	0x3fefffffffffffaa, 0x3d3deffc7e6a6017
	.quad	0x3fefffffffffffb1, 0x3d3b7b040832f310
	.quad	0x3fefffffffffffb8, 0x3d3938e021f36d76
	.quad	0x3fefffffffffffbe, 0x3d37258610b3b233
	.quad	0x3fefffffffffffc3, 0x3d353d3bfc82a909
	.quad	0x3fefffffffffffc8, 0x3d337c92babdc2fd
	.quad	0x3fefffffffffffcd, 0x3d31e06010120f6a
	.quad	0x3fefffffffffffd1, 0x3d3065b9616170d4
	.quad	0x3fefffffffffffd5, 0x3d2e13dd96b3753b
	.quad	0x3fefffffffffffd9, 0x3d2b950d32467392
	.quad	0x3fefffffffffffdc, 0x3d294a72263259a5
	.quad	0x3fefffffffffffdf, 0x3d272fd93e036cdc
	.quad	0x3fefffffffffffe2, 0x3d254164576929ab
	.quad	0x3fefffffffffffe4, 0x3d237b83c521fe96
	.quad	0x3fefffffffffffe7, 0x3d21daf033182e96
	.quad	0x3fefffffffffffe9, 0x3d205ca50205d26a
	.quad	0x3fefffffffffffeb, 0x3d1dfbb6235639fa
	.quad	0x3fefffffffffffed, 0x3d1b7807e294781f
	.quad	0x3fefffffffffffee, 0x3d19298add70a734
	.quad	0x3feffffffffffff0, 0x3d170beaf9c7ffb6
	.quad	0x3feffffffffffff1, 0x3d151b2cd6709222
	.quad	0x3feffffffffffff3, 0x3d1353a6cf7f7fff
	.quad	0x3feffffffffffff4, 0x3d11b1fa8cbe84a7
	.quad	0x3feffffffffffff5, 0x3d10330f0fd69921
	.quad	0x3feffffffffffff6, 0x3d0da81670f96f9b
	.quad	0x3feffffffffffff7, 0x3d0b24a16b4d09aa
	.quad	0x3feffffffffffff7, 0x3d08d6eeb6efdbd6
	.quad	0x3feffffffffffff8, 0x3d06ba91ac734786
	.quad	0x3feffffffffffff9, 0x3d04cb7966770ab5
	.quad	0x3feffffffffffff9, 0x3d0305e9721d0981
	.quad	0x3feffffffffffffa, 0x3d01667311fff70a
	.quad	0x3feffffffffffffb, 0x3cffd3de10d62855
	.quad	0x3feffffffffffffb, 0x3cfd1aefbcd48d0c
	.quad	0x3feffffffffffffb, 0x3cfa9cc93c25aca9
	.quad	0x3feffffffffffffc, 0x3cf85487ee3ea735
	.quad	0x3feffffffffffffc, 0x3cf63daf8b4b1e0c
	.quad	0x3feffffffffffffd, 0x3cf45421e69a6ca1
	.quad	0x3feffffffffffffd, 0x3cf294175802d99a
	.quad	0x3feffffffffffffd, 0x3cf0fa17bf41068f
	.quad	0x3feffffffffffffd, 0x3cef05e82aae2bb9
	.quad	0x3feffffffffffffe, 0x3cec578101b29058
	.quad	0x3feffffffffffffe, 0x3ce9e39dc5dd2f7c
	.quad	0x3feffffffffffffe, 0x3ce7a553a728bbf2
	.quad	0x3feffffffffffffe, 0x3ce5982008db1304
	.quad	0x3feffffffffffffe, 0x3ce3b7e00422e51b
	.quad	0x3feffffffffffffe, 0x3ce200c898d9ee3e
	.quad	0x3fefffffffffffff, 0x3ce06f5f7eb65a56
	.quad	0x3fefffffffffffff, 0x3cde00e9148a1d25
	.quad	0x3fefffffffffffff, 0x3cdb623734024e92
	.quad	0x3fefffffffffffff, 0x3cd8fd4e01891bf8
	.quad	0x3fefffffffffffff, 0x3cd6cd44c7470d89
	.quad	0x3fefffffffffffff, 0x3cd4cd9c04158cd7
	.quad	0x3fefffffffffffff, 0x3cd2fa34bf5c8344
	.quad	0x3fefffffffffffff, 0x3cd14f4890ff2461
	.quad	0x3fefffffffffffff, 0x3ccf92c49dfa4df5
	.quad	0x3fefffffffffffff, 0x3ccccaaea71ab0df
	.quad	0x3fefffffffffffff, 0x3cca40829f001197
	.quad	0x3ff0000000000000, 0x3cc7eef13b59e96c
	.quad	0x3ff0000000000000, 0x3cc5d11e1a252bf5
	.quad	0x3ff0000000000000, 0x3cc3e296303b2297
	.quad	0x3ff0000000000000, 0x3cc21f47009f43ce
	.quad	0x3ff0000000000000, 0x3cc083768c5e4542
	.quad	0x3ff0000000000000, 0x3cbe1777d831265f
	.quad	0x3ff0000000000000, 0x3cbb69f10b0191b5
	.quad	0x3ff0000000000000, 0x3cb8f8a3a05b5b53
	.quad	0x3ff0000000000000, 0x3cb6be573c40c8e7
	.quad	0x3ff0000000000000, 0x3cb4b645ba991fdb
	.align	64
	.quad	0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff /* _AbsMask */
	.align	64
	.quad	0x4017f80000000000, 0x4017f80000000000, 0x4017f80000000000, 0x4017f80000000000, 0x4017f80000000000, 0x4017f80000000000, 0x4017f80000000000, 0x4017f80000000000 /* _MaxThreshold = 6.0 - 1.0/128.0 */
	.align	64
	.quad	0x42c0000000000000, 0x42c0000000000000, 0x42c0000000000000, 0x42c0000000000000, 0x42c0000000000000, 0x42c0000000000000, 0x42c0000000000000, 0x42c0000000000000 /* SRound */
	.align	64
	.quad	0x2ff0000000000000, 0x2ff0000000000000, 0x2ff0000000000000, 0x2ff0000000000000, 0x2ff0000000000000, 0x2ff0000000000000, 0x2ff0000000000000, 0x2ff0000000000000 /* _U2THreshold */
	.align	64
	.quad	0xbfa6c16db05bdea5, 0xbfa6c16db05bdea5, 0xbfa6c16db05bdea5, 0xbfa6c16db05bdea5, 0xbfa6c16db05bdea5, 0xbfa6c16db05bdea5, 0xbfa6c16db05bdea5, 0xbfa6c16db05bdea5 /* _poly_1_0 */
	.align	64
	.quad	0x3fc1111235a363b1, 0x3fc1111235a363b1, 0x3fc1111235a363b1, 0x3fc1111235a363b1, 0x3fc1111235a363b1, 0x3fc1111235a363b1, 0x3fc1111235a363b1, 0x3fc1111235a363b1 /* _poly_1_1 */
	.align	64
	.quad	0x3fcc71ca1c71eb57, 0x3fcc71ca1c71eb57, 0x3fcc71ca1c71eb57, 0x3fcc71ca1c71eb57, 0x3fcc71ca1c71eb57, 0x3fcc71ca1c71eb57, 0x3fcc71ca1c71eb57, 0x3fcc71ca1c71eb57 /* _poly_3_0 */
	.align	64
	.quad	0xbfd9999c2be2dda8, 0xbfd9999c2be2dda8, 0xbfd9999c2be2dda8, 0xbfd9999c2be2dda8, 0xbfd9999c2be2dda8, 0xbfd9999c2be2dda8, 0xbfd9999c2be2dda8, 0xbfd9999c2be2dda8 /* _poly_3_1 */
	.align	64
	.quad	0xbfc5555800001B4F, 0xbfc5555800001B4F, 0xbfc5555800001B4F, 0xbfc5555800001B4F, 0xbfc5555800001B4F, 0xbfc5555800001B4F, 0xbfc5555800001B4F, 0xbfc5555800001B4F /* _poly_5_0 */
	.align	64
	.quad	0x3fb9999E2BE2F122, 0x3fb9999E2BE2F122, 0x3fb9999E2BE2F122, 0x3fb9999E2BE2F122, 0x3fb9999E2BE2F122, 0x3fb9999E2BE2F122, 0x3fb9999E2BE2F122, 0x3fb9999E2BE2F122 /* _poly_5_1 */
	.align	64
	.quad	0xbfd55555555547f6, 0xbfd55555555547f6, 0xbfd55555555547f6, 0xbfd55555555547f6, 0xbfd55555555547f6, 0xbfd55555555547f6, 0xbfd55555555547f6, 0xbfd55555555547f6 /* _poly_1_2 */
	.align	64
	.quad	0x3fdfffffffffd4cd, 0x3fdfffffffffd4cd, 0x3fdfffffffffd4cd, 0x3fdfffffffffd4cd, 0x3fdfffffffffd4cd, 0x3fdfffffffffd4cd, 0x3fdfffffffffd4cd, 0x3fdfffffffffd4cd /* _poly_3_2 */
	.align	64
	.quad	0x3fe5555555554b0c, 0x3fe5555555554b0c, 0x3fe5555555554b0c, 0x3fe5555555554b0c, 0x3fe5555555554b0c, 0x3fe5555555554b0c, 0x3fe5555555554b0c, 0x3fe5555555554b0c /* _poly_1_3 */
	.align	64
	.quad	0xbfd5555555555555, 0xbfd5555555555555, 0xbfd5555555555555, 0xbfd5555555555555, 0xbfd5555555555555, 0xbfd5555555555555, 0xbfd5555555555555, 0xbfd5555555555555 /* _poly_3_3 */
	.align	64
	.quad	0x00000000ffffffff, 0x00000000ffffffff, 0x00000000ffffffff, 0x00000000ffffffff, 0x00000000ffffffff, 0x00000000ffffffff, 0x00000000ffffffff, 0x00000000ffffffff /* _Mask32 */
	.align	64
	.type	__svml_derf_data_internal, @object
	.size	__svml_derf_data_internal, .-__svml_derf_data_internal

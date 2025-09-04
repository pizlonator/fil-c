#name: Test of SVE2.1 and SME2.1 non-widening BFloat16 instructions.
#as: -march=armv9.4-a+b16b16
#objdump: -dr
#xfail: *-*-*

[^:]+:     file format .*


[^:]+:

[^:]+:
.*:	65008200 	bfadd	z0.h, p0\/m, z0.h, z16.h
.*:	65008501 	bfadd	z1.h, p1\/m, z1.h, z8.h
.*:	65008882 	bfadd	z2.h, p2\/m, z2.h, z4.h
.*:	65009044 	bfadd	z4.h, p4\/m, z4.h, z2.h
.*:	65009828 	bfadd	z8.h, p6\/m, z8.h, z1.h
.*:	65009c10 	bfadd	z16.h, p7\/m, z16.h, z0.h
.*:	65068200 	bfmax	z0.h, p0\/m, z0.h, z16.h
.*:	65068501 	bfmax	z1.h, p1\/m, z1.h, z8.h
.*:	65068882 	bfmax	z2.h, p2\/m, z2.h, z4.h
.*:	65069044 	bfmax	z4.h, p4\/m, z4.h, z2.h
.*:	65069828 	bfmax	z8.h, p6\/m, z8.h, z1.h
.*:	65069c10 	bfmax	z16.h, p7\/m, z16.h, z0.h
.*:	65048200 	bfmaxnm	z0.h, p0\/m, z0.h, z16.h
.*:	65048501 	bfmaxnm	z1.h, p1\/m, z1.h, z8.h
.*:	65048882 	bfmaxnm	z2.h, p2\/m, z2.h, z4.h
.*:	65049044 	bfmaxnm	z4.h, p4\/m, z4.h, z2.h
.*:	65049828 	bfmaxnm	z8.h, p6\/m, z8.h, z1.h
.*:	65049c10 	bfmaxnm	z16.h, p7\/m, z16.h, z0.h
.*:	65078200 	bfmin	z0.h, p0\/m, z0.h, z16.h
.*:	65078501 	bfmin	z1.h, p1\/m, z1.h, z8.h
.*:	65078882 	bfmin	z2.h, p2\/m, z2.h, z4.h
.*:	65079044 	bfmin	z4.h, p4\/m, z4.h, z2.h
.*:	65079828 	bfmin	z8.h, p6\/m, z8.h, z1.h
.*:	65079c10 	bfmin	z16.h, p7\/m, z16.h, z0.h
.*:	65058200 	bfminnm	z0.h, p0\/m, z0.h, z16.h
.*:	65058501 	bfminnm	z1.h, p1\/m, z1.h, z8.h
.*:	65058882 	bfminnm	z2.h, p2\/m, z2.h, z4.h
.*:	65059044 	bfminnm	z4.h, p4\/m, z4.h, z2.h
.*:	65059828 	bfminnm	z8.h, p6\/m, z8.h, z1.h
.*:	65059c10 	bfminnm	z16.h, p7\/m, z16.h, z0.h
.*:	65100080 	bfadd	z0.h, z4.h, z16.h
.*:	65080101 	bfadd	z1.h, z8.h, z8.h
.*:	65040182 	bfadd	z2.h, z12.h, z4.h
.*:	65020204 	bfadd	z4.h, z16.h, z2.h
.*:	65010288 	bfadd	z8.h, z20.h, z1.h
.*:	65000310 	bfadd	z16.h, z24.h, z0.h
.*:	64302480 	bfclamp	z0.h, z4.h, z16.h
.*:	64282501 	bfclamp	z1.h, z8.h, z8.h
.*:	64242582 	bfclamp	z2.h, z12.h, z4.h
.*:	64222604 	bfclamp	z4.h, z16.h, z2.h
.*:	64212688 	bfclamp	z8.h, z20.h, z1.h
.*:	64202710 	bfclamp	z16.h, z24.h, z0.h
.*:	65300080 	bfmla	z0.h, p0\/m, z4.h, z16.h
.*:	65280501 	bfmla	z1.h, p1\/m, z8.h, z8.h
.*:	65240982 	bfmla	z2.h, p2\/m, z12.h, z4.h
.*:	65221204 	bfmla	z4.h, p4\/m, z16.h, z2.h
.*:	65211a88 	bfmla	z8.h, p6\/m, z20.h, z1.h
.*:	65201f10 	bfmla	z16.h, p7\/m, z24.h, z0.h
.*:	647e0a00 	bfmla	z0.h, z16.h, z6.h\[7\]
.*:	64750901 	bfmla	z1.h, z8.h, z5.h\[6\]
.*:	646409c2 	bfmla	z2.h, z14.h, z4.h\[4\]
.*:	64320aa4 	bfmla	z4.h, z21.h, z2.h\[2\]
.*:	64290988 	bfmla	z8.h, z12.h, z1.h\[1\]
.*:	64200950 	bfmla	z16.h, z10.h, z0.h\[0\]
.*:	65302080 	bfmls	z0.h, p0\/m, z4.h, z16.h
.*:	65282501 	bfmls	z1.h, p1\/m, z8.h, z8.h
.*:	65242982 	bfmls	z2.h, p2\/m, z12.h, z4.h
.*:	65223204 	bfmls	z4.h, p4\/m, z16.h, z2.h
.*:	65213a88 	bfmls	z8.h, p6\/m, z20.h, z1.h
.*:	65203f10 	bfmls	z16.h, p7\/m, z24.h, z0.h
.*:	647e0e00 	bfmls	z0.h, z16.h, z6.h\[7\]
.*:	64750d01 	bfmls	z1.h, z8.h, z5.h\[6\]
.*:	64640dc2 	bfmls	z2.h, z14.h, z4.h\[4\]
.*:	64320ea4 	bfmls	z4.h, z21.h, z2.h\[2\]
.*:	64290d88 	bfmls	z8.h, z12.h, z1.h\[1\]
.*:	64200d50 	bfmls	z16.h, z10.h, z0.h\[0\]
.*:	65028200 	bfmul	z0.h, p0\/m, z0.h, z16.h
.*:	65028501 	bfmul	z1.h, p1\/m, z1.h, z8.h
.*:	65028882 	bfmul	z2.h, p2\/m, z2.h, z4.h
.*:	65029044 	bfmul	z4.h, p4\/m, z4.h, z2.h
.*:	65029828 	bfmul	z8.h, p6\/m, z8.h, z1.h
.*:	65029c10 	bfmul	z16.h, p7\/m, z16.h, z0.h
.*:	65100880 	bfmul	z0.h, z4.h, z16.h
.*:	65080901 	bfmul	z1.h, z8.h, z8.h
.*:	65040982 	bfmul	z2.h, z12.h, z4.h
.*:	65020a04 	bfmul	z4.h, z16.h, z2.h
.*:	65010a88 	bfmul	z8.h, z20.h, z1.h
.*:	65000b10 	bfmul	z16.h, z24.h, z0.h
.*:	647e2a00 	bfmul	z0.h, z16.h, z6.h\[7\]
.*:	64752901 	bfmul	z1.h, z8.h, z5.h\[6\]
.*:	646429c2 	bfmul	z2.h, z14.h, z4.h\[4\]
.*:	64322aa4 	bfmul	z4.h, z21.h, z2.h\[2\]
.*:	64292988 	bfmul	z8.h, z12.h, z1.h\[1\]
.*:	64202950 	bfmul	z16.h, z10.h, z0.h\[0\]
.*:	65018200 	bfsub	z0.h, p0\/m, z0.h, z16.h
.*:	65018501 	bfsub	z1.h, p1\/m, z1.h, z8.h
.*:	65018882 	bfsub	z2.h, p2\/m, z2.h, z4.h
.*:	65019044 	bfsub	z4.h, p4\/m, z4.h, z2.h
.*:	65019828 	bfsub	z8.h, p6\/m, z8.h, z1.h
.*:	65019c10 	bfsub	z16.h, p7\/m, z16.h, z0.h
.*:	65100480 	bfsub	z0.h, z4.h, z16.h
.*:	65080501 	bfsub	z1.h, z8.h, z8.h
.*:	65040582 	bfsub	z2.h, z12.h, z4.h
.*:	65020604 	bfsub	z4.h, z16.h, z2.h
.*:	65010688 	bfsub	z8.h, z20.h, z1.h
.*:	65000710 	bfsub	z16.h, z24.h, z0.h
.*:	0420bca3 	movprfx	z3, z5
.*:	64302483 	bfclamp	z3.h, z4.h, z16.h
.*:	0420bca3 	movprfx	z3, z5
.*:	647e0a03 	bfmla	z3.h, z16.h, z6.h\[7\]
.*:	0420bca3 	movprfx	z3, z5
.*:	647e0e03 	bfmls	z3.h, z16.h, z6.h\[7\]

#name: Tests for CPA instructions ((M)ADDPT and (M)SUBPT).
#as: -march=armv9.5-a
#as: -march=armv8-a+cpa
#objdump: -dr

[^:]+:     file format .*


[^:]+:

[^:]+:
.*:	9a002000 	addpt	x0, x0, x0
.*:	9a00201f 	addpt	sp, x0, x0
.*:	9a0023e0 	addpt	x0, sp, x0
.*:	9a1f2000 	addpt	x0, x0, xzr
.*:	9a002000 	addpt	x0, x0, x0
.*:	9a003c00 	addpt	x0, x0, x0, lsl #7
.*:	9a1d35a8 	addpt	x8, x13, x29, lsl #5

.*:	da002000 	subpt	x0, x0, x0
.*:	da00201f 	subpt	sp, x0, x0
.*:	da0023e0 	subpt	x0, sp, x0
.*:	da1f2000 	subpt	x0, x0, xzr
.*:	da002000 	subpt	x0, x0, x0
.*:	da003c00 	subpt	x0, x0, x0, lsl #7
.*:	da162941 	subpt	x1, x10, x22, lsl #2

.*:	9b600000 	maddpt	x0, x0, x0, x0
.*:	9b60001f 	maddpt	xzr, x0, x0, x0
.*:	9b6003e0 	maddpt	x0, xzr, x0, x0
.*:	9b7f0000 	maddpt	x0, x0, xzr, x0
.*:	9b607c00 	maddpt	x0, x0, x0, xzr
.*:	9b617153 	maddpt	x19, x10, x1, x28

.*:	9b608000 	msubpt	x0, x0, x0, x0
.*:	9b60801f 	msubpt	xzr, x0, x0, x0
.*:	9b6083e0 	msubpt	x0, xzr, x0, x0
.*:	9b7f8000 	msubpt	x0, x0, xzr, x0
.*:	9b60fc00 	msubpt	x0, x0, x0, xzr
.*:	9b69d5a4 	msubpt	x4, x13, x9, x21

#name: Tests for CPA+SVE instructions.
#as: -march=armv9.5-a+sve
#as: -march=armv8-a+sve+cpa
#objdump: -dr

[^:]+:     file format .*


[^:]+:

[^:]+:
.*:	04c40000 	addpt	z0.d, p0/m, z0.d, z0.d
.*:	04c4001f 	addpt	z31.d, p0/m, z31.d, z0.d
.*:	04c41c00 	addpt	z0.d, p7/m, z0.d, z0.d
.*:	04c403e0 	addpt	z0.d, p0/m, z0.d, z31.d
.*:	04c41fff 	addpt	z31.d, p7/m, z31.d, z31.d
.*:	04c41229 	addpt	z9.d, p4/m, z9.d, z17.d

.*:	04c50000 	subpt	z0.d, p0/m, z0.d, z0.d
.*:	04c5001f 	subpt	z31.d, p0/m, z31.d, z0.d
.*:	04c51c00 	subpt	z0.d, p7/m, z0.d, z0.d
.*:	04c503e0 	subpt	z0.d, p0/m, z0.d, z31.d
.*:	04c51fff 	subpt	z31.d, p7/m, z31.d, z31.d
.*:	04c50c5c 	subpt	z28.d, p3/m, z28.d, z2.d

.*:	04e00800 	addpt	z0.d, z0.d, z0.d
.*:	04e0081f 	addpt	z31.d, z0.d, z0.d
.*:	04e00be0 	addpt	z0.d, z31.d, z0.d
.*:	04ff0800 	addpt	z0.d, z0.d, z31.d
.*:	04ff0bff 	addpt	z31.d, z31.d, z31.d
.*:	04e90acd 	addpt	z13.d, z22.d, z9.d

.*:	04e00c00 	subpt	z0.d, z0.d, z0.d
.*:	04e00c1f 	subpt	z31.d, z0.d, z0.d
.*:	04e00fe0 	subpt	z0.d, z31.d, z0.d
.*:	04ff0c00 	subpt	z0.d, z0.d, z31.d
.*:	04ff0fff 	subpt	z31.d, z31.d, z31.d
.*:	04f00d3e 	subpt	z30.d, z9.d, z16.d

.*:	44c0d800 	madpt	z0.d, z0.d, z0.d
.*:	44c0d81f 	madpt	z31.d, z0.d, z0.d
.*:	44dfd800 	madpt	z0.d, z31.d, z0.d
.*:	44c0dbe0 	madpt	z0.d, z0.d, z31.d
.*:	44dfdbff 	madpt	z31.d, z31.d, z31.d
.*:	44cbdb44 	madpt	z4.d, z11.d, z26.d

.*:	44c0d000 	mlapt	z0.d, z0.d, z0.d
.*:	44c0d01f 	mlapt	z31.d, z0.d, z0.d
.*:	44c0d3e0 	mlapt	z0.d, z31.d, z0.d
.*:	44dfd000 	mlapt	z0.d, z0.d, z31.d
.*:	44dfd3ff 	mlapt	z31.d, z31.d, z31.d
.*:	44c6d26a 	mlapt	z10.d, z19.d, z6.d

.*:	0420bd41 	movprfx	z1, z10
.*:	04c40041 	addpt	z1.d, p0/m, z1.d, z2.d
.*:	04d13141 	movprfx	z1.d, p4/m, z10.d
.*:	04c41041 	addpt	z1.d, p4/m, z1.d, z2.d

.*:	0420bc01 	movprfx	z1, z0
.*:	04c50041 	subpt	z1.d, p0/m, z1.d, z2.d
.*:	04d13141 	movprfx	z1.d, p4/m, z10.d
.*:	04c51041 	subpt	z1.d, p4/m, z1.d, z2.d

.*:	0420bc01 	movprfx	z1, z0
.*:	44c2d861 	madpt	z1.d, z2.d, z3.d
.*:	0420bc01 	movprfx	z1, z0
.*:	44c3d041 	mlapt	z1.d, z2.d, z3.d

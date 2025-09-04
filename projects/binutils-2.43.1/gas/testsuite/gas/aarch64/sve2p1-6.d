#name: Test of SVE2.1 tblq, tbxq, uzpq[1-2] and zipq[1-2] instruction.
#as: -march=armv9.4-a
#objdump: -dr

[^:]+:     file format .*


[^:]+:

[^:]+:
.*:	4400f800 	tblq	z0.b, {z0.b}, z0.b
.*:	4400f81f 	tblq	z31.b, {z0.b}, z0.b
.*:	44c0f800 	tblq	z0.d, {z0.d}, z0.d
.*:	4400fbe0 	tblq	z0.b, {z31.b}, z0.b
.*:	441ff800 	tblq	z0.b, {z0.b}, z31.b
.*:	44dffbff 	tblq	z31.d, {z31.d}, z31.d
.*:	444ff945 	tblq	z5.h, {z10.h}, z15.h
.*:	4487f861 	tblq	z1.s, {z3.s}, z7.s
.*:	05203400 	tbxq	z0.b, z0.b, z0.b
.*:	0520341f 	tbxq	z31.b, z0.b, z0.b
.*:	05e03400 	tbxq	z0.d, z0.d, z0.d
.*:	052037e0 	tbxq	z0.b, z31.b, z0.b
.*:	053f3400 	tbxq	z0.b, z0.b, z31.b
.*:	05ff37ff 	tbxq	z31.d, z31.d, z31.d
.*:	05af3545 	tbxq	z5.s, z10.s, z15.s
.*:	057435ea 	tbxq	z10.h, z15.h, z20.h
.*:	4400e800 	uzpq1	z0.b, z0.b, z0.b
.*:	4400e81f 	uzpq1	z31.b, z0.b, z0.b
.*:	44c0e800 	uzpq1	z0.d, z0.d, z0.d
.*:	4400ebe0 	uzpq1	z0.b, z31.b, z0.b
.*:	441fe800 	uzpq1	z0.b, z0.b, z31.b
.*:	44dfebff 	uzpq1	z31.d, z31.d, z31.d
.*:	448fe945 	uzpq1	z5.s, z10.s, z15.s
.*:	4454e9ea 	uzpq1	z10.h, z15.h, z20.h
.*:	4400ec00 	uzpq2	z0.b, z0.b, z0.b
.*:	4400ec1f 	uzpq2	z31.b, z0.b, z0.b
.*:	44c0ec00 	uzpq2	z0.d, z0.d, z0.d
.*:	4400efe0 	uzpq2	z0.b, z31.b, z0.b
.*:	441fec00 	uzpq2	z0.b, z0.b, z31.b
.*:	44dfefff 	uzpq2	z31.d, z31.d, z31.d
.*:	448fed45 	uzpq2	z5.s, z10.s, z15.s
.*:	4454edea 	uzpq2	z10.h, z15.h, z20.h
.*:	4400e000 	zipq1	z0.b, z0.b, z0.b
.*:	4400e01f 	zipq1	z31.b, z0.b, z0.b
.*:	44c0e000 	zipq1	z0.d, z0.d, z0.d
.*:	4400e3e0 	zipq1	z0.b, z31.b, z0.b
.*:	441fe000 	zipq1	z0.b, z0.b, z31.b
.*:	44dfe3ff 	zipq1	z31.d, z31.d, z31.d
.*:	448fe145 	zipq1	z5.s, z10.s, z15.s
.*:	4454e1ea 	zipq1	z10.h, z15.h, z20.h
.*:	4400e400 	zipq2	z0.b, z0.b, z0.b
.*:	4400e41f 	zipq2	z31.b, z0.b, z0.b
.*:	44c0e400 	zipq2	z0.d, z0.d, z0.d
.*:	4400e7e0 	zipq2	z0.b, z31.b, z0.b
.*:	441fe400 	zipq2	z0.b, z0.b, z31.b
.*:	44dfe7ff 	zipq2	z31.d, z31.d, z31.d
.*:	448fe545 	zipq2	z5.s, z10.s, z15.s
.*:	4454e5ea 	zipq2	z10.h, z15.h, z20.h

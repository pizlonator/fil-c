#as: -march=armv8-a+sme-f8f16
#objdump: -dr

.*:     file format .*


Disassembly of section \.text:

0+ <\.text>:
 *[0-9a-f]+:	c1d00020 	fdot	za\.h\[w8, 0, vgx2\], {z0\.b-z1\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c1df0020 	fdot	za\.h\[w8, 0, vgx2\], {z0\.b-z1\.b}, z15\.b\[0\]
 *[0-9a-f]+:	c1d003e0 	fdot	za\.h\[w8, 0, vgx2\], {z30\.b-z31\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c1d00820 	fdot	za\.h\[w8, 0, vgx2\], {z0\.b-z1\.b}, z0\.b\[4\]
 *[0-9a-f]+:	c1d00c28 	fdot	za\.h\[w8, 0, vgx2\], {z0\.b-z1\.b}, z0\.b\[7\]
 *[0-9a-f]+:	c1d00027 	fdot	za\.h\[w8, 7, vgx2\], {z0\.b-z1\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c1d06020 	fdot	za\.h\[w11, 0, vgx2\], {z0\.b-z1\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c1109040 	fdot	za\.h\[w8, 0, vgx4\], {z0\.b-z3\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c11f9040 	fdot	za\.h\[w8, 0, vgx4\], {z0\.b-z3\.b}, z15\.b\[0\]
 *[0-9a-f]+:	c11093c0 	fdot	za\.h\[w8, 0, vgx4\], {z28\.b-z31\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c1109840 	fdot	za\.h\[w8, 0, vgx4\], {z0\.b-z3\.b}, z0\.b\[4\]
 *[0-9a-f]+:	c1109c48 	fdot	za\.h\[w8, 0, vgx4\], {z0\.b-z3\.b}, z0\.b\[7\]
 *[0-9a-f]+:	c1109047 	fdot	za\.h\[w8, 7, vgx4\], {z0\.b-z3\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c110f040 	fdot	za\.h\[w11, 0, vgx4\], {z0\.b-z3\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c1201008 	fdot	za\.h\[w8, 0, vgx2\], {z0\.b-z1\.b}, z0\.b
 *[0-9a-f]+:	c12f1008 	fdot	za\.h\[w8, 0, vgx2\], {z0\.b-z1\.b}, z15\.b
 *[0-9a-f]+:	c12013e8 	fdot	za\.h\[w8, 0, vgx2\], {z31\.b-z0\.b}, z0\.b
 *[0-9a-f]+:	c120100f 	fdot	za\.h\[w8, 7, vgx2\], {z0\.b-z1\.b}, z0\.b
 *[0-9a-f]+:	c1207008 	fdot	za\.h\[w11, 0, vgx2\], {z0\.b-z1\.b}, z0\.b
 *[0-9a-f]+:	c1301008 	fdot	za\.h\[w8, 0, vgx4\], {z0\.b-z3\.b}, z0\.b
 *[0-9a-f]+:	c13f1008 	fdot	za\.h\[w8, 0, vgx4\], {z0\.b-z3\.b}, z15\.b
 *[0-9a-f]+:	c13013e8 	fdot	za\.h\[w8, 0, vgx4\], {z31\.b-z2\.b}, z0\.b
 *[0-9a-f]+:	c130100f 	fdot	za\.h\[w8, 7, vgx4\], {z0\.b-z3\.b}, z0\.b
 *[0-9a-f]+:	c1307008 	fdot	za\.h\[w11, 0, vgx4\], {z0\.b-z3\.b}, z0\.b
 *[0-9a-f]+:	c1a01020 	fdot	za\.h\[w8, 0, vgx2\], {z0\.b-z1\.b}, {z0\.b-z1\.b}
 *[0-9a-f]+:	c1be1020 	fdot	za\.h\[w8, 0, vgx2\], {z0\.b-z1\.b}, {z30\.b-z31\.b}
 *[0-9a-f]+:	c1a013e0 	fdot	za\.h\[w8, 0, vgx2\], {z30\.b-z31\.b}, {z0\.b-z1\.b}
 *[0-9a-f]+:	c1a01027 	fdot	za\.h\[w8, 7, vgx2\], {z0\.b-z1\.b}, {z0\.b-z1\.b}
 *[0-9a-f]+:	c1a07020 	fdot	za\.h\[w11, 0, vgx2\], {z0\.b-z1\.b}, {z0\.b-z1\.b}
 *[0-9a-f]+:	c1a11020 	fdot	za\.h\[w8, 0, vgx4\], {z0\.b-z3\.b}, {z0\.b-z3\.b}
 *[0-9a-f]+:	c1bd1020 	fdot	za\.h\[w8, 0, vgx4\], {z0\.b-z3\.b}, {z28\.b-z31\.b}
 *[0-9a-f]+:	c1a113a0 	fdot	za\.h\[w8, 0, vgx4\], {z28\.b-z31\.b}, {z0\.b-z3\.b}
 *[0-9a-f]+:	c1a11027 	fdot	za\.h\[w8, 7, vgx4\], {z0\.b-z3\.b}, {z0\.b-z3\.b}
 *[0-9a-f]+:	c1a17020 	fdot	za\.h\[w11, 0, vgx4\], {z0\.b-z3\.b}, {z0\.b-z3\.b}
 *[0-9a-f]+:	c1d01020 	fvdot	za\.h\[w8, 0, vgx2\], {z0\.b-z1\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c1d07020 	fvdot	za\.h\[w11, 0, vgx2\], {z0\.b-z1\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c1d01027 	fvdot	za\.h\[w8, 7, vgx2\], {z0\.b-z1\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c1d013e0 	fvdot	za\.h\[w8, 0, vgx2\], {z30\.b-z31\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c1df1020 	fvdot	za\.h\[w8, 0, vgx2\], {z0\.b-z1\.b}, z15\.b\[0\]
 *[0-9a-f]+:	c1d01428 	fvdot	za\.h\[w8, 0, vgx2\], {z0\.b-z1\.b}, z0\.b\[3\]
 *[0-9a-f]+:	c1d01c28 	fvdot	za\.h\[w8, 0, vgx2\], {z0\.b-z1\.b}, z0\.b\[7\]

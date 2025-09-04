#as: -march=armv8-a+sme-f8f32
#objdump: -dr

.*:     file format .*


Disassembly of section \.text:

0+ <\.text>:
 *[0-9a-f]+:	c1500038 	fdot	za\.s\[w8, 0, vgx2\], {z0\.b-z1\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c15f0038 	fdot	za\.s\[w8, 0, vgx2\], {z0\.b-z1\.b}, z15\.b\[0\]
 *[0-9a-f]+:	c15003f8 	fdot	za\.s\[w8, 0, vgx2\], {z30\.b-z31\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c1500c38 	fdot	za\.s\[w8, 0, vgx2\], {z0\.b-z1\.b}, z0\.b\[3\]
 *[0-9a-f]+:	c150003f 	fdot	za\.s\[w8, 7, vgx2\], {z0\.b-z1\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c1506038 	fdot	za\.s\[w11, 0, vgx2\], {z0\.b-z1\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c1508008 	fdot	za\.s\[w8, 0, vgx4\], {z0\.b-z3\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c15f8008 	fdot	za\.s\[w8, 0, vgx4\], {z0\.b-z3\.b}, z15\.b\[0\]
 *[0-9a-f]+:	c1508388 	fdot	za\.s\[w8, 0, vgx4\], {z28\.b-z31\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c1508c08 	fdot	za\.s\[w8, 0, vgx4\], {z0\.b-z3\.b}, z0\.b\[3\]
 *[0-9a-f]+:	c150800f 	fdot	za\.s\[w8, 7, vgx4\], {z0\.b-z3\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c150e008 	fdot	za\.s\[w11, 0, vgx4\], {z0\.b-z3\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c1201018 	fdot	za\.s\[w8, 0, vgx2\], {z0\.b-z1\.b}, z0\.b
 *[0-9a-f]+:	c12f1018 	fdot	za\.s\[w8, 0, vgx2\], {z0\.b-z1\.b}, z15\.b
 *[0-9a-f]+:	c12013f8 	fdot	za\.s\[w8, 0, vgx2\], {z31\.b-z0\.b}, z0\.b
 *[0-9a-f]+:	c120101f 	fdot	za\.s\[w8, 7, vgx2\], {z0\.b-z1\.b}, z0\.b
 *[0-9a-f]+:	c1207018 	fdot	za\.s\[w11, 0, vgx2\], {z0\.b-z1\.b}, z0\.b
 *[0-9a-f]+:	c1301018 	fdot	za\.s\[w8, 0, vgx4\], {z0\.b-z3\.b}, z0\.b
 *[0-9a-f]+:	c13f1018 	fdot	za\.s\[w8, 0, vgx4\], {z0\.b-z3\.b}, z15\.b
 *[0-9a-f]+:	c13013f8 	fdot	za\.s\[w8, 0, vgx4\], {z31\.b-z2\.b}, z0\.b
 *[0-9a-f]+:	c130101f 	fdot	za\.s\[w8, 7, vgx4\], {z0\.b-z3\.b}, z0\.b
 *[0-9a-f]+:	c1307018 	fdot	za\.s\[w11, 0, vgx4\], {z0\.b-z3\.b}, z0\.b
 *[0-9a-f]+:	c1a01030 	fdot	za\.s\[w8, 0, vgx2\], {z0\.b-z1\.b}, {z0\.b-z1\.b}
 *[0-9a-f]+:	c1be1030 	fdot	za\.s\[w8, 0, vgx2\], {z0\.b-z1\.b}, {z30\.b-z31\.b}
 *[0-9a-f]+:	c1a013f0 	fdot	za\.s\[w8, 0, vgx2\], {z30\.b-z31\.b}, {z0\.b-z1\.b}
 *[0-9a-f]+:	c1a01037 	fdot	za\.s\[w8, 7, vgx2\], {z0\.b-z1\.b}, {z0\.b-z1\.b}
 *[0-9a-f]+:	c1a07030 	fdot	za\.s\[w11, 0, vgx2\], {z0\.b-z1\.b}, {z0\.b-z1\.b}
 *[0-9a-f]+:	c1a11030 	fdot	za\.s\[w8, 0, vgx4\], {z0\.b-z3\.b}, {z0\.b-z3\.b}
 *[0-9a-f]+:	c1bd1030 	fdot	za\.s\[w8, 0, vgx4\], {z0\.b-z3\.b}, {z28\.b-z31\.b}
 *[0-9a-f]+:	c1a113b0 	fdot	za\.s\[w8, 0, vgx4\], {z28\.b-z31\.b}, {z0\.b-z3\.b}
 *[0-9a-f]+:	c1a11037 	fdot	za\.s\[w8, 7, vgx4\], {z0\.b-z3\.b}, {z0\.b-z3\.b}
 *[0-9a-f]+:	c1a17030 	fdot	za\.s\[w11, 0, vgx4\], {z0\.b-z3\.b}, {z0\.b-z3\.b}
 *[0-9a-f]+:	c1d00800 	fvdotb	za\.s\[w8, 0, vgx4\], {z0\.b-z1\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c1d06800 	fvdotb	za\.s\[w11, 0, vgx4\], {z0\.b-z1\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c1d00807 	fvdotb	za\.s\[w8, 7, vgx4\], {z0\.b-z1\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c1d00bc0 	fvdotb	za\.s\[w8, 0, vgx4\], {z30\.b-z31\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c1df0800 	fvdotb	za\.s\[w8, 0, vgx4\], {z0\.b-z1\.b}, z15\.b\[0\]
 *[0-9a-f]+:	c1d00808 	fvdotb	za\.s\[w8, 0, vgx4\], {z0\.b-z1\.b}, z0\.b\[1\]
 *[0-9a-f]+:	c1d00c08 	fvdotb	za\.s\[w8, 0, vgx4\], {z0\.b-z1\.b}, z0\.b\[3\]
 *[0-9a-f]+:	c1d00810 	fvdott	za\.s\[w8, 0, vgx4\], {z0\.b-z1\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c1d06810 	fvdott	za\.s\[w11, 0, vgx4\], {z0\.b-z1\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c1d00817 	fvdott	za\.s\[w8, 7, vgx4\], {z0\.b-z1\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c1d00bd0 	fvdott	za\.s\[w8, 0, vgx4\], {z30\.b-z31\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c1df0810 	fvdott	za\.s\[w8, 0, vgx4\], {z0\.b-z1\.b}, z15\.b\[0\]
 *[0-9a-f]+:	c1d00818 	fvdott	za\.s\[w8, 0, vgx4\], {z0\.b-z1\.b}, z0\.b\[1\]
 *[0-9a-f]+:	c1d00c18 	fvdott	za\.s\[w8, 0, vgx4\], {z0\.b-z1\.b}, z0\.b\[3\]

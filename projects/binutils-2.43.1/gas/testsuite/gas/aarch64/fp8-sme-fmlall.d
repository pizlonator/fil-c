#as: -march=armv8-a+sme-f8f32
#objdump: -dr

.*:     file format .*


Disassembly of section \.text:

0+ <\.text>:
 *[0-9a-f]+:	c1400000 	fmlall	za\.s\[w8, 0:3], z0\.b, z0\.b\[0\]
 *[0-9a-f]+:	c1406000 	fmlall	za\.s\[w11, 0:3], z0\.b, z0\.b\[0\]
 *[0-9a-f]+:	c1400003 	fmlall	za\.s\[w8, 12:15], z0\.b, z0\.b\[0\]
 *[0-9a-f]+:	c14003e0 	fmlall	za\.s\[w8, 0:3], z31\.b, z0\.b\[0\]
 *[0-9a-f]+:	c14f0000 	fmlall	za\.s\[w8, 0:3], z0\.b, z15\.b\[0\]
 *[0-9a-f]+:	c1400c00 	fmlall	za\.s\[w8, 0:3], z0\.b, z0\.b\[3\]
 *[0-9a-f]+:	c1409c00 	fmlall	za\.s\[w8, 0:3], z0\.b, z0\.b\[15\]
 *[0-9a-f]+:	c1900020 	fmlall	za\.s\[w8, 0:3, vgx2\], {z0\.b-z1\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c1906020 	fmlall	za\.s\[w11, 0:3, vgx2\], {z0\.b-z1\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c1900021 	fmlall	za\.s\[w8, 4:7, vgx2\], {z0\.b-z1\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c19003e0 	fmlall	za\.s\[w8, 0:3, vgx2\], {z30\.b-z31\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c19f0020 	fmlall	za\.s\[w8, 0:3, vgx2\], {z0\.b-z1\.b}, z15\.b\[0\]
 *[0-9a-f]+:	c1900022 	fmlall	za\.s\[w8, 0:3, vgx2\], {z0\.b-z1\.b}, z0\.b\[1\]
 *[0-9a-f]+:	c1900426 	fmlall	za\.s\[w8, 0:3, vgx2\], {z0\.b-z1\.b}, z0\.b\[7\]
 *[0-9a-f]+:	c1900c26 	fmlall	za\.s\[w8, 0:3, vgx2\], {z0\.b-z1\.b}, z0\.b\[15\]
 *[0-9a-f]+:	c1108040 	fmlall	za\.s\[w8, 0:3, vgx4\], {z0\.b-z3\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c110e040 	fmlall	za\.s\[w11, 0:3, vgx4\], {z0\.b-z3\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c1108041 	fmlall	za\.s\[w8, 4:7, vgx4\], {z0\.b-z3\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c11083c0 	fmlall	za\.s\[w8, 0:3, vgx4\], {z28\.b-z31\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c11f8040 	fmlall	za\.s\[w8, 0:3, vgx4\], {z0\.b-z3\.b}, z15\.b\[0\]
 *[0-9a-f]+:	c1108042 	fmlall	za\.s\[w8, 0:3, vgx4\], {z0\.b-z3\.b}, z0\.b\[1\]
 *[0-9a-f]+:	c1108446 	fmlall	za\.s\[w8, 0:3, vgx4\], {z0\.b-z3\.b}, z0\.b\[7\]
 *[0-9a-f]+:	c1108c46 	fmlall	za\.s\[w8, 0:3, vgx4\], {z0\.b-z3\.b}, z0\.b\[15\]
 *[0-9a-f]+:	c1300400 	fmlall	za\.s\[w8, 0:3\], z0\.b, z0\.b
 *[0-9a-f]+:	c1306400 	fmlall	za\.s\[w11, 0:3\], z0\.b, z0\.b
 *[0-9a-f]+:	c1300403 	fmlall	za\.s\[w8, 12:15\], z0\.b, z0\.b
 *[0-9a-f]+:	c13007e0 	fmlall	za\.s\[w8, 0:3\], z31\.b, z0\.b
 *[0-9a-f]+:	c13f0400 	fmlall	za\.s\[w8, 0:3\], z0\.b, z15\.b
 *[0-9a-f]+:	c1200002 	fmlall	za\.s\[w8, 0:3, vgx2\], {z0\.b-z1\.b}, z0\.b
 *[0-9a-f]+:	c1206002 	fmlall	za\.s\[w11, 0:3, vgx2\], {z0\.b-z1\.b}, z0\.b
 *[0-9a-f]+:	c1200003 	fmlall	za\.s\[w8, 4:7, vgx2\], {z0\.b-z1\.b}, z0\.b
 *[0-9a-f]+:	c12003e2 	fmlall	za\.s\[w8, 0:3, vgx2\], {z31\.b-z0\.b}, z0\.b
 *[0-9a-f]+:	c12f0002 	fmlall	za\.s\[w8, 0:3, vgx2\], {z0\.b-z1\.b}, z15\.b
 *[0-9a-f]+:	c1300002 	fmlall	za\.s\[w8, 0:3, vgx4\], {z0\.b-z3\.b}, z0\.b
 *[0-9a-f]+:	c1306002 	fmlall	za\.s\[w11, 0:3, vgx4\], {z0\.b-z3\.b}, z0\.b
 *[0-9a-f]+:	c1300003 	fmlall	za\.s\[w8, 4:7, vgx4\], {z0\.b-z3\.b}, z0\.b
 *[0-9a-f]+:	c13003e2 	fmlall	za\.s\[w8, 0:3, vgx4\], {z31\.b-z2\.b}, z0\.b
 *[0-9a-f]+:	c13f0002 	fmlall	za\.s\[w8, 0:3, vgx4\], {z0\.b-z3\.b}, z15\.b
 *[0-9a-f]+:	c1a00020 	fmlall	za\.s\[w8, 0:3, vgx2\], {z0\.b-z1\.b}, {z0\.b-z1\.b}
 *[0-9a-f]+:	c1a06020 	fmlall	za\.s\[w11, 0:3, vgx2\], {z0\.b-z1\.b}, {z0\.b-z1\.b}
 *[0-9a-f]+:	c1a00021 	fmlall	za\.s\[w8, 4:7, vgx2\], {z0\.b-z1\.b}, {z0\.b-z1\.b}
 *[0-9a-f]+:	c1a003e0 	fmlall	za\.s\[w8, 0:3, vgx2\], {z30\.b-z31\.b}, {z0\.b-z1\.b}
 *[0-9a-f]+:	c1be0020 	fmlall	za\.s\[w8, 0:3, vgx2\], {z0\.b-z1\.b}, {z30\.b-z31\.b}
 *[0-9a-f]+:	c1a10020 	fmlall	za\.s\[w8, 0:3, vgx4\], {z0\.b-z3\.b}, {z0\.b-z3\.b}
 *[0-9a-f]+:	c1a16020 	fmlall	za\.s\[w11, 0:3, vgx4\], {z0\.b-z3\.b}, {z0\.b-z3\.b}
 *[0-9a-f]+:	c1a10021 	fmlall	za\.s\[w8, 4:7, vgx4\], {z0\.b-z3\.b}, {z0\.b-z3\.b}
 *[0-9a-f]+:	c1a103a0 	fmlall	za\.s\[w8, 0:3, vgx4\], {z28\.b-z31\.b}, {z0\.b-z3\.b}
 *[0-9a-f]+:	c1bd0020 	fmlall	za\.s\[w8, 0:3, vgx4\], {z0\.b-z3\.b}, {z28\.b-z31\.b}

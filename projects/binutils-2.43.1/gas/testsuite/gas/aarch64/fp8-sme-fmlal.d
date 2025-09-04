#as: -march=armv8-a+sme-f8f16
#objdump: -dr

.*:     file format .*


Disassembly of section \.text:

0+ <\.text>:
 *[0-9a-f]+:	c1c00000 	fmlal	za\.h\[w8, 0:1], z0\.b, z0\.b\[0\]
 *[0-9a-f]+:	c1c06000 	fmlal	za\.h\[w11, 0:1], z0\.b, z0\.b\[0\]
 *[0-9a-f]+:	c1c00007 	fmlal	za\.h\[w8, 14:15], z0\.b, z0\.b\[0\]
 *[0-9a-f]+:	c1c003e0 	fmlal	za\.h\[w8, 0:1], z31\.b, z0\.b\[0\]
 *[0-9a-f]+:	c1cf0000 	fmlal	za\.h\[w8, 0:1], z0\.b, z15\.b\[0\]
 *[0-9a-f]+:	c1c00408 	fmlal	za\.h\[w8, 0:1], z0\.b, z0\.b\[3\]
 *[0-9a-f]+:	c1c08c08 	fmlal	za\.h\[w8, 0:1], z0\.b, z0\.b\[15\]
 *[0-9a-f]+:	c1901030 	fmlal	za\.h\[w8, 0:1, vgx2\], {z0\.b-z1\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c1907030 	fmlal	za\.h\[w11, 0:1, vgx2\], {z0\.b-z1\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c1901033 	fmlal	za\.h\[w8, 6:7, vgx2\], {z0\.b-z1\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c19013f0 	fmlal	za\.h\[w8, 0:1, vgx2\], {z30\.b-z31\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c19f1030 	fmlal	za\.h\[w8, 0:1, vgx2\], {z0\.b-z1\.b}, z15\.b\[0\]
 *[0-9a-f]+:	c1901034 	fmlal	za\.h\[w8, 0:1, vgx2\], {z0\.b-z1\.b}, z0\.b\[1\]
 *[0-9a-f]+:	c190143c 	fmlal	za\.h\[w8, 0:1, vgx2\], {z0\.b-z1\.b}, z0\.b\[7\]
 *[0-9a-f]+:	c1901c3c 	fmlal	za\.h\[w8, 0:1, vgx2\], {z0\.b-z1\.b}, z0\.b\[15\]
 *[0-9a-f]+:	c1909020 	fmlal	za\.h\[w8, 0:1, vgx4\], {z0\.b-z3\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c190f020 	fmlal	za\.h\[w11, 0:1, vgx4\], {z0\.b-z3\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c1909023 	fmlal	za\.h\[w8, 6:7, vgx4\], {z0\.b-z3\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c19093a0 	fmlal	za\.h\[w8, 0:1, vgx4\], {z28\.b-z31\.b}, z0\.b\[0\]
 *[0-9a-f]+:	c19f9020 	fmlal	za\.h\[w8, 0:1, vgx4\], {z0\.b-z3\.b}, z15\.b\[0\]
 *[0-9a-f]+:	c1909024 	fmlal	za\.h\[w8, 0:1, vgx4\], {z0\.b-z3\.b}, z0\.b\[1\]
 *[0-9a-f]+:	c190942c 	fmlal	za\.h\[w8, 0:1, vgx4\], {z0\.b-z3\.b}, z0\.b\[7\]
 *[0-9a-f]+:	c1909c2c 	fmlal	za\.h\[w8, 0:1, vgx4\], {z0\.b-z3\.b}, z0\.b\[15\]
 *[0-9a-f]+:	c1300c00 	fmlal	za\.h\[w8, 0:1\], z0\.b, z0\.b
 *[0-9a-f]+:	c1306c00 	fmlal	za\.h\[w11, 0:1\], z0\.b, z0\.b
 *[0-9a-f]+:	c1300c07 	fmlal	za\.h\[w8, 14:15\], z0\.b, z0\.b
 *[0-9a-f]+:	c1300fe0 	fmlal	za\.h\[w8, 0:1\], z31\.b, z0\.b
 *[0-9a-f]+:	c13f0c00 	fmlal	za\.h\[w8, 0:1\], z0\.b, z15\.b
 *[0-9a-f]+:	c1200804 	fmlal	za\.h\[w8, 0:1, vgx2\], {z0\.b-z1\.b}, z0\.b
 *[0-9a-f]+:	c1206804 	fmlal	za\.h\[w11, 0:1, vgx2\], {z0\.b-z1\.b}, z0\.b
 *[0-9a-f]+:	c1200807 	fmlal	za\.h\[w8, 6:7, vgx2\], {z0\.b-z1\.b}, z0\.b
 *[0-9a-f]+:	c1200be4 	fmlal	za\.h\[w8, 0:1, vgx2\], {z31\.b-z0\.b}, z0\.b
 *[0-9a-f]+:	c12f0804 	fmlal	za\.h\[w8, 0:1, vgx2\], {z0\.b-z1\.b}, z15\.b
 *[0-9a-f]+:	c1300804 	fmlal	za\.h\[w8, 0:1, vgx4\], {z0\.b-z3\.b}, z0\.b
 *[0-9a-f]+:	c1306804 	fmlal	za\.h\[w11, 0:1, vgx4\], {z0\.b-z3\.b}, z0\.b
 *[0-9a-f]+:	c1300807 	fmlal	za\.h\[w8, 6:7, vgx4\], {z0\.b-z3\.b}, z0\.b
 *[0-9a-f]+:	c1300be4 	fmlal	za\.h\[w8, 0:1, vgx4\], {z31\.b-z2\.b}, z0\.b
 *[0-9a-f]+:	c13f0804 	fmlal	za\.h\[w8, 0:1, vgx4\], {z0\.b-z3\.b}, z15\.b
 *[0-9a-f]+:	c1a00820 	fmlal	za\.h\[w8, 0:1, vgx2\], {z0\.b-z1\.b}, {z0\.b-z1\.b}
 *[0-9a-f]+:	c1a06820 	fmlal	za\.h\[w11, 0:1, vgx2\], {z0\.b-z1\.b}, {z0\.b-z1\.b}
 *[0-9a-f]+:	c1a00823 	fmlal	za\.h\[w8, 6:7, vgx2\], {z0\.b-z1\.b}, {z0\.b-z1\.b}
 *[0-9a-f]+:	c1a00be0 	fmlal	za\.h\[w8, 0:1, vgx2\], {z30\.b-z31\.b}, {z0\.b-z1\.b}
 *[0-9a-f]+:	c1be0820 	fmlal	za\.h\[w8, 0:1, vgx2\], {z0\.b-z1\.b}, {z30\.b-z31\.b}
 *[0-9a-f]+:	c1a10820 	fmlal	za\.h\[w8, 0:1, vgx4\], {z0\.b-z3\.b}, {z0\.b-z3\.b}
 *[0-9a-f]+:	c1a16820 	fmlal	za\.h\[w11, 0:1, vgx4\], {z0\.b-z3\.b}, {z0\.b-z3\.b}
 *[0-9a-f]+:	c1a10823 	fmlal	za\.h\[w8, 6:7, vgx4\], {z0\.b-z3\.b}, {z0\.b-z3\.b}
 *[0-9a-f]+:	c1a10ba0 	fmlal	za\.h\[w8, 0:1, vgx4\], {z28\.b-z31\.b}, {z0\.b-z3\.b}
 *[0-9a-f]+:	c1bd0820 	fmlal	za\.h\[w8, 0:1, vgx4\], {z0\.b-z3\.b}, {z28\.b-z31\.b}

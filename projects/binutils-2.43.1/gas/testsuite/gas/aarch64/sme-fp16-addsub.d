#as: -march=armv8-a+sme-f8f16
#objdump: -dr

.*:     file format .*


Disassembly of section \.text:

0+ <\.text>:
 *[0-9a-f]+:	c1a41c00 	fadd	za\.h\[w8, 0, vgx2\], {z0\.h-z1\.h}
 *[0-9a-f]+:	c1a47c00 	fadd	za\.h\[w11, 0, vgx2\], {z0\.h-z1\.h}
 *[0-9a-f]+:	c1a41c07 	fadd	za\.h\[w8, 7, vgx2\], {z0\.h-z1\.h}
 *[0-9a-f]+:	c1a41fc0 	fadd	za\.h\[w8, 0, vgx2\], {z30\.h-z31\.h}
 *[0-9a-f]+:	c1a51c00 	fadd	za\.h\[w8, 0, vgx4\], {z0\.h-z3\.h}
 *[0-9a-f]+:	c1a57c00 	fadd	za\.h\[w11, 0, vgx4\], {z0\.h-z3\.h}
 *[0-9a-f]+:	c1a51c07 	fadd	za\.h\[w8, 7, vgx4\], {z0\.h-z3\.h}
 *[0-9a-f]+:	c1a51f80 	fadd	za\.h\[w8, 0, vgx4\], {z28\.h-z31\.h}
 *[0-9a-f]+:	c1a41c08 	fsub	za\.h\[w8, 0, vgx2\], {z0\.h-z1\.h}
 *[0-9a-f]+:	c1a47c08 	fsub	za\.h\[w11, 0, vgx2\], {z0\.h-z1\.h}
 *[0-9a-f]+:	c1a41c0f 	fsub	za\.h\[w8, 7, vgx2\], {z0\.h-z1\.h}
 *[0-9a-f]+:	c1a41fc8 	fsub	za\.h\[w8, 0, vgx2\], {z30\.h-z31\.h}
 *[0-9a-f]+:	c1a51c08 	fsub	za\.h\[w8, 0, vgx4\], {z0\.h-z3\.h}
 *[0-9a-f]+:	c1a57c08 	fsub	za\.h\[w11, 0, vgx4\], {z0\.h-z3\.h}
 *[0-9a-f]+:	c1a51c0f 	fsub	za\.h\[w8, 7, vgx4\], {z0\.h-z3\.h}
 *[0-9a-f]+:	c1a51f88 	fsub	za\.h\[w8, 0, vgx4\], {z28\.h-z31\.h}

#as: -march=armv8-a+sme-f8f32
#objdump: -dr

.*:     file format .*


Disassembly of section \.text:

0+ <\.text>:
 *[0-9a-f]+:	80a00000 	fmopa	za0\.s, p0/m, p0/m, z0\.b, z0\.b
 *[0-9a-f]+:	80a00003 	fmopa	za3\.s, p0/m, p0/m, z0\.b, z0\.b
 *[0-9a-f]+:	80a01c00 	fmopa	za0\.s, p7/m, p0/m, z0\.b, z0\.b
 *[0-9a-f]+:	80a0e000 	fmopa	za0\.s, p0/m, p7/m, z0\.b, z0\.b
 *[0-9a-f]+:	80a003e0 	fmopa	za0\.s, p0/m, p0/m, z31\.b, z0\.b
 *[0-9a-f]+:	80bf0000 	fmopa	za0\.s, p0/m, p0/m, z0\.b, z31\.b

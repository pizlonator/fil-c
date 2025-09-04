#as: -march=armv8-a+sme-f8f16
#objdump: -dr

.*:     file format .*


Disassembly of section \.text:

0+ <\.text>:
 *[0-9a-f]+:	80a00008 	fmopa	za0\.h, p0/m, p0/m, z0\.b, z0\.b
 *[0-9a-f]+:	80a00009 	fmopa	za1\.h, p0/m, p0/m, z0\.b, z0\.b
 *[0-9a-f]+:	80a01c08 	fmopa	za0\.h, p7/m, p0/m, z0\.b, z0\.b
 *[0-9a-f]+:	80a0e008 	fmopa	za0\.h, p0/m, p7/m, z0\.b, z0\.b
 *[0-9a-f]+:	80a003e8 	fmopa	za0\.h, p0/m, p0/m, z31\.b, z0\.b
 *[0-9a-f]+:	80bf0008 	fmopa	za0\.h, p0/m, p0/m, z0\.b, z31\.b

#as: -march=armv8-a+sve2+fp8dot4
#as: -march=armv8-a+ssve-fp8dot4
#objdump: -dr

.*:     file format .*


Disassembly of section \.text:

0+ <\.text>:
 *[0-9a-f]+:	64608400 	fdot	z0\.s, z0\.b, z0\.b
 *[0-9a-f]+:	647d8777 	fdot	z23\.s, z27\.b, z29\.b
 *[0-9a-f]+:	0420bff9 	movprfx	z25, z31
 *[0-9a-f]+:	647386b9 	fdot	z25\.s, z21\.b, z19\.b
 *[0-9a-f]+:	64604400 	fdot	z0\.s, z0\.b, z0\.b\[0\]
 *[0-9a-f]+:	646d4777 	fdot	z23\.s, z27\.b, z5\.b\[1\]
 *[0-9a-f]+:	0420bff9 	movprfx	z25, z31
 *[0-9a-f]+:	647f46b9 	fdot	z25\.s, z21\.b, z7\.b\[3\]

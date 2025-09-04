#as: -march=armv8-a+sve2+fp8dot2
#as: -march=armv8-a+ssve-fp8dot2
#objdump: -dr

.*:     file format .*


Disassembly of section \.text:

0+ <\.text>:
 *[0-9a-f]+:	64208400 	fdot	z0\.h, z0\.b, z0\.b
 *[0-9a-f]+:	643d8777 	fdot	z23\.h, z27\.b, z29\.b
 *[0-9a-f]+:	0420bff9 	movprfx	z25, z31
 *[0-9a-f]+:	643386b9 	fdot	z25\.h, z21\.b, z19\.b
 *[0-9a-f]+:	64204400 	fdot	z0\.h, z0\.b, z0\.b\[0\]
 *[0-9a-f]+:	642d4f77 	fdot	z23\.h, z27\.b, z5\.b\[3\]
 *[0-9a-f]+:	0420bff9 	movprfx	z25, z31
 *[0-9a-f]+:	643f4eb9 	fdot	z25\.h, z21\.b, z7\.b\[7\]

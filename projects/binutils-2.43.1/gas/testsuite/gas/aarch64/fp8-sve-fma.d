#as: -march=armv8-a+sve2+fp8fma
#as: -march=armv8-a+ssve-fp8fma
#objdump: -dr

.*:     file format .*


Disassembly of section \.text:

0+ <\.text>:
 *[0-9a-f]+:	64a08800 	fmlalb	z0\.h, z0\.b, z0\.b
 *[0-9a-f]+:	64bd8b77 	fmlalb	z23\.h, z27\.b, z29\.b
 *[0-9a-f]+:	0420bff9 	movprfx	z25, z31
 *[0-9a-f]+:	64b38ab9 	fmlalb	z25\.h, z21\.b, z19\.b
 *[0-9a-f]+:	64205000 	fmlalb	z0\.h, z0\.b, z0\.b\[0\]
 *[0-9a-f]+:	64255777 	fmlalb	z23\.h, z27\.b, z5\.b\[1\]
 *[0-9a-f]+:	0420bff9 	movprfx	z25, z31
 *[0-9a-f]+:	642f5ab9 	fmlalb	z25\.h, z21\.b, z7\.b\[6\]
 *[0-9a-f]+:	643f5eb9 	fmlalb	z25\.h, z21\.b, z7\.b\[15\]
 *[0-9a-f]+:	64208800 	fmlallbb	z0\.s, z0\.b, z0\.b
 *[0-9a-f]+:	643d8b77 	fmlallbb	z23\.s, z27\.b, z29\.b
 *[0-9a-f]+:	0420bff9 	movprfx	z25, z31
 *[0-9a-f]+:	64338ab9 	fmlallbb	z25\.s, z21\.b, z19\.b
 *[0-9a-f]+:	6420c000 	fmlallbb	z0\.s, z0\.b, z0\.b\[0\]
 *[0-9a-f]+:	6425c777 	fmlallbb	z23\.s, z27\.b, z5\.b\[1\]
 *[0-9a-f]+:	0420bff9 	movprfx	z25, z31
 *[0-9a-f]+:	642fcab9 	fmlallbb	z25\.s, z21\.b, z7\.b\[6\]
 *[0-9a-f]+:	643fceb9 	fmlallbb	z25\.s, z21\.b, z7\.b\[15\]
 *[0-9a-f]+:	64209800 	fmlallbt	z0\.s, z0\.b, z0\.b
 *[0-9a-f]+:	643d9b77 	fmlallbt	z23\.s, z27\.b, z29\.b
 *[0-9a-f]+:	0420bff9 	movprfx	z25, z31
 *[0-9a-f]+:	64339ab9 	fmlallbt	z25\.s, z21\.b, z19\.b
 *[0-9a-f]+:	6460c000 	fmlallbt	z0\.s, z0\.b, z0\.b\[0\]
 *[0-9a-f]+:	6465c777 	fmlallbt	z23\.s, z27\.b, z5\.b\[1\]
 *[0-9a-f]+:	0420bff9 	movprfx	z25, z31
 *[0-9a-f]+:	646fcab9 	fmlallbt	z25\.s, z21\.b, z7\.b\[6\]
 *[0-9a-f]+:	647fceb9 	fmlallbt	z25\.s, z21\.b, z7\.b\[15\]
 *[0-9a-f]+:	6420a800 	fmlalltb	z0\.s, z0\.b, z0\.b
 *[0-9a-f]+:	643dab77 	fmlalltb	z23\.s, z27\.b, z29\.b
 *[0-9a-f]+:	0420bff9 	movprfx	z25, z31
 *[0-9a-f]+:	6433aab9 	fmlalltb	z25\.s, z21\.b, z19\.b
 *[0-9a-f]+:	64a0c000 	fmlalltb	z0\.s, z0\.b, z0\.b\[0\]
 *[0-9a-f]+:	64a5c777 	fmlalltb	z23\.s, z27\.b, z5\.b\[1\]
 *[0-9a-f]+:	0420bff9 	movprfx	z25, z31
 *[0-9a-f]+:	64afcab9 	fmlalltb	z25\.s, z21\.b, z7\.b\[6\]
 *[0-9a-f]+:	64bfceb9 	fmlalltb	z25\.s, z21\.b, z7\.b\[15\]
 *[0-9a-f]+:	6420b800 	fmlalltt	z0\.s, z0\.b, z0\.b
 *[0-9a-f]+:	643dbb77 	fmlalltt	z23\.s, z27\.b, z29\.b
 *[0-9a-f]+:	0420bff9 	movprfx	z25, z31
 *[0-9a-f]+:	6433bab9 	fmlalltt	z25\.s, z21\.b, z19\.b
 *[0-9a-f]+:	64e0c000 	fmlalltt	z0\.s, z0\.b, z0\.b\[0\]
 *[0-9a-f]+:	64e5c777 	fmlalltt	z23\.s, z27\.b, z5\.b\[1\]
 *[0-9a-f]+:	0420bff9 	movprfx	z25, z31
 *[0-9a-f]+:	64efcab9 	fmlalltt	z25\.s, z21\.b, z7\.b\[6\]
 *[0-9a-f]+:	64ffceb9 	fmlalltt	z25\.s, z21\.b, z7\.b\[15\]
 *[0-9a-f]+:	64a09800 	fmlalt	z0\.h, z0\.b, z0\.b
 *[0-9a-f]+:	64bd9b77 	fmlalt	z23\.h, z27\.b, z29\.b
 *[0-9a-f]+:	0420bff9 	movprfx	z25, z31
 *[0-9a-f]+:	64b39ab9 	fmlalt	z25\.h, z21\.b, z19\.b
 *[0-9a-f]+:	64a05000 	fmlalt	z0\.h, z0\.b, z0\.b\[0\]
 *[0-9a-f]+:	64a55777 	fmlalt	z23\.h, z27\.b, z5\.b\[1\]
 *[0-9a-f]+:	0420bff9 	movprfx	z25, z31
 *[0-9a-f]+:	64af5ab9 	fmlalt	z25\.h, z21\.b, z7\.b\[6\]
 *[0-9a-f]+:	64bf5eb9 	fmlalt	z25\.h, z21\.b, z7\.b\[15\]

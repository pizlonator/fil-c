#as: -march=armv8-a+fp8fma
#objdump: -dr

.*:     file format .*


Disassembly of section \.text:

0+ <\.text>:
 *[0-9a-f]+:	0ec0fc00 	fmlalb	v0\.8h, v0\.16b, v0\.16b
 *[0-9a-f]+:	0eddff77 	fmlalb	v23\.8h, v27\.16b, v29\.16b
 *[0-9a-f]+:	0ed3feb9 	fmlalb	v25\.8h, v21\.16b, v19\.16b
 *[0-9a-f]+:	4ec0fc00 	fmlalt	v0\.8h, v0\.16b, v0\.16b
 *[0-9a-f]+:	4eddff77 	fmlalt	v23\.8h, v27\.16b, v29\.16b
 *[0-9a-f]+:	4ed3feb9 	fmlalt	v25\.8h, v21\.16b, v19\.16b
 *[0-9a-f]+:	0fc00000 	fmlalb	v0\.8h, v0\.16b, v0\.b\[0\]
 *[0-9a-f]+:	0fdd0377 	fmlalb	v23\.8h, v27\.16b, v5\.b\[3\]
 *[0-9a-f]+:	0fe702b9 	fmlalb	v25\.8h, v21\.16b, v7\.b\[4\]
 *[0-9a-f]+:	0fc70ab9 	fmlalb	v25\.8h, v21\.16b, v7\.b\[8\]
 *[0-9a-f]+:	4fc00000 	fmlalt	v0\.8h, v0\.16b, v0\.b\[0\]
 *[0-9a-f]+:	4fdd0377 	fmlalt	v23\.8h, v27\.16b, v5\.b\[3\]
 *[0-9a-f]+:	4fe702b9 	fmlalt	v25\.8h, v21\.16b, v7\.b\[4\]
 *[0-9a-f]+:	4fc70ab9 	fmlalt	v25\.8h, v21\.16b, v7\.b\[8\]
 *[0-9a-f]+:	0e00c400 	fmlallbb	v0\.4s, v0\.16b, v0\.16b
 *[0-9a-f]+:	0e1dc777 	fmlallbb	v23\.4s, v27\.16b, v29\.16b
 *[0-9a-f]+:	0e13c6b9 	fmlallbb	v25\.4s, v21\.16b, v19\.16b
 *[0-9a-f]+:	0e40c400 	fmlallbt	v0\.4s, v0\.16b, v0\.16b
 *[0-9a-f]+:	0e5dc777 	fmlallbt	v23\.4s, v27\.16b, v29\.16b
 *[0-9a-f]+:	0e53c6b9 	fmlallbt	v25\.4s, v21\.16b, v19\.16b
 *[0-9a-f]+:	4e00c400 	fmlalltb	v0\.4s, v0\.16b, v0\.16b
 *[0-9a-f]+:	4e1dc777 	fmlalltb	v23\.4s, v27\.16b, v29\.16b
 *[0-9a-f]+:	4e13c6b9 	fmlalltb	v25\.4s, v21\.16b, v19\.16b
 *[0-9a-f]+:	4e40c400 	fmlalltt	v0\.4s, v0\.16b, v0\.16b
 *[0-9a-f]+:	4e5dc777 	fmlalltt	v23\.4s, v27\.16b, v29\.16b
 *[0-9a-f]+:	4e53c6b9 	fmlalltt	v25\.4s, v21\.16b, v19\.16b
 *[0-9a-f]+:	2f008000 	fmlallbb	v0\.4s, v0\.16b, v0\.b\[0\]
 *[0-9a-f]+:	2f1d8377 	fmlallbb	v23\.4s, v27\.16b, v5\.b\[3\]
 *[0-9a-f]+:	2f2782b9 	fmlallbb	v25\.4s, v21\.16b, v7\.b\[4\]
 *[0-9a-f]+:	2f078ab9 	fmlallbb	v25\.4s, v21\.16b, v7\.b\[8\]
 *[0-9a-f]+:	2f408000 	fmlallbt	v0\.4s, v0\.16b, v0\.b\[0\]
 *[0-9a-f]+:	2f5d8377 	fmlallbt	v23\.4s, v27\.16b, v5\.b\[3\]
 *[0-9a-f]+:	2f6782b9 	fmlallbt	v25\.4s, v21\.16b, v7\.b\[4\]
 *[0-9a-f]+:	2f478ab9 	fmlallbt	v25\.4s, v21\.16b, v7\.b\[8\]
 *[0-9a-f]+:	6f008000 	fmlalltb	v0\.4s, v0\.16b, v0\.b\[0\]
 *[0-9a-f]+:	6f1d8377 	fmlalltb	v23\.4s, v27\.16b, v5\.b\[3\]
 *[0-9a-f]+:	6f2782b9 	fmlalltb	v25\.4s, v21\.16b, v7\.b\[4\]
 *[0-9a-f]+:	6f078ab9 	fmlalltb	v25\.4s, v21\.16b, v7\.b\[8\]
 *[0-9a-f]+:	6f408000 	fmlalltt	v0\.4s, v0\.16b, v0\.b\[0\]
 *[0-9a-f]+:	6f5d8377 	fmlalltt	v23\.4s, v27\.16b, v5\.b\[3\]
 *[0-9a-f]+:	6f6782b9 	fmlalltt	v25\.4s, v21\.16b, v7\.b\[4\]
 *[0-9a-f]+:	6f478ab9 	fmlalltt	v25\.4s, v21\.16b, v7\.b\[8\]

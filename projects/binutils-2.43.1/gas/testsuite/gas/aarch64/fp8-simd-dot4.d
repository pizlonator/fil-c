#as: -march=armv8-a+fp8dot4
#objdump: -dr

.*:     file format .*


Disassembly of section \.text:

0+ <\.text>:
 *[0-9a-f]+:	0e00fc00 	fdot	v0\.2s, v0\.8b, v0\.8b
 *[0-9a-f]+:	0e1dff77 	fdot	v23\.2s, v27\.8b, v29\.8b
 *[0-9a-f]+:	0e13feb9 	fdot	v25\.2s, v21\.8b, v19\.8b
 *[0-9a-f]+:	4e00fc00 	fdot	v0\.4s, v0\.16b, v0\.16b
 *[0-9a-f]+:	4e1dff77 	fdot	v23\.4s, v27\.16b, v29\.16b
 *[0-9a-f]+:	4e13feb9 	fdot	v25\.4s, v21\.16b, v19\.16b
 *[0-9a-f]+:	0f000000 	fdot	v0\.2s, v0\.8b, v0\.4b\[0\]
 *[0-9a-f]+:	0f3d0377 	fdot	v23\.2s, v27\.8b, v29\.4b\[1\]
 *[0-9a-f]+:	0f330ab9 	fdot	v25\.2s, v21\.8b, v19\.4b\[3\]
 *[0-9a-f]+:	4f000000 	fdot	v0\.4s, v0\.16b, v0\.4b\[0\]
 *[0-9a-f]+:	4f3d0377 	fdot	v23\.4s, v27\.16b, v29\.4b\[1\]
 *[0-9a-f]+:	4f330ab9 	fdot	v25\.4s, v21\.16b, v19\.4b\[3\]

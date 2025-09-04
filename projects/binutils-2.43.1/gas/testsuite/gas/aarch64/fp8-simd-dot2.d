#as: -march=armv8-a+fp8dot2
#objdump: -dr

.*:     file format .*


Disassembly of section \.text:

0+ <\.text>:
 *[0-9a-f]+:	0e40fc00 	fdot	v0\.4h, v0\.8b, v0\.8b
 *[0-9a-f]+:	0e5dff77 	fdot	v23\.4h, v27\.8b, v29\.8b
 *[0-9a-f]+:	0e53feb9 	fdot	v25\.4h, v21\.8b, v19\.8b
 *[0-9a-f]+:	4e40fc00 	fdot	v0\.8h, v0\.16b, v0\.16b
 *[0-9a-f]+:	4e5dff77 	fdot	v23\.8h, v27\.16b, v29\.16b
 *[0-9a-f]+:	4e53feb9 	fdot	v25\.8h, v21\.16b, v19\.16b
 *[0-9a-f]+:	0f400000 	fdot	v0\.4h, v0\.8b, v0\.2b\[0\]
 *[0-9a-f]+:	0f500000 	fdot	v0\.4h, v0\.8b, v0\.2b\[1\]
 *[0-9a-f]+:	0f6e0377 	fdot	v23\.4h, v27\.8b, v14\.2b\[2\]
 *[0-9a-f]+:	0f4d0ab9 	fdot	v25\.4h, v21\.8b, v13\.2b\[4\]
 *[0-9a-f]+:	0f7d0ab9 	fdot	v25\.4h, v21\.8b, v13\.2b\[7\]
 *[0-9a-f]+:	4f400000 	fdot	v0\.8h, v0\.16b, v0\.2b\[0\]
 *[0-9a-f]+:	4f500000 	fdot	v0\.8h, v0\.16b, v0\.2b\[1\]
 *[0-9a-f]+:	4f6e0377 	fdot	v23\.8h, v27\.16b, v14\.2b\[2\]
 *[0-9a-f]+:	4f4d0ab9 	fdot	v25\.8h, v21\.16b, v13\.2b\[4\]
 *[0-9a-f]+:	4f7d0ab9 	fdot	v25\.8h, v21\.16b, v13\.2b\[7\]

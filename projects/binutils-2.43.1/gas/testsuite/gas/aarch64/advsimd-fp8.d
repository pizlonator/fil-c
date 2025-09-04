#as: -march=armv8.5-a+fp8
#objdump: -dr

.*:     file format .*

Disassembly of section \.text:

0+ <.*>:

[ ]*[0-9a-f]+:	2ea17800 	bf1cvtl	v0.8h, v0.8b
[ ]*[0-9a-f]+:	2ea1781f 	bf1cvtl	v31.8h, v0.8b
[ ]*[0-9a-f]+:	2ea17be0 	bf1cvtl	v0.8h, v31.8b
[ ]*[0-9a-f]+:	2ea17bff 	bf1cvtl	v31.8h, v31.8b
[ ]*[0-9a-f]+:	2ee17800 	bf2cvtl	v0.8h, v0.8b
[ ]*[0-9a-f]+:	2ee1781f 	bf2cvtl	v31.8h, v0.8b
[ ]*[0-9a-f]+:	2ee17be0 	bf2cvtl	v0.8h, v31.8b
[ ]*[0-9a-f]+:	2ee17bff 	bf2cvtl	v31.8h, v31.8b
[ ]*[0-9a-f]+:	2e217800 	f1cvtl	v0.8h, v0.8b
[ ]*[0-9a-f]+:	2e21781f 	f1cvtl	v31.8h, v0.8b
[ ]*[0-9a-f]+:	2e217be0 	f1cvtl	v0.8h, v31.8b
[ ]*[0-9a-f]+:	2e217bff 	f1cvtl	v31.8h, v31.8b
[ ]*[0-9a-f]+:	2e617800 	f2cvtl	v0.8h, v0.8b
[ ]*[0-9a-f]+:	2e61781f 	f2cvtl	v31.8h, v0.8b
[ ]*[0-9a-f]+:	2e617be0 	f2cvtl	v0.8h, v31.8b
[ ]*[0-9a-f]+:	2e617bff 	f2cvtl	v31.8h, v31.8b
[ ]*[0-9a-f]+:	6ea17800 	bf1cvtl2	v0.8h, v0.16b
[ ]*[0-9a-f]+:	6ea1781f 	bf1cvtl2	v31.8h, v0.16b
[ ]*[0-9a-f]+:	6ea17be0 	bf1cvtl2	v0.8h, v31.16b
[ ]*[0-9a-f]+:	6ea17bff 	bf1cvtl2	v31.8h, v31.16b
[ ]*[0-9a-f]+:	6ee17800 	bf2cvtl2	v0.8h, v0.16b
[ ]*[0-9a-f]+:	6ee1781f 	bf2cvtl2	v31.8h, v0.16b
[ ]*[0-9a-f]+:	6ee17be0 	bf2cvtl2	v0.8h, v31.16b
[ ]*[0-9a-f]+:	6ee17bff 	bf2cvtl2	v31.8h, v31.16b
[ ]*[0-9a-f]+:	6e217800 	f1cvtl2	v0.8h, v0.16b
[ ]*[0-9a-f]+:	6e21781f 	f1cvtl2	v31.8h, v0.16b
[ ]*[0-9a-f]+:	6e217be0 	f1cvtl2	v0.8h, v31.16b
[ ]*[0-9a-f]+:	6e217bff 	f1cvtl2	v31.8h, v31.16b
[ ]*[0-9a-f]+:	6e617800 	f2cvtl2	v0.8h, v0.16b
[ ]*[0-9a-f]+:	6e61781f 	f2cvtl2	v31.8h, v0.16b
[ ]*[0-9a-f]+:	6e617be0 	f2cvtl2	v0.8h, v31.16b
[ ]*[0-9a-f]+:	6e617bff 	f2cvtl2	v31.8h, v31.16b
[ ]*[0-9a-f]+:	2ec03c00 	fscale	v0.4h, v0.4h, v0.4h
[ ]*[0-9a-f]+:	2ec03c1f 	fscale	v31.4h, v0.4h, v0.4h
[ ]*[0-9a-f]+:	2ec03fe0 	fscale	v0.4h, v31.4h, v0.4h
[ ]*[0-9a-f]+:	2edf3c00 	fscale	v0.4h, v0.4h, v31.4h
[ ]*[0-9a-f]+:	2ec03fff 	fscale	v31.4h, v31.4h, v0.4h
[ ]*[0-9a-f]+:	2edf3fe0 	fscale	v0.4h, v31.4h, v31.4h
[ ]*[0-9a-f]+:	2ec03fff 	fscale	v31.4h, v31.4h, v0.4h
[ ]*[0-9a-f]+:	2edf3fff 	fscale	v31.4h, v31.4h, v31.4h
[ ]*[0-9a-f]+:	6ec03c00 	fscale	v0.8h, v0.8h, v0.8h
[ ]*[0-9a-f]+:	6ec03c1f 	fscale	v31.8h, v0.8h, v0.8h
[ ]*[0-9a-f]+:	6ec03fe0 	fscale	v0.8h, v31.8h, v0.8h
[ ]*[0-9a-f]+:	6edf3c00 	fscale	v0.8h, v0.8h, v31.8h
[ ]*[0-9a-f]+:	6ec03fff 	fscale	v31.8h, v31.8h, v0.8h
[ ]*[0-9a-f]+:	6edf3fe0 	fscale	v0.8h, v31.8h, v31.8h
[ ]*[0-9a-f]+:	6ec03fff 	fscale	v31.8h, v31.8h, v0.8h
[ ]*[0-9a-f]+:	6edf3fff 	fscale	v31.8h, v31.8h, v31.8h
[ ]*[0-9a-f]+:	2ea0fc00 	fscale	v0.2s, v0.2s, v0.2s
[ ]*[0-9a-f]+:	2ea0fc1f 	fscale	v31.2s, v0.2s, v0.2s
[ ]*[0-9a-f]+:	2ea0ffe0 	fscale	v0.2s, v31.2s, v0.2s
[ ]*[0-9a-f]+:	2ebffc00 	fscale	v0.2s, v0.2s, v31.2s
[ ]*[0-9a-f]+:	2ea0ffff 	fscale	v31.2s, v31.2s, v0.2s
[ ]*[0-9a-f]+:	2ebfffe0 	fscale	v0.2s, v31.2s, v31.2s
[ ]*[0-9a-f]+:	2ea0ffff 	fscale	v31.2s, v31.2s, v0.2s
[ ]*[0-9a-f]+:	2ebfffff 	fscale	v31.2s, v31.2s, v31.2s
[ ]*[0-9a-f]+:	6ea0fc00 	fscale	v0.4s, v0.4s, v0.4s
[ ]*[0-9a-f]+:	6ea0fc1f 	fscale	v31.4s, v0.4s, v0.4s
[ ]*[0-9a-f]+:	6ea0ffe0 	fscale	v0.4s, v31.4s, v0.4s
[ ]*[0-9a-f]+:	6ebffc00 	fscale	v0.4s, v0.4s, v31.4s
[ ]*[0-9a-f]+:	6ea0ffff 	fscale	v31.4s, v31.4s, v0.4s
[ ]*[0-9a-f]+:	6ebfffe0 	fscale	v0.4s, v31.4s, v31.4s
[ ]*[0-9a-f]+:	6ea0ffff 	fscale	v31.4s, v31.4s, v0.4s
[ ]*[0-9a-f]+:	6ebfffff 	fscale	v31.4s, v31.4s, v31.4s
[ ]*[0-9a-f]+:	6ee0fc00 	fscale	v0.2d, v0.2d, v0.2d
[ ]*[0-9a-f]+:	6ee0fc1f 	fscale	v31.2d, v0.2d, v0.2d
[ ]*[0-9a-f]+:	6ee0ffe0 	fscale	v0.2d, v31.2d, v0.2d
[ ]*[0-9a-f]+:	6efffc00 	fscale	v0.2d, v0.2d, v31.2d
[ ]*[0-9a-f]+:	6ee0ffff 	fscale	v31.2d, v31.2d, v0.2d
[ ]*[0-9a-f]+:	6effffe0 	fscale	v0.2d, v31.2d, v31.2d
[ ]*[0-9a-f]+:	6ee0ffff 	fscale	v31.2d, v31.2d, v0.2d
[ ]*[0-9a-f]+:	6effffff 	fscale	v31.2d, v31.2d, v31.2d
[ ]*[0-9a-f]+:	0e40f400 	fcvtn	v0.8b, v0.4h, v0.4h
[ ]*[0-9a-f]+:	0e40f41f 	fcvtn	v31.8b, v0.4h, v0.4h
[ ]*[0-9a-f]+:	0e40f7e0 	fcvtn	v0.8b, v31.4h, v0.4h
[ ]*[0-9a-f]+:	0e5ff400 	fcvtn	v0.8b, v0.4h, v31.4h
[ ]*[0-9a-f]+:	0e40f7ff 	fcvtn	v31.8b, v31.4h, v0.4h
[ ]*[0-9a-f]+:	0e5ff7e0 	fcvtn	v0.8b, v31.4h, v31.4h
[ ]*[0-9a-f]+:	0e5ff7ff 	fcvtn	v31.8b, v31.4h, v31.4h
[ ]*[0-9a-f]+:	4e40f400 	fcvtn	v0.16b, v0.8h, v0.8h
[ ]*[0-9a-f]+:	4e40f41f 	fcvtn	v31.16b, v0.8h, v0.8h
[ ]*[0-9a-f]+:	4e40f7e0 	fcvtn	v0.16b, v31.8h, v0.8h
[ ]*[0-9a-f]+:	4e5ff400 	fcvtn	v0.16b, v0.8h, v31.8h
[ ]*[0-9a-f]+:	4e40f7ff 	fcvtn	v31.16b, v31.8h, v0.8h
[ ]*[0-9a-f]+:	4e5ff7e0 	fcvtn	v0.16b, v31.8h, v31.8h
[ ]*[0-9a-f]+:	4e5ff7ff 	fcvtn	v31.16b, v31.8h, v31.8h
[ ]*[0-9a-f]+:	0e00f400 	fcvtn	v0.8b, v0.4s, v0.4s
[ ]*[0-9a-f]+:	0e00f41f 	fcvtn	v31.8b, v0.4s, v0.4s
[ ]*[0-9a-f]+:	0e00f7e0 	fcvtn	v0.8b, v31.4s, v0.4s
[ ]*[0-9a-f]+:	0e1ff400 	fcvtn	v0.8b, v0.4s, v31.4s
[ ]*[0-9a-f]+:	0e00f7ff 	fcvtn	v31.8b, v31.4s, v0.4s
[ ]*[0-9a-f]+:	0e1ff7e0 	fcvtn	v0.8b, v31.4s, v31.4s
[ ]*[0-9a-f]+:	0e1ff7ff 	fcvtn	v31.8b, v31.4s, v31.4s
[ ]*[0-9a-f]+:	4e00f400 	fcvtn2	v0.16b, v0.4s, v0.4s
[ ]*[0-9a-f]+:	4e00f41f 	fcvtn2	v31.16b, v0.4s, v0.4s
[ ]*[0-9a-f]+:	4e00f7e0 	fcvtn2	v0.16b, v31.4s, v0.4s
[ ]*[0-9a-f]+:	4e1ff400 	fcvtn2	v0.16b, v0.4s, v31.4s
[ ]*[0-9a-f]+:	4e00f7ff 	fcvtn2	v31.16b, v31.4s, v0.4s
[ ]*[0-9a-f]+:	4e1ff7e0 	fcvtn2	v0.16b, v31.4s, v31.4s
[ ]*[0-9a-f]+:	4e1ff7ff 	fcvtn2	v31.16b, v31.4s, v31.4s

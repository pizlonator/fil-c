#objdump: -dr --prefix-addresses --show-raw-insn
#name: MIPS divide and multiply macros with --trap
#as: -32 --trap
#warning_output: brtr-opt.l
#source: brtr-opt.s

.*: +file format .*mips.*

Disassembly of section \.text:
[0-9a-f]+ <[^>]*> 0000 703c 	teq	zero,zero,0x7
[0-9a-f]+ <[^>]*> 0000 703c 	teq	zero,zero,0x7
[0-9a-f]+ <[^>]*> 0002 703c 	teq	v0,zero,0x7
[0-9a-f]+ <[^>]*> 0044 ab3c 	div	zero,a0,v0
[0-9a-f]+ <[^>]*> 3020 ffff 	li	at,-1
[0-9a-f]+ <[^>]*> b422 fffe 	bne	v0,at,[0-9a-f]+ <[^>]*>
			[0-9a-f]+: R_MICROMIPS_PC16_S1	\.L\^_0
[0-9a-f]+ <[^>]*> 41a1 8000 	lui	at,0x8000
[0-9a-f]+ <[^>]*> 0024 603c 	teq	a0,at,0x6
[0-9a-f]+ <\.L\^_0> 4606      	mfhi	a2
[0-9a-f]+ <[^>]*> 4440      	xor	s0,s0,s0
[0-9a-f]+ <[^>]*> 0002 703c 	teq	v0,zero,0x7
[0-9a-f]+ <[^>]*> 0044 bb3c 	divu	zero,a0,v0
[0-9a-f]+ <[^>]*> 4606      	mfhi	a2
[0-9a-f]+ <[^>]*> 4440      	xor	s0,s0,s0
[0-9a-f]+ <[^>]*> 4449      	xor	s1,s1,s1
[0-9a-f]+ <[^>]*> 0044 8b3c 	mult	a0,v0
[0-9a-f]+ <[^>]*> 4646      	mflo	a2
[0-9a-f]+ <[^>]*> 00c6 f880 	sra	a2,a2,0x1f
[0-9a-f]+ <[^>]*> 4601      	mfhi	at
[0-9a-f]+ <[^>]*> 0026 6c3c 	tne	a2,at,0x6
[0-9a-f]+ <[^>]*> 4646      	mflo	a2
[0-9a-f]+ <[^>]*> 4440      	xor	s0,s0,s0
[0-9a-f]+ <[^>]*> 3020 0002 	li	at,2
[0-9a-f]+ <[^>]*> 0024 9b3c 	multu	a0,at
[0-9a-f]+ <[^>]*> 4601      	mfhi	at
[0-9a-f]+ <[^>]*> 4646      	mflo	a2
[0-9a-f]+ <[^>]*> 0001 6c3c 	tne	at,zero,0x6
[0-9a-f]+ <[^>]*> 0000 703c 	teq	zero,zero,0x7
[0-9a-f]+ <[^>]*> 0000 703c 	teq	zero,zero,0x7
[0-9a-f]+ <[^>]*> 0002 703c 	teq	v0,zero,0x7
[0-9a-f]+ <[^>]*> 0044 ab3c 	div	zero,a0,v0
[0-9a-f]+ <[^>]*> 3020 ffff 	li	at,-1
[0-9a-f]+ <[^>]*> b422 fffe 	bne	v0,at,[0-9a-f]+ <[^>]*>
			[0-9a-f]+: R_MICROMIPS_PC16_S1	\.L\^_1
[0-9a-f]+ <[^>]*> 41a1 8000 	lui	at,0x8000
[0-9a-f]+ <[^>]*> 0024 603c 	teq	a0,at,0x6
[0-9a-f]+ <\.L\^_1> 4606      	mfhi	a2
[0-9a-f]+ <[^>]*> 4440      	xor	s0,s0,s0
[0-9a-f]+ <[^>]*> 0002 703c 	teq	v0,zero,0x7
[0-9a-f]+ <[^>]*> 0044 bb3c 	divu	zero,a0,v0
[0-9a-f]+ <[^>]*> 4606      	mfhi	a2
[0-9a-f]+ <[^>]*> 4440      	xor	s0,s0,s0
[0-9a-f]+ <[^>]*> 4449      	xor	s1,s1,s1
[0-9a-f]+ <[^>]*> 0044 8b3c 	mult	a0,v0
[0-9a-f]+ <[^>]*> 4646      	mflo	a2
[0-9a-f]+ <[^>]*> 00c6 f880 	sra	a2,a2,0x1f
[0-9a-f]+ <[^>]*> 4601      	mfhi	at
[0-9a-f]+ <[^>]*> 0026 6c3c 	tne	a2,at,0x6
[0-9a-f]+ <[^>]*> 4646      	mflo	a2
[0-9a-f]+ <[^>]*> 4440      	xor	s0,s0,s0
[0-9a-f]+ <[^>]*> 3020 0002 	li	at,2
[0-9a-f]+ <[^>]*> 0024 9b3c 	multu	a0,at
[0-9a-f]+ <[^>]*> 4601      	mfhi	at
[0-9a-f]+ <[^>]*> 4646      	mflo	a2
[0-9a-f]+ <[^>]*> 0001 6c3c 	tne	at,zero,0x6
[0-9a-f]+ <[^>]*> 0000 703c 	teq	zero,zero,0x7
[0-9a-f]+ <[^>]*> 0000 703c 	teq	zero,zero,0x7
[0-9a-f]+ <[^>]*> 0002 703c 	teq	v0,zero,0x7
[0-9a-f]+ <[^>]*> 0044 ab3c 	div	zero,a0,v0
[0-9a-f]+ <[^>]*> 3020 ffff 	li	at,-1
[0-9a-f]+ <[^>]*> b422 fffe 	bne	v0,at,[0-9a-f]+ <[^>]*>
			[0-9a-f]+: R_MICROMIPS_PC16_S1	\.L\^_2
[0-9a-f]+ <[^>]*> 41a1 8000 	lui	at,0x8000
[0-9a-f]+ <[^>]*> 0024 603c 	teq	a0,at,0x6
[0-9a-f]+ <\.L\^_2> 4606      	mfhi	a2
[0-9a-f]+ <[^>]*> 4440      	xor	s0,s0,s0
[0-9a-f]+ <[^>]*> 0002 703c 	teq	v0,zero,0x7
[0-9a-f]+ <[^>]*> 0044 bb3c 	divu	zero,a0,v0
[0-9a-f]+ <[^>]*> 4606      	mfhi	a2
[0-9a-f]+ <[^>]*> 4440      	xor	s0,s0,s0
[0-9a-f]+ <[^>]*> 4449      	xor	s1,s1,s1
[0-9a-f]+ <[^>]*> 0044 8b3c 	mult	a0,v0
[0-9a-f]+ <[^>]*> 4646      	mflo	a2
[0-9a-f]+ <[^>]*> 00c6 f880 	sra	a2,a2,0x1f
[0-9a-f]+ <[^>]*> 4601      	mfhi	at
[0-9a-f]+ <[^>]*> 0026 6c3c 	tne	a2,at,0x6
[0-9a-f]+ <[^>]*> 4646      	mflo	a2
[0-9a-f]+ <[^>]*> 4440      	xor	s0,s0,s0
[0-9a-f]+ <[^>]*> 3020 0002 	li	at,2
[0-9a-f]+ <[^>]*> 0024 9b3c 	multu	a0,at
[0-9a-f]+ <[^>]*> 4601      	mfhi	at
[0-9a-f]+ <[^>]*> 4646      	mflo	a2
[0-9a-f]+ <[^>]*> 0001 6c3c 	tne	at,zero,0x6
	\.\.\.

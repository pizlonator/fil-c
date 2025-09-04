#objdump: -dr --prefix-addresses --show-raw-insn
#name: MIPS divide and multiply macros with --break
#as: -32 --break
#warning_output: brtr-opt.l
#source: brtr-opt.s

.*: +file format .*mips.*

Disassembly of section \.text:
[0-9a-f]+ <[^>]*> 4687      	break	0x7
[0-9a-f]+ <[^>]*> 4687      	break	0x7
[0-9a-f]+ <[^>]*> b402 fffe 	bnez	v0,[0-9a-f]+ <[^>]*>
			[0-9a-f]+: R_MICROMIPS_PC16_S1	\.L\^_0
[0-9a-f]+ <[^>]*> 0044 ab3c 	div	zero,a0,v0
[0-9a-f]+ <[^>]*> 4687      	break	0x7
[0-9a-f]+ <\.L\^_0> 3020 ffff 	li	at,-1
[0-9a-f]+ <[^>]*> b422 fffe 	bne	v0,at,[0-9a-f]+ <[^>]*>
			[0-9a-f]+: R_MICROMIPS_PC16_S1	\.L\^_1
[0-9a-f]+ <[^>]*> 41a1 8000 	lui	at,0x8000
[0-9a-f]+ <[^>]*> b424 fffe 	bne	a0,at,[0-9a-f]+ <[^>]*>
			[0-9a-f]+: R_MICROMIPS_PC16_S1	\.L\^_1
[0-9a-f]+ <[^>]*> 0c00      	nop
[0-9a-f]+ <[^>]*> 4686      	break	0x6
[0-9a-f]+ <\.L\^_1> 4606      	mfhi	a2
[0-9a-f]+ <[^>]*> 4440      	xor	s0,s0,s0
[0-9a-f]+ <[^>]*> b402 fffe 	bnez	v0,[0-9a-f]+ <[^>]*>
			[0-9a-f]+: R_MICROMIPS_PC16_S1	\.L\^_2
[0-9a-f]+ <[^>]*> 0044 bb3c 	divu	zero,a0,v0
[0-9a-f]+ <[^>]*> 4687      	break	0x7
[0-9a-f]+ <\.L\^_2> 4606      	mfhi	a2
[0-9a-f]+ <[^>]*> 4440      	xor	s0,s0,s0
[0-9a-f]+ <[^>]*> 4449      	xor	s1,s1,s1
[0-9a-f]+ <[^>]*> 0044 8b3c 	mult	a0,v0
[0-9a-f]+ <[^>]*> 4646      	mflo	a2
[0-9a-f]+ <[^>]*> 00c6 f880 	sra	a2,a2,0x1f
[0-9a-f]+ <[^>]*> 4601      	mfhi	at
[0-9a-f]+ <[^>]*> 9426 fffe 	beq	a2,at,[0-9a-f]+ <[^>]*>
			[0-9a-f]+: R_MICROMIPS_PC16_S1	\.L\^_3
[0-9a-f]+ <[^>]*> 0c00      	nop
[0-9a-f]+ <[^>]*> 4686      	break	0x6
[0-9a-f]+ <\.L\^_3> 4646      	mflo	a2
[0-9a-f]+ <[^>]*> 4440      	xor	s0,s0,s0
[0-9a-f]+ <[^>]*> 3020 0002 	li	at,2
[0-9a-f]+ <[^>]*> 0024 9b3c 	multu	a0,at
[0-9a-f]+ <[^>]*> 4601      	mfhi	at
[0-9a-f]+ <[^>]*> 4646      	mflo	a2
[0-9a-f]+ <[^>]*> 9401 fffe 	beqz	at,[0-9a-f]+ <[^>]*>
			[0-9a-f]+: R_MICROMIPS_PC16_S1	\.L\^_4
[0-9a-f]+ <[^>]*> 0c00      	nop
[0-9a-f]+ <[^>]*> 4686      	break	0x6
[0-9a-f]+ <\.L\^_4> 4687      	break	0x7
[0-9a-f]+ <[^>]*> 4687      	break	0x7
[0-9a-f]+ <[^>]*> b402 fffe 	bnez	v0,[0-9a-f]+ <[^>]*>
			[0-9a-f]+: R_MICROMIPS_PC16_S1	\.L\^_5
[0-9a-f]+ <[^>]*> 0044 ab3c 	div	zero,a0,v0
[0-9a-f]+ <[^>]*> 4687      	break	0x7
[0-9a-f]+ <\.L\^_5> 3020 ffff 	li	at,-1
[0-9a-f]+ <[^>]*> b422 fffe 	bne	v0,at,[0-9a-f]+ <[^>]*>
			[0-9a-f]+: R_MICROMIPS_PC16_S1	\.L\^_6
[0-9a-f]+ <[^>]*> 41a1 8000 	lui	at,0x8000
[0-9a-f]+ <[^>]*> b424 fffe 	bne	a0,at,[0-9a-f]+ <[^>]*>
			[0-9a-f]+: R_MICROMIPS_PC16_S1	\.L\^_6
[0-9a-f]+ <[^>]*> 0c00      	nop
[0-9a-f]+ <[^>]*> 4686      	break	0x6
[0-9a-f]+ <\.L\^_6> 4606      	mfhi	a2
[0-9a-f]+ <[^>]*> 4440      	xor	s0,s0,s0
[0-9a-f]+ <[^>]*> b402 fffe 	bnez	v0,[0-9a-f]+ <[^>]*>
			[0-9a-f]+: R_MICROMIPS_PC16_S1	\.L\^_7
[0-9a-f]+ <[^>]*> 0044 bb3c 	divu	zero,a0,v0
[0-9a-f]+ <[^>]*> 4687      	break	0x7
[0-9a-f]+ <\.L\^_7> 4606      	mfhi	a2
[0-9a-f]+ <[^>]*> 4440      	xor	s0,s0,s0
[0-9a-f]+ <[^>]*> 4449      	xor	s1,s1,s1
[0-9a-f]+ <[^>]*> 0044 8b3c 	mult	a0,v0
[0-9a-f]+ <[^>]*> 4646      	mflo	a2
[0-9a-f]+ <[^>]*> 00c6 f880 	sra	a2,a2,0x1f
[0-9a-f]+ <[^>]*> 4601      	mfhi	at
[0-9a-f]+ <[^>]*> 9426 fffe 	beq	a2,at,[0-9a-f]+ <[^>]*>
			[0-9a-f]+: R_MICROMIPS_PC16_S1	\.L\^_8
[0-9a-f]+ <[^>]*> 0c00      	nop
[0-9a-f]+ <[^>]*> 4686      	break	0x6
[0-9a-f]+ <\.L\^_8> 4646      	mflo	a2
[0-9a-f]+ <[^>]*> 4440      	xor	s0,s0,s0
[0-9a-f]+ <[^>]*> 3020 0002 	li	at,2
[0-9a-f]+ <[^>]*> 0024 9b3c 	multu	a0,at
[0-9a-f]+ <[^>]*> 4601      	mfhi	at
[0-9a-f]+ <[^>]*> 4646      	mflo	a2
[0-9a-f]+ <[^>]*> 9401 fffe 	beqz	at,[0-9a-f]+ <[^>]*>
			[0-9a-f]+: R_MICROMIPS_PC16_S1	\.L\^_9
[0-9a-f]+ <[^>]*> 0c00      	nop
[0-9a-f]+ <[^>]*> 4686      	break	0x6
[0-9a-f]+ <\.L\^_9> 4687      	break	0x7
[0-9a-f]+ <[^>]*> 4687      	break	0x7
[0-9a-f]+ <[^>]*> b402 fffe 	bnez	v0,[0-9a-f]+ <[^>]*>
			[0-9a-f]+: R_MICROMIPS_PC16_S1	\.L\^_10
[0-9a-f]+ <[^>]*> 0044 ab3c 	div	zero,a0,v0
[0-9a-f]+ <[^>]*> 4687      	break	0x7
[0-9a-f]+ <\.L\^_10> 3020 ffff 	li	at,-1
[0-9a-f]+ <[^>]*> b422 fffe 	bne	v0,at,[0-9a-f]+ <[^>]*>
			[0-9a-f]+: R_MICROMIPS_PC16_S1	\.L\^_11
[0-9a-f]+ <[^>]*> 41a1 8000 	lui	at,0x8000
[0-9a-f]+ <[^>]*> b424 fffe 	bne	a0,at,[0-9a-f]+ <[^>]*>
			[0-9a-f]+: R_MICROMIPS_PC16_S1	\.L\^_11
[0-9a-f]+ <[^>]*> 0c00      	nop
[0-9a-f]+ <[^>]*> 4686      	break	0x6
[0-9a-f]+ <\.L\^_11> 4606      	mfhi	a2
[0-9a-f]+ <[^>]*> 4440      	xor	s0,s0,s0
[0-9a-f]+ <[^>]*> b402 fffe 	bnez	v0,[0-9a-f]+ <[^>]*>
			[0-9a-f]+: R_MICROMIPS_PC16_S1	\.L\^_12
[0-9a-f]+ <[^>]*> 0044 bb3c 	divu	zero,a0,v0
[0-9a-f]+ <[^>]*> 4687      	break	0x7
[0-9a-f]+ <\.L\^_12> 4606      	mfhi	a2
[0-9a-f]+ <[^>]*> 4440      	xor	s0,s0,s0
[0-9a-f]+ <[^>]*> 4449      	xor	s1,s1,s1
[0-9a-f]+ <[^>]*> 0044 8b3c 	mult	a0,v0
[0-9a-f]+ <[^>]*> 4646      	mflo	a2
[0-9a-f]+ <[^>]*> 00c6 f880 	sra	a2,a2,0x1f
[0-9a-f]+ <[^>]*> 4601      	mfhi	at
[0-9a-f]+ <[^>]*> 9426 fffe 	beq	a2,at,[0-9a-f]+ <[^>]*>
			[0-9a-f]+: R_MICROMIPS_PC16_S1	\.L\^_13
[0-9a-f]+ <[^>]*> 0c00      	nop
[0-9a-f]+ <[^>]*> 4686      	break	0x6
[0-9a-f]+ <\.L\^_13> 4646      	mflo	a2
[0-9a-f]+ <[^>]*> 4440      	xor	s0,s0,s0
[0-9a-f]+ <[^>]*> 3020 0002 	li	at,2
[0-9a-f]+ <[^>]*> 0024 9b3c 	multu	a0,at
[0-9a-f]+ <[^>]*> 4601      	mfhi	at
[0-9a-f]+ <[^>]*> 4646      	mflo	a2
[0-9a-f]+ <[^>]*> 9401 fffe 	beqz	at,[0-9a-f]+ <[^>]*>
			[0-9a-f]+: R_MICROMIPS_PC16_S1	\.L\^_14
[0-9a-f]+ <[^>]*> 0c00      	nop
[0-9a-f]+ <[^>]*> 4686      	break	0x6
	\.\.\.
	\.\.\.

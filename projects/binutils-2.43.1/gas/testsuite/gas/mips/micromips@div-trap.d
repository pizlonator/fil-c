#as: -32 -trap
#objdump: -dr --prefix-addresses --show-raw-insn
#name: MIPS div with traps
#source: div.s

# Test the div macro.

.*: +file format .*mips.*

Disassembly of section \.text:
[0-9a-f]+ <[^>]*> 00a4 ab3c 	div	zero,a0,a1
[0-9a-f]+ <[^>]*> 0005 703c 	teq	a1,zero,0x7
[0-9a-f]+ <[^>]*> 00a4 ab3c 	div	zero,a0,a1
[0-9a-f]+ <[^>]*> 3020 ffff 	li	at,-1
[0-9a-f]+ <[^>]*> b425 fffe 	bne	a1,at,[0-9a-f]+ <[^>]*>
			[0-9a-f]+: R_MICROMIPS_PC16_S1	\.L\^_0
[0-9a-f]+ <[^>]*> 41a1 8000 	lui	at,0x8000
[0-9a-f]+ <[^>]*> 0024 603c 	teq	a0,at,0x6
[0-9a-f]+ <\.L\^_0> 4644      	mflo	a0
[0-9a-f]+ <[^>]*> 0006 703c 	teq	a2,zero,0x7
[0-9a-f]+ <[^>]*> 00c5 ab3c 	div	zero,a1,a2
[0-9a-f]+ <[^>]*> 3020 ffff 	li	at,-1
[0-9a-f]+ <[^>]*> b426 fffe 	bne	a2,at,[0-9a-f]+ <[^>]*>
			[0-9a-f]+: R_MICROMIPS_PC16_S1	\.L\^_1
[0-9a-f]+ <[^>]*> 41a1 8000 	lui	at,0x8000
[0-9a-f]+ <[^>]*> 0025 603c 	teq	a1,at,0x6
[0-9a-f]+ <\.L\^_1> 4644      	mflo	a0
[0-9a-f]+ <[^>]*> 0c84      	move	a0,a0
[0-9a-f]+ <[^>]*> 0c85      	move	a0,a1
[0-9a-f]+ <[^>]*> 0080 2190 	neg	a0,a0
[0-9a-f]+ <[^>]*> 00a0 2190 	neg	a0,a1
[0-9a-f]+ <[^>]*> 3020 0002 	li	at,2
[0-9a-f]+ <[^>]*> 0024 ab3c 	div	zero,a0,at
[0-9a-f]+ <[^>]*> 4644      	mflo	a0
[0-9a-f]+ <[^>]*> 3020 0002 	li	at,2
[0-9a-f]+ <[^>]*> 0025 ab3c 	div	zero,a1,at
[0-9a-f]+ <[^>]*> 4644      	mflo	a0
[0-9a-f]+ <[^>]*> 5020 8000 	li	at,0x8000
[0-9a-f]+ <[^>]*> 0024 ab3c 	div	zero,a0,at
[0-9a-f]+ <[^>]*> 4644      	mflo	a0
[0-9a-f]+ <[^>]*> 5020 8000 	li	at,0x8000
[0-9a-f]+ <[^>]*> 0025 ab3c 	div	zero,a1,at
[0-9a-f]+ <[^>]*> 4644      	mflo	a0
[0-9a-f]+ <[^>]*> 3020 8000 	li	at,-32768
[0-9a-f]+ <[^>]*> 0024 ab3c 	div	zero,a0,at
[0-9a-f]+ <[^>]*> 4644      	mflo	a0
[0-9a-f]+ <[^>]*> 3020 8000 	li	at,-32768
[0-9a-f]+ <[^>]*> 0025 ab3c 	div	zero,a1,at
[0-9a-f]+ <[^>]*> 4644      	mflo	a0
[0-9a-f]+ <[^>]*> 41a1 0001 	lui	at,0x1
[0-9a-f]+ <[^>]*> 0024 ab3c 	div	zero,a0,at
[0-9a-f]+ <[^>]*> 4644      	mflo	a0
[0-9a-f]+ <[^>]*> 41a1 0001 	lui	at,0x1
[0-9a-f]+ <[^>]*> 0025 ab3c 	div	zero,a1,at
[0-9a-f]+ <[^>]*> 4644      	mflo	a0
[0-9a-f]+ <[^>]*> 41a1 0001 	lui	at,0x1
[0-9a-f]+ <[^>]*> 5021 a5a5 	ori	at,at,0xa5a5
[0-9a-f]+ <[^>]*> 0024 ab3c 	div	zero,a0,at
[0-9a-f]+ <[^>]*> 4644      	mflo	a0
[0-9a-f]+ <[^>]*> 41a1 0001 	lui	at,0x1
[0-9a-f]+ <[^>]*> 5021 a5a5 	ori	at,at,0xa5a5
[0-9a-f]+ <[^>]*> 0025 ab3c 	div	zero,a1,at
[0-9a-f]+ <[^>]*> 4644      	mflo	a0
[0-9a-f]+ <[^>]*> 00a4 bb3c 	divu	zero,a0,a1
[0-9a-f]+ <[^>]*> 0005 703c 	teq	a1,zero,0x7
[0-9a-f]+ <[^>]*> 00a4 bb3c 	divu	zero,a0,a1
[0-9a-f]+ <[^>]*> 4644      	mflo	a0
[0-9a-f]+ <[^>]*> 0006 703c 	teq	a2,zero,0x7
[0-9a-f]+ <[^>]*> 00c5 bb3c 	divu	zero,a1,a2
[0-9a-f]+ <[^>]*> 4644      	mflo	a0
[0-9a-f]+ <[^>]*> 0c84      	move	a0,a0
[0-9a-f]+ <[^>]*> 0006 703c 	teq	a2,zero,0x7
[0-9a-f]+ <[^>]*> 00c5 ab3c 	div	zero,a1,a2
[0-9a-f]+ <[^>]*> 3020 ffff 	li	at,-1
[0-9a-f]+ <[^>]*> b426 fffe 	bne	a2,at,[0-9a-f]+ <[^>]*>
			[0-9a-f]+: R_MICROMIPS_PC16_S1	\.L\^_2
[0-9a-f]+ <[^>]*> 41a1 8000 	lui	at,0x8000
[0-9a-f]+ <[^>]*> 0025 603c 	teq	a1,at,0x6
[0-9a-f]+ <\.L\^_2> 4604      	mfhi	a0
[0-9a-f]+ <[^>]*> 3020 0002 	li	at,2
[0-9a-f]+ <[^>]*> 0025 bb3c 	divu	zero,a1,at
[0-9a-f]+ <[^>]*> 4604      	mfhi	a0
	\.\.\.

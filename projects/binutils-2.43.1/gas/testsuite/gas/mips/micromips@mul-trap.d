#as: -32 -trap
#objdump: -dr --prefix-addresses --show-raw-insn
#name: MIPS mul with traps
#source: mul.s

# Test the mul macro.

.*: +file format .*mips.*

Disassembly of section \.text:
[0-9a-f]+ <[^>]*> 00a4 2210 	mul	a0,a0,a1
[0-9a-f]+ <[^>]*> 00c5 2210 	mul	a0,a1,a2
[0-9a-f]+ <[^>]*> 3020 0000 	li	at,0
[0-9a-f]+ <[^>]*> 0025 8b3c 	mult	a1,at
[0-9a-f]+ <[^>]*> 4644      	mflo	a0
[0-9a-f]+ <[^>]*> 3020 0001 	li	at,1
[0-9a-f]+ <[^>]*> 0025 8b3c 	mult	a1,at
[0-9a-f]+ <[^>]*> 4644      	mflo	a0
[0-9a-f]+ <[^>]*> 5020 8000 	li	at,0x8000
[0-9a-f]+ <[^>]*> 0025 8b3c 	mult	a1,at
[0-9a-f]+ <[^>]*> 4644      	mflo	a0
[0-9a-f]+ <[^>]*> 3020 8000 	li	at,-32768
[0-9a-f]+ <[^>]*> 0025 8b3c 	mult	a1,at
[0-9a-f]+ <[^>]*> 4644      	mflo	a0
[0-9a-f]+ <[^>]*> 41a1 0001 	lui	at,0x1
[0-9a-f]+ <[^>]*> 0025 8b3c 	mult	a1,at
[0-9a-f]+ <[^>]*> 4644      	mflo	a0
[0-9a-f]+ <[^>]*> 41a1 0001 	lui	at,0x1
[0-9a-f]+ <[^>]*> 5021 a5a5 	ori	at,at,0xa5a5
[0-9a-f]+ <[^>]*> 0025 8b3c 	mult	a1,at
[0-9a-f]+ <[^>]*> 4644      	mflo	a0
[0-9a-f]+ <[^>]*> 00a4 8b3c 	mult	a0,a1
[0-9a-f]+ <[^>]*> 4644      	mflo	a0
[0-9a-f]+ <[^>]*> 0084 f880 	sra	a0,a0,0x1f
[0-9a-f]+ <[^>]*> 4601      	mfhi	at
[0-9a-f]+ <[^>]*> 0024 6c3c 	tne	a0,at,0x6
[0-9a-f]+ <[^>]*> 4644      	mflo	a0
[0-9a-f]+ <[^>]*> 00c5 8b3c 	mult	a1,a2
[0-9a-f]+ <[^>]*> 4644      	mflo	a0
[0-9a-f]+ <[^>]*> 0084 f880 	sra	a0,a0,0x1f
[0-9a-f]+ <[^>]*> 4601      	mfhi	at
[0-9a-f]+ <[^>]*> 0024 6c3c 	tne	a0,at,0x6
[0-9a-f]+ <[^>]*> 4644      	mflo	a0
[0-9a-f]+ <[^>]*> 00a4 9b3c 	multu	a0,a1
[0-9a-f]+ <[^>]*> 4601      	mfhi	at
[0-9a-f]+ <[^>]*> 4644      	mflo	a0
[0-9a-f]+ <[^>]*> 0001 6c3c 	tne	at,zero,0x6
[0-9a-f]+ <[^>]*> 00c5 9b3c 	multu	a1,a2
[0-9a-f]+ <[^>]*> 4601      	mfhi	at
[0-9a-f]+ <[^>]*> 4644      	mflo	a0
[0-9a-f]+ <[^>]*> 0001 6c3c 	tne	at,zero,0x6
	\.\.\.

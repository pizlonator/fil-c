#as: -32 -trap
#objdump: -dr --prefix-addresses --show-raw-insn
#name: MIPS 64-bit div with traps
#source: div64.s

# Test the div macro.

.*: +file format .*mips.*

Disassembly of section \.text:
[0-9a-f]+ <[^>]*> 0006 703c 	teq	a2,zero,0x7
[0-9a-f]+ <[^>]*> 58c5 ab3c 	ddiv	zero,a1,a2
[0-9a-f]+ <[^>]*> 3020 ffff 	li	at,-1
[0-9a-f]+ <[^>]*> b426 fffe 	bne	a2,at,[0-9a-f]+ <[^>]*>
			[0-9a-f]+: R_MICROMIPS_PC16_S1	\.L\^_0
[0-9a-f]+ <[^>]*> 3020 0001 	li	at,1
[0-9a-f]+ <[^>]*> 5821 f808 	dsll32	at,at,0x1f
[0-9a-f]+ <[^>]*> 0025 603c 	teq	a1,at,0x6
[0-9a-f]+ <\.L\^_0> 4644      	mflo	a0
[0-9a-f]+ <[^>]*> 3020 0002 	li	at,2
[0-9a-f]+ <[^>]*> 5825 bb3c 	ddivu	zero,a1,at
[0-9a-f]+ <[^>]*> 4644      	mflo	a0
[0-9a-f]+ <[^>]*> 5020 8000 	li	at,0x8000
[0-9a-f]+ <[^>]*> 5825 ab3c 	ddiv	zero,a1,at
[0-9a-f]+ <[^>]*> 4604      	mfhi	a0
[0-9a-f]+ <[^>]*> 3020 8000 	li	at,-32768
[0-9a-f]+ <[^>]*> 5825 bb3c 	ddivu	zero,a1,at
[0-9a-f]+ <[^>]*> 4604      	mfhi	a0
	\.\.\.

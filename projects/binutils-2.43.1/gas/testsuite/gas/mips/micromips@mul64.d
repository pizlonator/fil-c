#as: -32
#objdump: -dr --prefix-addresses --show-raw-insn
#name: MIPS 64-bit mul
#source: mul64.s

# Test the mul macro.

.*: +file format .*mips.*

Disassembly of section \.text:
[0-9a-f]+ <[^>]*> 58c5 9b3c 	dmultu	a1,a2
[0-9a-f]+ <[^>]*> 4644      	mflo	a0
[0-9a-f]+ <[^>]*> 3020 0001 	li	at,1
[0-9a-f]+ <[^>]*> 5825 8b3c 	dmult	a1,at
[0-9a-f]+ <[^>]*> 4644      	mflo	a0
[0-9a-f]+ <[^>]*> 58c5 8b3c 	dmult	a1,a2
[0-9a-f]+ <[^>]*> 4644      	mflo	a0
[0-9a-f]+ <[^>]*> 5884 f888 	dsra32	a0,a0,0x1f
[0-9a-f]+ <[^>]*> 4601      	mfhi	at
[0-9a-f]+ <[^>]*> 9424 fffe 	beq	a0,at,[0-9a-f]+ <[^>]*>
			[0-9a-f]+: R_MICROMIPS_PC16_S1	\.L\^_0
[0-9a-f]+ <[^>]*> 0c00      	nop
[0-9a-f]+ <[^>]*> 4686      	break	0x6
[0-9a-f]+ <\.L\^_0> 4644      	mflo	a0
[0-9a-f]+ <[^>]*> 58c5 9b3c 	dmultu	a1,a2
[0-9a-f]+ <[^>]*> 4601      	mfhi	at
[0-9a-f]+ <[^>]*> 4644      	mflo	a0
[0-9a-f]+ <[^>]*> 9401 fffe 	beqz	at,[0-9a-f]+ <[^>]*>
			[0-9a-f]+: R_MICROMIPS_PC16_S1	\.L\^_1
[0-9a-f]+ <[^>]*> 0c00      	nop
[0-9a-f]+ <[^>]*> 4686      	break	0x6
	\.\.\.
	\.\.\.

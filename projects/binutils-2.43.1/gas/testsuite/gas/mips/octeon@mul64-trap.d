#as: -32 -trap
#objdump: -dr --prefix-addresses --show-raw-insn
#name: MIPS 64-bit mul with traps
#source: mul64.s

# Test the mul macro.

.*: +file format .*mips.*

Disassembly of section \.text:
[0-9a-f]+ <[^>]*> 70a62003 	dmul	a0,a1,a2
[0-9a-f]+ <[^>]*> 24010001 	li	at,1
[0-9a-f]+ <[^>]*> 00a1001c 	dmult	a1,at
[0-9a-f]+ <[^>]*> 00002012 	mflo	a0
[0-9a-f]+ <[^>]*> 00a6001c 	dmult	a1,a2
[0-9a-f]+ <[^>]*> 00002012 	mflo	a0
[0-9a-f]+ <[^>]*> 000427ff 	dsra32	a0,a0,0x1f
[0-9a-f]+ <[^>]*> 00000810 	mfhi	at
[0-9a-f]+ <[^>]*> 008101b6 	tne	a0,at,0x6
[0-9a-f]+ <[^>]*> 00002012 	mflo	a0
[0-9a-f]+ <[^>]*> 00a6001d 	dmultu	a1,a2
[0-9a-f]+ <[^>]*> 00000810 	mfhi	at
[0-9a-f]+ <[^>]*> 00002012 	mflo	a0
[0-9a-f]+ <[^>]*> 002001b6 	tne	at,zero,0x6
	\.\.\.

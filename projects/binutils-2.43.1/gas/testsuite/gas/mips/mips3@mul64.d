#as: -32
#objdump: -dr --prefix-addresses
#name: MIPS 64-bit mul
#source: mul64.s

# Test the mul macro.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> dmultu	a1,a2
0+0004 <[^>]*> mflo	a0
0+0008 <[^>]*> li	at,1
0+000c <[^>]*> nop
0+0010 <[^>]*> dmult	a1,at
0+0014 <[^>]*> mflo	a0
	...
0+0020 <[^>]*> dmult	a1,a2
0+0024 <[^>]*> mflo	a0
0+0028 <[^>]*> dsra32	a0,a0,0x1f
0+002c <[^>]*> mfhi	at
0+0030 <[^>]*> beq	a0,at,0+3c <foo\+(0x|)3c>
0+0034 <[^>]*> nop
0+0038 <[^>]*> break	(0x0,0x6|0x6)
0+003c <[^>]*> mflo	a0
	...
0+0048 <[^>]*> dmultu	a1,a2
0+004c <[^>]*> mfhi	at
0+0050 <[^>]*> mflo	a0
0+0054 <[^>]*> beqz	at,0+60 <foo\+(0x|)60>
0+0058 <[^>]*> nop
0+005c <[^>]*> break	(0x0,0x6|0x6)
	...

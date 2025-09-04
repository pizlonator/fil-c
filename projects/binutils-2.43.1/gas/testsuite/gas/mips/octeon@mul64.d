#as: -32
#objdump: -dr --prefix-addresses
#name: MIPS 64-bit mul
#source: mul64.s

# Test the mul macro.

.*: +file format .*mips.*

Disassembly of section \.text:
0+0000 <[^>]*> dmul	a0,a1,a2
0+0004 <[^>]*> li	at,1
0+0008 <[^>]*> dmult	a1,at
0+000c <[^>]*> mflo	a0
0+0010 <[^>]*> dmult	a1,a2
0+0014 <[^>]*> mflo	a0
0+0018 <[^>]*> dsra32	a0,a0,0x1f
0+001c <[^>]*> mfhi	at
0+0020 <[^>]*> beq	a0,at,0+2c <foo\+(0x|)2c>
0+0024 <[^>]*> nop
0+0028 <[^>]*> break	(0x0,0x6|0x6)
0+002c <[^>]*> mflo	a0
0+0030 <[^>]*> dmultu	a1,a2
0+0034 <[^>]*> mfhi	at
0+0038 <[^>]*> mflo	a0
0+003c <[^>]*> beqz	at,0+48 <foo\+(0x|)48>
0+0040 <[^>]*> nop
0+0044 <[^>]*> break	(0x0,0x6|0x6)
	\.\.\.

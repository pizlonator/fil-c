#as: -32
#objdump: -dr --prefix-addresses
#name: MIPS 64-bit mul

# Test the mul macro.

.*: +file format .*mips.*

Disassembly of section \.text:
0+0000 <[^>]*> dmultu	a1,a2
0+0004 <[^>]*> mflo	a0
0+0008 <[^>]*> li	at,1
0+000c <[^>]*> dmult	a1,at
0+0010 <[^>]*> mflo	a0
0+0014 <[^>]*> dmult	a1,a2
0+0018 <[^>]*> mflo	a0
0+001c <[^>]*> dsra32	a0,a0,0x1f
0+0020 <[^>]*> mfhi	at
0+0024 <[^>]*> beq	a0,at,0+30 <foo\+(0x|)30>
0+0028 <[^>]*> nop
0+002c <[^>]*> break	(0x0,0x6|0x6)
0+0030 <[^>]*> mflo	a0
0+0034 <[^>]*> dmultu	a1,a2
0+0038 <[^>]*> mfhi	at
0+003c <[^>]*> mflo	a0
0+0040 <[^>]*> beqz	at,0+4c <foo\+(0x|)4c>
0+0044 <[^>]*> nop
0+0048 <[^>]*> break	(0x0,0x6|0x6)
	\.\.\.

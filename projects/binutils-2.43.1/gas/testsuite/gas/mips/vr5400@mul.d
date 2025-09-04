#as: -32
#objdump: -dr --prefix-addresses
#name: MIPS mul
#source: mul.s

# Test the mul macro.

.*: +file format .*mips.*

Disassembly of section \.text:
0+0000 <[^>]*> multu	a0,a1
0+0004 <[^>]*> mflo	a0
	\.\.\.
0+0010 <[^>]*> mul	a0,a1,a2
0+0014 <[^>]*> li	at,0
0+0018 <[^>]*> mult	a1,at
0+001c <[^>]*> mflo	a0
0+0020 <[^>]*> li	at,1
0+0024 <[^>]*> nop
0+0028 <[^>]*> mult	a1,at
0+002c <[^>]*> mflo	a0
0+0030 <[^>]*> li	at,0x8000
0+0034 <[^>]*> nop
0+0038 <[^>]*> mult	a1,at
0+003c <[^>]*> mflo	a0
0+0040 <[^>]*> li	at,-32768
0+0044 <[^>]*> nop
0+0048 <[^>]*> mult	a1,at
0+004c <[^>]*> mflo	a0
0+0050 <[^>]*> lui	at,0x1
0+0054 <[^>]*> nop
0+0058 <[^>]*> mult	a1,at
0+005c <[^>]*> mflo	a0
0+0060 <[^>]*> lui	at,0x1
0+0064 <[^>]*> ori	at,at,0xa5a5
0+0068 <[^>]*> mult	a1,at
0+006c <[^>]*> mflo	a0
	\.\.\.
0+0078 <[^>]*> mult	a0,a1
0+007c <[^>]*> mflo	a0
0+0080 <[^>]*> sra	a0,a0,0x1f
0+0084 <[^>]*> mfhi	at
0+0088 <[^>]*> beq	a0,at,0+94 <foo\+(0x|)94>
0+008c <[^>]*> nop
0+0090 <[^>]*> break	(0x0,0x6|0x6)
0+0094 <[^>]*> mflo	a0
	\.\.\.
0+00a0 <[^>]*> mult	a1,a2
0+00a4 <[^>]*> mflo	a0
0+00a8 <[^>]*> sra	a0,a0,0x1f
0+00ac <[^>]*> mfhi	at
0+00b0 <[^>]*> beq	a0,at,0+bc <foo\+(0x|)bc>
0+00b4 <[^>]*> nop
0+00b8 <[^>]*> break	(0x0,0x6|0x6)
0+00bc <[^>]*> mflo	a0
	\.\.\.
0+00c8 <[^>]*> multu	a0,a1
0+00cc <[^>]*> mfhi	at
0+00d0 <[^>]*> mflo	a0
0+00d4 <[^>]*> beqz	at,0+e0 <foo\+(0x|)e0>
0+00d8 <[^>]*> nop
0+00dc <[^>]*> break	(0x0,0x6|0x6)
0+00e0 <[^>]*> multu	a1,a2
0+00e4 <[^>]*> mfhi	at
0+00e8 <[^>]*> mflo	a0
0+00ec <[^>]*> beqz	at,0+f8 <foo\+(0x|)f8>
0+00f0 <[^>]*> nop
0+00f4 <[^>]*> break	(0x0,0x6|0x6)
	\.\.\.

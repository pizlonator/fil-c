#as: -32
#objdump: -dr --prefix-addresses
#name: MIPS mul
#source: mul.s

# Test the mul macro.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> multu	a0,a1
0+0004 <[^>]*> mflo	a0
	...
0+0010 <[^>]*> multu	a1,a2
0+0014 <[^>]*> mflo	a0
0+0018 <[^>]*> li	at,0
0+001c <[^>]*> nop
0+0020 <[^>]*> mult	a1,at
0+0024 <[^>]*> mflo	a0
0+0028 <[^>]*> li	at,1
0+002c <[^>]*> nop
0+0030 <[^>]*> mult	a1,at
0+0034 <[^>]*> mflo	a0
0+0038 <[^>]*> li	at,0x8000
0+003c <[^>]*> nop
0+0040 <[^>]*> mult	a1,at
0+0044 <[^>]*> mflo	a0
0+0048 <[^>]*> li	at,-32768
0+004c <[^>]*> nop
0+0050 <[^>]*> mult	a1,at
0+0054 <[^>]*> mflo	a0
0+0058 <[^>]*> lui	at,0x1
0+005c <[^>]*> nop
0+0060 <[^>]*> mult	a1,at
0+0064 <[^>]*> mflo	a0
0+0068 <[^>]*> lui	at,0x1
0+006c <[^>]*> ori	at,at,0xa5a5
0+0070 <[^>]*> mult	a1,at
0+0074 <[^>]*> mflo	a0
	...
0+0080 <[^>]*> mult	a0,a1
0+0084 <[^>]*> mflo	a0
0+0088 <[^>]*> sra	a0,a0,0x1f
0+008c <[^>]*> mfhi	at
0+0090 <[^>]*> beq	a0,at,0+9c <foo\+(0x|)9c>
0+0094 <[^>]*> nop
0+0098 <[^>]*> break	(0x0,0x6|0x6)
0+009c <[^>]*> mflo	a0
	...
0+00a8 <[^>]*> mult	a1,a2
0+00ac <[^>]*> mflo	a0
0+00b0 <[^>]*> sra	a0,a0,0x1f
0+00b4 <[^>]*> mfhi	at
0+00b8 <[^>]*> beq	a0,at,0+c4 <foo\+(0x|)c4>
0+00bc <[^>]*> nop
0+00c0 <[^>]*> break	(0x0,0x6|0x6)
0+00c4 <[^>]*> mflo	a0
	...
0+00d0 <[^>]*> multu	a0,a1
0+00d4 <[^>]*> mfhi	at
0+00d8 <[^>]*> mflo	a0
0+00dc <[^>]*> beqz	at,0+e8 <foo\+(0x|)e8>
0+00e0 <[^>]*> nop
0+00e4 <[^>]*> break	(0x0,0x6|0x6)
0+00e8 <[^>]*> multu	a1,a2
0+00ec <[^>]*> mfhi	at
0+00f0 <[^>]*> mflo	a0
0+00f4 <[^>]*> beqz	at,0+100 <foo\+(0x|)100>
0+00f8 <[^>]*> nop
0+00fc <[^>]*> break	(0x0,0x6|0x6)
	...

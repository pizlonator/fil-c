#as: -32
#objdump: -dr --prefix-addresses
#name: MIPS 64-bit div
#source: div64.s

# Test the div macro.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> bnez	a2,0+000c <foo\+0xc>
0+0004 <[^>]*> ddiv	zero,a1,a2
0+0008 <[^>]*> break	(0x0,0x7|0x7)
0+000c <[^>]*> (daddiu	at,zero,-1|li	at,-1)
0+0010 <[^>]*> bne	a2,at,0+0028 <foo\+0x28>
0+0014 <[^>]*> (daddiu	at,zero,1|li	at,1)
0+0018 <[^>]*> dsll32	at,at,0x1f
0+001c <[^>]*> bne	a1,at,0+0028 <foo\+0x28>
0+0020 <[^>]*> nop
0+0024 <[^>]*> break	(0x0,0x6|0x6)
0+0028 <[^>]*> mflo	a0
0+002c <[^>]*> li	at,2
0+0030 <[^>]*> nop
0+0034 <[^>]*> ddivu	zero,a1,at
0+0038 <[^>]*> mflo	a0
0+003c <[^>]*> li	at,0x8000
0+0040 <[^>]*> nop
0+0044 <[^>]*> ddiv	zero,a1,at
0+0048 <[^>]*> mfhi	a0
0+004c <[^>]*> li	at,-32768
0+0050 <[^>]*> nop
0+0054 <[^>]*> ddivu	zero,a1,at
0+0058 <[^>]*> mfhi	a0
	...

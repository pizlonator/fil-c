#as: -EB -mdialect=normal
#source: mem.s
#objdump: -dr -M hex
#name: eBPF MEM instructions, modulus lddw, big endian

.*: +file format .*bpf.*

Disassembly of section .text:

0+ <.text>:
   0:	20 00 00 00 00 00 be ef 	ldabsw 0xbeef
   8:	28 00 00 00 00 00 be ef 	ldabsh 0xbeef
  10:	30 00 00 00 00 00 be ef 	ldabsb 0xbeef
  18:	40 03 00 00 00 00 be ef 	ldindw %r3,0xbeef
  20:	48 05 00 00 00 00 be ef 	ldindh %r5,0xbeef
  28:	50 07 00 00 00 00 be ef 	ldindb %r7,0xbeef
  30:	61 21 7e ef 00 00 00 00 	ldxw %r2,\[%r1\+0x7eef\]
  38:	69 21 7e ef 00 00 00 00 	ldxh %r2,\[%r1\+0x7eef\]
  40:	71 21 7e ef 00 00 00 00 	ldxb %r2,\[%r1\+0x7eef\]
  48:	79 21 ff fe 00 00 00 00 	ldxdw %r2,\[%r1\+0xfffe\]
  50:	63 12 7e ef 00 00 00 00 	stxw \[%r1\+0x7eef\],%r2
  58:	6b 12 7e ef 00 00 00 00 	stxh \[%r1\+0x7eef\],%r2
  60:	73 12 7e ef 00 00 00 00 	stxb \[%r1\+0x7eef\],%r2
  68:	7b 12 ff fe 00 00 00 00 	stxdw \[%r1\+0xfffe\],%r2
  70:	72 10 7e ef 11 22 33 44 	stb \[%r1\+0x7eef\],0x11223344
  78:	6a 10 7e ef 11 22 33 44 	sth \[%r1\+0x7eef\],0x11223344
  80:	62 10 7e ef 11 22 33 44 	stw \[%r1\+0x7eef\],0x11223344
  88:	7a 10 ff fe 11 22 33 44 	stdw \[%r1\+0xfffe\],0x11223344
  90:	81 21 7e ef 00 00 00 00 	ldxsw %r2,\[%r1\+0x7eef\]
  98:	89 21 7e ef 00 00 00 00 	ldxsh %r2,\[%r1\+0x7eef\]
  a0:	91 21 7e ef 00 00 00 00 	ldxsb %r2,\[%r1\+0x7eef\]
  a8:	99 21 7e ef 00 00 00 00 	ldxsdw %r2,\[%r1\+0x7eef\]
  b0:	79 21 00 00 00 00 00 00 	ldxdw %r2,\[%r1\+0x0\]
  b8:	40 03 00 00 00 00 00 00 	ldindw %r3,0x0

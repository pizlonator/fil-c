#as: -EB -mdialect=pseudoc
#objdump: -dr -M hex,pseudoc
#source: mem-pseudoc.s
#name: eBPF MEM instructions, modulus lddw, pseudo-c syntax, big-endian

.*: +file format .*bpf.*

Disassembly of section .text:

0+ <.text>:
   0:	20 00 00 00 00 00 be ef 	r0=\*\(u32\*\)skb\[0xbeef\]
   8:	28 00 00 00 00 00 be ef 	r0=\*\(u16\*\)skb\[0xbeef\]
  10:	30 00 00 00 00 00 be ef 	r0=\*\(u8\*\)skb\[0xbeef\]
  18:	40 03 00 00 00 00 be ef 	r0=\*\(u32\*\)skb\[r3\+0xbeef\]
  20:	48 05 00 00 00 00 be ef 	r0=\*\(u16\*\)skb\[r5\+0xbeef\]
  28:	50 07 00 00 00 00 be ef 	r0=\*\(u8\*\)skb\[r7\+0xbeef\]
  30:	61 21 7e ef 00 00 00 00 	r2=\*\(u32\*\)\(r1\+0x7eef\)
  38:	69 21 7e ef 00 00 00 00 	r2=\*\(u16\*\)\(r1\+0x7eef\)
  40:	71 21 7e ef 00 00 00 00 	r2=\*\(u8\*\)\(r1\+0x7eef\)
  48:	79 21 ff fe 00 00 00 00 	r2=\*\(u64\*\)\(r1\+0xfffe\)
  50:	63 12 7e ef 00 00 00 00 	\*\(u32\*\)\(r1\+0x7eef\)=r2
  58:	6b 12 7e ef 00 00 00 00 	\*\(u16\*\)\(r1\+0x7eef\)=r2
  60:	73 12 7e ef 00 00 00 00 	\*\(u8\*\)\(r1\+0x7eef\)=r2
  68:	7b 12 ff fe 00 00 00 00 	\*\(u64\*\)\(r1\+0xfffe\)=r2
  70:	72 10 7e ef 11 22 33 44 	\*\(u8\*\)\(r1\+0x7eef\)=0x11223344
  78:	6a 10 7e ef 11 22 33 44 	\*\(u16\*\)\(r1\+0x7eef\)=0x11223344
  80:	62 10 7e ef 11 22 33 44 	\*\(u32\*\)\(r1\+0x7eef\)=0x11223344
  88:	7a 10 ff fe 11 22 33 44 	\*\(u64\*\)\(r1\+0xfffe\)=0x11223344
  90:	81 21 7e ef 00 00 00 00 	r2=\*\(s32\*\)\(r1\+0x7eef\)
  98:	89 21 7e ef 00 00 00 00 	r2=\*\(s16\*\)\(r1\+0x7eef\)
  a0:	91 21 7e ef 00 00 00 00 	r2=\*\(s8\*\)\(r1\+0x7eef\)
  a8:	99 21 7e ef 00 00 00 00 	r2=\*\(s64\*\)\(r1\+0x7eef\)
  b0:	61 21 00 00 00 00 00 00 	r2=\*\(u32\*\)\(r1\+0x0\)

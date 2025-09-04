#objdump: -dr
#as: -march=armv9.5-a+sme2
#as: -march=armv8-a+faminmax+sme2

.*:     file format .*

Disassembly of section \.text:

0+ <.*>:
   0:	c160b140 	famax	{z0.h-z1.h}, {z0.h-z1.h}, {z0.h-z1.h}
   4:	c160b15e 	famax	{z30.h-z31.h}, {z30.h-z31.h}, {z0.h-z1.h}
   8:	c17eb140 	famax	{z0.h-z1.h}, {z0.h-z1.h}, {z30.h-z31.h}
   c:	c17ab152 	famax	{z18.h-z19.h}, {z18.h-z19.h}, {z26.h-z27.h}
  10:	c1a0b140 	famax	{z0.s-z1.s}, {z0.s-z1.s}, {z0.s-z1.s}
  14:	c1a0b15e 	famax	{z30.s-z31.s}, {z30.s-z31.s}, {z0.s-z1.s}
  18:	c1beb140 	famax	{z0.s-z1.s}, {z0.s-z1.s}, {z30.s-z31.s}
  1c:	c1aab144 	famax	{z4.s-z5.s}, {z4.s-z5.s}, {z10.s-z11.s}
  20:	c1e0b140 	famax	{z0.d-z1.d}, {z0.d-z1.d}, {z0.d-z1.d}
  24:	c1e0b15e 	famax	{z30.d-z31.d}, {z30.d-z31.d}, {z0.d-z1.d}
  28:	c1feb140 	famax	{z0.d-z1.d}, {z0.d-z1.d}, {z30.d-z31.d}
  2c:	c1e8b15c 	famax	{z28.d-z29.d}, {z28.d-z29.d}, {z8.d-z9.d}
  30:	c160b940 	famax	{z0.h-z3.h}, {z0.h-z3.h}, {z0.h-z3.h}
  34:	c160b95c 	famax	{z28.h-z31.h}, {z28.h-z31.h}, {z0.h-z3.h}
  38:	c17cb940 	famax	{z0.h-z3.h}, {z0.h-z3.h}, {z28.h-z31.h}
  3c:	c174b950 	famax	{z16.h-z19.h}, {z16.h-z19.h}, {z20.h-z23.h}
  40:	c1a0b940 	famax	{z0.s-z3.s}, {z0.s-z3.s}, {z0.s-z3.s}
  44:	c1a0b95c 	famax	{z28.s-z31.s}, {z28.s-z31.s}, {z0.s-z3.s}
  48:	c1bcb940 	famax	{z0.s-z3.s}, {z0.s-z3.s}, {z28.s-z31.s}
  4c:	c1a4b958 	famax	{z24.s-z27.s}, {z24.s-z27.s}, {z4.s-z7.s}
  50:	c1e0b940 	famax	{z0.d-z3.d}, {z0.d-z3.d}, {z0.d-z3.d}
  54:	c1e0b95c 	famax	{z28.d-z31.d}, {z28.d-z31.d}, {z0.d-z3.d}
  58:	c1fcb940 	famax	{z0.d-z3.d}, {z0.d-z3.d}, {z28.d-z31.d}
  5c:	c1e8b94c 	famax	{z12.d-z15.d}, {z12.d-z15.d}, {z8.d-z11.d}
  60:	c128b94c 	.inst	0xc128b94c ; undefined
  64:	c160b141 	famin	{z0.h-z1.h}, {z0.h-z1.h}, {z0.h-z1.h}
  68:	c160b15f 	famin	{z30.h-z31.h}, {z30.h-z31.h}, {z0.h-z1.h}
  6c:	c17eb141 	famin	{z0.h-z1.h}, {z0.h-z1.h}, {z30.h-z31.h}
  70:	c17ab153 	famin	{z18.h-z19.h}, {z18.h-z19.h}, {z26.h-z27.h}
  74:	c1a0b141 	famin	{z0.s-z1.s}, {z0.s-z1.s}, {z0.s-z1.s}
  78:	c1a0b15f 	famin	{z30.s-z31.s}, {z30.s-z31.s}, {z0.s-z1.s}
  7c:	c1beb141 	famin	{z0.s-z1.s}, {z0.s-z1.s}, {z30.s-z31.s}
  80:	c1aab145 	famin	{z4.s-z5.s}, {z4.s-z5.s}, {z10.s-z11.s}
  84:	c1e0b141 	famin	{z0.d-z1.d}, {z0.d-z1.d}, {z0.d-z1.d}
  88:	c1e0b15f 	famin	{z30.d-z31.d}, {z30.d-z31.d}, {z0.d-z1.d}
  8c:	c1feb141 	famin	{z0.d-z1.d}, {z0.d-z1.d}, {z30.d-z31.d}
  90:	c1e8b15d 	famin	{z28.d-z29.d}, {z28.d-z29.d}, {z8.d-z9.d}
  94:	c160b941 	famin	{z0.h-z3.h}, {z0.h-z3.h}, {z0.h-z3.h}
  98:	c160b95d 	famin	{z28.h-z31.h}, {z28.h-z31.h}, {z0.h-z3.h}
  9c:	c17cb941 	famin	{z0.h-z3.h}, {z0.h-z3.h}, {z28.h-z31.h}
  a0:	c174b951 	famin	{z16.h-z19.h}, {z16.h-z19.h}, {z20.h-z23.h}
  a4:	c1a0b941 	famin	{z0.s-z3.s}, {z0.s-z3.s}, {z0.s-z3.s}
  a8:	c1a0b95d 	famin	{z28.s-z31.s}, {z28.s-z31.s}, {z0.s-z3.s}
  ac:	c1bcb941 	famin	{z0.s-z3.s}, {z0.s-z3.s}, {z28.s-z31.s}
  b0:	c1a4b959 	famin	{z24.s-z27.s}, {z24.s-z27.s}, {z4.s-z7.s}
  b4:	c1e0b941 	famin	{z0.d-z3.d}, {z0.d-z3.d}, {z0.d-z3.d}
  b8:	c1e0b95d 	famin	{z28.d-z31.d}, {z28.d-z31.d}, {z0.d-z3.d}
  bc:	c1fcb941 	famin	{z0.d-z3.d}, {z0.d-z3.d}, {z28.d-z31.d}
  c0:	c1e8b94d 	famin	{z12.d-z15.d}, {z12.d-z15.d}, {z8.d-z11.d}
  c4:	c128b94d 	.inst	0xc128b94d ; undefined

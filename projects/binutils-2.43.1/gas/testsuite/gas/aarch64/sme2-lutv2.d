#objdump: -dr
#as: -march=armv8-a+sme2+sme2p1+sme-lutv2

.*:     file format .*

Disassembly of section \.text:

0+ <.*>:
[^:]+:	c04f03e0 	movt	zt0\[0, mul vl\], z0
[^:]+:	c04f03ff 	movt	zt0\[0, mul vl\], z31
[^:]+:	c04f33e0 	movt	zt0\[3, mul vl\], z0
[^:]+:	c04f33ff 	movt	zt0\[3, mul vl\], z31
[^:]+:	c04f23f9 	movt	zt0\[2, mul vl\], z25
[^:]+:	c08b0000 	luti4	{z0.b-z3.b}, zt0, {z0-z1}
[^:]+:	c08b001c 	luti4	{z28.b-z31.b}, zt0, {z0-z1}
[^:]+:	c08b03c0 	luti4	{z0.b-z3.b}, zt0, {z30-z31}
[^:]+:	c08b0194 	luti4	{z20.b-z23.b}, zt0, {z12-z13}
[^:]+:	c09b0000 	luti4	{z0.b, z4.b, z8.b, z12.b}, zt0, {z0-z1}
[^:]+:	c09b0013 	luti4	{z19.b, z23.b, z27.b, z31.b}, zt0, {z0-z1}
[^:]+:	c09b03c0 	luti4	{z0.b, z4.b, z8.b, z12.b}, zt0, {z30-z31}
[^:]+:	c09b0191 	luti4	{z17.b, z21.b, z25.b, z29.b}, zt0, {z12-z13}
[^:]+:	c08b0194 	luti4	{z20.b-z23.b}, zt0, {z12-z13}
[^:]+:	c08b2194 	.inst	0xc08b2194 ; undefined
[^:]+:	c09b2191 	.inst	0xc09b2191 ; undefined

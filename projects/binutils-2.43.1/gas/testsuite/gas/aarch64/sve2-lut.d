#objdump: -dr
#as: -march=armv9.5-a+sve2
#as: -march=armv8-a+lut+sve2

.*:     file format .*

Disassembly of section \.text:

0+ <.*>:
[^:]+:	4520b000 	luti2	z0.b, \{z0.b\}, z0\[0\]
[^:]+:	4520b01f 	luti2	z31.b, \{z0.b\}, z0\[0\]
[^:]+:	4520b3e0 	luti2	z0.b, \{z31.b\}, z0\[0\]
[^:]+:	453fb000 	luti2	z0.b, \{z0.b\}, z31\[0\]
[^:]+:	45e0b000 	luti2	z0.b, \{z0.b\}, z0\[3\]
[^:]+:	45afb124 	luti2	z4.b, \{z9.b\}, z15\[2\]
[^:]+:	4520a800 	luti2	z0.h, \{z0.h\}, z0\[0\]
[^:]+:	4520a81f 	luti2	z31.h, \{z0.h\}, z0\[0\]
[^:]+:	4520abe0 	luti2	z0.h, \{z31.h\}, z0\[0\]
[^:]+:	453fa800 	luti2	z0.h, \{z0.h\}, z31\[0\]
[^:]+:	45e0b800 	luti2	z0.h, \{z0.h\}, z0\[7\]
[^:]+:	456fa924 	luti2	z4.h, \{z9.h\}, z15\[2\]
[^:]+:	452fb924 	luti2	z4.h, \{z9.h\}, z15\[1\]
[^:]+:	45afa924 	luti2	z4.h, \{z9.h\}, z15\[4\]
[^:]+:	4560a400 	luti4	z0.b, \{z0.b\}, z0\[0\]
[^:]+:	4560a41f 	luti4	z31.b, \{z0.b\}, z0\[0\]
[^:]+:	4560a7e0 	luti4	z0.b, \{z31.b\}, z0\[0\]
[^:]+:	457fa400 	luti4	z0.b, \{z0.b\}, z31\[0\]
[^:]+:	45e0a400 	luti4	z0.b, \{z0.b\}, z0\[1\]
[^:]+:	45efa524 	luti4	z4.b, \{z9.b\}, z15\[1\]
[^:]+:	4520bc00 	luti4	z0.h, \{z0.h\}, z0\[0\]
[^:]+:	4520bc1f 	luti4	z31.h, \{z0.h\}, z0\[0\]
[^:]+:	4520bfc0 	luti4	z0.h, \{z30.h\}, z0\[0\]
[^:]+:	453fbc00 	luti4	z0.h, \{z0.h\}, z31\[0\]
[^:]+:	45e0bc00 	luti4	z0.h, \{z0.h\}, z0\[3\]
[^:]+:	45afbd24 	luti4	z4.h, \{z9.h\}, z15\[2\]
[^:]+:	4520b400 	luti4	z0.h, \{z0.h-z1.h\}, z0\[0\]
[^:]+:	4520b41f 	luti4	z31.h, \{z0.h-z1.h\}, z0\[0\]
[^:]+:	4520b7c0 	luti4	z0.h, \{z30.h-z31.h\}, z0\[0\]
[^:]+:	4520b7e0 	luti4	z0.h, \{z31.h-z0.h\}, z0\[0\]
[^:]+:	453fb400 	luti4	z0.h, \{z0.h-z1.h\}, z31\[0\]
[^:]+:	45e0b400 	luti4	z0.h, \{z0.h-z1.h\}, z0\[3\]
[^:]+:	45afb524 	luti4	z4.h, \{z9.h-z10.h\}, z15\[2\]

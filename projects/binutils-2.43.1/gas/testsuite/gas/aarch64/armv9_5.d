#objdump: -dr

.*:     file format .*

Disassembly of section \.text:

0+ <.*>:
[^:]+:	9a002000 	addpt	x0, x0, x0
[^:]+:	0ec01c00 	famax	v0.4h, v0.4h, v0.4h
[^:]+:	4e801000 	luti2	v0.16b, {v0.16b}, v0\[0\]

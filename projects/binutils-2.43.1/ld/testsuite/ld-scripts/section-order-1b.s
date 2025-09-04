	.section .text.xxx
	.globl	xxx
xxx:
	.dc.a 0

	.section .text.yyy
	.globl	yyy
yyy:
	.dc.a 0

	.section .big
	.global big
big:
	.dc.a 0

	.section .baz
	.global baz
baz:
	.dc.a 0

	.section .text.qqq
	.global qqq
qqq:
	.dc.a 0

	.section .data.ccc
	.global ccc
ccc:
	.dc.a 0

	.data
	.global data_symbol
data_symbol:
	.dc.a 0

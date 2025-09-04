	.section .text.foo
	.globl	foo
foo:
	.dc.a 0

	.section .text.bar
	.globl	bar
bar:
	.dc.a 0

	.section .data.small
	.globl small
small:
	.dc.a 0

	.section .bar
	.global bar
bart:
	.dc.a 0

	.section .text.zzz
	.global zzz
zzz:
	.dc.a 0

	.section .data.bbb
	.global bbb
bbb:
	.dc.a 0

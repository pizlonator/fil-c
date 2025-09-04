	.section .text.foobar,"axG",%progbits,foo,comdat
	.weak foo
	.type foo,%function
foo:
	.nop
	.size foo, . - foo
	.text
	.global _start
	.set _start,foo
	.dc.a baz

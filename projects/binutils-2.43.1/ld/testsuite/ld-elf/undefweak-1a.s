	.section .text.foobar,"axG",%progbits,foo,comdat
	.weak foo
	.type foo,%function
foo:
	.nop
	.size foo, . - foo
	.weak bar
	.set bar, foo
	.text
	.global baz
	.type baz,%function
baz:
	.dc.a foo
	.size baz, . - baz

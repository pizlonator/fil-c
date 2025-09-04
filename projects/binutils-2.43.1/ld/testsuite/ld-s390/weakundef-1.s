.text
	.globl foo
foo:
	larl	%r1,d
	larl	%r1,wd
	larl	%r1,wu
	brasl	%r1,wu
	crl	%r1,wu
	lrl	%r1,wu
	strl	%r1,wu
	exrl	%r1,wu
	pfdrl	%r1,wu
	.weak	wd
	.weak	wu
.data
d:	.quad 0x123
wd:	.quad 0x123

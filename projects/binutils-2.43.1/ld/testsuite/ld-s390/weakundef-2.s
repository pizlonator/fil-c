.text
	.globl foo
foo:
	larl	%r1,d@PLT
	larl	%r1,wd@PLT
	larl	%r1,wu@PLT
	brasl	%r14,d@PLT
	brasl	%r14,wd@PLT
	brasl	%r14,wu@PLT
	.weak	wu
	.type d,@function
d:
	br	%r14
	.weak	wd
	.type wd,@function
wd:
	br	%r14

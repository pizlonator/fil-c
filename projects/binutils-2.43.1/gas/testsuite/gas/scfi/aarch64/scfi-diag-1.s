	.text
	.globl  foo
	.type   foo, @function
foo:
	str     x19, [sp, x1]
	.size   foo, .-foo

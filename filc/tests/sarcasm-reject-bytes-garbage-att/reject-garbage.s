# `.long 0x12345678` is not an x86_64 instruction encoding (78 56 34 12 =
# `js` into a ModRM that never terminates the run as a modeled instruction):
# the run cannot be fully decoded, so it stays data and the body is rejected
# with the ordinary data-in-a-function-body error.
	.text
	.globl	garbage
	.type	garbage, @function
garbage:                        ;! long(long)
	movq	%rdi, %rax
	.long	0x12345678
	ret
	.size	garbage, .-garbage
	.section	.note.GNU-stack,"",@progbits

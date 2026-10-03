# An if/else where BOTH branches end in `ud2` and there is nothing after the
# join label and no `ret` anywhere: every path ends at a ud2, so no path falls
# off the end of the body. Compile-only: calling the function traps on either
# branch.
	.text
	.globl	ud2_ifelse_both
	.type	ud2_ifelse_both, @function
ud2_ifelse_both:                ;! void(long)
	endbr64
	testq	%rdi, %rdi
	je	.Lelse
	ud2
.Lelse:
	ud2
	.size	ud2_ifelse_both, .-ud2_ifelse_both
	.section	.note.GNU-stack,"",@progbits

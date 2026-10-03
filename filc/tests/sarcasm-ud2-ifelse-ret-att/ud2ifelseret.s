# An if/else where the else branch ends in `ud2` (no `ret`, no jump over the
# join label) and the then branch does `ret`: the ud2 branch cannot fall into
# the code after it (a trap never falls through), so the body validates. This
# is exactly the shape that used to force a pointless `ret` after the ud2.
# Runnable: the then branch executes and returns the argument.
	.text
	.globl	ud2_ifelse_ret
	.type	ud2_ifelse_ret, @function
ud2_ifelse_ret:                 ;! long(long)
	endbr64
	testq	%rdi, %rdi
	je	.Lelse
	movq	%rdi, %rax
	ret
.Lelse:
	ud2
	.size	ud2_ifelse_ret, .-ud2_ifelse_ret
	.section	.note.GNU-stack,"",@progbits

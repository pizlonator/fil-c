# A loop containing a conditional break AFTER the `ud2`: the `jne .Lbreak`
# break edge is DEAD (ud2 raises #UD and never falls through, so the jne is
# unreachable), yet the body still validates: the loop is unreachable past the
# ud2, and .Lbreak is reached through the caller-side guard instead. Runnable:
# the guard skips the loop, so the function returns its argument.
	.text
	.globl	ud2_loop_deadbreak
	.type	ud2_loop_deadbreak, @function
ud2_loop_deadbreak:             ;! long(long)
	endbr64
	testq	%rdi, %rdi
	je	.Lbreak
.Ltop:
	ud2
	jne	.Lbreak
	jmp	.Ltop
.Lbreak:
	movq	%rdi, %rax
	ret
	.size	ud2_loop_deadbreak, .-ud2_loop_deadbreak
	.section	.note.GNU-stack,"",@progbits

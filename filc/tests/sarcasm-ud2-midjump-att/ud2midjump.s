# A `ud2` in the MIDDLE of a function, followed by code that is reachable via
# an explicit jump over the trap: the ud2 kills only the fall-through edge, so
# the code after it survives (still emitted, still reachable through .Lx).
# Runnable: the jump lands past the trap and the function returns.
	.text
	.globl	ud2_midjump
	.type	ud2_midjump, @function
ud2_midjump:                    ;! long(long)
	endbr64
	jmp	.Lx
	ud2
.Lx:
	movq	%rdi, %rax
	ret
	.size	ud2_midjump, .-ud2_midjump
	.section	.note.GNU-stack,"",@progbits

# An if/else where only ONE branch ends in `ud2` (terminal, fine) and the
# OTHER branch falls off the end of the function: the else branch reaches
# .Lelse with no `ret`, so the body can fall off its end and must be rejected.
# A ud2 on the sibling branch does not excuse the escaping path.
	.text
	.globl	ud2_ifelse_falloff
	.type	ud2_ifelse_falloff, @function
ud2_ifelse_falloff:             ;! void(long)
	endbr64
	testq	%rdi, %rdi
	je	.Lelse
	ud2
.Lelse:
	.size	ud2_ifelse_falloff, .-ud2_ifelse_falloff
	.section	.note.GNU-stack,"",@progbits

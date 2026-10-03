# A loop entered CONDITIONALLY (the je branches into the loop) whose skip path
# jumps over the loop and then falls off the end of the body: the ud2 inside
# the loop terminates only the loop's own paths, but the skip path reaches
# .Lend with no `ret`, so the body can fall off its end and must be rejected.
# ud2-terminal does NOT rescue bodies with genuinely escaping paths.
	.text
	.globl	ud2_cond_entry
	.type	ud2_cond_entry, @function
ud2_cond_entry:                 ;! void(long)
	endbr64
	testq	%rdi, %rdi
	je	.Ltop
	jmp	.Lend
.Ltop:
	ud2
	jmp	.Ltop
.Lend:
	.size	ud2_cond_entry, .-ud2_cond_entry
	.section	.note.GNU-stack,"",@progbits

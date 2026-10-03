# A loop entered CONDITIONALLY (the je into the loop, with the skip path
# jumping over it) that can ALSO break out before its ud2 (the in-loop `je
# .Lend`), with no `ret` after the loop: both the skip path and the break path
# reach .Lend and fall off the end of the body, so the body is rejected.
	.text
	.globl	ud2_cond_entry_break
	.type	ud2_cond_entry_break, @function
ud2_cond_entry_break:           ;! void(long)
	endbr64
	testq	%rdi, %rdi
	je	.Ltop
	jmp	.Lend
.Ltop:
	testq	%rdi, %rdi
	je	.Lend
	ud2
	jmp	.Ltop
.Lend:
	.size	ud2_cond_entry_break, .-ud2_cond_entry_break
	.section	.note.GNU-stack,"",@progbits

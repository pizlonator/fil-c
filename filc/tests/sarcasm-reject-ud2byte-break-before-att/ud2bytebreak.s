# The `.byte 0x0f,0x0b` spelling of ud2 (the form OpenSSL perlasm emits) is
# decoded to the `ud2` mnemonic before validation, so it participates in the
# control-flow validation exactly like the spelled instruction: the loop below
# can break out before the trap, and the break path falls off the end of the
# body with no `ret`, so the body is rejected.
	.text
	.globl	ud2byte_break_before
	.type	ud2byte_break_before, @function
ud2byte_break_before:           ;! void(long)
	endbr64
.Ltop:
	testq	%rdi, %rdi
	je	.Lend
	.byte	0x0f,0x0b
	jmp	.Ltop
.Lend:
	.size	ud2byte_break_before, .-ud2byte_break_before
	.section	.note.GNU-stack,"",@progbits

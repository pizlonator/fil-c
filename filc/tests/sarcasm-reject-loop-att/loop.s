/* `loop` is a conditional back-edge with an implicit rcx decrement/test that the
   CFG and register model do not track (it fell into the unknown-mnemonic fallback
   with control="none", silently dropping the edge): rejected with a clear
   message. Compilers haven't emitted these in decades. */
	.text
	.globl	f
	.type	f, @function
f:                              ;! long(long)
	movl	$0, %eax
.Lx:	addq	$2, %rax
	loop	.Lx
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits

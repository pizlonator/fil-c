/* `jrcxz` tests rcx (not the flags), but it matches the jcc mnemonic pattern, so
   it was misclassified as a flags-reading conditional branch with the rcx use
   unmodeled: rejected like `loop` (use an explicit test/cmp + conditional
   branch). */
	.text
	.globl	f
	.type	f, @function
f:                              ;! long(long)
	movl	$0, %eax
	jrcxz	.Lx
	addq	$2, %rax
.Lx:	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits

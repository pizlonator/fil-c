/* `lock xaddq %rax, (%rdi)` parses with mnemonic "lock", so the prefixed
   instruction's memory read-modify-write (and its atomicity) was completely
   unmodeled: rejected (rep/lock prefixes have implicit register effects that
   cannot be modeled). */
	.text
	.globl	f
	.type	f, @function
f:                              ;! long(ptr)
	movq	$1, %rax
	lock xaddq	%rax, (%rdi)
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits

	.text
	# A 4-byte (narrow) scalar store to the same slot offset is a FULL def of
	# the slot web -- the colored register zero-extends -- so it kills the
	# pointer-ness the `;! store ptr` seeded. A later `;! load ptr` then
	# carries a null capability (with a zeroed intval), and the deref traps.
	# Sound: a stale capability could only trap, never access out of bounds.
	.globl	kill_then_load
	.type	kill_then_load, @function
kill_then_load:                 ;! long(ptr)
	pushq	%rbp
	movq	%rsp, %rbp
	subq	$32, %rsp
	movq	%rdi, -8(%rbp)     ;! store ptr
	movl	$0, -8(%rbp)       # 4-byte scalar overwrite: kills the capability
	# the load below carries a null capability now
	movq	-8(%rbp), %rax     ;! load ptr
	movl	(%rax), %eax
	leave
	ret
	.size	kill_then_load, .-kill_then_load
	.section	.note.GNU-stack,"",@progbits

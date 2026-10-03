	.text
	# A pointer parked in a frame slot with `;! store ptr` must survive a CALL:
	# the slot web is an ordinary GPR web, so the register allocator moves it
	# to a callee-saved color or a spill across the call, and the capability's
	# lower stays rooted at its seed -- so a GC while the callee runs (or while
	# the caller is at a safepoint) keeps the object alive. After the call the
	# slot is reloaded with `;! load ptr` and dereferenced.
	.globl	slot_across_call
	.type	slot_across_call, @function
slot_across_call:               ;! long(ptr, ptr)
	pushq	%rbp
	movq	%rsp, %rbp
	subq	$32, %rsp
	movq	%rdi, %r10
	# park the object pointer in a frame slot
	movq	%rsi, -8(%rbp)     ;! store ptr
	# call a function that allocates (GC pressure while the slot holds the
	# pointer) -- the callee also scribbles its own frame
	movq	%rsi, %rdi
	call	churn              ;! long(ptr)
	# the slot still holds the pointer to the object: reload and deref it
	movq	-8(%rbp), %rax     ;! load ptr
	movq	(%rax), %rdx
	movq	8(%rax), %rcx
	# and deref through the register we kept (the first argument)
	movq	(%r10), %rsi
	addq	%rcx, %rdx
	addq	%rsi, %rdx
	movq	%rdx, %rax
	leave
	ret
	.size	slot_across_call, .-slot_across_call
	.section	.note.GNU-stack,"",@progbits

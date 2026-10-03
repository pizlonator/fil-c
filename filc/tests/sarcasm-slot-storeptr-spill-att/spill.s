	.text
	# A `;! store ptr` / `;! load ptr` roundtrip through a frame slot under
	# register pressure heavy enough to force sarcasm's register allocator to
	# spill: fourteen values stay live across the roundtrip (five callee-saved
	# pushes plus five spill slots plus four call-clobbered registers), so the
	# slot web and its lower get colored (and spilled) alongside them. All
	# fifteen results must survive. The slots sit BELOW the pushed saves (the
	# transient pad occupies -8..-40(%rbp)).
	.globl	spill_roundtrip
	.type	spill_roundtrip, @function
spill_roundtrip:                ;! long(ptr)
	pushq	%rbp
	movq	%rsp, %rbp
	pushq	%rbx
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15
	subq	$80, %rsp
	# fourteen live values, none of them the pointer
	movq	$0x1000, %rbx
	movq	$0x1001, %r12
	movq	$0x1002, %r13
	movq	$0x1003, %r14
	movq	$0x1004, %r15
	movq	$0x1005, -48(%rbp)
	movq	$0x1006, -56(%rbp)
	movq	$0x1007, -64(%rbp)
	movq	$0x1008, -72(%rbp)
	movq	$0x1009, -80(%rbp)
	movq	$0x100a, %rcx
	movq	$0x100b, %rdx
	movq	$0x100c, %r8
	movq	$0x100d, %r9
	# the capability roundtrip (the slot sits below the pad and the spills)
	movq	%rdi, -88(%rbp)    ;! store ptr
	# more pressure between the store and the load
	movq	$0x2000, %rsi
	addq	%rsi, %rcx
	addq	%rsi, %rdx
	addq	%rsi, %r8
	addq	%rsi, %r9
	movq	-88(%rbp), %rax    ;! load ptr
	movq	(%rax), %rdi       # deref: needs the slot's capability
	# fold everything so every live value is observable
	addq	%rbx, %rdi
	addq	%r12, %rdi
	addq	%r13, %rdi
	addq	%r14, %rdi
	addq	%r15, %rdi
	addq	%rcx, %rdi
	addq	%rdx, %rdi
	addq	%r8, %rdi
	addq	%r9, %rdi
	addq	-48(%rbp), %rdi
	addq	-56(%rbp), %rdi
	addq	-64(%rbp), %rdi
	addq	-72(%rbp), %rdi
	addq	-80(%rbp), %rdi
	movq	%rdi, %rax
	addq	$80, %rsp
	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%rbx
	leave
	ret
	.size	spill_roundtrip, .-spill_roundtrip
	.section	.note.GNU-stack,"",@progbits

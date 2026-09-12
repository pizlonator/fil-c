	.text
	# cmov preserves capabilities: `cmov src, dst` computes
	# dst = cond ? src : dst, and the capability follows the VALUE.
	# If the source wins and carries a capability, the destination
	# adopts it; if the destination wins, its value and capability
	# are unchanged. A cmov merging two live pointers gets a dynamic
	# (lockstep) lower that tracks the condition at runtime.
	.globl	cmov_static_take
	.type	cmov_static_take, @function
	# Destination starts scalar (null); taking the pointer source adopts
	# its capability (static sharing — one origin, no widening needed).
cmov_static_take:               ;! ptr(ptr,long)
	endbr64
	movq	$0, %rax
	testq	%rsi, %rsi
	cmovneq	%rdi, %rax
	ret
	.size	cmov_static_take, .-cmov_static_take
	.globl	cmov_keep_dest
	.type	cmov_keep_dest, @function
	# Destination starts as a pointer; a scalar source must not disturb
	# its capability when the condition loses.
cmov_keep_dest:                 ;! ptr(ptr,long)
	endbr64
	movq	%rdi, %rax
	testq	%rsi, %rsi
	cmovneq	%rsi, %rax
	ret
	.size	cmov_keep_dest, .-cmov_keep_dest
	.globl	cmov_merge_load
	.type	cmov_merge_load, @function
	# Merging two live data pointers (different objects): no static lower
	# is correct, so the merge widens to a dynamic lower maintained by a
	# conditional lower cmov — the in-asm dereference below checks against
	# whichever capability the executed path produced.
cmov_merge_load:                ;! long(ptr,ptr,long)
	endbr64
	movq	%rdi, %rax
	testq	%rdx, %rdx
	cmovneq	%rsi, %rax
	movq	(%rax), %rax
	ret
	.size	cmov_merge_load, .-cmov_merge_load
	.globl	cmov_merge_store
	.type	cmov_merge_store, @function
	# Same merge, store direction.
cmov_merge_store:               ;! void(ptr,ptr,long,long)
	endbr64
	movq	%rdi, %rax
	testq	%rdx, %rdx
	cmovneq	%rsi, %rax
	movq	%rcx, (%rax)
	ret
	.size	cmov_merge_store, .-cmov_merge_store
	.section	.note.GNU-stack,"",@progbits

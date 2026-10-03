	.text
	.globl	f
	.type	f, @function
f:                              ;! void()
	subq	$64, %rsp
	# `;! store ptr` on an FP/SIMD (xmm) slot access: a capability is an
	# 8-byte general-register value, and vector slots materialize into real
	# memory that cannot hold one.
	movq	%xmm0, -8(%rsp)    ;! store ptr
	addq	$64, %rsp
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits

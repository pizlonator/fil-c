# Feature (lea-save carriers at provable depths) inside a B2 shared-tail
# shape: `b2_jump` tail-joins into `b2_owner`'s shared tail, so the owner's
# tail is cloned into the jumper. The lea sits behind the dispatch merge
# label (each function's prologue scan stops at its own prefix, so the lea
# runs at a perturbed depth in both), and the CLONE re-runs the same lea at
# its own depth — both carriers park at provable depths, resolve statically
# (the clone's into its banded coordinate space), and the function compiles
# with the carriers dropped and real values flowing.
	.text
	.globl	b2_jump
	.type	b2_jump, @function
b2_jump:                        ;! long(long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rdi, %rbx
	testq	%rdi, %rdi
	jz	.Lskip_jump
	jmp	.Lshared
.Lskip_jump:
	xorl	%eax, %eax
	addq	$64, %rsp
	popq	%rbx
	ret
	.size	b2_jump, .-b2_jump
	.globl	b2_owner
	.type	b2_owner, @function
b2_owner:                       ;! long(long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rdi, %rbx
	nop
.Lshared:
	subq	$64, %rsp
	leaq	16(%rsp), %rbx		# carrier: entry_rsp - (d - 16)
	movq	%rdi, 32(%rbx)		# store x
	movq	%rdi, %rax
	addq	32(%rbx), %rax		# 2x (read back through the carrier)
	addq	$64, %rsp
	addq	$64, %rsp
	popq	%rbx
	ret
	.size	b2_owner, .-b2_owner
	.section	.note.GNU-stack,"",@progbits

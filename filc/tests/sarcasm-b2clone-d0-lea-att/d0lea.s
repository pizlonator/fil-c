# Feature (D0 interior lea-save carriers) on a B2 SHARED-TAIL CLONE: the
# jumper tail-joins into the owner's body, so the owner's tail (from the
# shared label on) is cloned into the jumper and executes at the jump
# site's depth — which equals the jumper's prologue depth (both functions
# push two registers and sub the same amount, the sha1-mb shared-body
# shape). The interior `leaq 48(%rsp), %r10` therefore runs at dd == D0 in
# both contexts; the D0-interior relaxation (now extended to clone
# statements) parks it as a carrier, the memory-only uses key slots through
# it, and the lea is dropped.
	.text
	.globl	d0lea_jump
	.type	d0lea_jump, @function
d0lea_jump:                     ;! long(long)
	pushq	%rbx
	pushq	%rbp
	subq	$112, %rsp
	movq	%rdi, %rbx
	jmp	.Lshared_d0
	.size	d0lea_jump, .-d0lea_jump
	.globl	d0lea_owner
	.type	d0lea_owner, @function
d0lea_owner:                    ;! long(long)
	pushq	%rbx
	pushq	%rbp
	subq	$112, %rsp
	andq	$-64, %rsp
	movq	%rdi, %rbx
	negq	%rbx
.Lshared_d0:
	leaq	48(%rsp), %r10		# interior carrier at dd == D0 in both contexts
	movq	%rbx, 0(%r10)
	movq	0(%r10), %rax
	addq	%rbx, %rax		# 2x (read back through the carrier)
	addq	$112, %rsp
	popq	%rbp
	popq	%rbx
	ret
	.size	d0lea_owner, .-d0lea_owner
	.section	.note.GNU-stack,"",@progbits

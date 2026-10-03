# Two stack-buffer groups carry absolute alignment identities that cannot
# share a placement region: both need 64-byte base alignment, but their
# bases sit 48 bytes apart while their required residues differ by 16 mod
# 64 (48 vs 0). One region pad shifts all groups uniformly, so no pad can
# satisfy both residue stories; the conflict is rejected and names the
# other group's declaration. Both groups live inside the same mid-function
# `and $-64, %rsp` window (entered via the targeted label .Lgo).
	.text
	.globl	f
	.type	f, @function
f:                              ;! long(size_t)
	subq	$512, %rsp
	jmp	.Lgo
.Lgo:
	andq	$-64, %rsp
	movq	%r10, 0(%rsp) #! stack buffer (a, %rsp, %rsp + 16)
	leaq	0(%rsp), %rbx
	addq	$8, %rbx
	movl	(%rbx), %eax #! stack buffer (a)
	movq	%r10, 48(%rsp) #! stack buffer (b, %rsp + 48, %rsp + 64)
	subq	$16, %rsp
	leaq	64(%rsp), %rcx
	addq	$8, %rcx
	movl	(%rcx), %ecx #! stack buffer (b)
	addq	$528, %rsp
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits

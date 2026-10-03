# A call from a DIFFERENT function to a mid-body label of a signatured
# function (not an entry-adjacent alias). This used to be rejected (the
# old reasoning — "entering mid-body skips the function's own frame setup"
# — was wrong): sarcasm now resolves it to a local subroutine whose region
# is a pristine copy of the owner's whole body entered at that label, and
# clones the reachable code into EACH caller. The clone runs on the caller's
# stack with the caller's registers, so this region — registers and a store
# through the caller's pointer argument only, no rsp/rbp addressing — is
# sound here. A region that DOES address the owner's frame stays rejected
# when the caller is a different function (see
# sarcasm-reject-localcall-midbody-frame-cross).
# Mechanism: localcall.discover's mid-label -> owner map claims the target
# through the fnRegion machinery (localcall.luau); the unannotated callsite
# is rewritten as a jump to the per-caller clone whose ret dispatches back
# to the continuation.
	.text
	.globl	sum5
	.type	sum5, @function
sum5:                           #! void(ptr,long,long,long,long,long)
	movq	%rsi, %rax
.Lsum5_mid:
	addq	%rdx, %rax
	addq	%rcx, %rax
	addq	%r8, %rax
	addq	%r9, %rax
	movq	%rax, (%rdi)
	ret
	.size	sum5, .-sum5
	.globl	caller6
	.type	caller6, @function
caller6:                        #! void(ptr)
	xorl	%eax, %eax
	movq	$1, %rsi
	movq	$2, %rdx
	movq	$3, %rcx
	movq	$4, %r8
	movq	$5, %r9
	call	.Lsum5_mid
	ret
	.size	caller6, .-caller6
	.section	.note.GNU-stack,"",@progbits

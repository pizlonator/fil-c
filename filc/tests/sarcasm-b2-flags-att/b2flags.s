# Flags defined in the CALLER and consumed inside a B2 clone. The jumper
# establishes flags with cmp and then unconditionally joins fb_owner's region
# mid-body; the clone's very first branch (je) reads those flags. Sarcasm must
# keep the flag liveness across the join edge (spilling/restoring around its
# own poll checks, which clobber flags). A second jumper enters the same
# region with a CONDITIONAL first-level B2 (jne) of the opposite polarity.
	.text
	.globl	fb_jmp
	.type	fb_jmp, @function
fb_jmp:                         ;! long(long,long)
	# %rdi = a, %rsi = b: flags live at the join edge.
	cmpq	%rsi, %rdi
	jmp	.Lfb_mid
	.size	fb_jmp, .-fb_jmp
	.globl	fb_jne
	.type	fb_jne, @function
fb_jne:                         ;! long(long,long)
	# conditional first-level B2 (opposite polarity into the same region);
	# the clone's je then sees the SAME flags (not taken when this jne was).
	cmpq	%rsi, %rdi
	jne	.Lfb_mid
	# fallthrough (equal): a*3 + b
	movq	%rdi, %rax
	leaq	(%rax,%rax,2), %rax
	addq	%rsi, %rax
	ret
	.size	fb_jne, .-fb_jne
	.globl	fb_owner
	.type	fb_owner, @function
fb_owner:                       ;! long(long,long)
	# owner entry (dead at runtime, like upstream's mid-body-joined bodies).
	nop
.Lfb_mid:
	# consume the caller's flags.
	je	.Lfb_eq
	# not equal: a*2 + b
	movq	%rdi, %rax
	addq	%rax, %rax
	addq	%rsi, %rax
	ret
.Lfb_eq:
	# equal: a*3 + b
	movq	%rdi, %rax
	leaq	(%rax,%rax,2), %rax
	addq	%rsi, %rax
	ret
	.size	fb_owner, .-fb_owner
	.section	.note.GNU-stack,"",@progbits

	.text

# ---------------------------------------------------------------------------
# pushfq with CHECKED memory accesses between the materialization and the
# restore: every injected bounds-check sequence writes EFLAGS, and each one is
# bracketed by sarcasm's save/restore (the program's flags are live here
# because the popfq below consumes the materialized word). The conditional
# branch after the popfq reads the flags the program had BEFORE the checks.

	.globl	pc_loads
	.type	pc_loads, @function
pc_loads:                       ;! long(ptr, long)
	movq	%rsi, %rax
	cmpq	$0, %rax
	pushfq
	movq	(%rdi), %r10         # checked load: injected bounds check
	movq	8(%rdi), %r11        # checked load: another injected check
	addq	%r10, %r11           # flags clobber with a live result
	popfq
	je	.Lcl_zero
	movq	%r11, %rax
	ret
.Lcl_zero:
	movq	%r11, %rax
	addq	$100, %rax
	ret
	.size	pc_loads, .-pc_loads

	.globl	pc_stores
	.type	pc_stores, @function
pc_stores:                      ;! long(ptr, long)
	movq	%rsi, %rax
	cmpq	$0, %rax
	pushfq
	movq	%rax, (%rdi)         # checked store: injected CanWrite + bounds checks
	movq	%rax, 8(%rdi)        # checked store
	subq	$1, %rax             # flags clobber (kept: returned on the taken path)
	popfq
	jne	.Lcs_nonzero
	movq	$5, %rax
	ret
.Lcs_nonzero:
	ret
	.size	pc_stores, .-pc_stores

	.globl	pc_loop
	.type	pc_loop, @function
pc_loop:                        ;! long(ptr, long, long)
	# a checked-load loop with the flags word live across the whole loop body
	# (including the back-edge pollcheck, another injected flag-writing
	# sequence): %r11 sums the first n words; the popfq restores the entry
	# condition (sel vs 0) and jg/jle reports it.
	movq	$0, %r11
	cmpq	$0, %rdx
	pushfq
	xorq	%r10, %r10
.Lcl_loop:
	cmpq	%rsi, %r10
	jae	.Lcl_done
	movq	(%rdi,%r10,8), %r12
	addq	%r12, %r11
	addq	$1, %r10
	jmp	.Lcl_loop
.Lcl_done:
	popfq
	jg	.Lcl_pos
	movq	%r11, %rax
	ret
.Lcl_pos:
	movq	%r11, %rax
	addq	$1000, %rax
	ret
	.size	pc_loop, .-pc_loop

	.globl	pc_df_checks
	.type	pc_df_checks, @function
pc_df_checks:                   ;! long(ptr, ptr, long)
	# the camellia residue-tail shape: a cld feeds DF into a checked rep
	# movsb, with the WHOLE thing wrapped in pushfq/popfq. The caller's DF
	# (set by an ABI-violating caller in the C main) must be restored by the
	# popfq even though the checked rep lowered its own DF test and the
	# injected checks clobbered flags in between. Returns the copied byte
	# count as a cheap liveness observable.
	pushfq
	cld
	movq	%rdx, %rcx
	rep movsb
	popfq
	movq	%rdx, %rax
	ret
	.size	pc_df_checks, .-pc_df_checks

# The ABI-violating caller: sets DF=1 and calls the helper above, which must
# (a) not trap (its cld makes the checked rep run forward) and (b) restore the
# caller's DF=1 via its popfq. The trailing cld is test hygiene (see
# pushfq-checks-main.c).
	.globl	pc_std_call
	.type	pc_std_call, @function
pc_std_call:                    ;! void(ptr, ptr, long)
	std
	call	pc_df_checks        ;! long(ptr, ptr, long)
	cld
	ret
	.size	pc_std_call, .-pc_std_call
	.section	.note.GNU-stack,"",@progbits

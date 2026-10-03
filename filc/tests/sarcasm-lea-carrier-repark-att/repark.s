# Feature (lea-save carriers at provable depths): a carrier register that was
# REDEFINED mid-body (killing the first carrier — the ordinary def-marking
# rule) can be re-parked by a second `leaq` at a different depth: the smap
# registers the fresh save exactly like the first, and accesses through the
# new carrier resolve against the new parked depth. The two carriers also
# address one shared slot through different (depth, disp) pairs — the keys
# are address-canonical, so both spellings hit one web — and the result
# proves it end to end.
	.text
	.globl	repark
	.type	repark, @function
repark:                         ;! long(long)
	jmp	.Lenter
.Lenter:
	subq	$64, %rsp
	leaq	16(%rsp), %rbx		# carrier #1: entry_rsp - 48
	movq	%rdi, -16(%rbx)		# [entry-64] = x
	movq	%rdi, %rbx		# redefine: carrier #1 dies, %rbx = x
	movq	%rbx, %rax
	shlq	$1, %rax
	addq	$1, %rax		# 2x+1
	leaq	48(%rsp), %rbx		# carrier #2: entry_rsp - 16
	movq	%rax, -32(%rbx)		# [entry-48] = 2x+1
	movq	-48(%rbx), %rax		# [entry-64] = x (same byte carrier #1 wrote)
	movq	-32(%rbx), %rdx		# [entry-48] = 2x+1
	addq	%rdx, %rax		# 3x+1
	addq	$64, %rsp
	ret
	.size	repark, .-repark
	.section	.note.GNU-stack,"",@progbits

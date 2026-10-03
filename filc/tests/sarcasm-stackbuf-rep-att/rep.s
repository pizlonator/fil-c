	.file	"rep.c"
	.text
# `#! stack buffer (...)` on rep string ops: the source or destination side is
# a stack-alias carrier (`leaq (%rsp), %reg`), the OTHER side is a checked
# heap pointer. The stack side is bounds-checked against the annotated
# buffer's range using the dynamic byte count (%rcx * element); the heap side
# keeps its usual capability checks.

# The aesni CBC shape: an IV is stored at (%rsp) and copied out to a heap
# destination through a stack-alias %rsi.
	.globl	sb_rep_iv_copy
	.type	sb_rep_iv_copy, @function
sb_rep_iv_copy:                 ;! void(ptr, ptr, size_t)
	subq	$32, %rsp
	movdqu	(%rsi), %xmm0
	movdqu	%xmm0, (%rsp)
	leaq	(%rsp), %rsi
	movq	%rdx, %rcx
	rep movsb #! stack buffer (t, %rsp, %rsp + 16)
	addq	$32, %rsp
	ret
	.size	sb_rep_iv_copy, .-sb_rep_iv_copy

# A fill (rep stosb into a stack-alias %rdi) followed by a copy back out
# through a stack-alias %rsi. Both sides of the movs are checked
# independently: %rsi against the buffer, %rdi against the heap object.
	.globl	sb_rep_fill_copy
	.type	sb_rep_fill_copy, @function
sb_rep_fill_copy:               ;! void(ptr, size_t, size_t)
	subq	$32, %rsp
	movq	%rdi, %rbx
	leaq	(%rsp), %rdi
	movq	%rsi, %rcx
	movq	%rdx, %rax
	rep stosb #! stack buffer (f, %rsp, %rsp + 16)
	leaq	(%rsp), %rsi
	movq	%rbx, %rdi
	movq	$16, %rcx
	rep movsb #! stack buffer (f)
	addq	$32, %rsp
	ret
	.size	sb_rep_fill_copy, .-sb_rep_fill_copy

# A zero count copies nothing and must not trap (hardware-faithful: the
# checks are skipped entirely when %rcx == 0).
	.globl	sb_rep_zero
	.type	sb_rep_zero, @function
sb_rep_zero:                    ;! void(ptr, ptr)
	subq	$32, %rsp
	movdqu	(%rsi), %xmm0
	movdqu	%xmm0, (%rsp)
	leaq	(%rsp), %rsi
	xorl	%ecx, %ecx
	rep movsb #! stack buffer (z, %rsp, %rsp + 16)
	addq	$32, %rsp
	ret
	.size	sb_rep_zero, .-sb_rep_zero
	.section	.note.GNU-stack,"",@progbits

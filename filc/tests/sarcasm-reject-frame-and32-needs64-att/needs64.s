	.text
# The `andq $-32, %rsp` note raises the frame's guaranteed alignment to 32, but
# the vmovdqa64 (zmm) access needs 64: the note is insufficient, so the aligned
# form is rejected outright (the unaligned vmovdqu64 form would be fine).
#
# Rejects with: "stack alignment is only 32 bytes but instruction requires 64
# byte alignment (align the frame with `and $-64, %rsp` in the prologue, or use
# the unaligned form (e.g. vmovdqu/vmovups))" on the `vmovdqa64 %zmm0, 32(%rsp)`
# store.
	.globl	needs64
	.type	needs64, @function
needs64:                        ;! long(ptr)
	pushq	%rbp
	movq	%rsp, %rbp
	subq	$192, %rsp
	andq	$-32, %rsp
	vmovdqa64	(%rdi), %zmm0
	vmovdqa64	%zmm0, 32(%rsp)
	movq	%rbp, %rsp
	popq	%rbp
	ret
	.size	needs64, .-needs64
	.section	.note.GNU-stack,"",@progbits

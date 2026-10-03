	.text
# The `andq $-32, %rsp` note proves 32-byte alignment, but each access's own
# congruence still has to hold in the note's base coordinates: 16(%rsp) is 16
# mod 32, a real address 16 bytes off the 32-byte boundary, so the aligned form
# cannot be proven safe (the unaligned vmovdqu form would be fine).
#
# Rejects with: "instruction requires 32 byte alignment, but offset is out of
# phase by 16 bytes (use an aligned offset, or use the unaligned form (e.g.
# vmovdqu/vmovups))" on the `vmovdqa %ymm0, 16(%rsp)` store.
	.globl	and32phase
	.type	and32phase, @function
and32phase:                     ;! long(ptr)
	pushq	%rbp
	movq	%rsp, %rbp
	subq	$192, %rsp
	andq	$-32, %rsp
	vmovdqa	%ymm0, 16(%rsp)
	movq	%rbp, %rsp
	popq	%rbp
	ret
	.size	and32phase, .-and32phase
	.section	.note.GNU-stack,"",@progbits

	.text
# Gap 2, the guard: the `and $-512, %rsp` note proves the frame's alignment,
# but each access's own congruence still has to hold — an offset congruent to
# 32 mod 64 names a real address 32 bytes off the 64-byte boundary, so the
# aligned form cannot be proven safe (the unaligned form would be fine).
	.globl	and512bad
	.type	and512bad, @function
and512bad:                       ;! long(ptr)
	subq	$192, %rsp
	andq	$-512, %rsp
	vmovdqa64	(%rdi), %zmm0
	vmovdqa64	%zmm0, 32(%rsp)     # 32 mod 64 != 0: misaligned
	addq	$192, %rsp
	ret
	.size	and512bad, .-and512bad
	.section	.note.GNU-stack,"",@progbits

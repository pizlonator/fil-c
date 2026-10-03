	.text
# Gap 2, the guard on the anchor path: accesses through a post-`and` anchor
# carrier key the same and-aligned coordinate system as direct rsp traffic,
# so an anchor offset congruent to 32 mod 64 is just as unprovable as a
# direct one — the aligned form stays rejected (guard: congruence genuinely
# fails).
	.globl	and512anchorbad
	.type	and512anchorbad, @function
and512anchorbad:                 ;! long(ptr)
	subq	$192, %rsp
	andq	$-512, %rsp
	leaq	64(%rsp), %r11         # the anchor carrier
	vmovdqa64	(%rdi), %zmm0
	vmovdqa64	%zmm0, 32(%r11)        # normalized 96: 96 mod 64 = 32
	addq	$192, %rsp
	ret
	.size	and512anchorbad, .-and512anchorbad
	.section	.note.GNU-stack,"",@progbits

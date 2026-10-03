	.text
# Gap 2 (alignment through post-`and` anchors): the poly1305 avx512 shape —
# the body anchors `leaq 64(%rsp), %r11` AFTER the alignment `and` and
# addresses the frame through it. The anchor parks a carrier (the same
# absolute entry-relative value on every path), so its accesses resolve to
# static normalized offsets — and they key the `and`'s coordinate system
# (the park site executes after the and), so a 64-byte-aligned access at an
# anchor offset congruent to 0 mod 64 provably keeps its alignment exactly
# like the direct rsp spelling of the same bytes (both land in one
# materialized cluster, one home).
	.globl	and512anchor
	.type	and512anchor, @function
and512anchor:                    ;! long(ptr)
	subq	$192, %rsp
	andq	$-512, %rsp
	leaq	64(%rsp), %r11         # the anchor carrier (parked depth 128)
	vmovdqa64	(%rdi), %zmm0
	vmovdqa64	%zmm0, 0(%r11)         # through the anchor: normalized 64
	vmovdqa64	%zmm0, 128(%rsp)       # direct rsp spelling: normalized 128
	vmovdqa64	0(%r11), %zmm1         # reload through the anchor
	vmovdqa64	128(%rsp), %zmm2       # reload direct
	vmovq	%xmm1, %rax
	cmpq	$17, %rax
	jne	.bad
	vmovq	%xmm2, %rax
	cmpq	$17, %rax
	jne	.bad
	movl	$1, %eax
	jmp	.done
.bad:
	xorl	%eax, %eax
.done:
	addq	$192, %rsp
	ret
	.size	and512anchor, .-and512anchor
	.section	.note.GNU-stack,"",@progbits

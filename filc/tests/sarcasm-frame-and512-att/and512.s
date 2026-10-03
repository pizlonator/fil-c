	.text
# Gap 2 (alignment through post-`and` anchors): a prologue `and $-512, %rsp`
# (transparent to the frame geometry — the prefix holds no slot traffic) makes
# the synthesized frame 512-aligned, so 64-byte-aligned vmovdqa64 traffic at
# offsets congruent to 0 mod 64 provably keeps its alignment. The frame
# extension covers traffic reaching past the `sub` (the OpenSSL poly1305
# avx512 shape: its table spills reach 0x140 above the post-and rsp while the
# sub is 0x128) — the dropped and's slack is unobservable in the output, so
# the widest post-and access extent grows the frame. Both slots reload the
# caller's data: same values, same alignment.
	.globl	and512
	.type	and512, @function
and512:                          ;! long(ptr)
	subq	$192, %rsp
	andq	$-512, %rsp
	vmovdqa64	(%rdi), %zmm0        # 64 bytes from the (64-aligned) caller buffer
	vmovdqa64	%zmm0, 0(%rsp)       # aligned store into the frame
	vmovdqa64	%zmm0, 192(%rsp)     # aligned store into the extended extent
	vmovdqa64	0(%rsp), %zmm1       # aligned reload
	vmovdqa64	192(%rsp), %zmm2     # aligned reload from the extent
	vmovq	%xmm1, %rax
	cmpq	$17, %rax               # the caller's qword pattern
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
	.size	and512, .-and512
	.section	.note.GNU-stack,"",@progbits

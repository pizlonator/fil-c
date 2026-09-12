# Intel-syntax twin of the -att chacha-shape test: dynamic 64-byte frame
# alignment combined with five incoming fast-CC stack-argument words. Exercises
# the Intel-syntax two-move entry-load emission (mov dst, [rsp+saveOff] ;
# mov dst, [dst+postPushDelta]).
	.intel_syntax noprefix
	.text
	.globl	sum9align64
	.type	sum9align64, @function
sum9align64:                    ;! long(long,long,long,long,long,long,long,long,long)
	mov	r10, QWORD PTR [rsp+8]
	mov	r11, QWORD PTR [rsp+16]
	mov	rax, QWORD PTR [rsp+24]
	push	rbx
	mov	rbx, rax
	sub	rsp, 256
	and	rsp, -64
	vpxorq	zmm0, zmm0, zmm0
	vmovdqa64	ZMMWORD PTR [rsp], zmm0
	vmovdqa64	zmm1, ZMMWORD PTR [rsp]
	vmovdqa64	ZMMWORD PTR [rsp+64], zmm1
	vmovdqa64	zmm2, ZMMWORD PTR [rsp+64]
	vmovq	rax, xmm2
	add	rax, rdi
	add	rax, rsi
	add	rax, rdx
	add	rax, rcx
	add	rax, r8
	add	rax, r9
	add	rax, r10
	add	rax, r11
	add	rax, rbx
	add	rsp, 256
	pop	rbx
	ret
	.size	sum9align64, .-sum9align64
	.section	.note.GNU-stack,"",@progbits

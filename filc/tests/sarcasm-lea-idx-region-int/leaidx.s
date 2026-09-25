	.intel_syntax noprefix
	.text
	# Intel-syntax twin of sarcasm-lea-idx-region-att: an indexed lea off %rsp
	# into the plain fixed frame, which sarcasm now REJECTS at compile time
	# (the D9 fixed-frame escape promotion that used to accept it was
	# removed) — "taking address of stack frame is not supported (cannot
	# prove safety)".
	.globl	leaidx_single
	.type	leaidx_single, @function
leaidx_single:                  ;! long(long)
	push	rbx
	push	r12
	sub	rsp, 64
	mov	r12, rdi
	lea	rdi, [rsp+16]        # frame+16 escapes: rejected at compile time
	call	fill32 ;! void(ptr)
	lea	rax, [rsp+r12*8+16]
	mov	rax, QWORD PTR [rax]
	add	rsp, 64
	pop	r12
	pop	rbx
	ret
	.size	leaidx_single, .-leaidx_single
	.globl	leaidx_two
	.type	leaidx_two, @function
leaidx_two:                     ;! long(long)
	push	rbx
	push	r12
	sub	rsp, 64
	mov	r12, rdi
	lea	rbx, [rsp+16]        # the second escaping lea (never reached)
	mov	rdi, rbx
	call	fill32 ;! void(ptr)
	lea	rax, [rbx+r12*8]
	mov	rax, QWORD PTR [rax]
	add	rsp, 64
	pop	r12
	pop	rbx
	ret
	.size	leaidx_two, .-leaidx_two
	.globl	fill32
	.type	fill32, @function
fill32:                         ;! void(ptr)
	endbr64
	mov	QWORD PTR [rdi], 100
	mov	QWORD PTR [rdi+8], 101
	mov	QWORD PTR [rdi+16], 102
	mov	QWORD PTR [rdi+24], 103
	ret
	.size	fill32, .-fill32
	.section	.note.GNU-stack,"",@progbits

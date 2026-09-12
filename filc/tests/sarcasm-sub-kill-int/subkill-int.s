	.intel_syntax noprefix
	.text
	# Intel-syntax twin of sarcasm-sub-kill-att: ptr-ptr `sub` and shifts
	# kill capabilities (see subkill.s for the AES_decrypt rationale).
	.globl	dec_select
	.type	dec_select, @function
dec_select:                     #! long(ptr,ptr)
	endbr64
	mov	rcx, rsi
	sub	rcx, rdi
	and	rcx, 0x300
	lea	rdi, [rdi+rcx]
	shr	rcx, 3
	add	rdi, rcx
	mov	rax, QWORD PTR [rdi]
	ret
	.size	dec_select, .-dec_select
	.globl	shr_ptr_kill
	.type	shr_ptr_kill, @function
shr_ptr_kill:                   #! long(ptr,ptr)
	endbr64
	mov	rax, rsi
	and	rax, 0x3f0
	shr	rax, 4
	add	rdi, rax
	mov	rax, QWORD PTR [rdi]
	ret
	.size	shr_ptr_kill, .-shr_ptr_kill
	.globl	sub_ptr_zero_add
	.type	sub_ptr_zero_add, @function
sub_ptr_zero_add:               #! long(ptr,ptr)
	endbr64
	mov	rax, rsi
	sub	rax, rsi
	add	rdi, rax
	mov	rax, QWORD PTR [rdi]
	ret
	.size	sub_ptr_zero_add, .-sub_ptr_zero_add
	.section	.note.GNU-stack,"",@progbits

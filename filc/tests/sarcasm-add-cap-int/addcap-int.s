	.intel_syntax noprefix
	.text
	# Intel-syntax twin of sarcasm-add-cap-att: `add` keeps capabilities,
	# `shr` never propagates one, and `use capability` disambiguates.
	.globl	add_shr_base
	.type	add_shr_base, @function
add_shr_base:                   #! long(ptr,long)
	endbr64
	mov	rax, rsi
	shr	rax, 3
	add	rdi, rax
	mov	rax, QWORD PTR [rdi]
	ret
	.size	add_shr_base, .-add_shr_base
	.globl	add_usecap
	.type	add_usecap, @function
add_usecap:                     #! long(ptr,ptr)
	endbr64
	mov	rax, rsi
	sub	rax, rsi
	add	rdi, rax #! use capability rdi
	mov	rax, QWORD PTR [rdi]
	ret
	.size	add_usecap, .-add_usecap
	.globl	sub_usecap
	.type	sub_usecap, @function
sub_usecap:                     #! long(ptr,long)
	endbr64
	sub	rdi, rsi #! use capability rdi
	mov	rax, QWORD PTR [rdi]
	ret
	.size	sub_usecap, .-sub_usecap
	.globl	lea_usecap_idx
	.type	lea_usecap_idx, @function
lea_usecap_idx:                 #! ptr(ptr,ptr)
	endbr64
	xor	rdi, rdi
	lea	rax, [rdi+rsi] #! use capability rsi
	ret
	.size	lea_usecap_idx, .-lea_usecap_idx
	.globl	shl_count_nocap
	.type	shl_count_nocap, @function
shl_count_nocap:                #! long(ptr,ptr)
	endbr64
	mov	rcx, rsi
	and	rcx, 3
	mov	rax, 8
	shl	rax, cl
	add	rdi, rax
	mov	rax, QWORD PTR [rdi]
	ret
	.size	shl_count_nocap, .-shl_count_nocap
	.section	.note.GNU-stack,"",@progbits

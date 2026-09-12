	.intel_syntax noprefix
	.text
	# Intel-syntax twin of sarcasm-usecap-mem-att.
	.globl	usecapmem_store
	.type	usecapmem_store, @function
usecapmem_store:                #! void(ptr,ptr,long)
	endbr64
	xor	rdi, rdi
	mov	QWORD PTR [rdi+rsi], rdx #! use capability rsi
	ret
	.size	usecapmem_store, .-usecapmem_store
	.globl	usecapmem_load
	.type	usecapmem_load, @function
usecapmem_load:                 #! long(ptr,ptr)
	endbr64
	xor	rdi, rdi
	mov	rax, QWORD PTR [rdi+rsi] #! use capability rsi
	ret
	.size	usecapmem_load, .-usecapmem_load
	.globl	usecapmem_disp_store
	.type	usecapmem_disp_store, @function
usecapmem_disp_store:           #! void(ptr,ptr,long)
	endbr64
	xor	rdi, rdi
	mov	QWORD PTR [rdi+rsi+16], rdx #! use capability rsi
	ret
	.size	usecapmem_disp_store, .-usecapmem_disp_store
	.globl	usecapmem_disp_load
	.type	usecapmem_disp_load, @function
usecapmem_disp_load:            #! long(ptr,ptr)
	endbr64
	xor	rdi, rdi
	mov	rax, QWORD PTR [rdi+rsi+16] #! use capability rsi
	ret
	.size	usecapmem_disp_load, .-usecapmem_disp_load
	.globl	usecapmem_vcopy
	.type	usecapmem_vcopy, @function
usecapmem_vcopy:                #! void(ptr,ptr,ptr)
	endbr64
	xor	rdi, rdi
	vmovdqu	ymm0, YMMWORD PTR [rdi+rsi] #! use capability rsi
	vmovdqu	YMMWORD PTR [rdx], ymm0
	vzeroupper
	ret
	.size	usecapmem_vcopy, .-usecapmem_vcopy
	.globl	usecapmem_vstore_base
	.type	usecapmem_vstore_base, @function
usecapmem_vstore_base:          #! void(ptr,ptr,ptr)
	endbr64
	vmovdqu	ymm0, YMMWORD PTR [rdx]
	xor	rdx, rdx
	vmovdqu	YMMWORD PTR [rsi+rdx], ymm0 #! use capability rsi
	vzeroupper
	ret
	.size	usecapmem_vstore_base, .-usecapmem_vstore_base
	.section	.note.GNU-stack,"",@progbits

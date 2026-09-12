	.intel_syntax noprefix
	.text
	# Intel-syntax twin of sarcasm-cmov-cap-att: cmov preserves
	# capabilities (static take, dest-keep, dynamic two-pointer merge).
	.globl	cmov_static_take
	.type	cmov_static_take, @function
cmov_static_take:               #! ptr(ptr,long)
	endbr64
	mov	rax, 0
	test	rsi, rsi
	cmovne	rax, rdi
	ret
	.size	cmov_static_take, .-cmov_static_take
	.globl	cmov_keep_dest
	.type	cmov_keep_dest, @function
cmov_keep_dest:                 #! ptr(ptr,long)
	endbr64
	mov	rax, rdi
	test	rsi, rsi
	cmovne	rax, rsi
	ret
	.size	cmov_keep_dest, .-cmov_keep_dest
	.globl	cmov_merge_load
	.type	cmov_merge_load, @function
cmov_merge_load:                #! long(ptr,ptr,long)
	endbr64
	mov	rax, rdi
	test	rdx, rdx
	cmovne	rax, rsi
	mov	rax, QWORD PTR [rax]
	ret
	.size	cmov_merge_load, .-cmov_merge_load
	.globl	cmov_merge_store
	.type	cmov_merge_store, @function
cmov_merge_store:               #! void(ptr,ptr,long,long)
	endbr64
	mov	rax, rdi
	test	rdx, rdx
	cmovne	rax, rsi
	mov	QWORD PTR [rax], rcx
	ret
	.size	cmov_merge_store, .-cmov_merge_store
	.section	.note.GNU-stack,"",@progbits

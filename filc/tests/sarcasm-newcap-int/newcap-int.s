	.intel_syntax noprefix
	.text
	# Intel-syntax twin of sarcasm-newcap-att.
	.globl	newcap_store
	.type	newcap_store, @function
newcap_store:                   #! void(ptr,ptr,long)
	endbr64
	xor	rdi, rdi
	mov	QWORD PTR [rdi+rsi], rdx #! new capability rsi
	ret
	.size	newcap_store, .-newcap_store
	.globl	newcap_load
	.type	newcap_load, @function
newcap_load:                    #! long(ptr,ptr)
	endbr64
	xor	rdi, rdi
	mov	rax, QWORD PTR [rdi+rsi] #! new capability rsi
	ret
	.size	newcap_load, .-newcap_load
	.globl	newcap_typo
	.type	newcap_typo, @function
newcap_typo:                    #! long(ptr,ptr)
	endbr64
	xor	rdi, rdi
	mov	rax, QWORD PTR [rdi+rsi] #! new capabiltiy rsi
	ret
	.size	newcap_typo, .-newcap_typo
	.section	.note.GNU-stack,"",@progbits

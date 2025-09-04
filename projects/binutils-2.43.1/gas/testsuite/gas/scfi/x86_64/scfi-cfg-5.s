	.text
	.globl foo
	.type foo, @function
foo:
	.cfi_startproc
	push  %rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	mov   %rsp, %rbp
	.cfi_def_cfa_register %rbp
	cmpl  $-1, %eax
	jne .L1
.L2:
	.cfi_remember_state
	call bar
	pop  %rbp
	.cfi_def_cfa_register %rsp
	.cfi_restore %rbp
	.cfi_def_cfa_offset 8
	ret

.L1:
	.cfi_restore_state
	testq %rax, %rax
	je .L2
	pop %rbp
	.cfi_def_cfa_register %rsp
	.cfi_restore %rbp
	.cfi_def_cfa_offset 8
	ret
	.cfi_endproc
	.size foo,.-foo

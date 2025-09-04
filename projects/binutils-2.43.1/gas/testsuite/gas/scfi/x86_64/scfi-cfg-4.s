	.text
	.globl  foo_handler
	.type   foo_handler, @function
foo_handler:
	.cfi_startproc
	pushq   %rbx
	.cfi_def_cfa_offset 16
	.cfi_offset %rbx, -16
	movl    %esi, %ebx
	subq    $16, %rsp
	.cfi_def_cfa_offset 32
	movl    current_style(%rip), %eax
	cmpl    $-1, %eax
	je      .L12
	testb   $4, %al
	jne     .L13
.L1:
	.cfi_remember_state
	addq    $16, %rsp
	.cfi_def_cfa_offset 16
	popq    %rbx
	.cfi_restore %rbx
	.cfi_def_cfa_offset 8
	ret
.L13:
	.cfi_restore_state
	movq    %rdi, 8(%rsp)
	call    foo_handler_v2
	testq   %rax, %rax
	jne     .L1
	movl    current_style(%rip), %eax
	movq    8(%rsp), %rdi
	jmp     .L3
.L12:
	addq    $16, %rsp
	.cfi_def_cfa_offset 16
	popq    %rbx
	.cfi_restore %rbx
	.cfi_def_cfa_offset 8
	jmp     xstrdup
	.cfi_endproc
	.size   foo_handler, .-foo_handler

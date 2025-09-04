# Testcase with jmp to function instead of a call.
# Also includes a jmp to label locally defined.
# The CFG creation process is not expected to warn about
# missing foo_handler_v1 or xstrdup.
	.text
	.globl  foo_handler
	.type   foo_handler, @function
foo_handler:
	.cfi_startproc
	movl    current_style(%rip), %eax
	jmp     .L2
.L2:
	cmpl    $-1, %eax
	je      .L5
	testb   $4, %al
	je      .L3
	jmp     foo_handler_v1
.L3:
	xorl    %eax, %eax
	ret
.L5:
	jmp     xstrdup
	.cfi_endproc
	.size   foo_handler, .-foo_handler

# New function with unconditional jump to a label previously defined
# in a different function.
	.globl  bar
	.type   bar, @function
bar:
	.cfi_startproc
	pushq   %rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq    %rsp, %rbp
	.cfi_def_cfa_register %rbp
	movl    $0, %eax
	jmp     .L2
	.cfi_endproc
	.size   bar, .-bar

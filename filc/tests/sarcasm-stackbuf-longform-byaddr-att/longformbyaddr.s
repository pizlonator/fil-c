	.file "longformbyaddr.c"
	.text
# LONG-form `#! stack buffer` declarations carried by accesses whose base is
# a COMPUTED buffer-address web (Feature A extension): the declaration is
# spelled %rsp-relative and resolves from the statement's own depth context,
# so it does not depend on the access's base register resolving. The base
# web must be tainted by a buffer-interior value lea (here: leaq 8(%rsp),
# %rbx, then computed on); the accesses lower to runtime bounds checks
# against the declared range followed by the raw instructions.

	.globl	lf_basic
	.type	lf_basic, @function
lf_basic:                       ;! long(size_t)
	subq	$64, %rsp
	movabsq	$0x1122334455667788, %r10
	movq	%r10, 8(%rsp) #! stack buffer (x, %rsp, %rsp + 32)
	leaq	8(%rsp), %rbx
	movl	(%rbx), %eax #! stack buffer (x, %rsp, %rsp + 32)
	movl	%eax, %r11d
	addq	$4, %rbx
	movl	(%rbx), %eax #! stack buffer (x, %rsp, %rsp + 32)
	shlq	$32, %rax
	orq	%r11, %rax
	addq	$64, %rsp
	ret
	.size	lf_basic, .-lf_basic

# ALU on the address (walking down the buffer) under long forms.
	.globl	lf_alu
	.type	lf_alu, @function
lf_alu:                         ;! long(size_t)
	subq	$64, %rsp
	movabsq	$0xfeedfacefeedface, %r10
	movq	%r10, 16(%rsp) #! stack buffer (y, %rsp, %rsp + 48)
	movq	$0x0102030405060708, %r10
	movq	%r10, 8(%rsp) #! stack buffer (y, %rsp, %rsp + 48)
	leaq	24(%rsp), %rbx
	addq	$-8, %rbx
	movl	(%rbx), %eax #! stack buffer (y, %rsp, %rsp + 48)
	subq	$8, %rbx
	movl	(%rbx), %r11d #! stack buffer (y, %rsp, %rsp + 48)
	shlq	$32, %rax
	orq	%r11, %rax
	addq	$64, %rsp
	ret
	.size	lf_alu, .-lf_alu
	.section	.note.GNU-stack,"",@progbits

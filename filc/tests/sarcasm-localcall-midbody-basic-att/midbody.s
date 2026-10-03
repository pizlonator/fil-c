# The aesni set_encrypt_key shape, reduced: a signatured function whose body
# `call`s a mid-body label (a call/ret local subroutine INSIDE the function,
# placed after the function's epilogue — the pristine OpenSSL layout). The
# mid-body label resolves to a local subroutine whose region is a pristine
# copy of the owner's whole body entered at that label; the clone is the
# reachable code (here: registers and a store through the owner's pointer —
# no frame slots), appended to the caller once and executed once per call
# (the single callsite runs in a loop, like aesni's ten
# `call .Lkey_expansion_128`s).
	.text
	.globl	expand
	.type	expand, @function
expand:                         ;! long(ptr,long)
	movq	%rdi, %rax          # the "key schedule" pointer
	movq	%rsi, %rcx          # rounds
.Lexpand_loop:
	movq	%rcx, %r10
	call	.Lexpand_round
	movq	%r9, (%rax)
	addq	$8, %rax
	subl	$1, %ecx
	jnz	.Lexpand_loop
	xorl	%eax, %eax
	ret
	.align	16
.Lexpand_round:
	leaq	(%r10,%r10), %r9    # r9 = 2*rounds ^ rounds + 1
	xorq	%rcx, %r9
	addq	$1, %r9
	ret
	.size	expand, .-expand
	.section	.note.GNU-stack,"",@progbits
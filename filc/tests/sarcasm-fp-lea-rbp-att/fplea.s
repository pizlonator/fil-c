	.text
# Frame-pointer establishment spelled with `leaq 0(%rsp), %rbp` — the lea form
# of `movq %rsp, %rbp` (OpenSSL aes-gcm-avx512's prologue idiom). Must behave
# exactly like the mov spelling: rbp holds the frame base from the lea on,
# rbp-relative traffic virtualizes, rbp- and rsp-spellings of one address
# share a slot web, and the epilogue teardown (movq %rbp,%rsp + popq %rbp)
# pairs with the push.
	.globl	fplea
	.type	fplea, @function
fplea:                          ;! unsigned(long)
	pushq	%rbp
	leaq	0(%rsp), %rbp      # the lea-spelled frame pointer establishment
	subq	$32, %rsp
	# rbp-relative slot traffic at several displacements
	movq	%rdi, -8(%rbp)
	movl	%edi, -16(%rbp)
	movb	$0x5a, -17(%rbp)
	# a slot written through rbp and read through the rsp spelling:
	# -4(%rbp) and 28(%rsp) are the same byte (rbp = rsp + 32 here) and
	# must share one slot web
	movl	$0x12345678, -4(%rbp)
	movl	28(%rsp), %edx
	# read back the rbp-relative traffic
	movq	-8(%rbp), %rax
	movzbl	-17(%rbp), %ecx
	# mix: 0x12345678 + (rdi's low dword) + 0x5a
	addl	%edx, %eax
	addl	%ecx, %eax
	addq	$32, %rsp
	movq	%rbp, %rsp
	popq	%rbp
	ret
	.size	fplea, .-fplea
	.section	.note.GNU-stack,"",@progbits

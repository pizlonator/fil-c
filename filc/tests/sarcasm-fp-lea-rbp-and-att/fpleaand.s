	.text
# The lea-spelled frame pointer establishment combined with an `and $-N, %rsp`
# frame alignment (the rsaz-avx2 prologue idiom, with OpenSSL's aes-gcm-avx512
# `leaq 0(%rsp), %rbp` in place of the mov): the and drops with its alignment
# note recorded, the establishment still parks the frame pointer, and both
# rsp-relative (post-and) and rbp-relative traffic virtualize consistently.
	.globl	fpleaand
	.type	fpleaand, @function
fpleaand:                       ;! unsigned(long)
	pushq	%rbp
	leaq	0(%rsp), %rbp      # the lea-spelled frame pointer establishment
	subq	$160, %rsp
	andq	$-32, %rsp         # vector-alignment note: dropped, frame emitted 32-aligned
	movq	%rdi, 8(%rsp)      # rsp-relative (post-and) traffic
	movq	8(%rsp), %rax
	movl	%eax, -12(%rbp)    # rbp-relative write of the same value's low dword
	movl	-12(%rbp), %edx
	movb	$0x7f, -13(%rbp)   # a byte into the same granule (sub=3)
	movzbl	-13(%rbp), %ecx
	shll	$8, %edx
	orl	%ecx, %edx         # low dword with byte 3 replaced by 0x7f
	movl	%edx, %eax
	movq	%rbp, %rsp
	popq	%rbp
	ret
	.size	fpleaand, .-fpleaand
	.section	.note.GNU-stack,"",@progbits

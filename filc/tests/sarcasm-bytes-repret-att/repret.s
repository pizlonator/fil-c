# OpenSSL perlasm ends EVERY function with `.byte 0xf3,0xc3` (rep ret; the
# 0xf3 is an AMD branch-prediction hint) instead of a spelled `ret`. Sarcasm
# decodes the byte sequence back into a real `ret` at parse time, so the frame
# pass's teardown analysis and the fall-off reachability check see an ordinary
# return — identical to the spelled form (a spelled `rep ret` keeps the
# prefix as the same hint and returns identically).
	.text
	.globl	repret
	.type	repret, @function
repret:                         ;! long(long)
	pushq	%rbx
	subq	$16, %rsp
	movq	%rdi, %rbx
	addq	$7, %rbx
	movq	%rbx, 0(%rsp)       # slot traffic inside the byte-framed body
	movq	0(%rsp), %rax
	addq	$16, %rsp
	popq	%rbx
	.byte	0xf3,0xc3           # rep ret
	.size	repret, .-repret
	.globl	repret2
	.type	repret2, @function
repret2:                        ;! long(long)
	pushq	%rbp
	movq	%rsp, %rbp
	subq	$16, %rsp
	movq	%rdi, -8(%rbp)
	movq	-8(%rbp), %rax
	addq	$5, %rax
	movq	%rbp, %rsp
	popq	%rbp
	# the same encoding split across two .byte lines (greedy accumulation
	# joins the halves into the one instruction)
	.byte	0xf3
	.byte	0xc3
	.size	repret2, .-repret2
	.section	.note.GNU-stack,"",@progbits

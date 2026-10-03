# One flag web feeding TWO conditional B1 tail calls (the sha1_block_data_order
# dispatcher shape with back-to-back jcc sites): each conditional tail branch
# becomes a jcc to a via block (call + epilogue jump) appended at the end of
# the body, yet the flags established before the first site must still be
# live at the second site (and at the local branch in tf_top2, where a local
# jcc consumes the same flag web after a B1 site was spliced in).
	.text
	.globl	tf_top
	.type	tf_top, @function
tf_top:                         ;! long(long,long)
	# both conditional B1 sites consume the cmp's flags; the fallthrough is
	# unreachable on hardware but must still compile.
	cmpq	%rsi, %rdi
	je	tf_eq
	jne	tf_ne
	movq	$-1, %rax
	ret
	.size	tf_top, .-tf_top
	.globl	tf_top2
	.type	tf_top2, @function
tf_top2:                        ;! long(long,long)
	# a B1 site followed by a LOCAL branch on the same flags
	cmpq	%rsi, %rdi
	je	tf_eq
	jne	.Ltf2_ne
.Ltf2_eq:
	# equal: a*3 + b
	movq	%rdi, %rax
	leaq	(%rax,%rax,2), %rax
	addq	%rsi, %rax
	ret
.Ltf2_ne:
	# not equal: a*2 + b
	movq	%rdi, %rax
	addq	%rax, %rax
	addq	%rsi, %rax
	ret
	.size	tf_top2, .-tf_top2
	.globl	tf_top3
	.type	tf_top3, @function
tf_top3:                        ;! long(long,long)
	# a conditional B1 site whose flags come from a cmp that ALSO feeds the
	# via-block ordering: jcc taken or not, the flags web is consumed at the
	# site itself.
	xorl	%eax, %eax
	cmpq	%rsi, %rdi
	jne	tf_ne
	leaq	5(%rdi), %rax
	ret
	.size	tf_top3, .-tf_top3
	.type	tf_eq, @function
tf_eq:                          ;! long(long,long)
	# equal: a*3 + b
	movq	%rdi, %rax
	leaq	(%rax,%rax,2), %rax
	addq	%rsi, %rax
	ret
	.size	tf_eq, .-tf_eq
	.type	tf_ne, @function
tf_ne:                          ;! long(long,long)
	# not equal: a*2 + b
	movq	%rdi, %rax
	addq	%rax, %rax
	addq	%rsi, %rax
	ret
	.size	tf_ne, .-tf_ne
	.section	.note.GNU-stack,"",@progbits

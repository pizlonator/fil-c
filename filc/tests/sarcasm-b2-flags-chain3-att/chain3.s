# A flag web surviving THREE chained B2 clones: c3_top sets flags with cmp and
# unconditionally joins c3_b's region; that clone nested-exits (unconditional)
# into c3_c's region, which does the same into c3_d's region, where a je
# finally consumes the flags — three joins later. The carried registers are
# updated en route with lea only, so no flag web is clobbered. c3_top2 joins
# c3_d's region directly with its own cmp, covering both polarities there.
	.text
	.globl	c3_top
	.type	c3_top, @function
c3_top:                         ;! long(long,long,long)
	# %rdi = a, %rsi = b, %rdx = c
	movq	%rsi, %r10
	movq	%rdx, %r11
	cmpq	%r11, %r10
	jmp	.Lc3_b
	.size	c3_top, .-c3_top
	.globl	c3_top2
	.type	c3_top2, @function
c3_top2:                        ;! long(long,long,long)
	# straight into c3_d's region with the same register/flag state
	movq	%rsi, %r10
	movq	%rdx, %r11
	leaq	3(%r10), %r10
	leaq	2(%r11), %r11
	cmpq	%r11, %r10
	jmp	.Lc3_d
	.size	c3_top2, .-c3_top2
	.globl	c3_b
	.type	c3_b, @function
c3_b:                           ;! long(long,long,long)
	nop
.Lc3_b:
	leaq	3(%r10), %r10
	jmp	.Lc3_c
	.size	c3_b, .-c3_b
	.globl	c3_c
	.type	c3_c, @function
c3_c:                           ;! long(long,long,long)
	nop
.Lc3_c:
	leaq	2(%r11), %r11
	jmp	.Lc3_d
	.size	c3_c, .-c3_c
	.globl	c3_d
	.type	c3_d, @function
c3_d:                           ;! long(long,long,long)
	nop
.Lc3_d:
	# the flags from the (grand)parent join are consumed here.
	je	.Lc3_eq
	# not equal: a + 2*r10 + r11
	leaq	(%rdi,%r10,2), %rax
	addq	%r11, %rax
	ret
.Lc3_eq:
	# equal: a + 4*r10 + r11 + 50
	leaq	(%rdi,%r10,4), %rax
	addq	%r11, %rax
	addq	$50, %rax
	ret
	.size	c3_d, .-c3_d
	.section	.note.GNU-stack,"",@progbits

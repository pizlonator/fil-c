# Chained B2 clones with nested CONDITIONAL exits and register/flag carry:
# cn_top unconditionally joins cn_b's region; cn_b's je is a nested
# conditional B2 exit into cn_c's region; cn_c re-establishes its own fresh
# flags and its je is ANOTHER nested conditional B2 exit (into cn_d's region),
# while a second conditional in cn_c is a conditional B1 tail exit (je cn_e)
# from INSIDE a clone — it becomes a via block that calls cn_e and returns
# through the jumper. Registers %r10/%r11 are defined in cn_top and used
# throughout the chain. Every outcome is reachable and distinguishable.
	.text
	.globl	cn_top
	.type	cn_top, @function
cn_top:                         ;! long(long,long,long)
	# %rdi = a, %rsi = b, %rdx = c
	movq	%rsi, %r10
	movq	%rdx, %r11
	jmp	.Lcn_b
	.size	cn_top, .-cn_top
	.globl	cn_b
	.type	cn_b, @function
cn_b:                           ;! long(long,long,long)
	nop
.Lcn_b:
	# nested conditional B2 exit #1 (flags set inside this clone)
	cmpq	%r11, %r10
	je	.Lcn_c
	# b != c: a + 2*b
	leaq	(%rdi,%r10,2), %rax
	ret
	.size	cn_b, .-cn_b
	.globl	cn_c
	.type	cn_c, @function
cn_c:                           ;! long(long,long,long)
	movq	%rsp, %rax
.Lcn_c:
	# arrived with b == c; fresh flags inside the clone.
	cmpq	%rdi, %r10
	# nested conditional B2 exit #2: taken iff a == b
	je	.Lcn_d
	# conditional B1 tail exit (call+epilogue) iff b == 11
	cmpq	$11, %r10
	je	cn_e
	# neither: a + b
	movq	%rdi, %rax
	addq	%r10, %rax
	ret
	.size	cn_c, .-cn_c
	.globl	cn_d
	.type	cn_d, @function
cn_d:                           ;! long(long,long,long)
	nop
.Lcn_d:
	# nested conditional B2 exit #2 landed here: 4*c + a - b
	leaq	(%r11,%r11,2), %rax
	addq	%r11, %rax
	addq	%rdi, %rax
	subq	%r10, %rax
	ret
	.size	cn_d, .-cn_d
	.globl	cn_e
	.type	cn_e, @function
cn_e:                           ;! long(long,long,long)
	# conditional B1 target: only the DECLARED args cross a call edge.
	# a + 77 + c
	movq	%rdx, %rax
	addq	$77, %rax
	addq	%rdi, %rax
	ret
	.size	cn_e, .-cn_e
	.section	.note.GNU-stack,"",@progbits

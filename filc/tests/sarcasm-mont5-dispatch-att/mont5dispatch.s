# Minimized bn_mul_mont_gather5 dispatch from OpenSSL's x86_64-mont5.pl,
# in the PRISTINE (un-hoisted) shape: the caller computes the dispatch value
# in %r11d and unconditionally B2-jmps into bn_mul4x_mont_gather5's region at
# .Lmul4x_enter; the CLONED region then does and/cmp on the caller-defined
# %r11d web and conditionally B2-exits (je) into bn_mulx4x_mont_gather5's
# entry-adjacent label. Sarcasm must keep the %r11 register web alive across
# the join and evaluate the je inside the clone.
	.text
	.globl	m5_top
	.type	m5_top, @function
m5_top:                         ;! long(long,long,long)
	# %rdi = selector, %rsi = a, %rdx = b. The caller defines %r11d...
	movl	%edi, %r11d
	jmp	.Lm5_4x_enter
	.size	m5_top, .-m5_top
	.globl	m5_4x
	.type	m5_4x, @function
m5_4x:                          ;! long(long,long,long)
	# owner entry (dead at runtime, exactly like upstream's
	# bn_mul4x_mont_gather5, which is only ever joined mid-body): a couple
	# of entry instructions and then the join label.
	nop
.Lm5_4x_enter:
	# ...and the CLONE consumes it: 32-bit and/cmp on the caller's web.
	andl	$0x80108, %r11d
	cmpl	$0x80108, %r11d
	je	.Lm5_x4x_enter
	# 4x body: a*4 + b
	movq	%rsi, %rax
	shlq	$2, %rax
	addq	%rdx, %rax
	ret
	.size	m5_4x, .-m5_4x
	.globl	m5_x4x
	.type	m5_x4x, @function
m5_x4x:                         ;! long(long,long,long)
	movq	%rsp, %rax
.Lm5_x4x_enter:
	# x4x body: a*8 + b + 1000 (distinguishable from the 4x body)
	movq	%rsi, %rax
	shlq	$3, %rax
	addq	%rdx, %rax
	addq	$1000, %rax
	ret
	.size	m5_x4x, .-m5_x4x
	.section	.note.GNU-stack,"",@progbits

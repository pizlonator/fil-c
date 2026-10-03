# A local call INSIDE a B2 clone (the mont5 mul4x_internal shape): lc_top
# unconditionally joins lc_own's region; the cloned region contains a plain
# `call lc_sub` to a same-file local subroutine (custom %r9/%r10 convention,
# no signature annotation), which must be cloned transitively into the jumper.
# The clone continues after the call and consumes the subroutine's results
# plus registers carried from the caller.
	.text
	.globl	lc_top
	.type	lc_top, @function
lc_top:                         ;! long(long,long,long)
	# custom-convention call: base in %r9, carried words in %r10/%r11
	movq	%rdi, %r9
	movq	%rsi, %r10
	movq	%rdx, %r11
	jmp	.Llc_mid
	.size	lc_top, .-lc_top
	.globl	lc_top2
	.type	lc_top2, @function
lc_top2:                        ;! long(long,long,long)
	# second jumper into the same region: its own clone of the region (and
	# therefore its own clone of the subroutine) must be created.
	movq	%rdi, %r9
	movq	%rsi, %r10
	movq	%rdx, %r11
	addq	$1000, %r9
	jmp	.Llc_mid
	.size	lc_top2, .-lc_top2
	.globl	lc_own
	.type	lc_own, @function
lc_own:                         ;! long(long,long,long)
	nop
.Llc_mid:
	call	lc_sub
	# lc_sub doubled %r9 in place and preserves %r10/%r11
	addq	%r10, %r9
	addq	%r11, %r9
	# fresh flags inside the clone, consuming the call's results
	cmpq	$0, %r9
	jns	.Llc_pos
	negq	%r9
.Llc_pos:
	movq	%r9, %rax
	ret
	.size	lc_own, .-lc_own
	.type	lc_sub, @function
lc_sub:
	# custom convention: %r9 = value; doubles it in place; preserves others
	addq	%r9, %r9
	ret
	.size	lc_sub, .-lc_sub
	.section	.note.GNU-stack,"",@progbits

# Feature 2 reject twin: a strictly-pre-and rsp-relative access whose modeled
# byte range OVERLAPS post-and traffic's range. Pre-and keys true
# entry-relative coordinates while post-and keys the and's slack=0
# convention, so one modeled range would name two real addresses a dynamic
# slack apart — the interval check rejects the function even though each
# side alone would compile (cf. sarcasm-preand-rsp-att, where the two sides
# occupy disjoint ranges).
	.text
	.globl	rej
	.type	rej, @function
rej:                            ;! long(long)
	jmp	.Lenter
.Lenter:
	subq	$128, %rsp
	movq	%rdi, 40(%rsp)		# pre-and store at o = -88
	movq	40(%rsp), %rax
	andq	$-32, %rsp		# mid-function realignment (dropped, recorded)
	movq	%rsi, 40(%rsp)		# post-and store at the SAME modeled offset
	movq	40(%rsp), %rsi		# (o = -88 again) -> overlap -> rejected
	addq	$128, %rsp
	ret
	.size	rej, .-rej
	.section	.note.GNU-stack,"",@progbits

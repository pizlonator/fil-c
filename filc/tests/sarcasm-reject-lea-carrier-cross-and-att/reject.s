# Feature (lea-save carriers at provable depths) reject twin: a carrier parked
# BEFORE a mid-function `and $-N, %rsp` and accessed AFTER it crosses the
# and's dynamic slack — pre- and post-and traffic share one numbering while
# the and shifts rsp by a dynamic amount, so the same key names different
# addresses on the two sides. The carrier relaxation changes nothing here:
# the crossing check rejects the shape exactly as it rejects the mov-save
# form (the single-clone twin of sarcasm-reject-b2-cross-and-att).
	.text
	.globl	rej
	.type	rej, @function
rej:                            ;! long(long)
	jmp	.Lenter
.Lenter:
	subq	$64, %rsp
	leaq	16(%rsp), %rbx		# carrier parked pre-and
	andq	$-32, %rsp		# mid-function realignment (dropped, recorded)
	movq	%rdi, 16(%rbx)		# access AFTER the and -> crossing -> rejected
	movq	%rbx, %rsp		# (epilogue would recover through the carrier)
	addq	$48, %rsp
	ret
	.size	rej, .-rej
	.section	.note.GNU-stack,"",@progbits

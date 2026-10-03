# Feature 3 reject twin: a masked stack access whose full (lowered) footprint
# lands OUTSIDE the input frame is a compile-time rejection — the lowering
# bounds-proves the FULL vector width against the frame, so an out-of-frame
# masked store cannot hide behind its mask (the masked-off lanes would still
# be touched by the widened traffic).
	.text
	.globl	rej
	.type	rej, @function
rej:                            ;! long(long)
	subq	$64, %rsp
	vmovdqu64	%zmm0, 128(%rsp){%k1}	# the frame ends at 64 -> rejected
	movq	%rdi, %rax
	addq	$64, %rsp
	ret
	.size	rej, .-rej
	.section	.note.GNU-stack,"",@progbits

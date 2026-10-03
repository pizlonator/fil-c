# An annotation ON a data-directive run transfers to the first decoded
# instruction: `.long 0x9066A4F3  #! stack buffer (...)` is a decoded rep
# movsb carrying that annotation — validated exactly like the spelled form.
# The copy's source is this function's own 32-byte frame (the stack buffer
# annotation covers [rsp, rsp+16)), the destination a checked heap pointer.
	.text
	.globl	annot_rep
	.type	annot_rep, @function
annot_rep:                      ;! void(ptr, ptr)
	subq	$32,%rsp
	# stash 16 bytes of the source into the frame temp (checked accesses)
	movq	(%rsi), %rax
	movq	%rax, (%rsp)
	movq	8(%rsi), %rax
	movq	%rax, 8(%rsp)
	# decoded rep, annotated: source = the frame temp, dest = the heap arg
	lea	(%rsp), %rsi
	movl	$16, %ecx
	.long	0x9066A4F3	#! stack buffer (annotrep, %rsp, %rsp + 16) # rep movsb
	addq	$32,%rsp
	ret
	.size	annot_rep, .-annot_rep
	.section	.note.GNU-stack,"",@progbits

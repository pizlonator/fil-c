# Feature (lea-save carriers at provable depths) reject twin: a lea at a
# perturbed provable depth whose anchor lies OUTSIDE the frame's real extent
# (here 0x10000 bytes above the entry rsp) is not a carrier — the target
# window check keeps the historical behavior: the carrier is refused and the
# rewrite rejects the frame-address escape exactly as before the relaxation.
# (With an in-bounds dereference the use-check would reject the access
# instead — same rejection, different site.)
	.text
	.globl	rej
	.type	rej, @function
rej:                            ;! long(long)
	jmp	.Lenter
.Lenter:
	subq	$64, %rsp
	leaq	0x10000(%rsp), %rbx	# anchors far above the frame -> rejected
	movq	%rdi, %rax		# (the carrier itself is never dereferenced)
	addq	$64, %rsp
	ret
	.size	rej, .-rej
	.section	.note.GNU-stack,"",@progbits

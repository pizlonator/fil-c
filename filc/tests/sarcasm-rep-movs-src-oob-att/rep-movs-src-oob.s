	.file	"rep-movs-src-oob.c"
	.text
	# Overrunning the SOURCE must panic: 8 quadwords (64 bytes) out of a
	# 2-quadword (16-byte) buffer into a large destination.
	.globl	rep_movsq
	.type	rep_movsq, @function
rep_movsq:                      ;! void(ptr, ptr, size_t)
	movq	%rdx, %rcx
	rep movsq
	ret
	.size	rep_movsq, .-rep_movsq
	.section	.note.GNU-stack,"",@progbits

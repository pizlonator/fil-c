	.file	"rep-stos-oob.c"
	.text
	# Overrunning the fill destination must panic: 8 quadwords (64 bytes)
	# into a 2-quadword (16-byte) buffer.
	.globl	rep_stosq
	.type	rep_stosq, @function
rep_stosq:                      ;! void(ptr, size_t, long)
	movq	%rsi, %rcx
	movq	%rdx, %rax
	rep stosq
	ret
	.size	rep_stosq, .-rep_stosq
	.section	.note.GNU-stack,"",@progbits

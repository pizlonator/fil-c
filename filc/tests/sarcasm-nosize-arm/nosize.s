/* Handwritten asm often omits .size.  The function splitter must end first's
   body at the next function's `.type ..., %function` declaration so that both
   functions are emitted. */
	.text
	.global	first
	.type	first, %function
first:                          ;! int(int)
	add	w0, w0, #1
	ret
	.global	second
	.type	second, %function
second:                         ;! int(int)
	add	w0, w0, #2
	ret
	.size	second, .-second
	.section	.note.GNU-stack,"",@progbits

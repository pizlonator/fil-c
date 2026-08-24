/* Large frames are allocated with a shifted immediate (`sub sp, sp, #10, lsl
   #12` = 40960 bytes) -- this is how clang/gcc emit them.  The frame-size
   accounting must apply the shift, otherwise every slot access is falsely
   rejected as outside the frame. */
	.text
	.global	bf
	.type	bf, %function
bf:                             ;! long(long)
	sub	sp, sp, #10, lsl #12
	add	x1, x0, #5
	str	x1, [sp, #39992]
	ldr	x0, [sp, #39992]
	add	sp, sp, #10, lsl #12
	ret
	.size	bf, .-bf
	.section	.note.GNU-stack,"",@progbits

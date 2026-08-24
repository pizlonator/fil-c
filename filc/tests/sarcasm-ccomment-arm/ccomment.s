/* The arm64 parser must strip C comments: inline block comments trailing an
   instruction, whole-line block comments, and multi-line block comments.
   Without stripping, the comment text was parsed into operands (crash) or
   as bogus instructions. */
	.text
	.global	loadadd
	.type	loadadd, %function
loadadd:                        ;! int(ptr)
	ldr	x0, [x0] /* hi */ ;! load ptr
	/* a whole-line block comment */
	ldr	w1, [x0]
	/* multi
	   line /* nested-looking
	   block comment */
	ldr	w2, [x0, #4]
	add	w0, w1, w2 /* trailing comment */
	ret
	.size	loadadd, .-loadadd
	.section	.note.GNU-stack,"",@progbits

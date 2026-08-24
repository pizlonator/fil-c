/* A .macro definition expands to code sarcasm cannot see, so macro definitions
   are rejected rather than silently dropped. */
	.text
	.macro	INCREMENT reg
	add	\reg, \reg, #1
	.endm
	.global	f
	.type	f, %function
f:                              ;! unsigned(void)
	mov	x0, #0
	INCREMENT x0
	ret
	.size	f, .-f

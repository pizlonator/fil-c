/* .equ defines a symbol-as-value whose uses sarcasm cannot track, so it is
   rejected (globals cannot be made memory-safe automatically). */
	.text
	.equ	MYCONST, 42
	.global	f
	.type	f, %function
f:                              ;! unsigned(void)
	mov	x0, MYCONST
	ret
	.size	f, .-f

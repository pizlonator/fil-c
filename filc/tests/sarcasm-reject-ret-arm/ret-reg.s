/* ret with an explicit register other than x30 is an indirect return through a
   raw register: rejected. */
	.text
	.global	f
	.type	f, %function
f:                              ;! void(void)
	ret	x5
	.size	f, .-f

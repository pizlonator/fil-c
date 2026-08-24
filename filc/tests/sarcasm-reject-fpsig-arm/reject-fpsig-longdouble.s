/* long double in a ;! signature is rejected (not yet supported). */
	.text
	.global	h
	.type	h, %function
h:                              ;! long double(ptr)
	ret
	.size	h, .-h

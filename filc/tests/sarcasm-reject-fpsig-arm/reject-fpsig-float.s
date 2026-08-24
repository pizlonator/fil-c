/* float arguments in a ;! signature are rejected (not yet supported). */
	.text
	.global	g
	.type	g, %function
g:                              ;! float(float, ptr)
	ret
	.size	g, .-g

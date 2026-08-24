/* An FP type in a CALLSITE annotation is rejected just like in an entry
   signature: the callsite thunk would have to marshal FP arguments. */
	.text
	.global	foo
	.type	foo, %function
foo:                            ;! int(ptr)
	bl	ext             ;! double(ptr, int)
	ret
	.size	foo, .-foo

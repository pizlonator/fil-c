	.text
	.global foo
	.type  foo, %function
foo:
	.cfi_startproc
.L7:
	add     w4, w3, w1
	cbnz    w4, .L7
	cbz     w4, .L10
	tbnz    w0, #31, .L7
.L10:
	ret
	.cfi_endproc
	.size   foo, .-foo

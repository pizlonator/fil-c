	.text
	.globl	conv
	.type	conv, %function
conv:                           ;! void(ptr, long, long)
	scvtf	d0, x1
	scvtf	d1, x2
	str	d0, [x0]
	str	d1, [x0, #8]
	ret
	.size	conv, .-conv
	.globl	roundtrip
	.type	roundtrip, %function
roundtrip:                      ;! long(long)
	scvtf	d0, x0
	fmov	x1, d0
	fmov	d1, x1
	fcvtzs	x0, d1
	ret
	.size	roundtrip, .-roundtrip
	.globl	lanes
	.type	lanes, %function
lanes:                          ;! long(long, long)
	dup	v0.2d, x0
	ins	v0.d[1], x1
	umov	x2, v0.d[1]
	smov	x3, v0.s[2]
	umov	w4, v0.b[1]
	smov	x5, v0.h[0]
	add	x0, x2, x3
	add	x0, x0, x4
	add	x0, x0, x5
	ret
	.size	lanes, .-lanes
	.section	.note.GNU-stack,"",@progbits
